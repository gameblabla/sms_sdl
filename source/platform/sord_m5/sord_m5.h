#ifndef SMSPLUS_SORD_M5_PLATFORM_H_
#define SMSPLUS_SORD_M5_PLATFORM_H_

#include <stdint.h>

void sordm5_port_w(uint16_t port, uint8_t data);
uint8_t sordm5_port_r(uint16_t port);
void sordm5_ctc_reset(void);
void sordm5_ctc_vdp_interrupt(void);
void sordm5_ctc_tick(int32_t cycles);
int32_t sordm5_ctc_irq_callback(void);

#endif /* SMSPLUS_SORD_M5_PLATFORM_H_ */
