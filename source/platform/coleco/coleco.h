#ifndef SMSPLUS_COLECO_PLATFORM_H_
#define SMSPLUS_COLECO_PLATFORM_H_

#include <stdint.h>

void coleco_port_w(uint16_t port, uint8_t data);
uint8_t coleco_port_r(uint16_t port);

#endif /* SMSPLUS_COLECO_PLATFORM_H_ */
