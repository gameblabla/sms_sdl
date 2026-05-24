#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shared.h"
#include "headless_platform.h"

#if !defined(MAME_PSG) && !defined(MAXIM_PSG)
extern sn76489_t psg_sn;
#endif

typedef struct playback_event
{
    uint64_t frame;
    input_t input;
} playback_event_t;

struct smsplus_headless_platform
{
    smsplus_headless_platform_options_t opt;
    FILE *input_record;
    FILE *audio_wav;
    FILE *trace;
    FILE *video_y4m;
    playback_event_t *playback;
    size_t playback_count;
    size_t playback_index;
    uint64_t audio_bytes;
    uint32_t video_w;
    uint32_t video_h;
};

static uint8_t clamp_u8(int32_t v)
{
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (uint8_t)v;
}

static uint16_t read_pixel565(uint32_t x, uint32_t y)
{
    if (!bitmap.data || x >= bitmap.width || y >= bitmap.height) return 0;
    return ((const uint16_t *)(const void *)(bitmap.data + y * bitmap.pitch))[x];
}

static void pixel_to_rgb(uint16_t p, uint8_t *r, uint8_t *g, uint8_t *b)
{
    *r = (uint8_t)((((p >> 11) & 0x1F) * 255) / 31);
    *g = (uint8_t)((((p >> 5) & 0x3F) * 255) / 63);
    *b = (uint8_t)(((p & 0x1F) * 255) / 31);
}

static int write_file_bytes(const char *path, const void *data, size_t size)
{
    FILE *fp = fopen(path, "wb");
    if (!fp) return 0;
    int ok = fwrite(data, 1, size, fp) == size;
    fclose(fp);
    return ok;
}

static void make_frame_path(char *out, size_t out_size, const char *prefix, uint64_t frame, const char *suffix)
{
    snprintf(out, out_size, "%s_%06llu%s", prefix, (unsigned long long)frame, suffix);
}

static int save_ppm(const char *path)
{
    uint32_t x0 = (uint32_t)((bitmap.viewport.x < 0) ? 0 : bitmap.viewport.x);
    uint32_t w = (uint32_t)((bitmap.viewport.w > 0) ? bitmap.viewport.w : 256);
    uint32_t h = (uint32_t)((bitmap.viewport.h > 0) ? bitmap.viewport.h : vdp.height);
    FILE *fp = fopen(path, "wb");
    if (!fp) return 0;

    fprintf(fp, "P6\n%u %u\n255\n", w, h);
    for (uint32_t y = 0; y < h; y++)
    {
        for (uint32_t x = 0; x < w; x++)
        {
            uint8_t rgb[3];
            uint16_t p = read_pixel565(x0 + x, y);
            pixel_to_rgb(p, &rgb[0], &rgb[1], &rgb[2]);
            fwrite(rgb, 1, sizeof(rgb), fp);
        }
    }
    fclose(fp);
    return 1;
}

static void wav_write_header(FILE *fp, uint32_t sample_rate, uint32_t data_size)
{
    uint32_t riff_size = 36u + data_size;
    uint16_t audio_format = 1;
    uint16_t channels = 2;
    uint16_t bits_per_sample = 16;
    uint32_t byte_rate = sample_rate * channels * (bits_per_sample / 8);
    uint16_t block_align = channels * (bits_per_sample / 8);

    fseek(fp, 0, SEEK_SET);
    fwrite("RIFF", 1, 4, fp);
    fwrite(&riff_size, 4, 1, fp);
    fwrite("WAVEfmt ", 1, 8, fp);
    uint32_t fmt_size = 16;
    fwrite(&fmt_size, 4, 1, fp);
    fwrite(&audio_format, 2, 1, fp);
    fwrite(&channels, 2, 1, fp);
    fwrite(&sample_rate, 4, 1, fp);
    fwrite(&byte_rate, 4, 1, fp);
    fwrite(&block_align, 2, 1, fp);
    fwrite(&bits_per_sample, 2, 1, fp);
    fwrite("data", 1, 4, fp);
    fwrite(&data_size, 4, 1, fp);
}

static int wav_open(smsplus_headless_platform_t *p)
{
    if (!p->opt.audio_wav_path) return 1;
    p->audio_wav = fopen(p->opt.audio_wav_path, "wb+");
    if (!p->audio_wav) return 0;
    wav_write_header(p->audio_wav, SOUND_FREQUENCY, 0);
    return 1;
}

static void wav_close(smsplus_headless_platform_t *p)
{
    if (!p->audio_wav) return;
    uint32_t bounded = p->audio_bytes > 0xFFFFFFFFull ? 0xFFFFFFFFu : (uint32_t)p->audio_bytes;
    wav_write_header(p->audio_wav, SOUND_FREQUENCY, bounded);
    fclose(p->audio_wav);
    p->audio_wav = NULL;
}

static void yuv_from_rgb(uint8_t r, uint8_t g, uint8_t b, uint8_t *y, uint8_t *u, uint8_t *v)
{
    *y = clamp_u8(((66 * r + 129 * g + 25 * b + 128) >> 8) + 16);
    *u = clamp_u8(((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128);
    *v = clamp_u8(((112 * r - 94 * g - 18 * b + 128) >> 8) + 128);
}

static int y4m_open(smsplus_headless_platform_t *p)
{
    if (!p->opt.video_y4m_path) return 1;
    p->video_w = (sms.console == CONSOLE_GG && !option.extra_gg) ? 160u : 256u;
    p->video_h = (sms.console == CONSOLE_GG && !option.extra_gg) ? 144u : 240u;
    p->video_y4m = fopen(p->opt.video_y4m_path, "wb");
    if (!p->video_y4m) return 0;
    if (sms.display == DISPLAY_PAL)
        fprintf(p->video_y4m, "YUV4MPEG2 W%u H%u F50:1 Ip A1:1 C444\n", p->video_w, p->video_h);
    else
        fprintf(p->video_y4m, "YUV4MPEG2 W%u H%u F60000:1001 Ip A1:1 C444\n", p->video_w, p->video_h);
    return 1;
}

static int y4m_write_frame(smsplus_headless_platform_t *p)
{
    if (!p->video_y4m) return 1;

    size_t plane = (size_t)p->video_w * p->video_h;
    uint8_t *ybuf = calloc(plane * 3, 1);
    if (!ybuf) return 0;
    uint8_t *ubuf = ybuf + plane;
    uint8_t *vbuf = ubuf + plane;

    uint32_t x0 = (uint32_t)((bitmap.viewport.x < 0) ? 0 : bitmap.viewport.x);
    uint32_t active_w = (bitmap.viewport.w > 0) ? (uint32_t)bitmap.viewport.w : p->video_w;
    uint32_t active_h = (bitmap.viewport.h > 0) ? (uint32_t)bitmap.viewport.h : (uint32_t)vdp.height;
    if (active_w > p->video_w) active_w = p->video_w;
    if (active_h > p->video_h) active_h = p->video_h;

    for (uint32_t yy = 0; yy < p->video_h; yy++)
    {
        for (uint32_t xx = 0; xx < p->video_w; xx++)
        {
            uint8_t r = 0, g = 0, b = 0;
            if (xx < active_w && yy < active_h)
            {
                uint16_t pix = read_pixel565(x0 + xx, yy);
                pixel_to_rgb(pix, &r, &g, &b);
            }
            yuv_from_rgb(r, g, b, &ybuf[yy * p->video_w + xx], &ubuf[yy * p->video_w + xx], &vbuf[yy * p->video_w + xx]);
        }
    }

    fwrite("FRAME\n", 1, 6, p->video_y4m);
    fwrite(ybuf, 1, plane * 3, p->video_y4m);
    free(ybuf);
    return 1;
}

static int parse_u64(const char *s, uint64_t *out)
{
    char *end = NULL;
    errno = 0;
    unsigned long long v = strtoull(s, &end, 0);
    if (errno || end == s) return 0;
    *out = (uint64_t)v;
    return 1;
}

static int parse_i32(const char *s, int32_t *out)
{
    char *end = NULL;
    errno = 0;
    long v = strtol(s, &end, 0);
    if (errno || end == s) return 0;
    *out = (int32_t)v;
    return 1;
}

static int playback_load(smsplus_headless_platform_t *p)
{
    if (!p->opt.input_playback_path) return 1;
    FILE *fp = fopen(p->opt.input_playback_path, "rb");
    if (!fp) return 0;

    char line[512];
    size_t cap = 0;
    while (fgets(line, sizeof(line), fp))
    {
        char *tok[16];
        size_t ntok = 0;
        char *save = NULL;
        char *s = strtok_r(line, " \t\r\n,", &save);
        if (!s || s[0] == '#') continue;
        if (strcmp(s, "frame") == 0) continue;
        while (s && ntok < 16)
        {
            tok[ntok++] = s;
            s = strtok_r(NULL, " \t\r\n,", &save);
        }
        if (ntok < 4) continue;

        if (p->playback_count == cap)
        {
            cap = cap ? cap * 2 : 256;
            playback_event_t *next = realloc(p->playback, cap * sizeof(*next));
            if (!next)
            {
                fclose(fp);
                return 0;
            }
            p->playback = next;
        }

        playback_event_t *ev = &p->playback[p->playback_count];
        memset(ev, 0, sizeof(*ev));
        uint64_t frame = 0, pad0 = 0, pad1 = 0, system = 0;
        uint64_t m5row[SORDM5_KEY_ROWS] = {0, 0, 0, 0, 0, 0, 0};
        uint64_t m5reset = 0;
        int32_t analog[4] = {0, 0, 0, 0};
        if (!parse_u64(tok[0], &frame) || !parse_u64(tok[1], &pad0) ||
            !parse_u64(tok[2], &pad1) || !parse_u64(tok[3], &system))
            continue;
        for (size_t i = 4; i < ntok && i < 8; i++)
            parse_i32(tok[i], &analog[i - 4]);
        for (size_t i = 8; i < ntok && i < 15; i++)
            parse_u64(tok[i], &m5row[i - 8]);
        if (ntok > 15) parse_u64(tok[15], &m5reset);
        ev->frame = frame;
        ev->input.pad[0] = (uint8_t)pad0;
        ev->input.pad[1] = (uint8_t)pad1;
        ev->input.system = (uint8_t)system;
        ev->input.analog[0][0] = analog[0];
        ev->input.analog[0][1] = analog[1];
        ev->input.analog[1][0] = analog[2];
        ev->input.analog[1][1] = analog[3];
        for (size_t i = 0; i < SORDM5_KEY_ROWS; i++) ev->input.m5_key[i] = (uint8_t)m5row[i];
        ev->input.m5_reset = (uint8_t)m5reset;
        p->playback_count++;
    }

    fclose(fp);
    return 1;
}

static int dump_state(const char *prefix)
{
    char path[1024];
    uint8_t mem[0x10000];
    for (uint32_t a = 0; a < 0x10000; a++)
        mem[a] = cpu_readmap[a >> 10][a & 0x03FF];

    snprintf(path, sizeof(path), "%s_mem.bin", prefix);
    if (!write_file_bytes(path, mem, sizeof(mem))) return 0;
    snprintf(path, sizeof(path), "%s_wram.bin", prefix);
    if (!write_file_bytes(path, sms.wram, sizeof(sms.wram))) return 0;
    snprintf(path, sizeof(path), "%s_cart_sram.bin", prefix);
    if (!write_file_bytes(path, cart.sram, sizeof(cart.sram))) return 0;
    snprintf(path, sizeof(path), "%s_vram.bin", prefix);
    if (!write_file_bytes(path, vdp.vram, sizeof(vdp.vram))) return 0;
    snprintf(path, sizeof(path), "%s_cram.bin", prefix);
    if (!write_file_bytes(path, vdp.cram, sizeof(vdp.cram))) return 0;
    snprintf(path, sizeof(path), "%s_vdp_regs.bin", prefix);
    if (!write_file_bytes(path, vdp.reg, sizeof(vdp.reg))) return 0;

#if defined(MAME_PSG)
    snprintf(path, sizeof(path), "%s_psg_context.bin", prefix);
    if (!write_file_bytes(path, &PSG, sizeof(PSG))) return 0;
#elif defined(MAXIM_PSG)
    snprintf(path, sizeof(path), "%s_psg_context.bin", prefix);
    if (!write_file_bytes(path, SN76489_GetContextPtr(0), SN76489_GetContextSize())) return 0;
#else
    snprintf(path, sizeof(path), "%s_psg_context.bin", prefix);
    if (!write_file_bytes(path, &psg_sn, sizeof(psg_sn))) return 0;
#endif

    uint32_t fm_size = FM_GetContextSize();
    if (fm_size)
    {
        uint8_t *fm_ctx = malloc(fm_size);
        if (fm_ctx)
        {
            FM_GetContext(fm_ctx);
            snprintf(path, sizeof(path), "%s_ym2413_context.bin", prefix);
            write_file_bytes(path, fm_ctx, fm_size);
            free(fm_ctx);
        }
    }

    snprintf(path, sizeof(path), "%s_cpu.txt", prefix);
    FILE *fp = fopen(path, "wb");
    if (!fp) return 0;
    fprintf(fp,
            "console=%u display=%u territory=%u\n"
            "pc=%04X sp=%04X af=%04X bc=%04X de=%04X hl=%04X ix=%04X iy=%04X wz=%04X\n"
            "i=%02X r=%02X iff1=%u iff2=%u im=%u halt=%u cycles=%d line=%d\n"
            "vdp_mode=%u vdp_height=%u vdp_lpf=%u vdp_status=%02X vdp_addr=%04X vdp_code=%u\n",
            sms.console, sms.display, sms.territory,
            Z80.pc.w.l, Z80.sp.w.l, Z80.af.w.l, Z80.bc.w.l, Z80.de.w.l,
            Z80.hl.w.l, Z80.ix.w.l, Z80.iy.w.l, Z80.wz.w.l,
            Z80.i, (uint8_t)((Z80.r & 0x7f) | (Z80.r2 & 0x80)), Z80.iff1, Z80.iff2,
            Z80.im, Z80.halt, z80_get_elapsed_cycles(), vdp.line,
            vdp.mode, vdp.height, vdp.lpf, vdp.status, vdp.addr, vdp.code);
    fclose(fp);
    return 1;
}

int smsplus_headless_platform_create(smsplus_headless_platform_t **out,
                                     const smsplus_headless_platform_options_t *options)
{
    smsplus_headless_platform_t *p = calloc(1, sizeof(*p));
    if (!p) return 0;
    if (options) p->opt = *options;

    if (p->opt.input_record_path)
    {
        p->input_record = fopen(p->opt.input_record_path, "wb");
        if (!p->input_record) goto fail;
        fprintf(p->input_record, "frame pad0 pad1 system analog00 analog01 analog10 analog11 m5y0 m5y1 m5y2 m5y3 m5y4 m5y5 m5y6 m5reset\n");
    }
    if (!playback_load(p)) goto fail;
    if (!wav_open(p)) goto fail;
    if (p->opt.trace_path)
    {
        p->trace = fopen(p->opt.trace_path, "wb");
        if (!p->trace) goto fail;
    }
    SMSPLUS_TRACE_SET_TRACE(p->trace);
    if (!y4m_open(p)) goto fail;

    *out = p;
    return 1;

fail:
    smsplus_headless_platform_destroy(p);
    return 0;
}

void smsplus_headless_platform_destroy(smsplus_headless_platform_t *p)
{
    if (!p) return;
    wav_close(p);
    if (p->input_record) fclose(p->input_record);
    if (p->trace) fclose(p->trace);
    if (p->video_y4m) fclose(p->video_y4m);
    free(p->playback);
    free(p);
}

int smsplus_headless_platform_begin_frame(smsplus_headless_platform_t *p, uint64_t frame)
{
    SMSPLUS_TRACE_SET_FRAME(frame);
    if (!p) return 1;
    while (p->playback_index < p->playback_count && p->playback[p->playback_index].frame <= frame)
    {
        input = p->playback[p->playback_index].input;
        p->playback_index++;
    }
    return 1;
}

int smsplus_headless_platform_end_frame(smsplus_headless_platform_t *p, uint64_t frame)
{
    SMSPLUS_TRACE_CPU_FRAME(frame);
    if (!p) return 1;

    if (p->input_record)
    {
        fprintf(p->input_record, "%llu 0x%02X 0x%02X 0x%02X %d %d %d %d 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n",
                (unsigned long long)frame, input.pad[0], input.pad[1], input.system,
                input.analog[0][0], input.analog[0][1], input.analog[1][0], input.analog[1][1],
                input.m5_key[0], input.m5_key[1], input.m5_key[2], input.m5_key[3],
                input.m5_key[4], input.m5_key[5], input.m5_key[6], input.m5_reset);
    }

    if (p->audio_wav && snd.output && snd.sample_count > 0)
    {
        size_t bytes = (size_t)snd.sample_count * 2u * sizeof(int16_t);
        fwrite(snd.output, 1, bytes, p->audio_wav);
        p->audio_bytes += bytes;
    }

    if (p->video_y4m && !y4m_write_frame(p)) return 0;

    if (p->opt.screenshot_every && p->opt.screenshot_prefix && ((frame + 1) % p->opt.screenshot_every == 0))
    {
        char path[1024];
        make_frame_path(path, sizeof(path), p->opt.screenshot_prefix, frame + 1, ".ppm");
        save_ppm(path);
    }

    if (p->opt.dump_every && p->opt.dump_prefix && ((frame + 1) % p->opt.dump_every == 0))
    {
        char path[1024];
        make_frame_path(path, sizeof(path), p->opt.dump_prefix, frame + 1, "");
        dump_state(path);
    }

    return 1;
}

int smsplus_headless_platform_save_final(smsplus_headless_platform_t *p, uint64_t frame)
{
    int ok = 1;
    if (!p) return 1;
    if (p->opt.screenshot_path)
        ok &= save_ppm(p->opt.screenshot_path);
    if (p->opt.dump_prefix)
    {
        char path[1024];
        make_frame_path(path, sizeof(path), p->opt.dump_prefix, frame, "_final");
        ok &= dump_state(path);
    }
    return ok;
}
