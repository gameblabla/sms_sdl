#ifndef SMSPLUS_INSPECT_H_
#define SMSPLUS_INSPECT_H_

#include <stdint.h>
#include <stdio.h>

#ifdef SMSPLUS_HEADLESS
void smsplus_inspect_set_trace(FILE *fp);
void smsplus_inspect_set_frame(uint64_t frame);
void smsplus_inspect_cpu_frame(uint64_t frame);
void smsplus_inspect_vdp_write(const char *chip, uint16_t port, uint8_t data,
                               int32_t line, uint16_t addr, uint8_t code);
void smsplus_inspect_psg_write(uint16_t port, uint8_t data);
void smsplus_inspect_ym_write(uint16_t port, uint8_t data);
void smsplus_inspect_mem_write(uint16_t addr, uint8_t data);

#define SMSPLUS_TRACE_SET_TRACE(fp) smsplus_inspect_set_trace(fp)
#define SMSPLUS_TRACE_SET_FRAME(f) smsplus_inspect_set_frame(f)
#define SMSPLUS_TRACE_CPU_FRAME(f) smsplus_inspect_cpu_frame(f)
#define SMSPLUS_TRACE_VDP_WRITE(chip, port, data) \
    smsplus_inspect_vdp_write((chip), (uint16_t)(port), (uint8_t)(data), vdp.line, vdp.addr, vdp.code)
#define SMSPLUS_TRACE_PSG_WRITE(port, data) smsplus_inspect_psg_write((uint16_t)(port), (uint8_t)(data))
#define SMSPLUS_TRACE_YM_WRITE(port, data) smsplus_inspect_ym_write((uint16_t)(port), (uint8_t)(data))
#define SMSPLUS_TRACE_MEM_WRITE(addr, data) smsplus_inspect_mem_write((uint16_t)(addr), (uint8_t)(data))
#else
#define SMSPLUS_TRACE_SET_TRACE(fp) ((void)0)
#define SMSPLUS_TRACE_SET_FRAME(f) ((void)0)
#define SMSPLUS_TRACE_CPU_FRAME(f) ((void)0)
#define SMSPLUS_TRACE_VDP_WRITE(chip, port, data) ((void)0)
#define SMSPLUS_TRACE_PSG_WRITE(port, data) ((void)0)
#define SMSPLUS_TRACE_YM_WRITE(port, data) ((void)0)
#define SMSPLUS_TRACE_MEM_WRITE(addr, data) ((void)0)
#endif

#endif /* SMSPLUS_INSPECT_H_ */
