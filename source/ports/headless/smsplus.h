#ifndef SMSPLUS_HEADLESS_SMSPLUS_H_
#define SMSPLUS_HEADLESS_SMSPLUS_H_

#include <stdint.h>

#define HOST_WIDTH_RESOLUTION 256
#define HOST_HEIGHT_RESOLUTION 240

#define VIDEO_WIDTH_SMS 256
#define VIDEO_HEIGHT_SMS 192
#define VIDEO_WIDTH_GG 160
#define VIDEO_HEIGHT_GG 144

#define LOCK_VIDEO   do { } while (0);
#define UNLOCK_VIDEO do { } while (0);

#define SOUND_FREQUENCY 44100

void smsp_state(uint8_t slot_number, uint8_t mode);

#endif /* SMSPLUS_HEADLESS_SMSPLUS_H_ */
