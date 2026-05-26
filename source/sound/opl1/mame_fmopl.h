/*
 * C99 adapter declarations for the MAME 0.72 FM OPL/Y8950 core.
 */
#ifndef SMSPLUS_MAME_FMOPL_H
#define SMSPLUS_MAME_FMOPL_H

#include <stdint.h>

#define BUILD_YM3812 0
#define BUILD_YM3526 1
#define BUILD_Y8950  1
#define OPL_SAMPLE_BITS 16

#ifndef INLINE
#define INLINE static inline
#endif

typedef int8_t INT8;
typedef uint8_t UINT8;
typedef int16_t INT16;
typedef uint16_t UINT16;
typedef int32_t INT32;
typedef uint32_t UINT32;

typedef int16_t stream_sample_t;
typedef stream_sample_t OPLSAMPLE;

typedef void (*OPL_TIMERHANDLER)(void *param, int timer, double period);
typedef void (*OPL_IRQHANDLER)(void *param, int irq);
typedef void (*OPL_UPDATEHANDLER)(void *param, int min_interval_us);
typedef void (*OPL_PORTHANDLER_W)(void *param, unsigned char data);
typedef unsigned char (*OPL_PORTHANDLER_R)(void *param);
typedef void (*STATUS_CHANGE_HANDLER)(void *chip, uint8_t status_bits);

typedef struct YM_DELTAT
{
    uint8_t *memory;
    int32_t *output_pointer;
    int32_t *pan;
    double freqbase;
    uint32_t memory_size;
    uint32_t memory_mask;
    int output_range;
    uint32_t now_addr;
    uint32_t now_step;
    uint32_t step;
    uint32_t start;
    uint32_t limit;
    uint32_t end;
    uint32_t delta;
    int32_t volume;
    int32_t acc;
    int32_t adpcmd;
    int32_t adpcml;
    int32_t prev_acc;
    uint8_t now_data;
    uint8_t CPU_data;
    uint8_t portstate;
    uint8_t control2;
    uint8_t portshift;
    uint8_t DRAMportshift;
    uint8_t memread;
    STATUS_CHANGE_HANDLER status_set_handler;
    STATUS_CHANGE_HANDLER status_reset_handler;
    void *status_change_which_chip;
    uint8_t status_change_EOS_bit;
    uint8_t status_change_BRDY_bit;
    uint8_t status_change_ZERO_bit;
    uint8_t PCM_BSY;
    uint8_t reg[16];
    uint8_t emulation_mode;
} YM_DELTAT;

#define YM_DELTAT_EMULATION_MODE_NORMAL 0
#define YM_DELTAT_EMULATION_MODE_YM2610 1

uint8_t YM_DELTAT_ADPCM_Read(YM_DELTAT *deltat);
void YM_DELTAT_ADPCM_Write(YM_DELTAT *deltat, int r, int v);
void YM_DELTAT_ADPCM_Reset(YM_DELTAT *deltat, int panidx, int mode);
void YM_DELTAT_ADPCM_CALC(YM_DELTAT *deltat);

void *ym3526_init(UINT32 clock, UINT32 rate);
void ym3526_shutdown(void *chip);
void ym3526_reset_chip(void *chip);
int ym3526_write(void *chip, int a, int v);
unsigned char ym3526_read(void *chip, int a);
int ym3526_timer_over(void *chip, int c);
void ym3526_set_timer_handler(void *chip, OPL_TIMERHANDLER timer_handler, void *param);
void ym3526_set_irq_handler(void *chip, OPL_IRQHANDLER IRQHandler, void *param);
void ym3526_set_update_handler(void *chip, OPL_UPDATEHANDLER UpdateHandler, void *param);
void ym3526_update_one(void *chip, OPLSAMPLE *buffer, int length);

void *y8950_init(UINT32 clock, UINT32 rate);
void y8950_shutdown(void *chip);
void y8950_reset_chip(void *chip);
int y8950_write(void *chip, int a, int v);
unsigned char y8950_read(void *chip, int a);
int y8950_timer_over(void *chip, int c);
void y8950_set_timer_handler(void *chip, OPL_TIMERHANDLER timer_handler, void *param);
void y8950_set_irq_handler(void *chip, OPL_IRQHANDLER IRQHandler, void *param);
void y8950_set_update_handler(void *chip, OPL_UPDATEHANDLER UpdateHandler, void *param);
void y8950_set_delta_t_memory(void *chip, void *deltat_mem_ptr, int deltat_mem_size);
void y8950_update_one(void *chip, OPLSAMPLE *buffer, int length);
void y8950_set_port_handler(void *chip, OPL_PORTHANDLER_W PortHandler_w, OPL_PORTHANDLER_R PortHandler_r, void *param);
void y8950_set_keyboard_handler(void *chip, OPL_PORTHANDLER_W KeyboardHandler_w, OPL_PORTHANDLER_R KeyboardHandler_r, void *param);

#endif /* SMSPLUS_MAME_FMOPL_H */
