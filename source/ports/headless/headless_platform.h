#ifndef SMSPLUS_HEADLESS_PLATFORM_H_
#define SMSPLUS_HEADLESS_PLATFORM_H_

#include <stdint.h>

typedef struct smsplus_headless_platform smsplus_headless_platform_t;

typedef struct smsplus_headless_platform_options
{
    const char *input_playback_path;
    const char *input_record_path;
    const char *audio_wav_path;
    const char *trace_path;
    const char *dump_prefix;
    const char *screenshot_path;
    const char *screenshot_prefix;
    const char *video_y4m_path;
    uint32_t dump_every;
    uint32_t screenshot_every;
    uint8_t quiet;
} smsplus_headless_platform_options_t;

int smsplus_headless_platform_create(smsplus_headless_platform_t **out,
                                     const smsplus_headless_platform_options_t *options);
void smsplus_headless_platform_destroy(smsplus_headless_platform_t *platform);
int smsplus_headless_platform_begin_frame(smsplus_headless_platform_t *platform, uint64_t frame);
int smsplus_headless_platform_end_frame(smsplus_headless_platform_t *platform, uint64_t frame);
int smsplus_headless_platform_save_final(smsplus_headless_platform_t *platform, uint64_t frame);

#endif /* SMSPLUS_HEADLESS_PLATFORM_H_ */
