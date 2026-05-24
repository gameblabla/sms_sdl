#ifndef SMSPLUS_SDL3_H
#define SMSPLUS_SDL3_H

#include <stdint.h>

#define HOST_WIDTH_RESOLUTION 960
#define HOST_HEIGHT_RESOLUTION 720

#define VIDEO_WIDTH_SMS 256
#define VIDEO_HEIGHT_SMS 192
#define VIDEO_WIDTH_GG 160
#define VIDEO_HEIGHT_GG 144

/* SDL3 updates the texture after system_frame(); core rendering writes directly into bitmap. */
#define LOCK_VIDEO do { } while (0)
#define UNLOCK_VIDEO do { } while (0)

void smsp_state(uint8_t slot_number, uint8_t mode);

#define SOUND_FREQUENCY 44100

#endif
