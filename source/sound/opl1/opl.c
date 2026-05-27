/*
 * MultiRexZ80
 *
 * Multi-system Z80 emulator based on SMS Plus GX by Eke-Eke, itself based on
 * SMS Plus by Charles MacDonald.
 *
 * Default project license: GPL-2.0-or-later.  File-specific notices below
 * are retained and take precedence for imported or derived components,
 * including MAME-derived code and other third-party modules.
 */

/*
 * MultiRexZ80 unified Yamaha OPL/OPLL sound core.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Integration/adapter code for MultiRexZ80: gameblabla.
 *
 * OPL1/Y8950 core:
 *   MAME 0.72-derived FM OPL/Delta-T core, GPL-2.0-or-later. Used for
 *   Psycho Soldier's YM3526 and Y8950.
 *
 * OPL2 fallback core:
 *   emu8950 v1.1.4, Copyright (C) 2001-2020 Mitsutaka Okazaki,
 *   MIT licensed. Retained for YM3812 callers.
 *
 * OPLL core:
 *   CrabEmu/MAME YM2413, Copyright (C) 2012/2016 Lawrence Sebald and
 *   Jarek Burczynski/Ernesto Corvi, GPL-2.0-or-later. Used for
 *   Master System FM Unit compatibility.
 *
 * This file keeps all Yamaha FM backends behind one C99 API.
 *
 * Accuracy merge notes:
 *   - The Y8950 Delta-T external-ROM path follows MAME/libvgm address
 *     wrapping semantics for mapped ADPCM-B memory.
 *   - The YM2413 path preserves the emulator mixer convention used by
 *     MAME/OpenMSX-derived OPLL cores: melody and rhythm are generated as
 *     separate buses and summed by the SMS mixer.
 */

#include "opl.h"
#include "crab_ym2413.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OPL_CLAMP16(v) ((v) > 32767 ? 32767 : ((v) < -32768 ? -32768 : (v)))
/* Output calibration against the supplied MAME recordings.  These gains do
 * not change chip state or timing; they normalize each backend at the mixer
 * boundary so imported cores keep the same relative loudness as MAME captures. */
#define OPL_GAIN_APPLY(v, num, den) OPL_CLAMP16(((int32_t)(v) * (int32_t)(num)) / (int32_t)(den))
#define OPL_OPLL_GAIN_NUM 29
#define OPL_OPLL_GAIN_DEN 16
#define OPL_EMU8950_GAIN_NUM 7
#define OPL_EMU8950_GAIN_DEN 2
#define OPL_STATUS_TIMER_B 0x20
#define OPL_STATUS_TIMER_A 0x40
#define OPL_STATUS_IRQ     0x80

/* ------------------------------------------------------------------------- */
/* Embedded emu8950 ADPCM declarations                                        */
/* ------------------------------------------------------------------------- */
#include <stdint.h>

typedef struct __OPL_ADPCM {
  uint32_t clk;

  uint8_t reg[0x20];

  uint8_t *wave;      /* ADPCM DATA */
  uint8_t *memory[2]; /* [0] RAM, [1] ROM */

  uint8_t status;

  uint32_t start_addr;
  uint32_t stop_addr;
  uint32_t play_addr;  /* Current play address * 2 */
  uint32_t delta_addr; /* 16bit address */
  uint32_t delta_n;
  uint32_t play_addr_mask;

  uint8_t play_start;

  int32_t output[2];
  uint32_t diff;

} OPL_ADPCM;

OPL_ADPCM *OPL_ADPCM_new(uint32_t clk);
void OPL_ADPCM_reset(OPL_ADPCM *);
void OPL_ADPCM_delete(OPL_ADPCM *);
void OPL_ADPCM_writeReg(OPL_ADPCM *, uint32_t reg, uint32_t val);
int16_t OPL_ADPCM_calc(OPL_ADPCM *);
uint8_t OPL_ADPCM_status(OPL_ADPCM *);
void OPL_ADPCM_resetStatus(OPL_ADPCM *);
void OPL_ADPCM_writeRAM(OPL_ADPCM *, uint32_t start, uint32_t length, const uint8_t *data);
void OPL_ADPCM_writeROM(OPL_ADPCM *, uint32_t start, uint32_t length, const uint8_t *data);

/* ------------------------------------------------------------------------- */
/* Embedded emu8950 OPL declarations                                          */
/* ------------------------------------------------------------------------- */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OPL_DEBUG 0

/* voice data */
typedef struct __OPL_PATCH {
  uint8_t TL, FB, EG, ML, AR, DR, SL, RR, KR, KL, AM, PM, WS;
} OPL_PATCH;

/* mask */
#define OPL_MASK_CH(x) (1 << (x))
#define OPL_MASK_HH (1 << 9)
#define OPL_MASK_CYM (1 << 10)
#define OPL_MASK_TOM (1 << 11)
#define OPL_MASK_SD (1 << 12)
#define OPL_MASK_BD (1 << 13)
#define OPL_MASK_ADPCM (1 << 14)
#define OPL_MASK_RHYTHM (OPL_MASK_HH | OPL_MASK_CYM | OPL_MASK_TOM | OPL_MASK_SD | OPL_MASK_BD)

/* rate conveter */
typedef struct __OPL_RateConv {
  int ch;
  double timer;
  double f_ratio;
  int16_t *sinc_table;
  int16_t **buf;
} OPL_RateConv;

OPL_RateConv *OPL_RateConv_new(double f_inp, double f_out, int ch);
void OPL_RateConv_reset(OPL_RateConv *conv);
void OPL_RateConv_putData(OPL_RateConv *conv, int ch, int16_t data);
int16_t OPL_RateConv_getData(OPL_RateConv *conv, int ch);
void OPL_RateConv_delete(OPL_RateConv *conv);

/* slot */
typedef struct __OPL_SLOT {
  uint8_t number;

  /* type flags:
   * 000000SM 
   *       |+-- M: 0:modulator 1:carrier
   *       +--- S: 0:normal 1:single slot mode (sd, tom, hh or cym) 
   */
  uint8_t type;

  OPL_PATCH __patch;  
  OPL_PATCH *patch;  /* = alias for __patch */

  /* slot output */
  int32_t output[2]; /* output value, latest and previous. */

  /* phase generator (pg) */
  uint16_t *wave_table; /* wave table */
  uint32_t pg_phase;    /* pg phase */
  uint32_t pg_out;      /* pg output, as index of wave table */
  uint8_t pg_keep;      /* if 1, pg_phase is preserved when key-on */
  uint16_t blk_fnum;    /* (block << 9) | f-number */
  uint16_t fnum;        /* f-number (9 bits) */
  uint8_t blk;          /* block (3 bits) */

  /* envelope generator (eg) */
  uint8_t eg_state;         /* current state */
  uint16_t tll;             /* total level + key scale level*/
  uint8_t rks;              /* key scale offset (rks) for eg speed */
  uint8_t eg_rate_h;        /* eg speed rate high 4bits */
  uint8_t eg_rate_l;        /* eg speed rate low 2bits */
  uint32_t eg_shift;        /* shift for eg global counter, controls envelope speed */
  int16_t eg_out;           /* eg output */

  uint32_t update_requests; /* flags to debounce update */

#if OPL_DEBUG
  uint8_t last_eg_state;
#endif
} OPL_SLOT;

typedef struct __OPL {
  OPL_ADPCM *adpcm;
  uint32_t clk;
  uint32_t rate;

  uint8_t chip_type;

  uint32_t adr;

  uint8_t csm_mode;
  uint8_t csm_key_count;
  uint8_t notesel;

  uint32_t inp_step;
  uint32_t out_step;
  uint32_t out_time;

  uint8_t reg[0x100];
  uint8_t test_flag;
  uint32_t slot_key_status;
  uint8_t rhythm_mode;

  uint32_t eg_counter;

  uint32_t pm_phase;
  uint32_t pm_dphase;

  int32_t am_phase;
  int32_t am_dphase;
  uint8_t lfo_am;

  uint32_t noise;
  uint8_t short_noise;

  OPL_SLOT slot[18];
  uint8_t ch_alg[9]; // alg for each channels

  uint8_t pan[16];
  float pan_fine[16][2];

  uint32_t mask;
  uint8_t am_mode;
  uint8_t pm_mode;

  /* channel output */
  /* 0..8:tone 9:bd 10:hh 11:sd 12:tom 13:cym 14:adpcm */
  int16_t ch_out[15];

  int16_t mix_out[2];

  OPL_RateConv *conv;

  uint32_t timer1_counter; //  80us counter
  uint32_t timer2_counter; // 320us counter
  void *timer1_user_data;
  void *timer2_user_data;
  void (*timer1_func)(void *user);
  void (*timer2_func)(void *user);
  uint8_t status;

} OPL;

OPL *OPL_new(uint32_t clk, uint32_t rate);
void OPL_delete(OPL *);

void OPL_reset(OPL *);

/** 
 * Set output wave sampling rate. 
 * @param rate sampling rate. If clock / 72 (typically 49716 or 49715 at 3.58MHz) is set, the internal rate converter is disabled.
 */
void OPL_setRate(OPL *opl, uint32_t rate);

/** 
 * Set internal calcuration quality. Currently no effects, just for compatibility.
 * >= v1.0.0 always synthesizes internal output at clock/72 Hz.
 */
void OPL_setQuality(OPL *opl, uint8_t q);

/**
 * Set OPL chip type.
 * @param type 0:Y8950, 1:YM3526, 2:YM3812
 */
void OPL_setChipType(OPL *opl, uint8_t type);

/** 
 * Set pan pot (extra function - not YM2413 chip feature)
 * @param ch 0..8:tone 9:bd 10:hh 11:sd 12:tom 13:cym 14,15:reserved
 * @param pan 0:mute 1:right 2:left 3:center 
 * ```
 * pan: 76543210
 *            |+- bit 1: enable Left output
 *            +-- bit 0: enable Right output
 * ```
 */
void OPL_setPan(OPL *opl, uint32_t ch, uint8_t pan);

/**
 * Set fine-grained panning
 * @param ch 0..8:tone 9:bd 10:hh 11:sd 12:tom 13:cym 14,15:reserved
 * @param pan output strength of left/right channel. 
 *            pan[0]: left, pan[1]: right. pan[0]=pan[1]=1.0f for center.
 */
void OPL_setPanFine(OPL *opl, uint32_t ch, float pan[2]);

void OPL_writeIO(OPL *opl, uint32_t reg, uint8_t val);
void OPL_writeReg(OPL *opl, uint32_t reg, uint8_t val);

/**
 * Calculate sample
 */
int16_t OPL_calc(OPL *opl);

/**
 * Calulate stereo sample
 */
void OPL_calcStereo(OPL *opl, int32_t out[2]);

/** 
 *  Set channel mask 
 *  @param mask mask flag: OPL_MASK_* can be used.
 *  - bit 0..8: mask for ch 1 to 9 (OPL_MASK_CH(i))
 *  - bit 9: mask for Hi-Hat (OPL_MASK_HH)
 *  - bit 10: mask for Top-Cym (OPL_MASK_CYM)
 *  - bit 11: mask for Tom (OPL_MASK_TOM)
 *  - bit 12: mask for Snare Drum (OPL_MASK_SD)
 *  - bit 13: mask for Bass Drum (OPL_MASK_BD)
 */
uint32_t OPL_setMask(OPL *, uint32_t mask);

/**
 * Toggler channel mask flag
 */
uint32_t OPL_toggleMask(OPL *, uint32_t mask);

uint8_t OPL_readIO(OPL *opl);

/**
 * Read OPL status register
 * @returns
 * 76543210
 * |||||  +- D0: PCM/BSY
 * ||||+---- D3: BUF/RDY
 * |||+----- D4: EOS
 * ||+------ D5: TIMER2
 * |+------- D6: TIMER1
 * +-------- D7: IRQ
 */
uint8_t OPL_status(OPL *opl);

void OPL_writeADPCMData(OPL *opl, uint8_t type, uint32_t start, uint32_t length, const uint8_t *data);

/* for compatibility */
#define OPL_set_rate OPL_setRate
#define OPL_set_quality OPL_setQuality
#define OPL_set_pan OPL_setPan
#define OPL_set_pan_fine OPL_setPanFine
#define OPL_calc_stereo OPL_calcStereo

#ifdef __cplusplus
}
#endif

/* ------------------------------------------------------------------------- */
/* Embedded emu8950 ADPCM implementation                                      */
/* ------------------------------------------------------------------------- */
/**
 * ADPCM for Y8950
 */
#include <stdio.h>
#include <stdlib.h>

#define DMAX 0x5FFF
#define DMIN 0x7F
#define DDEF 0x7F

#define DECODE_MAX 32767
#define DECODE_MIN (-32768)

#define CLAP(min, x, max) ((x < min) ? min : (max < x) ? max : x)

/* Bitmask for register $07 */
#define R07_RESET 1
#define R07_SP_OFF 8
#define R07_REPEAT 16
#define R07_MEMORY_DATA 32
#define R07_REC 64
#define R07_START 128

/* Bitmask for register $08 */
#define R08_ROM 1
#define R08_64K 2
#define R08_DA_AD 4
#define R08_SAMPL 8
#define R08_NOTE_SET 64
#define R08_CSM 128

/* Bit for status register */
#define STATUS_PCM_BSY 1
#define STATUS_BUF_RDY 8
#define STATUS_EOS 16

#define RAM_SIZE (256 * 1024)
#define ROM_SIZE (256 * 1024)


OPL_ADPCM *OPL_ADPCM_new(uint32_t clk) {
  OPL_ADPCM *_this;

  _this = (OPL_ADPCM *)malloc(sizeof(OPL_ADPCM));
  if (!_this)
    return NULL;

  _this->clk = clk;

  /* 256Kbytes RAM */
  _this->memory[0] = (uint8_t *)malloc(RAM_SIZE);
  if (!_this->memory[0])
    goto Error_Exit;
  memset(_this->memory[0], 0, RAM_SIZE);

  /* 256Kbytes ROM */
  _this->memory[1] = (uint8_t *)malloc(ROM_SIZE);
  if (!_this->memory[1])
    goto Error_Exit;
  memset(_this->memory[1], 0, ROM_SIZE);

  OPL_ADPCM_reset(_this);

  return _this;

Error_Exit:
  OPL_ADPCM_delete(_this);
  return NULL;
}

void OPL_ADPCM_delete(OPL_ADPCM *_this) {
  if (_this) {
    free(_this->memory[0]);
    free(_this->memory[1]);
    free(_this);
  }
}

void OPL_ADPCM_reset(OPL_ADPCM *_this) {
  int i;

  for (i = 0; i < 0x20; i++)
    _this->reg[i] = 0;

  _this->play_start = 0;
  _this->status = 0;
  _this->play_addr = 0;
  _this->start_addr = 0;
  _this->stop_addr = 0;
  _this->delta_addr = 0;
  _this->delta_n = 0;
  _this->wave = _this->memory[0];
  _this->play_addr_mask = _this->reg[0x08] & R08_64K ? (1 << 17) - 1 : (1 << 19) - 1;
  _this->output[0] = _this->output[1] = 0;
}

#define DELTA_ADDR_MAX (1 << 16)
#define DELTA_ADDR_MASK (DELTA_ADDR_MAX - 1)

/* Update OPL_ADPCM data stage (Register $0F) */
static inline int update_stage(OPL_ADPCM *_this) {
  _this->delta_addr += _this->delta_n;

  if (_this->delta_addr & DELTA_ADDR_MAX) {
    _this->delta_addr &= DELTA_ADDR_MASK;
    _this->play_addr = (_this->play_addr + 1) & (_this->play_addr_mask);

    if (_this->play_addr == (_this->stop_addr & _this->play_addr_mask)) {
      if (_this->reg[0x07] & R07_REPEAT) {
        _this->play_addr = _this->start_addr & (_this->play_addr_mask);
      } else {
        _this->play_start = 0;
        _this->status &= ~STATUS_PCM_BSY;
        _this->status |= STATUS_EOS;
      }
    } else {
      _this->reg[0x0F] = _this->wave[_this->play_addr >> 1];
    }

    return 1;
  }

  return 0;
}

static inline void adpcm_update_output(OPL_ADPCM *_this, uint32_t val) {
  static uint32_t F[] = {
      57, 57, 57, 57, 77, 102, 128, 153 // This table values are from ymdelta.c by Tatsuyuki Satoh.
  };

  _this->output[1] = _this->output[0];

  if (val & 8)
    _this->output[0] -= (_this->diff * ((val & 7) * 2 + 1)) >> 3;
  else
    _this->output[0] += (_this->diff * ((val & 7) * 2 + 1)) >> 3;

  _this->output[0] = CLAP(DECODE_MIN, _this->output[0], DECODE_MAX);
  _this->diff = CLAP(DMIN, (_this->diff * F[val & 7]) >> 6, DMAX);
}

static inline uint32_t calc(OPL_ADPCM *_this) {
  uint32_t val;

  if (_this->play_start && update_stage(_this)) {
    if (_this->play_addr & 1)
      val = _this->reg[0x0F] & 0x0F;
    else
      val = _this->reg[0x0F] >> 4;

    adpcm_update_output(_this, val);
  }

  return ((_this->output[0] + _this->output[1]) * (_this->reg[0x12] & 0xff)) >> 13;
}

int16_t OPL_ADPCM_calc(OPL_ADPCM *_this) {
  if (_this->reg[0x07] & R07_SP_OFF)
    return 0;

  return calc(_this);
}

/* mode= 0:RAM256k 1:ROM 2:RAM64k */
uint32_t decode_start_address(uint8_t mode, uint8_t l, uint8_t h) {
  switch (mode) {
  case 0:
    return ((h << 8) | l) << 2;
  default:
    return ((h << 8) | l) << 5;  
  }
}

uint32_t decode_stop_address(uint8_t mode, uint8_t l, uint8_t h) {
  switch (mode) {
  case 0:
    return (((h << 8) | l) << 2) | 3;
  default:
    return (((h << 8) | l) << 5) | 31;
  }
}

void OPL_ADPCM_writeReg(OPL_ADPCM *_this, uint32_t adr, uint32_t data) {
  adr &= 0x1f;
  data &= 0xff;

  switch (adr) {
  case 0x07: /* START/REC/MEM DATA/REPEAT/SP-OFF/RESET */
    if (data & R07_RESET) {
      _this->play_start = 0;
      break;
    }
    if (data & R07_START) {
      _this->play_start = 1;
      _this->play_addr = _this->start_addr & _this->play_addr_mask;
      _this->delta_addr = 0;
      _this->output[0] = 0;
      _this->output[1] = 0;
      _this->diff = DDEF;
      _this->status |= STATUS_PCM_BSY; 
    }
    _this->reg[0x07] = data;
    break;

  case 0x08: /* CSM/KEY BOARD SPLIT/SAMPLE/DA AD/64K/ROM */
    _this->reg[0x08] = data;
    _this->wave = _this->reg[0x08] & R08_ROM ? _this->memory[1] : _this->memory[0];
    _this->play_addr_mask = _this->reg[0x08] & R08_64K ? (1 << 17) - 1 : (1 << 19) - 1;
    break;

  case 0x09: /* START ADDRESS (L) */
  case 0x0A: /* START ADDRESS (H) */
    _this->reg[adr] = data;
    _this->start_addr = decode_start_address(_this->reg[0x08] & 3, _this->reg[0x09], _this->reg[0x0A]) << 1;
    break;

  case 0x0B: /* STOP ADDRESS (L) */
  case 0x0C: /* STOP ADDRESS (H) */
    _this->reg[adr] = data;
    _this->stop_addr = decode_stop_address(_this->reg[0x08] & 3, _this->reg[0x0B], _this->reg[0x0C]) << 1;
    break;

  case 0x0D: /* PRESCALE (L) */
    _this->reg[0x0D] = data;
    break;

  case 0x0E: /* PRESCALE (H) */
    _this->reg[0x0E] = data;
    break;

  case 0x0F: /* OPL_ADPCM-DATA */
    _this->reg[0x0F] = data;

    if ((_this->reg[0x07] & R07_REC) && (_this->reg[0x07] & R07_MEMORY_DATA)) {
      _this->wave[_this->play_addr >> 1] = data;
      _this->play_addr = (_this->play_addr + 2) & (_this->play_addr_mask);
      if (_this->play_addr >= (_this->stop_addr & _this->play_addr_mask)) {
        //_this->status |= STATUS_EOS; /* Bug? */
      }
    }
    break;

  case 0x10: /* DELTA-N (L) */
  case 0x11: /* DELTA-N (H) */
    _this->reg[adr] = data;
    _this->delta_n = (_this->reg[0x11] << 8) | _this->reg[0x10];
    break;

  case 0x12: /* ENVELOP CONTROL */
    _this->reg[0x12] = data;
    break;

  default:
    break;
  }
}

/**
 * 76543210
 *    ||  +- D0: PCM-BSY
 *    |+---- D3: BUF-RDY
 *    +----- D4: EOS
 * IRQ bit (D7) is not implemented on this module.
 */
uint8_t OPL_ADPCM_status(OPL_ADPCM *_this) { 
  // BUF_RDY is always 1 - it is not accurate but practically okay.
  return _this->status | STATUS_BUF_RDY;
}

void OPL_ADPCM_resetStatus(OPL_ADPCM *_this) {
  _this->status = 0;
}

void OPL_ADPCM_writeRAM(OPL_ADPCM *_this, uint32_t start, uint32_t length, const uint8_t *data) {
  if (start >= RAM_SIZE) return;
  if (start + length > RAM_SIZE) {
    length = RAM_SIZE - start;
  }
  memcpy(_this->memory[0] + start, data, length);
}

void OPL_ADPCM_writeROM(OPL_ADPCM *_this, uint32_t start, uint32_t length, const uint8_t *data) {
  if (start >= ROM_SIZE) return;
  if (start + length > ROM_SIZE) {
    length = ROM_SIZE - start;
  }
  memcpy(_this->memory[1] + start, data, length);
}

/* ------------------------------------------------------------------------- */
/* Embedded emu8950 OPL implementation                                        */
/* ------------------------------------------------------------------------- */
/**
 * emu8950 v1.1.4
 * https://github.com/digital-sound-antiques/emu8950
 * Copyright (C) 2001-2020 Mitsutaka Okazaki
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef INLINE
#if defined(_MSC_VER)
#define INLINE __inline
#elif defined(__GNUC__)
#define INLINE __inline__
#else
#define INLINE inline
#endif
#endif

#define _PI_ 3.14159265358979323846264338327950288

enum __OPL_EG_STATE { ATTACK, DECAY, SUSTAIN, RELEASE, UNKNOWN };
enum __OPL_TYPE { TYPE_Y8950 = 0, TYPE_YM3526, TYPE_YM3812, TYPE_MAX };

/* phase increment counter */
#define DP_BITS 20
#define DP_WIDTH (1 << DP_BITS)
#define DP_BASE_BITS (DP_BITS - PG_BITS)

/* dynamic range of envelope output */
#define EG_STEP 0.1875
#define EG_BITS 9
#define EG_MUTE ((1 << EG_BITS) - 1)
#define EG_MAX (0x1f0) // 93dB

/* dynamic range of total level */
#define TL_STEP 0.75
#define TL_BITS 6

/* dynamic range of sustine level */
#define SL_STEP 3.0
#define SL_BITS 4

#define TL2EG(tl) ((tl) << 2)

/* sine table */
#define PG_BITS 10 /* 2^10 = 1024 length sine table */
#define PG_WIDTH (1 << PG_BITS)

/* clang-format off */
/* exp_table[x] = round((exp2((double)x / 256.0) - 1) * 1024) */
static uint16_t exp_table[256] = {
0,    3,    6,    8,    11,   14,   17,   20,   22,   25,   28,   31,   34,   37,   40,   42,
45,   48,   51,   54,   57,   60,   63,   66,   69,   72,   75,   78,   81,   84,   87,   90,
93,   96,   99,   102,  105,  108,  111,  114,  117,  120,  123,  126,  130,  133,  136,  139,
142,  145,  148,  152,  155,  158,  161,  164,  168,  171,  174,  177,  181,  184,  187,  190,
194,  197,  200,  204,  207,  210,  214,  217,  220,  224,  227,  231,  234,  237,  241,  244,
248,  251,  255,  258,  262,  265,  268,  272,  276,  279,  283,  286,  290,  293,  297,  300,
304,  308,  311,  315,  318,  322,  326,  329,  333,  337,  340,  344,  348,  352,  355,  359,
363,  367,  370,  374,  378,  382,  385,  389,  393,  397,  401,  405,  409,  412,  416,  420,
424,  428,  432,  436,  440,  444,  448,  452,  456,  460,  464,  468,  472,  476,  480,  484,
488,  492,  496,  501,  505,  509,  513,  517,  521,  526,  530,  534,  538,  542,  547,  551,
555,  560,  564,  568,  572,  577,  581,  585,  590,  594,  599,  603,  607,  612,  616,  621,
625,  630,  634,  639,  643,  648,  652,  657,  661,  666,  670,  675,  680,  684,  689,  693,
698,  703,  708,  712,  717,  722,  726,  731,  736,  741,  745,  750,  755,  760,  765,  770,
774,  779,  784,  789,  794,  799,  804,  809,  814,  819,  824,  829,  834,  839,  844,  849,
854,  859,  864,  869,  874,  880,  885,  890,  895,  900,  906,  911,  916,  921,  927,  932,
937,  942,  948,  953,  959,  964,  969,  975,  980,  986,  991,  996, 1002, 1007, 1013, 1018
};
/* logsin_table[x] = round(-log2(sin((x + 0.5) * PI / (PG_WIDTH / 4) / 2)) * 256) */
static uint16_t logsin_table[PG_WIDTH / 4] = {
2137, 1731, 1543, 1419, 1326, 1252, 1190, 1137, 1091, 1050, 1013, 979,  949,  920,  894,  869, 
846,  825,  804,  785,  767,  749,  732,  717,  701,  687,  672,  659,  646,  633,  621,  609, 
598,  587,  576,  566,  556,  546,  536,  527,  518,  509,  501,  492,  484,  476,  468,  461,
453,  446,  439,  432,  425,  418,  411,  405,  399,  392,  386,  380,  375,  369,  363,  358,  
352,  347,  341,  336,  331,  326,  321,  316,  311,  307,  302,  297,  293,  289,  284,  280,
276,  271,  267,  263,  259,  255,  251,  248,  244,  240,  236,  233,  229,  226,  222,  219, 
215,  212,  209,  205,  202,  199,  196,  193,  190,  187,  184,  181,  178,  175,  172,  169, 
167,  164,  161,  159,  156,  153,  151,  148,  146,  143,  141,  138,  136,  134,  131,  129,  
127,  125,  122,  120,  118,  116,  114,  112,  110,  108,  106,  104,  102,  100,  98,   96,   
94,   92,   91,   89,   87,   85,   83,   82,   80,   78,   77,   75,   74,   72,   70,   69,
67,   66,   64,   63,   62,   60,   59,   57,   56,   55,   53,   52,   51,   49,   48,   47,  
46,   45,   43,   42,   41,   40,   39,   38,   37,   36,   35,   34,   33,   32,   31,   30,  
29,   28,   27,   26,   25,   24,   23,   23,   22,   21,   20,   20,   19,   18,   17,   17,   
16,   15,   15,   14,   13,   13,   12,   12,   11,   10,   10,   9,    9,    8,    8,    7,    
7,    7,    6,    6,    5,    5,    5,    4,    4,    4,    3,    3,    3,    2,    2,    2,
2,    1,    1,    1,    1,    1,    1,    1,    0,    0,    0,    0,    0,    0,    0,    0,
};
/* clang-format on */

static uint16_t wave_table_map[4][PG_WIDTH];

/* pitch modulator */
#define PM_PG_BITS 3
#define PM_PG_WIDTH (1 << PM_PG_BITS)
#define PM_DP_BITS 22
#define PM_DP_WIDTH (1 << PM_DP_BITS)

/* offset to fnum, rough approximation of 14 cents depth. */
static int8_t pm_table[8][PM_PG_WIDTH] = {
    {0, 0, 0, 0, 0, 0, 0, 0},    // fnum = 000xxxxx
    {0, 0, 1, 0, 0, 0, -1, 0},   // fnum = 001xxxxx
    {0, 1, 2, 1, 0, -1, -2, -1}, // fnum = 010xxxxx
    {0, 1, 3, 1, 0, -1, -3, -1}, // fnum = 011xxxxx
    {0, 2, 4, 2, 0, -2, -4, -2}, // fnum = 100xxxxx
    {0, 2, 5, 2, 0, -2, -5, -2}, // fnum = 101xxxxx
    {0, 3, 6, 3, 0, -3, -6, -3}, // fnum = 110xxxxx
    {0, 3, 7, 3, 0, -3, -7, -3}, // fnum = 111xxxxx
};

/* amplitude lfo table */
/* The following envelop pattern is verified on real YM2413. */
/* each element repeates 64 cycles */
static uint8_t am_table[210] = {0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,  1,  1,  1,  1,  //
                                2,  2,  2,  2,  2,  2,  2,  2,  3,  3,  3,  3,  3,  3,  3,  3,  //
                                4,  4,  4,  4,  4,  4,  4,  4,  5,  5,  5,  5,  5,  5,  5,  5,  //
                                6,  6,  6,  6,  6,  6,  6,  6,  7,  7,  7,  7,  7,  7,  7,  7,  //
                                8,  8,  8,  8,  8,  8,  8,  8,  9,  9,  9,  9,  9,  9,  9,  9,  //
                                10, 10, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 11, 11, //
                                12, 12, 12, 12, 12, 12, 12, 12,                                 //
                                13, 13, 13,                                                     //
                                12, 12, 12, 12, 12, 12, 12, 12,                                 //
                                11, 11, 11, 11, 11, 11, 11, 11, 10, 10, 10, 10, 10, 10, 10, 10, //
                                9,  9,  9,  9,  9,  9,  9,  9,  8,  8,  8,  8,  8,  8,  8,  8,  //
                                7,  7,  7,  7,  7,  7,  7,  7,  6,  6,  6,  6,  6,  6,  6,  6,  //
                                5,  5,  5,  5,  5,  5,  5,  5,  4,  4,  4,  4,  4,  4,  4,  4,  //
                                3,  3,  3,  3,  3,  3,  3,  3,  2,  2,  2,  2,  2,  2,  2,  2,  //
                                1,  1,  1,  1,  1,  1,  1,  1,  0,  0,  0,  0,  0,  0,  0};

/* envelope decay increment step table */
static uint8_t eg_step_tables[4][8] = {
    {0, 1, 0, 1, 0, 1, 0, 1},
    {0, 1, 0, 1, 1, 1, 0, 1},
    {0, 1, 1, 1, 0, 1, 1, 1},
    {0, 1, 1, 1, 1, 1, 1, 1},
};
static uint8_t eg_step_tables_fast[4][8] = {
    {1, 1, 1, 1, 1, 1, 1, 1},
    {1, 1, 1, 2, 1, 1, 1, 2},
    {1, 2, 1, 2, 1, 2, 1, 2},
    {1, 2, 2, 2, 1, 2, 2, 2},
};

static uint32_t ml_table[16] = {1,     1 * 2, 2 * 2,  3 * 2,  4 * 2,  5 * 2,  6 * 2,  7 * 2,
                                8 * 2, 9 * 2, 10 * 2, 10 * 2, 12 * 2, 12 * 2, 15 * 2, 15 * 2};

#define dB2(x) ((x) * 2)
static double kl_table[16] = {dB2(0.000),  dB2(9.000),  dB2(12.000), dB2(13.875), dB2(15.000), dB2(16.125),
                              dB2(16.875), dB2(17.625), dB2(18.000), dB2(18.750), dB2(19.125), dB2(19.500),
                              dB2(19.875), dB2(20.250), dB2(20.625), dB2(21.000)};

static uint32_t tll_table[8 * 16][1 << TL_BITS][4];
static int32_t rks_table[2][32][2];

#define min(i, j) (((i) < (j)) ? (i) : (j))
#define max(i, j) (((i) > (j)) ? (i) : (j))

/***************************************************

           Internal Sample Rate Converter

****************************************************/
/* Note: to disable internal rate converter, set clock/72 to output sampling rate. */

/*
 * LW is truncate length of sinc(x) calculation.
 * Lower LW is faster, higher LW results better quality.
 * LW must be a non-zero positive even number, no upper limit.
 * LW=16 or greater is recommended when upsampling.
 * LW=8 is practically okay for downsampling.
 */
#define LW 16

/* resolution of sinc(x) table. sinc(x) where 0.0<=x<1.0 corresponds to sinc_table[0...SINC_RESO-1] */
#define SINC_RESO 256
#define SINC_AMP_BITS 12

// double hamming(double x) { return 0.54 - 0.46 * cos(2 * PI * x); }
static double blackman(double x) { return 0.42 - 0.5 * cos(2 * _PI_ * x) + 0.08 * cos(4 * _PI_ * x); }
static double sinc(double x) { return (x == 0.0 ? 1.0 : sin(_PI_ * x) / (_PI_ * x)); }
static double windowed_sinc(double x) { return blackman(0.5 + 0.5 * x / (LW / 2)) * sinc(x); }

/* f_inp: input frequency. f_out: output frequencey, ch: number of channels */
OPL_RateConv *OPL_RateConv_new(double f_inp, double f_out, int ch) {
  OPL_RateConv *conv = malloc(sizeof(OPL_RateConv));
  int i;

  conv->ch = ch;
  conv->f_ratio = f_inp / f_out;
  conv->buf = malloc(sizeof(void *) * ch);
  for (i = 0; i < ch; i++) {
    conv->buf[i] = malloc(sizeof(conv->buf[0][0]) * LW);
  }

  /* create sinc_table for positive 0 <= x < LW/2 */
  conv->sinc_table = malloc(sizeof(conv->sinc_table[0]) * SINC_RESO * LW / 2);
  for (i = 0; i < SINC_RESO * LW / 2; i++) {
    const double x = (double)i / SINC_RESO;
    if (f_out < f_inp) {
      /* for downsampling */
      conv->sinc_table[i] = (int16_t)((1 << SINC_AMP_BITS) * windowed_sinc(x / conv->f_ratio) / conv->f_ratio);
    } else {
      /* for upsampling */
      conv->sinc_table[i] = (int16_t)((1 << SINC_AMP_BITS) * windowed_sinc(x));
    }
  }

  return conv;
}

static INLINE int16_t lookup_sinc_table(int16_t *table, double x) {
  int16_t index = (int16_t)(x * SINC_RESO);
  if (index < 0)
    index = -index;
  return table[min(SINC_RESO * LW / 2 - 1, index)];
}

void OPL_RateConv_reset(OPL_RateConv *conv) {
  int i;
  conv->timer = 0;
  for (i = 0; i < conv->ch; i++) {
    memset(conv->buf[i], 0, sizeof(conv->buf[i][0]) * LW);
  }
}

/* put original data to this converter at f_inp. */
void OPL_RateConv_putData(OPL_RateConv *conv, int ch, int16_t data) {
  int16_t *buf = conv->buf[ch];
  int i;
  for (i = 0; i < LW - 1; i++) {
    buf[i] = buf[i + 1];
  }
  buf[LW - 1] = data;
}

/* get resampled data from this converter at f_out. */
/* this function must be called f_out / f_inp times per one putData call. */
int16_t OPL_RateConv_getData(OPL_RateConv *conv, int ch) {
  int16_t *buf = conv->buf[ch];
  int32_t sum = 0;
  int k;
  double dn;
  conv->timer += conv->f_ratio;
  dn = conv->timer - floor(conv->timer);
  conv->timer = dn;

  for (k = 0; k < LW; k++) {
    double x = ((double)k - (LW / 2 - 1)) - dn;
    sum += buf[k] * lookup_sinc_table(conv->sinc_table, x);
  }
  return sum >> SINC_AMP_BITS;
}

void OPL_RateConv_delete(OPL_RateConv *conv) {
  int i;
  for (i = 0; i < conv->ch; i++) {
    free(conv->buf[i]);
  }
  free(conv->buf);
  free(conv->sinc_table);
  free(conv);
}

/***************************************************

                  Create tables

****************************************************/
static void makeSinTable(void) {
  int x;

  for (x = 0; x < PG_WIDTH; x++) {
    if (x < PG_WIDTH / 4) {
      wave_table_map[0][x] = logsin_table[x];
    } else if (x < PG_WIDTH / 2) {
      wave_table_map[0][x] = logsin_table[PG_WIDTH / 2 - x - 1];
    } else {
      wave_table_map[0][x] = 0x8000 | wave_table_map[0][PG_WIDTH - x - 1];
    }
  }

  for (x = 0; x < PG_WIDTH; x++) {
    if (x < PG_WIDTH / 2) {
      wave_table_map[1][x] = wave_table_map[0][x];
    } else {
      wave_table_map[1][x] = 0xfff;
    }
  }

  for (x = 0; x < PG_WIDTH; x++) {
    if (x < PG_WIDTH / 2) {
      wave_table_map[2][x] = wave_table_map[0][x];
    } else {
      wave_table_map[2][x] = wave_table_map[0][x - PG_WIDTH / 2];
    }
  }

  for (x = 0; x < PG_WIDTH; x++) {
    if (x < PG_WIDTH / 4) {
      wave_table_map[3][x] = wave_table_map[0][x];
    } else if (x < PG_WIDTH / 2) {
      wave_table_map[3][x] = 0xfff;
    } else if (x < PG_WIDTH * 3 / 4) {
      wave_table_map[3][x] = wave_table_map[0][x - PG_WIDTH / 2];
    } else {
      wave_table_map[3][x] = 0xfff;
    }
  }
}

static void makeTllTable(void) {

  int32_t tmp;
  int32_t fnum, block, TL, KL, kx;

  for (fnum = 0; fnum < 16; fnum++) {
    for (block = 0; block < 8; block++) {
      for (TL = 0; TL < 64; TL++) {
        for (KL = 0; KL < 4; KL++) {
          kx = ((KL & 1) << 1) | ((KL >> 1) & 1);
          if (KL == 0) {
            tll_table[(block << 4) | fnum][TL][KL] = TL2EG(TL);
          } else {
            tmp = (int32_t)(kl_table[fnum] - dB2(3.000) * (7 - block));
            if (tmp <= 0)
              tll_table[(block << 4) | fnum][TL][KL] = TL2EG(TL);
            else
              tll_table[(block << 4) | fnum][TL][KL] = (uint32_t)((tmp >> (3 - kx)) / EG_STEP) + TL2EG(TL);
          }
        }
      }
    }
  }
}

static void makeRksTable(void) {
  int fnum8, fnum9, blk;
  int blk_fnum98;
  for (fnum8 = 0; fnum8 < 2; fnum8++)
    for (fnum9 = 0; fnum9 < 2; fnum9++)
      for (blk = 0; blk < 8; blk++) {
        blk_fnum98 = (blk << 2) | (fnum9 << 1) | fnum8;
        rks_table[0][blk_fnum98][1] = (blk << 1) + fnum9;
        rks_table[0][blk_fnum98][0] = blk >> 1;
        rks_table[1][blk_fnum98][1] = (blk << 1) + (fnum9 & fnum8);
        rks_table[1][blk_fnum98][0] = blk >> 1;
      }
}

static uint8_t table_initialized = 0;

static void initializeTables() {
  makeTllTable();
  makeRksTable();
  makeSinTable();
  table_initialized = 1;
}

/*********************************************************

                      Synthesizing

*********************************************************/
#define SLOT_BD1 12
#define SLOT_BD2 13
#define SLOT_HH 14
#define SLOT_SD 15
#define SLOT_TOM 16
#define SLOT_CYM 17

/* utility macros */
#define MOD(o, x) (&(o)->slot[(x) << 1])
#define CAR(o, x) (&(o)->slot[((x) << 1) | 1])
#define BIT(s, b) (((s) >> (b)) & 1)

#if OPL_DEBUG
static void _debug_print_patch(OPL_SLOT *slot) {
  OPL_PATCH *p = slot->patch;
  printf("[slot#%d am:%d pm:%d eg:%d kr:%d ml:%d kl:%d tl:%d ws:%d fb:%d A:%d D:%d S:%d R:%d]\n", slot->number, //
         p->AM, p->PM, p->EG, p->KR, p->ML,                                                                     //
         p->KL, p->TL, p->WS, p->FB,                                                                            //
         p->AR, p->DR, p->SL, p->RR);
}

static char *_debug_eg_state_name(OPL_SLOT *slot) {
  switch (slot->eg_state) {
  case ATTACK:
    return "attack";
  case DECAY:
    return "decay";
  case SUSTAIN:
    return "sustain";
  case RELEASE:
    return "release";
  default:
    return "unknown";
  }
}

static INLINE void _debug_print_slot_info(OPL_SLOT *slot) {
  char *name = _debug_eg_state_name(slot);
  _debug_print_patch(slot);
  printf("[slot#%d state:%s fnum:%03x rate:%d-%d]\n", slot->number, name, slot->blk_fnum, slot->eg_rate_h,
         slot->eg_rate_l);
  fflush(stdout);
}
#endif

static INLINE int get_parameter_rate(OPL_SLOT *slot) {
  switch (slot->eg_state) {
  case ATTACK:
    return slot->patch->AR;
  case DECAY:
    return slot->patch->DR;
  case SUSTAIN:
    return slot->patch->EG ? 0 : slot->patch->RR;
  case RELEASE:
    return slot->patch->RR;
  default:
    return 0;
  }
}

enum SLOT_UPDATE_FLAG {
  UPDATE_WS = 1,
  UPDATE_TLL = 2,
  UPDATE_RKS = 4,
  UPDATE_EG = 8,
  UPDATE_ALL = 255,
};

static INLINE void request_update(OPL_SLOT *slot, int flag) { slot->update_requests |= flag; }

static void commit_slot_update(OPL_SLOT *slot, uint8_t notesel) {

  if (slot->update_requests & UPDATE_WS) {
    slot->wave_table = wave_table_map[slot->patch->WS & 3];
  }

  if (slot->update_requests & UPDATE_TLL) {
    if ((slot->type & 1) == 0) {
      slot->tll = tll_table[slot->blk_fnum >> 6][slot->patch->TL][slot->patch->KL];
    } else {
      slot->tll = tll_table[slot->blk_fnum >> 6][slot->patch->TL][slot->patch->KL];
    }
  }

  if (slot->update_requests & UPDATE_RKS) {
    slot->rks = rks_table[notesel][slot->blk_fnum >> 8][slot->patch->KR];
  }

  if (slot->update_requests & (UPDATE_RKS | UPDATE_EG)) {
    int p_rate = get_parameter_rate(slot);

    if (p_rate == 0) {
      slot->eg_shift = 0;
      slot->eg_rate_h = 0;
      slot->eg_rate_l = 0;
    } else {
      slot->eg_rate_h = min(15, p_rate + (slot->rks >> 2));
      slot->eg_rate_l = slot->rks & 3;
      if (slot->eg_state == ATTACK) {
        slot->eg_shift = (0 < slot->eg_rate_h && slot->eg_rate_h < 12) ? (12 - slot->eg_rate_h) : 0;
      } else {
        slot->eg_shift = (slot->eg_rate_h < 12) ? (12 - slot->eg_rate_h) : 0;
      }
    }
  }

#if OPL_DEBUG
  if (slot->last_eg_state != slot->eg_state) {
    _debug_print_slot_info(slot);
    slot->last_eg_state = slot->eg_state;
  }
#endif

  slot->update_requests = 0;
}

static void reset_slot(OPL_SLOT *slot, int number) {
  slot->patch = &(slot->__patch);
  memset(slot->patch, 0, sizeof(OPL_PATCH));
  slot->number = number;
  slot->type = number % 2;
  slot->pg_keep = 0;
  slot->wave_table = wave_table_map[0];
  slot->pg_phase = 0;
  slot->output[0] = 0;
  slot->output[1] = 0;
  slot->eg_state = RELEASE;
  slot->eg_shift = 0;
  slot->rks = 0;
  slot->tll = 0;
  slot->blk_fnum = 0;
  slot->blk = 0;
  slot->fnum = 0;
  slot->pg_out = 0;
  slot->eg_out = EG_MUTE;
}

static INLINE void slotOn(OPL *opl, int i) {
  OPL_SLOT *slot = &opl->slot[i];
  slot->rks = rks_table[opl->notesel][slot->blk_fnum >> 8][slot->patch->KR];
  if (min(15, slot->patch->AR + (slot->rks >> 2)) == 15) {
    slot->eg_state = DECAY;
    slot->eg_out = 0;
  } else {
    slot->eg_state = ATTACK;
  }
  if (!slot->pg_keep) {
    slot->pg_phase = 0;
  }
  request_update(slot, UPDATE_EG);
}

static INLINE void slotOff(OPL *opl, int i) {
  OPL_SLOT *slot = &opl->slot[i];
  slot->eg_state = RELEASE;
  request_update(slot, UPDATE_EG);
}

static INLINE void update_key_status(OPL *opl) {
  const uint8_t r14 = opl->reg[0xbd];
  const uint8_t rhythm_mode = BIT(r14, 5);
  uint32_t new_slot_key_status = 0;
  uint32_t updated_status;
  int ch;

  if (opl->csm_mode && opl->csm_key_count) {
    new_slot_key_status = 0x3ffff;
  }

  for (ch = 0; ch < 9; ch++)
    if (opl->reg[0xB0 + ch] & 0x20)
      new_slot_key_status |= 3 << (ch * 2);

  if (rhythm_mode) {
    if (r14 & 0x10)
      new_slot_key_status |= 3 << SLOT_BD1;

    if (r14 & 0x01)
      new_slot_key_status |= 1 << SLOT_HH;

    if (r14 & 0x08)
      new_slot_key_status |= 1 << SLOT_SD;

    if (r14 & 0x04)
      new_slot_key_status |= 1 << SLOT_TOM;

    if (r14 & 0x02)
      new_slot_key_status |= 1 << SLOT_CYM;
  }

  updated_status = opl->slot_key_status ^ new_slot_key_status;

  if (updated_status) {
    int i;
    for (i = 0; i < 18; i++)
      if (BIT(updated_status, i)) {
        if (BIT(new_slot_key_status, i)) {
          slotOn(opl, i);
        } else {
          slotOff(opl, i);
        }
      }
  }

  opl->slot_key_status = new_slot_key_status;
}

/* set f-Nnmber ( fnum : 10bit ) */
static INLINE void set_fnumber(OPL *opl, int ch, int fnum) {
  OPL_SLOT *car = CAR(opl, ch);
  OPL_SLOT *mod = MOD(opl, ch);
  car->fnum = fnum;
  car->blk_fnum = (car->blk_fnum & 0x1c00) | (fnum & 0x3ff);
  mod->fnum = fnum;
  mod->blk_fnum = (mod->blk_fnum & 0x1c00) | (fnum & 0x3ff);
  request_update(car, UPDATE_EG | UPDATE_RKS | UPDATE_TLL);
  request_update(mod, UPDATE_EG | UPDATE_RKS | UPDATE_TLL);
}

/* set block data (blk : 3bit ) */
static INLINE void set_block(OPL *opl, int ch, int blk) {
  OPL_SLOT *car = CAR(opl, ch);
  OPL_SLOT *mod = MOD(opl, ch);
  car->blk = blk;
  car->blk_fnum = ((blk & 7) << 10) | (car->blk_fnum & 0x3ff);
  mod->blk = blk;
  mod->blk_fnum = ((blk & 7) << 10) | (mod->blk_fnum & 0x3ff);
  request_update(car, UPDATE_EG | UPDATE_RKS | UPDATE_TLL);
  request_update(mod, UPDATE_EG | UPDATE_RKS | UPDATE_TLL);
}

static INLINE void update_rhythm_mode(OPL *opl) {
  const uint8_t new_rhythm_mode = (opl->reg[0xbd] >> 5) & 1;

  if (opl->rhythm_mode != new_rhythm_mode) {
    if (new_rhythm_mode) {
      opl->slot[SLOT_HH].type = 3;
      opl->slot[SLOT_HH].pg_keep = 1;
      opl->slot[SLOT_SD].type = 3;
      opl->slot[SLOT_TOM].type = 3;
      opl->slot[SLOT_CYM].type = 3;
      opl->slot[SLOT_CYM].pg_keep = 1;
    } else {
      opl->slot[SLOT_HH].type = 0;
      opl->slot[SLOT_HH].pg_keep = 0;
      opl->slot[SLOT_SD].type = 1;
      opl->slot[SLOT_TOM].type = 0;
      opl->slot[SLOT_CYM].type = 1;
      opl->slot[SLOT_CYM].pg_keep = 0;
    }
  }
  opl->rhythm_mode = new_rhythm_mode;
}

static void update_ampm(OPL *opl) {
  const uint32_t pm_inc = (opl->test_flag & 8) ? opl->pm_dphase << 10 : opl->pm_dphase;
  const uint32_t am_inc = (opl->test_flag & 8) ? 64 : 1;
  if (opl->test_flag & 2) {
    opl->pm_phase = 0;
    opl->am_phase = 0;
  } else {
    opl->pm_phase = (opl->pm_phase + pm_inc) & (PM_DP_WIDTH - 1);
    opl->am_phase += am_inc;
  }
  opl->lfo_am = am_table[(opl->am_phase >> 6) % sizeof(am_table)] >> (opl->am_mode ? 0 : 2);
}

static void update_noise(OPL *opl, int cycle) {
  int i;
  for (i = 0; i < cycle; i++) {
    if (opl->noise & 1) {
      opl->noise ^= 0x800200;
    }
    opl->noise >>= 1;
  }
}

static void update_short_noise(OPL *opl) {
  const uint32_t pg_hh = opl->slot[SLOT_HH].pg_out;
  const uint32_t pg_cym = opl->slot[SLOT_CYM].pg_out;

  const uint8_t h_bit2 = BIT(pg_hh, PG_BITS - 8);
  const uint8_t h_bit7 = BIT(pg_hh, PG_BITS - 3);
  const uint8_t h_bit3 = BIT(pg_hh, PG_BITS - 7);

  const uint8_t c_bit3 = BIT(pg_cym, PG_BITS - 7);
  const uint8_t c_bit5 = BIT(pg_cym, PG_BITS - 5);

  opl->short_noise = (h_bit2 ^ h_bit7) | (h_bit3 ^ c_bit5) | (c_bit3 ^ c_bit5);
}

static INLINE void calc_phase(OPL_SLOT *slot, int32_t pm_phase, uint8_t pm_mode, uint8_t reset) {
  int8_t pm = 0;
  if (slot->patch->PM) {
    pm = pm_table[(slot->fnum >> 7) & 7][pm_phase >> (PM_DP_BITS - PM_PG_BITS)];
    pm >>= (pm_mode ? 0 : 1);
  }

  if (reset) {
    slot->pg_phase = 0;
  }
  slot->pg_phase += (((slot->fnum & 0x3ff) + pm) * ml_table[slot->patch->ML]) << slot->blk >> 1;
  slot->pg_phase &= (DP_WIDTH - 1);
  slot->pg_out = slot->pg_phase >> DP_BASE_BITS;
}

static INLINE uint8_t lookup_attack_step(OPL_SLOT *slot, uint32_t counter) {
  int index = (counter >> slot->eg_shift) & 7;
  switch (slot->eg_rate_h) {
  case 13:
    return eg_step_tables_fast[slot->eg_rate_l][index];
  case 14:
    return eg_step_tables_fast[slot->eg_rate_l][index] << 1;
  case 0:
  case 15:
    return 0;
  default:
    return eg_step_tables[slot->eg_rate_l][index];
  }
}

static INLINE uint8_t lookup_decay_step(OPL_SLOT *slot, uint32_t counter) {
  int index = (counter >> slot->eg_shift) & 7;
  switch (slot->eg_rate_h) {
  case 0:
    return 0;
  case 13:
    return eg_step_tables_fast[slot->eg_rate_l][index];
  case 14:
    return eg_step_tables_fast[slot->eg_rate_l][index] << 1;
  case 15:
    return 4;
  default:
    return eg_step_tables[slot->eg_rate_l][index];
  }
}

static INLINE void calc_envelope(OPL_SLOT *slot, uint16_t eg_counter, uint8_t test) {

  uint16_t mask = (1 << slot->eg_shift) - 1;
  uint8_t s;

  if (slot->eg_state == ATTACK) {
    if (0 < slot->eg_out && slot->eg_rate_h > 0 && (eg_counter & mask) == 0) {
      s = lookup_attack_step(slot, eg_counter);
      slot->eg_out += (~slot->eg_out * s) >> 3;
    }
  } else {
    if (slot->eg_rate_h > 0 && (eg_counter & mask) == 0) {
      slot->eg_out = min(EG_MUTE, slot->eg_out + lookup_decay_step(slot, eg_counter));
    }
  }

  switch (slot->eg_state) {
  case ATTACK:
    if (slot->eg_out == 0) {
      slot->eg_state = DECAY;
      request_update(slot, UPDATE_EG);
    }
    break;

  case DECAY:
    if ((slot->patch->SL != 15) && (slot->eg_out >> 4) == slot->patch->SL) {
      slot->eg_state = SUSTAIN;
      request_update(slot, UPDATE_EG);
    }
    break;

  case SUSTAIN:
  case RELEASE:
  default:
    break;
  }

  if (test) {
    slot->eg_out = 0;
  }
}

static void update_slots(OPL *opl) {
  int i;
  opl->eg_counter++;

  for (i = 0; i < 18; i++) {
    OPL_SLOT *slot = &opl->slot[i];
    if (slot->update_requests) {
      commit_slot_update(slot, opl->notesel);
    }
    calc_envelope(slot, opl->eg_counter, opl->test_flag & 1);
    calc_phase(slot, opl->pm_phase, opl->pm_mode, opl->test_flag & 4);
  }
}

/* input: 0..8191 output: -4095..4095 */
static INLINE int16_t lookup_exp_table(uint16_t i) {
  /* from andete's expressoin */
  int16_t t = (exp_table[(i & 0xff) ^ 0xff] + 1024);
  int16_t res = t >> ((i & 0x7f00) >> 8);
  return ((i & 0x8000) ? ~res : res) << 1;
}

static INLINE int16_t to_linear(uint16_t h, OPL_SLOT *slot, int16_t am) {
  uint16_t att;
  if (slot->eg_out >= EG_MAX)
    return 0;

  att = min(EG_MUTE, (slot->eg_out + slot->tll + am)) << 3;
  return lookup_exp_table(h + att);
}

static INLINE int16_t calc_slot_car(OPL *opl, int ch, int16_t fm) {
  OPL_SLOT *slot = CAR(opl, ch);

  uint8_t am = slot->patch->AM ? opl->lfo_am : 0;

  slot->output[1] = slot->output[0];
  slot->output[0] = to_linear(slot->wave_table[(slot->pg_out + 2 * (fm >> 1)) & (PG_WIDTH - 1)], slot, am);

  return slot->output[0];
}

static INLINE int16_t calc_slot_mod(OPL *opl, int ch) {
  OPL_SLOT *slot = MOD(opl, ch);

  int16_t fm = slot->patch->FB > 0 ? (slot->output[1] + slot->output[0]) >> (9 - slot->patch->FB) : 0;
  uint8_t am = slot->patch->AM ? opl->lfo_am : 0;

  slot->output[1] = slot->output[0];
  slot->output[0] = to_linear(slot->wave_table[(slot->pg_out + fm) & (PG_WIDTH - 1)], slot, am);

  return slot->output[0];
}

static INLINE int16_t calc_slot_tom(OPL *opl) {
  OPL_SLOT *slot = &(opl->slot[SLOT_TOM]);

  return to_linear(slot->wave_table[slot->pg_out], slot, 0);
}

/* Specify phase offset directly based on 10-bit (1024-length) sine table */
#define _PD(phase) ((PG_BITS < 10) ? (phase >> (10 - PG_BITS)) : (phase << (PG_BITS - 10)))

static INLINE int16_t calc_slot_snare(OPL *opl) {
  OPL_SLOT *slot = &(opl->slot[SLOT_SD]);

  uint32_t phase;

  if (BIT(opl->slot[SLOT_HH].pg_out, PG_BITS - 2))
    phase = (opl->noise & 1) ? _PD(0x300) : _PD(0x200);
  else
    phase = (opl->noise & 1) ? _PD(0x0) : _PD(0x100);

  return to_linear(slot->wave_table[phase], slot, 0);
}

static INLINE int16_t calc_slot_cym(OPL *opl) {
  OPL_SLOT *slot = &(opl->slot[SLOT_CYM]);

  uint32_t phase = opl->short_noise ? _PD(0x300) : _PD(0x100);

  return to_linear(slot->wave_table[phase], slot, 0);
}

static INLINE int16_t calc_slot_hat(OPL *opl) {
  OPL_SLOT *slot = &(opl->slot[SLOT_HH]);

  uint32_t phase;

  if (opl->short_noise)
    phase = (opl->noise & 1) ? _PD(0x2d0) : _PD(0x234);
  else
    phase = (opl->noise & 1) ? _PD(0x34) : _PD(0xd0);

  return to_linear(slot->wave_table[phase], slot, 0);
}

#define _MO(x) (-(x) >> 1)
#define _RO(x) (x)

static INLINE int16_t calc_fm(OPL *opl, int ch) {
  if (opl->ch_alg[ch]) {
    return calc_slot_car(opl, ch, 0) + calc_slot_mod(opl, ch);
  }
  return calc_slot_car(opl, ch, calc_slot_mod(opl, ch));
}

static void latch_timer1(OPL *opl) { opl->timer1_counter = opl->reg[0x02] << 2; }

static void latch_timer2(OPL *opl) { opl->timer2_counter = opl->reg[0x03] << 4; }

static void csm_key_on(OPL *opl) {
  opl->csm_key_count = 1;
  update_key_status(opl);
}

static void csm_key_off(OPL *opl) {
  opl->csm_key_count = 0;
  update_key_status(opl);
}

static void update_timer(OPL *opl) {
  if (opl->csm_mode && 0 < opl->csm_key_count) {
    csm_key_off(opl);
  }

  if (opl->reg[0x04] & 0x01) {
    opl->timer1_counter++;
    if (opl->timer1_counter >> 10) {
      opl->status |= 0x40; // timer1 overflow
      if (opl->csm_mode) {
        csm_key_on(opl);
      }
      if (opl->timer1_func) {
        opl->timer1_func(opl->timer1_user_data);
      }
      latch_timer1(opl);
    }
  }

  if (opl->reg[0x04] & 0x02) {
    opl->timer2_counter++;
    if (opl->timer2_counter >> 12) {
      opl->status |= 0x20; // timer2 overflow
      if (opl->timer2_func) {
        opl->timer2_func(opl->timer2_user_data);
      }
      latch_timer2(opl);
    }
  }
}

static void update_output(OPL *opl) {
  int16_t *out;
  int i;

  update_timer(opl);
  update_ampm(opl);
  update_short_noise(opl);
  update_slots(opl);

  out = opl->ch_out;

  /* CH1-6 */
  for (i = 0; i < 6; i++) {
    if (!(opl->mask & OPL_MASK_CH(i))) {
      out[i] = _MO(calc_fm(opl, i));
    }
  }

  /* CH7 */
  if (!opl->rhythm_mode) {
    if (!(opl->mask & OPL_MASK_CH(6))) {
      out[6] = _MO(calc_fm(opl, 6));
    }
  } else {
    if (!(opl->mask & OPL_MASK_BD)) {
      out[9] = _RO(calc_fm(opl, 6));
    }
  }
  update_noise(opl, 14);

  /* CH8 */
  if (!opl->rhythm_mode) {
    if (!(opl->mask & OPL_MASK_CH(7))) {
      out[7] = _MO(calc_fm(opl, 7));
    }
  } else {
    if (!(opl->mask & OPL_MASK_HH)) {
      out[10] = _RO(calc_slot_hat(opl));
    }
    if (!(opl->mask & OPL_MASK_SD)) {
      out[11] = _RO(calc_slot_snare(opl));
    }
  }
  update_noise(opl, 2);

  /* CH9 */
  if (!opl->rhythm_mode) {
    if (!(opl->mask & OPL_MASK_CH(8))) {
      out[8] = _MO(calc_fm(opl, 8));
    }
  } else {
    if (!(opl->mask & OPL_MASK_TOM)) {
      out[12] = _RO(calc_slot_tom(opl));
    }
    if (!(opl->mask & OPL_MASK_CYM)) {
      out[13] = _RO(calc_slot_cym(opl));
    }
  }
  update_noise(opl, 2);

  /* ADPCM */
  if (opl->adpcm != NULL && !(opl->mask & OPL_MASK_ADPCM)) {
    out[14] = OPL_ADPCM_calc(opl->adpcm);
  }
}

INLINE static void mix_output(OPL *opl) {
  int16_t out = 0;
  int i;
  for (i = 0; i < 15; i++) {
    out += opl->ch_out[i];
  }
  if (opl->conv) {
    OPL_RateConv_putData(opl->conv, 0, out);
  } else {
    opl->mix_out[0] = out;
  }
}

INLINE static void mix_output_stereo(OPL *opl) {
  int16_t *out = opl->mix_out;
  int i;
  out[0] = out[1] = 0;
  for (i = 0; i < 15; i++) {
    if (opl->pan[i] & 2)
      out[0] += (int16_t)(opl->ch_out[i] * opl->pan_fine[i][0]);
    if (opl->pan[i] & 1)
      out[1] += (int16_t)(opl->ch_out[i] * opl->pan_fine[i][1]);
  }
  if (opl->conv) {
    OPL_RateConv_putData(opl->conv, 0, out[0]);
    OPL_RateConv_putData(opl->conv, 1, out[1]);
  }
}

/***********************************************************

                   External Interfaces

***********************************************************/

OPL *OPL_new(uint32_t clk, uint32_t rate) {
  OPL *opl;

  if (!table_initialized) {
    initializeTables();
  }

  opl = (OPL *)calloc(1, sizeof(OPL));
  if (opl == NULL)
    return NULL;

  opl->adpcm = NULL;
  opl->clk = clk;
  opl->rate = rate;
  opl->mask = 0;
  opl->conv = NULL;
  opl->mix_out[0] = 0;
  opl->mix_out[1] = 0;
  opl->timer1_func = NULL;
  opl->timer1_user_data = NULL;
  opl->timer2_func = NULL;
  opl->timer2_user_data = NULL;

  OPL_reset(opl);

  return opl;
}

void OPL_delete(OPL *opl) {
  if (opl->conv) {
    OPL_RateConv_delete(opl->conv);
    opl->conv = NULL;
  }
  if (opl->adpcm) {
    OPL_ADPCM_delete(opl->adpcm);
    opl->adpcm = NULL;
  }
  free(opl);
}

static void reset_rate_conversion_params(OPL *opl) {
  const double f_out = opl->rate;
  const double f_inp = opl->clk / 72;

  opl->out_time = 0;
  opl->out_step = ((uint32_t)f_inp) << 8;
  opl->inp_step = ((uint32_t)f_out) << 8;

  if (opl->conv) {
    OPL_RateConv_delete(opl->conv);
    opl->conv = NULL;
  }

  if (floor(f_inp) != f_out && floor(f_inp + 0.5) != f_out) {
    opl->conv = OPL_RateConv_new(f_inp, f_out, 2);
  }

  if (opl->conv) {
    OPL_RateConv_reset(opl->conv);
  }
}

void refresh_adpcm_object(OPL *opl) {
  if (opl->chip_type == TYPE_Y8950) {
    if (opl->adpcm == NULL) {
      opl->adpcm = OPL_ADPCM_new(opl->clk);
    }
  } else {
    if (opl->adpcm != NULL) {
      free(opl->adpcm);
      opl->adpcm = NULL;
    }
  }
  if (opl->adpcm != NULL) {
    OPL_ADPCM_reset(opl->adpcm);
  }
}

void OPL_reset(OPL *opl) {
  int i;

  if (!opl)
    return;

  opl->adr = 0;

  opl->csm_mode = 0;
  opl->csm_key_count = 0;
  opl->notesel = 0;

  opl->status = 0;
  opl->timer1_counter = 0;
  opl->timer2_counter = 0;

  opl->pm_phase = 0;
  opl->am_phase = 0;

  opl->noise = 1;
  opl->mask = 0;

  opl->rhythm_mode = 0;
  opl->slot_key_status = 0;
  opl->eg_counter = 0;

  reset_rate_conversion_params(opl);

  for (i = 0; i < 18; i++) {
    reset_slot(&opl->slot[i], i);
  }

  for (i = 0; i < 9; i++) {
    opl->ch_alg[i] = 0;
  }

  for (i = 0; i < 0x100; i++) {
    opl->reg[i] = 0;
  }
  opl->reg[0x04] = 0x18; // MASK_EOS | MASK_BUF_RDY

  opl->pm_dphase = PM_DP_WIDTH / (1024 * 8);

  for (i = 0; i < 15; i++) {
    opl->pan[i] = 3;
    opl->pan_fine[i][1] = opl->pan_fine[i][0] = 1.0f;
  }

  for (i = 0; i < 15; i++) {
    opl->ch_out[i] = 0;
  }

  refresh_adpcm_object(opl);
}

void OPL_setRate(OPL *opl, uint32_t rate) {
  opl->rate = rate;
  reset_rate_conversion_params(opl);
}

void OPL_setQuality(OPL *opl, uint8_t q) { (void)opl; (void)q; }

void OPL_setChipType(OPL *opl, uint8_t type) {
  if (type < TYPE_MAX) {
    opl->chip_type = type;
    refresh_adpcm_object(opl);
  }
}

void OPL_writeIO(OPL *opl, uint32_t adr, uint8_t val) {
  if (adr & 1)
    OPL_writeReg(opl, opl->adr, val);
  else
    opl->adr = val;
}

void OPL_setPan(OPL *opl, uint32_t ch, uint8_t pan) { opl->pan[ch & 15] = pan; }

void OPL_setPanFine(OPL *opl, uint32_t ch, float pan[2]) {
  opl->pan_fine[ch & 15][0] = pan[0];
  opl->pan_fine[ch & 15][1] = pan[1];
}

int16_t OPL_calc(OPL *opl) {
  while (opl->out_step > opl->out_time) {
    opl->out_time += opl->inp_step;
    update_output(opl);
    mix_output(opl);
  }
  opl->out_time -= opl->out_step;
  if (opl->conv) {
    opl->mix_out[0] = OPL_RateConv_getData(opl->conv, 0);
  }
  return opl->mix_out[0];
}

void OPL_calcStereo(OPL *opl, int32_t out[2]) {
  while (opl->out_step > opl->out_time) {
    opl->out_time += opl->inp_step;
    update_output(opl);
    mix_output_stereo(opl);
  }
  opl->out_time -= opl->out_step;
  if (opl->conv) {
    out[0] = OPL_RateConv_getData(opl->conv, 0);
    out[1] = OPL_RateConv_getData(opl->conv, 1);
  } else {
    out[0] = opl->mix_out[0];
    out[1] = opl->mix_out[1];
  }
}

uint32_t OPL_setMask(OPL *opl, uint32_t mask) {
  uint32_t ret;

  if (opl) {
    ret = opl->mask;
    opl->mask = mask;
    return ret;
  } else
    return 0;
}

uint32_t OPL_toggleMask(OPL *opl, uint32_t mask) {
  uint32_t ret;

  if (opl) {
    ret = opl->mask;
    opl->mask ^= mask;
    return ret;
  } else
    return 0;
}

void OPL_writeReg(OPL *opl, uint32_t reg, uint8_t data) {

  int32_t s, c;

  static int32_t stbl[32] = {0,  2,  4,  1,  3,  5,  -1, -1, 6,  8,  10, 7,  9,  11, -1, -1,
                             12, 14, 16, 13, 15, 17, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

  reg = reg & 0xff;

  if ((reg == 0x04) && (data & 0x80)) {
    // IRQ RESET
    opl->status = 0;
    opl->reg[0x04] &= 0x7f;
    if (opl->adpcm) {
      OPL_ADPCM_resetStatus(opl->adpcm);
    }
    return;
  }

  opl->reg[reg] = data;

  if (reg == 0x01) {

    opl->test_flag = data;

  } else if (reg == 0x04) {

    if (data & 0x01) {
      latch_timer1(opl);
    }
    if (data & 0x02) {
      latch_timer2(opl);
    }

  } else if (0x07 <= reg && reg <= 0x12) {

    if (reg == 0x08) {
      opl->csm_mode = (data >> 7) & 1;
      opl->notesel = (data >> 6) & 1;
    }

    if (opl->adpcm != NULL && opl->chip_type == TYPE_Y8950) {
      if (reg == 0x08)
        OPL_ADPCM_writeReg(opl->adpcm, reg, (data & 0x0f) | 0x80);
      else
        OPL_ADPCM_writeReg(opl->adpcm, reg, data);
    }

  } else if (0x20 <= reg && reg < 0x40) {

    s = stbl[reg - 0x20];
    if (s >= 0) {
      opl->slot[s].patch->AM = (data >> 7) & 1;
      opl->slot[s].patch->PM = (data >> 6) & 1;
      opl->slot[s].patch->EG = (data >> 5) & 1;
      opl->slot[s].patch->KR = (data >> 4) & 1;
      opl->slot[s].patch->ML = (data) & 15;
      request_update(&(opl->slot[s]), UPDATE_ALL);
    }

  } else if (0x40 <= reg && reg < 0x60) {

    s = stbl[reg - 0x40];
    if (s >= 0) {
      opl->slot[s].patch->KL = (data >> 6) & 3;
      opl->slot[s].patch->TL = (data) & 63;
      request_update(&(opl->slot[s]), UPDATE_ALL);
    }

  } else if (0x60 <= reg && reg < 0x80) {

    s = stbl[reg - 0x60];
    if (s >= 0) {
      opl->slot[s].patch->AR = (data >> 4) & 15;
      opl->slot[s].patch->DR = (data) & 15;
      request_update(&(opl->slot[s]), UPDATE_EG);
    }

  } else if (0x80 <= reg && reg < 0xa0) {

    s = stbl[reg - 0x80];
    if (s >= 0) {
      opl->slot[s].patch->SL = (data >> 4) & 15;
      opl->slot[s].patch->RR = (data) & 15;
      request_update(&(opl->slot[s]), UPDATE_EG);
    }

  } else if (0xa0 <= reg && reg < 0xa9) {

    c = reg - 0xa0;
    set_fnumber(opl, c, data + ((opl->reg[reg + 0x10] & 3) << 8));

  } else if (0xb0 <= reg && reg < 0xb9) {

    c = reg - 0xb0;
    set_fnumber(opl, c, ((data & 3) << 8) + opl->reg[reg - 0x10]);
    set_block(opl, c, (data >> 2) & 7);
    update_key_status(opl);

  } else if (0xc0 <= reg && reg < 0xc9) {

    c = reg - 0xc0;
    opl->slot[c * 2].patch->FB = (data >> 1) & 7;
    opl->ch_alg[c] = data & 1;

  } else if (reg == 0xbd) {

    update_rhythm_mode(opl);
    update_key_status(opl);
    opl->am_mode = (data >> 7) & 1;
    opl->pm_mode = (data >> 6) & 1;

  } else if (0xe0 <= reg && reg < 0x100) {
    if (opl->chip_type == TYPE_YM3812 && (opl->reg[0x01] & 0x20)) {
      s = stbl[reg - 0xe0];
      if (s >= 0) {
        opl->slot[s].patch->WS = data & 3;
        request_update(&(opl->slot[s]), UPDATE_WS);
      }
    }
  }
}

uint8_t OPL_readIO(OPL *opl) { return opl->reg[opl->adr]; }

uint8_t OPL_status(OPL *opl) {
  uint8_t status = opl->status;

  if (opl->adpcm) {
    status |= OPL_ADPCM_status(opl->adpcm);
  }

  status &= ~(opl->reg[0x04] & 0x78); // IRQ MASK

  if (status & 0x78) {
    return status | 0x80; // IRQ=1
  }
  return status & 0x7f; // IRQ = 0
}

void OPL_writeADPCMData(OPL *opl, uint8_t type, uint32_t start, uint32_t length, const uint8_t *data) {
  if (opl->adpcm != NULL) {
    if (type == 0) {
      OPL_ADPCM_writeRAM(opl->adpcm, start, length, data);
    } else {
      OPL_ADPCM_writeROM(opl->adpcm, start, length, data);
    }
  }
}

/* Unified wrapper                                                            */
/* ------------------------------------------------------------------------- */

#include "mame_fmopl.h"

struct opl_chip
{
    opl_chip_type_t type;
    uint32_t clock;
    uint32_t rate;
    uint8_t address;
    OPL *fm;
    void *mame_fm;
    YM2413 *crab_opll;
    opl_irq_cb_t irq_cb;
    void *irq_opaque;
    uint8_t last_irq;
    const uint8_t *adpcm_rom;
    uint32_t adpcm_size;
    double timer_period[2];
    double timer_elapsed[2];
    uint8_t timer_enabled[2];
    int16_t *opll_tmp;
    int32_t opll_tmp_capacity;
};

static int opl_is_fm_type(opl_chip_type_t type)
{
    return type == OPL_CHIP_YM3526 || type == OPL_CHIP_YM3812 || type == OPL_CHIP_Y8950;
}

static int opl_is_mame_fm_type(opl_chip_type_t type)
{
    return type == OPL_CHIP_YM3526 || type == OPL_CHIP_Y8950;
}

static int opl_is_opll_type(opl_chip_type_t type)
{
    return type == OPL_CHIP_YM2413;
}

static uint8_t opl_emu8950_chip_type(opl_chip_type_t type)
{
    switch (type)
    {
    case OPL_CHIP_Y8950:  return 0;
    case OPL_CHIP_YM3526: return 1;
    case OPL_CHIP_YM3812: return 2;
    default:             return 1;
    }
}

static void opl_update_irq(opl_chip_t *chip)
{
    uint8_t status;
    uint8_t state;
    if (!chip || !chip->fm) return;
    status = OPL_status(chip->fm);
    state = (status & OPL_STATUS_IRQ) ? 1 : 0;
    if (chip->irq_cb && state != chip->last_irq)
        chip->irq_cb(chip->irq_opaque, state);
    chip->last_irq = state;
}

static void opl_mame_irq_callback(void *param, int irq)
{
    opl_chip_t *chip = (opl_chip_t *)param;
    uint8_t state = irq ? 1 : 0;
    if (!chip) return;
    if (chip->irq_cb && state != chip->last_irq)
        chip->irq_cb(chip->irq_opaque, state);
    chip->last_irq = state;
}

static void opl_mame_timer_callback(void *param, int timer, double period)
{
    opl_chip_t *chip = (opl_chip_t *)param;
    if (!chip || timer < 0 || timer > 1) return;
    chip->timer_period[timer] = period;
    chip->timer_elapsed[timer] = 0.0;
    chip->timer_enabled[timer] = period > 0.0;
}

static void opl_mame_tick_timers(opl_chip_t *chip, int32_t samples)
{
    int timer;
    double elapsed;
    if (!chip || !chip->mame_fm || samples <= 0 || !chip->rate) return;
    elapsed = (double)samples / (double)chip->rate;
    for (timer = 0; timer < 2; timer++)
    {
        if (!chip->timer_enabled[timer] || chip->timer_period[timer] <= 0.0)
            continue;
        chip->timer_elapsed[timer] += elapsed;
        while (chip->timer_enabled[timer] && chip->timer_elapsed[timer] >= chip->timer_period[timer])
        {
            chip->timer_elapsed[timer] -= chip->timer_period[timer];
            if (chip->type == OPL_CHIP_YM3526)
                ym3526_timer_over(chip->mame_fm, timer);
            else if (chip->type == OPL_CHIP_Y8950)
                y8950_timer_over(chip->mame_fm, timer);
        }
    }
}

static void opl_mame_set_handlers(opl_chip_t *chip)
{
    if (!chip || !chip->mame_fm) return;
    if (chip->type == OPL_CHIP_YM3526)
    {
        ym3526_set_irq_handler(chip->mame_fm, opl_mame_irq_callback, chip);
        ym3526_set_timer_handler(chip->mame_fm, opl_mame_timer_callback, chip);
    }
    else if (chip->type == OPL_CHIP_Y8950)
    {
        y8950_set_irq_handler(chip->mame_fm, opl_mame_irq_callback, chip);
        y8950_set_timer_handler(chip->mame_fm, opl_mame_timer_callback, chip);
    }
}

opl_chip_t *OPL_Create(opl_chip_type_t type, uint32_t clock, uint32_t sample_rate)
{
    opl_chip_t *chip = (opl_chip_t *)calloc(1, sizeof(*chip));
    if (!chip) return NULL;

    chip->type = type;
    chip->clock = clock ? clock : 3579545u;
    chip->rate = sample_rate ? sample_rate : 44100u;

    if (opl_is_mame_fm_type(type))
    {
        if (type == OPL_CHIP_YM3526)
            chip->mame_fm = ym3526_init(chip->clock, chip->rate);
        else
            chip->mame_fm = y8950_init(chip->clock, chip->rate);
        if (!chip->mame_fm)
        {
            free(chip);
            return NULL;
        }
        opl_mame_set_handlers(chip);
    }
    else if (opl_is_fm_type(type))
    {
        chip->fm = OPL_new(chip->clock, chip->rate);
        if (!chip->fm)
        {
            free(chip);
            return NULL;
        }
        OPL_setChipType(chip->fm, opl_emu8950_chip_type(type));
        OPL_setRate(chip->fm, chip->rate);
        OPL_setQuality(chip->fm, 0);
    }
    else if (opl_is_opll_type(type))
    {
        chip->crab_opll = ym2413_init((int32_t)chip->clock, (int32_t)chip->rate);
        if (!chip->crab_opll)
        {
            free(chip);
            return NULL;
        }
        ym2413_reset(chip->crab_opll);
    }
    else
    {
        free(chip);
        return NULL;
    }
    return chip;
}

void OPL_Destroy(opl_chip_t *chip)
{
    if (!chip) return;
    if (chip->mame_fm)
    {
        if (chip->type == OPL_CHIP_YM3526)
            ym3526_shutdown(chip->mame_fm);
        else if (chip->type == OPL_CHIP_Y8950)
            y8950_shutdown(chip->mame_fm);
    }
    if (chip->fm) OPL_delete(chip->fm);
    if (chip->crab_opll) ym2413_shutdown(chip->crab_opll);
    free(chip->opll_tmp);
    free(chip);
}

void OPL_Reset(opl_chip_t *chip)
{
    if (!chip) return;
    chip->address = 0;
    chip->last_irq = 0;
    memset(chip->timer_period, 0, sizeof(chip->timer_period));
    memset(chip->timer_elapsed, 0, sizeof(chip->timer_elapsed));
    memset(chip->timer_enabled, 0, sizeof(chip->timer_enabled));
    if (chip->mame_fm)
    {
        if (chip->type == OPL_CHIP_YM3526)
            ym3526_reset_chip(chip->mame_fm);
        else if (chip->type == OPL_CHIP_Y8950)
        {
            y8950_reset_chip(chip->mame_fm);
            if (chip->adpcm_rom && chip->adpcm_size)
                y8950_set_delta_t_memory(chip->mame_fm, (void *)chip->adpcm_rom, (int)chip->adpcm_size);
        }
        opl_mame_set_handlers(chip);
    }
    else if (chip->fm)
    {
        OPL_reset(chip->fm);
        OPL_setChipType(chip->fm, opl_emu8950_chip_type(chip->type));
        OPL_setRate(chip->fm, chip->rate);
        if (chip->type == OPL_CHIP_Y8950 && chip->adpcm_rom && chip->adpcm_size)
            OPL_writeADPCMData(chip->fm, 1, 0, chip->adpcm_size, chip->adpcm_rom);
        opl_update_irq(chip);
    }
    else if (chip->crab_opll)
    {
        ym2413_reset(chip->crab_opll);
    }
}

void OPL_SetIRQHandler(opl_chip_t *chip, opl_irq_cb_t cb, void *opaque)
{
    if (!chip) return;
    chip->irq_cb = cb;
    chip->irq_opaque = opaque;
    if (chip->mame_fm)
        opl_mame_set_handlers(chip);
    else
        opl_update_irq(chip);
}

void OPL_SetADPCMROM(opl_chip_t *chip, const uint8_t *rom, uint32_t size)
{
    if (!chip) return;
    chip->adpcm_rom = rom;
    chip->adpcm_size = size;
    if (chip->mame_fm && chip->type == OPL_CHIP_Y8950 && rom && size)
        y8950_set_delta_t_memory(chip->mame_fm, (void *)rom, (int)size);
    else if (chip->fm && chip->type == OPL_CHIP_Y8950 && rom && size)
        OPL_writeADPCMData(chip->fm, 1, 0, size, rom);
}

uint8_t OPL_ReadStatus(opl_chip_t *chip)
{
    uint8_t status;
    if (!chip) return 0xff;
    if (chip->mame_fm)
    {
        if (chip->type == OPL_CHIP_YM3526)
            return ym3526_read(chip->mame_fm, 0);
        if (chip->type == OPL_CHIP_Y8950)
            return y8950_read(chip->mame_fm, 0);
    }
    if (!chip->fm) return 0xff;
    status = OPL_status(chip->fm);
    if (chip->type == OPL_CHIP_YM3526 || chip->type == OPL_CHIP_YM3812)
        status |= 0x06;
    return status;
}

void OPL_Write(opl_chip_t *chip, uint32_t offset, uint8_t data)
{
    if (!chip) return;
    offset &= 1;
    if (!offset)
        chip->address = data;

    if (chip->mame_fm)
    {
        if (chip->type == OPL_CHIP_YM3526)
            ym3526_write(chip->mame_fm, (int)offset, data);
        else if (chip->type == OPL_CHIP_Y8950)
            y8950_write(chip->mame_fm, (int)offset, data);
    }
    else if (chip->fm)
    {
        OPL_writeIO(chip->fm, offset, data);
        opl_update_irq(chip);
    }
    else if (chip->crab_opll)
    {
        ym2413_write(chip->crab_opll, (int32_t)offset, data);
    }
}

void OPL_UpdateMono(opl_chip_t *chip, int16_t *buffer, int32_t samples)
{
    int32_t i;
    if (!buffer || samples <= 0) return;
    if (!chip)
    {
        memset(buffer, 0, (size_t)samples * sizeof(int16_t));
        return;
    }
    if (chip->mame_fm)
    {
        if (chip->type == OPL_CHIP_YM3526)
            ym3526_update_one(chip->mame_fm, buffer, samples);
        else if (chip->type == OPL_CHIP_Y8950)
            y8950_update_one(chip->mame_fm, buffer, samples);
        opl_mame_tick_timers(chip, samples);
        return;
    }
    if (chip->fm)
    {
        for (i = 0; i < samples; i++)
            buffer[i] = (int16_t)OPL_GAIN_APPLY(OPL_calc(chip->fm), OPL_EMU8950_GAIN_NUM, OPL_EMU8950_GAIN_DEN);
        opl_update_irq(chip);
        return;
    }
    if (chip->crab_opll)
    {
        int32_t i;
        int16_t *out[2];
        if (chip->opll_tmp_capacity < samples)
        {
            int16_t *new_tmp = (int16_t *)realloc(chip->opll_tmp, (size_t)samples * sizeof(int16_t));
            if (!new_tmp)
            {
                memset(buffer, 0, (size_t)samples * sizeof(int16_t));
                return;
            }
            chip->opll_tmp = new_tmp;
            chip->opll_tmp_capacity = samples;
        }
        out[0] = buffer;
        out[1] = chip->opll_tmp;
        ym2413_update(chip->crab_opll, out, samples);
        for (i = 0; i < samples; i++)
            buffer[i] = (int16_t)OPL_GAIN_APPLY(((int32_t)buffer[i] + chip->opll_tmp[i]) / 2, OPL_OPLL_GAIN_NUM, OPL_OPLL_GAIN_DEN);
        return;
    }
    memset(buffer, 0, (size_t)samples * sizeof(int16_t));
}

void OPL_UpdateStereo(opl_chip_t *chip, int16_t *left, int16_t *right, int32_t samples)
{
    int32_t i;
    if (!left || !right || samples <= 0) return;
    if (!chip)
    {
        memset(left, 0, (size_t)samples * sizeof(int16_t));
        memset(right, 0, (size_t)samples * sizeof(int16_t));
        return;
    }
    if (chip->mame_fm)
    {
        OPL_UpdateMono(chip, left, samples);
        memcpy(right, left, (size_t)samples * sizeof(int16_t));
        return;
    }
    if (chip->fm)
    {
        for (i = 0; i < samples; i++)
        {
            int32_t out[2] = {0, 0};
            OPL_calcStereo(chip->fm, out);
            left[i] = (int16_t)OPL_GAIN_APPLY(out[0], OPL_EMU8950_GAIN_NUM, OPL_EMU8950_GAIN_DEN);
            right[i] = (int16_t)OPL_GAIN_APPLY(out[1], OPL_EMU8950_GAIN_NUM, OPL_EMU8950_GAIN_DEN);
        }
        opl_update_irq(chip);
        return;
    }
    if (chip->crab_opll)
    {
        int16_t *out[2];
        out[0] = left;
        out[1] = right;
        ym2413_update(chip->crab_opll, out, samples);
        for (i = 0; i < samples; i++)
        {
            left[i] = (int16_t)OPL_GAIN_APPLY(left[i], OPL_OPLL_GAIN_NUM, OPL_OPLL_GAIN_DEN);
            right[i] = (int16_t)OPL_GAIN_APPLY(right[i], OPL_OPLL_GAIN_NUM, OPL_OPLL_GAIN_DEN);
        }
        return;
    }
    memset(left, 0, (size_t)samples * sizeof(int16_t));
    memset(right, 0, (size_t)samples * sizeof(int16_t));
}


#define OPL_WRAPPER_STATE_MAGIC   0x53504c4fu /* "OLPS" native-endian */
#define OPL_WRAPPER_STATE_VERSION 1u

typedef struct
{
    uint32_t magic;
    uint32_t version;
    uint32_t total_size;
    uint32_t type;
    uint32_t clock;
    uint32_t rate;
    uint32_t core_size;
    uint8_t address;
    uint8_t last_irq;
    uint8_t timer_enabled[2];
    double timer_period[2];
    double timer_elapsed[2];
} opl_wrapper_state_t;

static uint32_t opl_core_state_size(opl_chip_t *chip)
{
    if (!chip || !chip->mame_fm) return 0;
    if (chip->type == OPL_CHIP_YM3526)
        return ym3526_state_size(chip->mame_fm);
    if (chip->type == OPL_CHIP_Y8950)
        return y8950_state_size(chip->mame_fm);
    return 0;
}

uint32_t OPL_GetStateSize(opl_chip_t *chip)
{
    uint32_t core = opl_core_state_size(chip);
    if (!chip || !core) return 0;
    return (uint32_t)(sizeof(opl_wrapper_state_t) + core);
}

int OPL_SaveState(opl_chip_t *chip, void *data, uint32_t size)
{
    opl_wrapper_state_t st;
    uint8_t *p = (uint8_t *)data;
    uint32_t core;
    if (!chip || !data) return 0;
    core = opl_core_state_size(chip);
    if (!core || size < sizeof(st) + core) return 0;
    memset(&st, 0, sizeof(st));
    st.magic = OPL_WRAPPER_STATE_MAGIC;
    st.version = OPL_WRAPPER_STATE_VERSION;
    st.total_size = (uint32_t)(sizeof(st) + core);
    st.type = (uint32_t)chip->type;
    st.clock = chip->clock;
    st.rate = chip->rate;
    st.core_size = core;
    st.address = chip->address;
    st.last_irq = chip->last_irq;
    memcpy(st.timer_enabled, chip->timer_enabled, sizeof(st.timer_enabled));
    memcpy(st.timer_period, chip->timer_period, sizeof(st.timer_period));
    memcpy(st.timer_elapsed, chip->timer_elapsed, sizeof(st.timer_elapsed));
    memcpy(p, &st, sizeof(st));
    p += sizeof(st);
    if (chip->type == OPL_CHIP_YM3526)
        return ym3526_save_state(chip->mame_fm, p, core);
    if (chip->type == OPL_CHIP_Y8950)
        return y8950_save_state(chip->mame_fm, p, core);
    return 0;
}

int OPL_LoadState(opl_chip_t *chip, const void *data, uint32_t size)
{
    opl_wrapper_state_t st;
    const uint8_t *p = (const uint8_t *)data;
    int ok = 0;
    if (!chip || !data || size < sizeof(st)) return 0;
    memcpy(&st, p, sizeof(st));
    if (st.magic != OPL_WRAPPER_STATE_MAGIC || st.version != OPL_WRAPPER_STATE_VERSION ||
        st.type != (uint32_t)chip->type || st.total_size > size ||
        st.total_size < sizeof(st) || st.core_size != st.total_size - sizeof(st))
        return 0;
    p += sizeof(st);
    if (chip->type == OPL_CHIP_YM3526)
        ok = ym3526_load_state(chip->mame_fm, p, st.core_size);
    else if (chip->type == OPL_CHIP_Y8950)
        ok = y8950_load_state(chip->mame_fm, p, st.core_size);
    if (!ok) return 0;
    chip->address = st.address;
    chip->last_irq = st.last_irq;
    memcpy(chip->timer_enabled, st.timer_enabled, sizeof(chip->timer_enabled));
    memcpy(chip->timer_period, st.timer_period, sizeof(chip->timer_period));
    memcpy(chip->timer_elapsed, st.timer_elapsed, sizeof(chip->timer_elapsed));
    if (chip->mame_fm)
        opl_mame_set_handlers(chip);
    return 1;
}
