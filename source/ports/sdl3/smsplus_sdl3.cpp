// SDL3 + Dear ImGui frontend for SMS Plus GX.
// This port is intentionally separate from the old SDL 1.2 frontend and the
// headless runner.  It drives the Sord M5 keyboard matrix directly while still
// exposing normal gamepad-style controls for SMS/GG/SG/Coleco games.

extern "C" {
#include "shared.h"
}

#include <SDL3/SDL.h>
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>
#include <unordered_map>

#ifndef SDL_BUTTON_LMASK
#define SDL_BUTTON_LMASK 0x01u
#endif
#ifndef SDL_SCANCODE_F5
#define SDL_SCANCODE_F5 ((SDL_Scancode)62)
#endif
#ifndef SDL_SCANCODE_F6
#define SDL_SCANCODE_F6 ((SDL_Scancode)63)
#endif
#ifndef SDL_SCANCODE_F8
#define SDL_SCANCODE_F8 ((SDL_Scancode)65)
#endif
#ifndef SDL_GAMEPAD_BUTTON_INVALID
#define SDL_GAMEPAD_BUTTON_INVALID ((SDL_GamepadButton)-1)
#endif

#ifndef SDL_TEXTUREACCESS_STREAMING
#define SDL_TEXTUREACCESS_STREAMING SDL_TEXTUREACCESS_STREAMING
#endif
#ifndef SDL_SCANCODE_KP_0
#define SDL_SCANCODE_KP_0 SDL_SCANCODE_0
#endif
#ifndef SDL_SCANCODE_KP_1
#define SDL_SCANCODE_KP_1 SDL_SCANCODE_1
#endif
#ifndef SDL_SCANCODE_KP_2
#define SDL_SCANCODE_KP_2 SDL_SCANCODE_2
#endif
#ifndef SDL_SCANCODE_KP_3
#define SDL_SCANCODE_KP_3 SDL_SCANCODE_3
#endif
#ifndef SDL_SCANCODE_KP_4
#define SDL_SCANCODE_KP_4 SDL_SCANCODE_4
#endif
#ifndef SDL_SCANCODE_KP_5
#define SDL_SCANCODE_KP_5 SDL_SCANCODE_5
#endif
#ifndef SDL_SCANCODE_KP_6
#define SDL_SCANCODE_KP_6 SDL_SCANCODE_6
#endif
#ifndef SDL_SCANCODE_KP_7
#define SDL_SCANCODE_KP_7 SDL_SCANCODE_7
#endif
#ifndef SDL_SCANCODE_KP_8
#define SDL_SCANCODE_KP_8 SDL_SCANCODE_8
#endif
#ifndef SDL_SCANCODE_KP_9
#define SDL_SCANCODE_KP_9 SDL_SCANCODE_9
#endif
#ifndef SDL_SCANCODE_KP_MULTIPLY
#define SDL_SCANCODE_KP_MULTIPLY SDL_SCANCODE_UNKNOWN
#endif
#ifndef SDL_SCANCODE_KP_HASH
#define SDL_SCANCODE_KP_HASH SDL_SCANCODE_UNKNOWN
#endif
#ifndef SDL_SCANCODE_APOSTROPHE
#define SDL_SCANCODE_APOSTROPHE SDL_SCANCODE_UNKNOWN
#endif
#ifndef SDL_SCANCODE_BACKSLASH
#define SDL_SCANCODE_BACKSLASH SDL_SCANCODE_UNKNOWN
#endif

extern "C" { t_config option; }

static void *g_pixels = nullptr;
static std::string g_sram_path;
static uint8_t g_m5_text_pulse[SORDM5_KEY_ROWS];

static constexpr int BITMAP_W = 256;
static constexpr int BITMAP_H = 313;

struct UiSettings
{
    bool keep_aspect = true;
    bool stretch = false;
    bool linear_filter = false;
    bool pixel_perfect = true;
    bool fullscreen = false;
    bool show_menu = true;
    bool show_keyboard = false;
    bool audio = true;
    bool force_lightgun = false;
    bool db_lightgun = false;
    bool mouse_lightgun = true;
    bool show_lightgun_cursor = true;
    int browser_index = 0;
    std::filesystem::path browser_dir = std::filesystem::current_path();
};

struct Binding
{
    const char *name;
    SDL_Scancode scan;
    SDL_GamepadButton button;
};

enum Action
{
    ACT_UP,
    ACT_DOWN,
    ACT_LEFT,
    ACT_RIGHT,
    ACT_A,
    ACT_B,
    ACT_M5_1,
    ACT_M5_2,
    ACT_PAUSE,
    ACT_MENU,
    ACT_VKBD,
    ACT_SAVE_STATE,
    ACT_LOAD_STATE,
    ACT_REWIND,
    ACT_COUNT
};

static std::array<Binding, ACT_COUNT> g_bindings = {{
    {"Up", SDL_SCANCODE_UP, SDL_GAMEPAD_BUTTON_DPAD_UP},
    {"Down", SDL_SCANCODE_DOWN, SDL_GAMEPAD_BUTTON_DPAD_DOWN},
    {"Left", SDL_SCANCODE_LEFT, SDL_GAMEPAD_BUTTON_DPAD_LEFT},
    {"Right", SDL_SCANCODE_RIGHT, SDL_GAMEPAD_BUTTON_DPAD_RIGHT},
    {"Button 1 / Space", SDL_SCANCODE_Z, SDL_GAMEPAD_BUTTON_SOUTH},
    {"Button 2 / Enter", SDL_SCANCODE_X, SDL_GAMEPAD_BUTTON_EAST},
    {"M5 1", SDL_SCANCODE_1, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER},
    {"M5 2", SDL_SCANCODE_2, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER},
    {"Pause", SDL_SCANCODE_RETURN, SDL_GAMEPAD_BUTTON_START},
    {"Menu", SDL_SCANCODE_ESCAPE, SDL_GAMEPAD_BUTTON_BACK},
    {"Virtual Keyboard", SDL_SCANCODE_TAB, SDL_GAMEPAD_BUTTON_NORTH},
    {"Save State", SDL_SCANCODE_F5, SDL_GAMEPAD_BUTTON_INVALID},
    {"Load State", SDL_SCANCODE_F8, SDL_GAMEPAD_BUTTON_INVALID},
    {"Rewind", SDL_SCANCODE_F6, SDL_GAMEPAD_BUTTON_INVALID},
}};

struct ColecoKeyBinding
{
    const char *name;
    uint8_t value;
    SDL_Scancode scan;
    SDL_Scancode alt_scan;
    SDL_GamepadButton button;
};

static std::array<ColecoKeyBinding, 12> g_coleco_key_bindings = {{
    {"Keypad 0", 0,  SDL_SCANCODE_0, SDL_SCANCODE_KP_0, SDL_GAMEPAD_BUTTON_INVALID},
    {"Keypad 1", 1,  SDL_SCANCODE_1, SDL_SCANCODE_KP_1, SDL_GAMEPAD_BUTTON_INVALID},
    {"Keypad 2", 2,  SDL_SCANCODE_2, SDL_SCANCODE_KP_2, SDL_GAMEPAD_BUTTON_INVALID},
    {"Keypad 3", 3,  SDL_SCANCODE_3, SDL_SCANCODE_KP_3, SDL_GAMEPAD_BUTTON_INVALID},
    {"Keypad 4", 4,  SDL_SCANCODE_4, SDL_SCANCODE_KP_4, SDL_GAMEPAD_BUTTON_INVALID},
    {"Keypad 5", 5,  SDL_SCANCODE_5, SDL_SCANCODE_KP_5, SDL_GAMEPAD_BUTTON_INVALID},
    {"Keypad 6", 6,  SDL_SCANCODE_6, SDL_SCANCODE_KP_6, SDL_GAMEPAD_BUTTON_INVALID},
    {"Keypad 7", 7,  SDL_SCANCODE_7, SDL_SCANCODE_KP_7, SDL_GAMEPAD_BUTTON_INVALID},
    {"Keypad 8", 8,  SDL_SCANCODE_8, SDL_SCANCODE_KP_8, SDL_GAMEPAD_BUTTON_INVALID},
    {"Keypad 9", 9,  SDL_SCANCODE_9, SDL_SCANCODE_KP_9, SDL_GAMEPAD_BUTTON_INVALID},
    {"Keypad *", 10, SDL_SCANCODE_APOSTROPHE, SDL_SCANCODE_KP_MULTIPLY, SDL_GAMEPAD_BUTTON_INVALID},
    {"Keypad #", 11, SDL_SCANCODE_BACKSLASH, SDL_SCANCODE_KP_HASH, SDL_GAMEPAD_BUTTON_INVALID},
}};

struct M5Key
{
    const char *label;
    uint8_t row;
    uint8_t mask;
    SDL_Keycode key;
    SDL_Scancode scan;
};

// Rows are active-high and follow the MAME m5.cpp input port layout where known.
// The direct number-row mapping is required by Pooyan's title screen.
static constexpr M5Key kM5Keys[] = {
    {"Ctrl", 0, 0x01, SDLK_LCTRL, SDL_SCANCODE_LCTRL},
    {"Func", 0, 0x02, SDLK_TAB, SDL_SCANCODE_TAB},
    {"Shift", 0, 0x04, SDLK_LSHIFT, SDL_SCANCODE_LSHIFT},
    {"RShift", 0, 0x08, SDLK_RSHIFT, SDL_SCANCODE_RSHIFT},
    {"Space", 0, 0x40, SDLK_SPACE, SDL_SCANCODE_SPACE},
    {"Enter", 0, 0x80, SDLK_RETURN, SDL_SCANCODE_RETURN},

    {"1", 1, 0x01, SDLK_1, SDL_SCANCODE_1}, {"2", 1, 0x02, SDLK_2, SDL_SCANCODE_2},
    {"3", 1, 0x04, SDLK_3, SDL_SCANCODE_3}, {"4", 1, 0x08, SDLK_4, SDL_SCANCODE_4},
    {"5", 1, 0x10, SDLK_5, SDL_SCANCODE_5}, {"6", 1, 0x20, SDLK_6, SDL_SCANCODE_6},
    {"7", 1, 0x40, SDLK_7, SDL_SCANCODE_7}, {"8", 1, 0x80, SDLK_8, SDL_SCANCODE_8},

    {"Q", 2, 0x01, SDLK_Q, SDL_SCANCODE_Q}, {"W", 2, 0x02, SDLK_W, SDL_SCANCODE_W},
    {"E", 2, 0x04, SDLK_E, SDL_SCANCODE_E}, {"R", 2, 0x08, SDLK_R, SDL_SCANCODE_R},
    {"T", 2, 0x10, SDLK_T, SDL_SCANCODE_T}, {"Y", 2, 0x20, SDLK_Y, SDL_SCANCODE_Y},
    {"U", 2, 0x40, SDLK_U, SDL_SCANCODE_U}, {"I", 2, 0x80, SDLK_I, SDL_SCANCODE_I},

    {"A", 3, 0x01, SDLK_A, SDL_SCANCODE_A}, {"S", 3, 0x02, SDLK_S, SDL_SCANCODE_S},
    {"D", 3, 0x04, SDLK_D, SDL_SCANCODE_D}, {"F", 3, 0x08, SDLK_F, SDL_SCANCODE_F},
    {"G", 3, 0x10, SDLK_G, SDL_SCANCODE_G}, {"H", 3, 0x20, SDLK_H, SDL_SCANCODE_H},
    {"J", 3, 0x40, SDLK_J, SDL_SCANCODE_J}, {"K", 3, 0x80, SDLK_K, SDL_SCANCODE_K},

    {"Z", 4, 0x01, SDLK_Z, SDL_SCANCODE_Z}, {"X", 4, 0x02, SDLK_X, SDL_SCANCODE_X},
    {"C", 4, 0x04, SDLK_C, SDL_SCANCODE_C}, {"V", 4, 0x08, SDLK_V, SDL_SCANCODE_V},
    {"B", 4, 0x10, SDLK_B, SDL_SCANCODE_B}, {"N", 4, 0x20, SDLK_N, SDL_SCANCODE_N},
    {"M", 4, 0x40, SDLK_M, SDL_SCANCODE_M}, {",", 4, 0x80, SDLK_COMMA, SDL_SCANCODE_COMMA},

    {"9", 5, 0x01, SDLK_9, SDL_SCANCODE_9}, {"0", 5, 0x02, SDLK_0, SDL_SCANCODE_0},
    {"-", 5, 0x04, SDLK_MINUS, SDL_SCANCODE_MINUS}, {"^", 5, 0x08, SDLK_EQUALS, SDL_SCANCODE_EQUALS},
    {".", 5, 0x10, SDLK_PERIOD, SDL_SCANCODE_PERIOD}, {"Down", 5, 0x20, SDLK_DOWN, SDL_SCANCODE_DOWN},
    {"_", 5, 0x40, SDLK_UNKNOWN, SDL_SCANCODE_GRAVE}, {"Back", 5, 0x80, SDLK_BACKSPACE, SDL_SCANCODE_BACKSPACE},

    {"O", 6, 0x01, SDLK_O, SDL_SCANCODE_O}, {"P", 6, 0x02, SDLK_P, SDL_SCANCODE_P},
    {"Up", 6, 0x04, SDLK_UP, SDL_SCANCODE_UP}, {"[", 6, 0x08, SDLK_LEFTBRACKET, SDL_SCANCODE_LEFTBRACKET},
    {"L", 6, 0x10, SDLK_L, SDL_SCANCODE_L}, {"Left", 6, 0x20, SDLK_LEFT, SDL_SCANCODE_LEFT},
    {"Right", 6, 0x40, SDLK_RIGHT, SDL_SCANCODE_RIGHT}, {"]", 6, 0x80, SDLK_RIGHTBRACKET, SDL_SCANCODE_RIGHTBRACKET},
};

struct RewindSnapshot
{
    uint64_t frame = 0;
    std::vector<uint8_t> data;
};

struct AppState
{
    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    SDL_Texture *texture = nullptr;
    SDL_AudioStream *audio_stream = nullptr;
    SDL_Gamepad *gamepad = nullptr;
    UiSettings ui;
    std::string rom_path;
    std::string bios_path;      // Generic/SMS BIOS path, also honored for legacy systems.
    std::string coleco_bios_path;
    std::string resolved_bios_path;
    std::string status;
    uint8_t requested_console = 0;
    bool running = true;
    bool rom_loaded = false;
    bool paused = false;
    int capture_binding = -1;
    int capture_gamepad_binding = -1;
    int capture_coleco_key_binding = -1;
    int capture_coleco_alt_key_binding = -1;
    int capture_coleco_gamepad_binding = -1;
    int vk_index = 0;
    int save_slot = 0;
    uint64_t frame_counter = 0;
    uint64_t last_autosave_frame = 0;
    uint64_t last_rewind_capture_frame = 0;
    bool autosave_enabled = true;
    bool rewind_enabled = true;
    bool rewind_hold = false;          // physical hotkey/gamepad hold state
    bool rewind_ui_hold = false;       // ImGui hold state carried from previous frame
    bool rewind_ui_hold_next = false;
    bool rewind_was_active = false;
    int autosave_interval_frames = 600;
    int rewind_interval_frames = 4;
    int rewind_step_display_frames = 2;
    int rewind_display_counter = 0;
    size_t rewind_max_snapshots = 900;
    std::vector<RewindSnapshot> rewind_snapshots;
    std::filesystem::path state_dir;
    std::filesystem::path save_dir;
    std::filesystem::path config_dir;
    SDL_Texture *state_thumb_texture = nullptr;
    std::filesystem::path state_thumb_path;
    int state_thumb_w = 0;
    int state_thumb_h = 0;
};

static void set_defaults()
{
    std::memset(&option, 0, sizeof(option));
    option.fullscreen = 0;
    option.fullspeed = 1;
    option.fm = 1;
    option.spritelimit = 1;
    option.tms_pal = 2;
    option.nosound = 0;
    option.soundlevel = 1;
    option.use_bios = 1;
    option.lcd_persistence = 1;
    option.lightgun_cursor = 1;
    option.lightgun_dpad_speed = 3;
}

static const char *ext_of(const std::string &path)
{
    const char *dot = std::strrchr(path.c_str(), '.');
    return dot ? dot : "";
}

static std::filesystem::path user_smsplus_dir()
{
#ifdef SMSPLUS_PORTABLE
    return std::filesystem::current_path();
#else
    const char *home = std::getenv("HOME");
    if (home && home[0]) return std::filesystem::path(home) / ".smsplus";
    return std::filesystem::current_path() / ".smsplus";
#endif
}

static void init_user_paths(AppState &app)
{
    std::filesystem::path root = user_smsplus_dir();
    app.config_dir = root;
    app.state_dir = root / "states";
    app.save_dir = root / "saves";
}


static void set_console_from_path(const std::string &path)
{
    const char *ext = ext_of(path);
    if (!SDL_strcasecmp(ext, ".m5")) option.console = 7;
    else if (!SDL_strcasecmp(ext, ".col")) option.console = 6;
    else if (!SDL_strcasecmp(ext, ".gg")) option.console = 3;
}

static uint8_t console_option_from_name(const char *name)
{
    if (!name) return 0;
    if (!SDL_strcasecmp(name, "sordm5") || !SDL_strcasecmp(name, "m5")) return 7;
    if (!SDL_strcasecmp(name, "coleco") || !SDL_strcasecmp(name, "colecovision")) return 6;
    if (!SDL_strcasecmp(name, "gg")) return 3;
    if (!SDL_strcasecmp(name, "ggms")) return 4;
    if (!SDL_strcasecmp(name, "sg") || !SDL_strcasecmp(name, "sg1000")) return 5;
    if (!SDL_strcasecmp(name, "sms2")) return 2;
    if (!SDL_strcasecmp(name, "sms")) return 1;
    return 0;
}

static bool load_exact(const std::string &path, uint8_t *dst, size_t dst_size, size_t min_size, size_t *actual = nullptr)
{
    FILE *fp = std::fopen(path.c_str(), "rb");
    if (!fp) return false;
    std::fseek(fp, 0, SEEK_END);
    long sz = std::ftell(fp);
    std::fseek(fp, 0, SEEK_SET);
    if (sz < 0 || static_cast<size_t>(sz) > dst_size || static_cast<size_t>(sz) < min_size)
    {
        std::fclose(fp);
        return false;
    }
    std::memset(dst, 0xFF, dst_size);
    size_t got = std::fread(dst, 1, static_cast<size_t>(sz), fp);
    std::fclose(fp);
    if (got != static_cast<size_t>(sz)) return false;
    if (actual) *actual = static_cast<size_t>(sz);
    return true;
}


static bool file_readable(const std::filesystem::path &path)
{
    std::error_code ec;
    return !path.empty() && std::filesystem::is_regular_file(path, ec);
}

static void append_bios_candidates(std::vector<std::filesystem::path> &out,
                                   const std::filesystem::path &dir,
                                   std::initializer_list<const char *> names)
{
    if (dir.empty()) return;
    for (const char *name : names)
        out.emplace_back(dir / name);
}

static std::string find_local_bios(const std::string &rom_path,
                                   std::initializer_list<const char *> names)
{
    std::vector<std::filesystem::path> candidates;
    std::error_code ec;

    append_bios_candidates(candidates, std::filesystem::current_path(ec), names);

    if (!rom_path.empty())
    {
        std::filesystem::path rp(rom_path);
        append_bios_candidates(candidates, rp.parent_path(), names);
    }

    const char *home = std::getenv("HOME");
    if (home && *home)
    {
        std::filesystem::path hp(home);
        append_bios_candidates(candidates, hp / ".smsplus" / "bios", names);
        append_bios_candidates(candidates, hp / ".smsplusgx" / "bios", names);
        append_bios_candidates(candidates, hp / ".config" / "smsplusgx" / "bios", names);
    }

    for (const auto &candidate : candidates)
        if (file_readable(candidate))
            return candidate.string();
    return {};
}

static bool init_bios(AppState &app)
{
    if (!bios.rom)
        bios.rom = static_cast<uint8_t *>(std::calloc(1, 0x100000));
    if (!bios.rom) return false;
    bios.enabled = 0;

    if (IS_SMS && !app.bios_path.empty())
    {
        size_t size = 0;
        if (!load_exact(app.bios_path, bios.rom, 0x100000, 1, &size)) return false;
        if (size < 0x4000) size = 0x4000;
        bios.enabled = static_cast<uint8_t>(option.use_bios | 2);
        bios.pages = static_cast<uint16_t>(size / 0x4000);
    }

    if (sms.console == CONSOLE_COLECO)
    {
        std::string path = app.coleco_bios_path.empty() ? app.bios_path : app.coleco_bios_path;
        if (path.empty())
            path = find_local_bios(app.rom_path, {"BIOS.col", "bios.col", "coleco.rom", "coleco.bin", "colecovision.rom", "COLECO.ROM"});
        if (path.empty())
        {
            app.status = "ColecoVision BIOS missing. Put BIOS.col next to the executable/ROM or pass --coleco-bios BIOS.col.";
            return false;
        }
        if (!load_exact(path, coleco.rom, sizeof(coleco.rom), 0x2000))
        {
            app.status = "Failed to load ColecoVision BIOS: " + path;
            return false;
        }
        app.coleco_bios_path = path;
        app.resolved_bios_path = path;
    }
    else if (sms.console == CONSOLE_SORDM5)
    {
        std::string path = app.bios_path;
        if (path.empty())
            path = find_local_bios(app.rom_path, {"sordm5bios.bin", "SORDM5BIOS.BIN", "m5bios.bin", "M5BIOS.BIN"});
        if (!path.empty() && !load_exact(path, coleco.rom, sizeof(coleco.rom), 0x2000))
        {
            app.status = "Failed to load Sord M5 BIOS: " + path;
            return false;
        }
        if (!path.empty()) app.resolved_bios_path = path;
    }
    return true;
}

static bool init_bitmap()
{
    if (!g_pixels)
        g_pixels = std::calloc(static_cast<size_t>(BITMAP_W) * BITMAP_H, SMSPLUS_RENDER_BYTES_PER_PIXEL);
    if (!g_pixels) return false;
    bitmap.width = BITMAP_W;
    bitmap.height = BITMAP_H;
    bitmap.depth = SMSPLUS_RENDER_DEPTH;
    bitmap.data = reinterpret_cast<uint8_t *>(g_pixels);
    bitmap.pitch = BITMAP_W * SMSPLUS_RENDER_BYTES_PER_PIXEL;
    bitmap.viewport.w = VIDEO_WIDTH_SMS;
    bitmap.viewport.h = VIDEO_HEIGHT_SMS;
    bitmap.viewport.x = 0;
    bitmap.viewport.y = 0;
    return true;
}

static std::string sanitize_component(std::string s)
{
    if (s.empty()) s = "cart";
    for (char &c : s)
    {
        unsigned char uc = static_cast<unsigned char>(c);
        if (!std::isalnum(uc) && c != '-' && c != '_' && c != '.') c = '_';
    }
    return s;
}

static bool ensure_state_dir(const std::filesystem::path &dir)
{
    std::error_code ec;
    if (std::filesystem::is_directory(dir, ec)) return true;
    return std::filesystem::create_directories(dir, ec) || std::filesystem::is_directory(dir, ec);
}

static std::filesystem::path state_path_for_slot(const std::string &rom_path, const std::filesystem::path &dir, int slot)
{
    std::filesystem::path rp(rom_path.empty() ? "cart" : rom_path);
    char suffix[64];
    std::snprintf(suffix, sizeof(suffix), "_%08X.slot%d.png", cart.crc, slot);
    return dir / (sanitize_component(rp.stem().string()) + suffix);
}

static std::filesystem::path autosave_path(const AppState &app)
{
    std::filesystem::path rp(app.rom_path.empty() ? "cart" : app.rom_path);
    char suffix[64];
    std::snprintf(suffix, sizeof(suffix), "_%08X.auto.png", cart.crc);
    return app.state_dir / (sanitize_component(rp.stem().string()) + suffix);
}

#ifndef SMSPLUS_RENDER_32BPP
static uint32_t rgb565_to_xrgb8888(uint16_t p)
{
    uint32_t r = (p >> 11) & 0x1F;
    uint32_t g = (p >> 5) & 0x3F;
    uint32_t b = p & 0x1F;
    r = (r << 3) | (r >> 2);
    g = (g << 2) | (g >> 4);
    b = (b << 3) | (b >> 2);
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}
#endif

static std::vector<uint32_t> capture_thumbnail(uint32_t &tw, uint32_t &th)
{
    int active_w = bitmap.viewport.w > 0 ? bitmap.viewport.w : 256;
    int active_h = bitmap.viewport.h > 0 ? bitmap.viewport.h : vdp.height;
    int src_x0 = std::max(0, bitmap.viewport.x);
    if (active_w <= 0) active_w = 256;
    if (active_h <= 0) active_h = 192;

    tw = static_cast<uint32_t>(std::min(active_w, 160));
    th = static_cast<uint32_t>(std::max(1, (active_h * static_cast<int>(tw) + active_w / 2) / active_w));
    std::vector<uint32_t> thumb(static_cast<size_t>(tw) * th, 0xFF000000u);

    for (uint32_t y = 0; y < th; y++)
    {
        int sy = static_cast<int>((static_cast<uint64_t>(y) * active_h) / th);
        const uint8_t *src_line = bitmap.data + sy * bitmap.pitch;
        for (uint32_t x = 0; x < tw; x++)
        {
            int sx = src_x0 + static_cast<int>((static_cast<uint64_t>(x) * active_w) / tw);
#ifdef SMSPLUS_RENDER_32BPP
            uint32_t p = *reinterpret_cast<const uint32_t *>(src_line + sx * 4);
            thumb[static_cast<size_t>(y) * tw + x] = 0xFF000000u | (p & 0x00FFFFFFu);
#else
            uint16_t p = *reinterpret_cast<const uint16_t *>(src_line + sx * 2);
            thumb[static_cast<size_t>(y) * tw + x] = rgb565_to_xrgb8888(p);
#endif
        }
    }
    return thumb;
}

static bool save_state_file(AppState &app, const std::filesystem::path &path)
{
    if (!app.rom_loaded)
    {
        app.status = "No game loaded.";
        return false;
    }
    if (!ensure_state_dir(path.parent_path()))
    {
        app.status = "Could not create state directory: " + path.parent_path().string();
        return false;
    }
    uint32_t tw = 0, th = 0;
    std::vector<uint32_t> thumb = capture_thumbnail(tw, th);
    if (!system_save_state_file_ex(path.string().c_str(), thumb.data(), tw, th, tw * sizeof(uint32_t)))
    {
        app.status = "Save state failed: " + path.string();
        return false;
    }
    app.status = "Saved state: " + path.string();
    return true;
}

static bool load_state_file(AppState &app, const std::filesystem::path &path)
{
    if (!app.rom_loaded)
    {
        app.status = "Load a game before loading a state.";
        return false;
    }
    if (!file_readable(path))
    {
        app.status = "State not found: " + path.string();
        return false;
    }
    if (!system_load_state_file(path.string().c_str()))
    {
        app.status = "Load state failed: " + path.string();
        return false;
    }
    app.status = "Loaded state: " + path.string();
    return true;
}

static void invalidate_state_thumbnail(AppState &app)
{
    if (app.state_thumb_texture)
    {
        SDL_DestroyTexture(app.state_thumb_texture);
        app.state_thumb_texture = nullptr;
    }
    app.state_thumb_path.clear();
    app.state_thumb_w = app.state_thumb_h = 0;
}

static void load_state_thumbnail(AppState &app, const std::filesystem::path &path)
{
    if (app.state_thumb_texture && app.state_thumb_path == path) return;
    invalidate_state_thumbnail(app);
    uint32_t *pixels = nullptr;
    uint32_t w = 0, h = 0;
    if (!system_state_png_read_thumbnail(path.string().c_str(), &pixels, &w, &h)) return;
    SDL_Texture *tex = SDL_CreateTexture(app.renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, (int)w, (int)h);
    if (tex)
    {
        SDL_UpdateTexture(tex, nullptr, pixels, (int)w * 4);
        SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
        app.state_thumb_texture = tex;
        app.state_thumb_path = path;
        app.state_thumb_w = (int)w;
        app.state_thumb_h = (int)h;
    }
    system_free_state_buffer(pixels);
}


static bool save_slot(AppState &app, int slot)
{
    return save_state_file(app, state_path_for_slot(app.rom_path, app.state_dir, slot));
}

static bool load_slot(AppState &app, int slot)
{
    return load_state_file(app, state_path_for_slot(app.rom_path, app.state_dir, slot));
}

static void capture_rewind_snapshot(AppState &app)
{
    if (!app.rom_loaded || !app.rewind_enabled) return;
    uint8_t *data = nullptr;
    uint32_t size = 0;
    if (!system_save_state_buffer(&data, &size)) return;
    RewindSnapshot snap;
    snap.frame = app.frame_counter;
    snap.data.assign(data, data + size);
    system_free_state_buffer(data);
    app.rewind_snapshots.push_back(std::move(snap));
    if (app.rewind_snapshots.size() > app.rewind_max_snapshots)
        app.rewind_snapshots.erase(app.rewind_snapshots.begin());
    app.last_rewind_capture_frame = app.frame_counter;
}

static void render_loaded_state_preview_frame()
{
    /*
     * Save states restore VDP/VRAM/CRAM/registers, not the host framebuffer.
     * For Nintendo-Online-style rewind feedback, repaint the current VDP state
     * immediately after each rewind step without running CPU/audio forward.
     */
    int32_t old_line = vdp.line;
    text_counter = 0;
    for (vdp.line = 0; vdp.line < vdp.lpf; vdp.line++)
        render_line(vdp.line);
    vdp.line = old_line;
}

static bool rewind_one_snapshot(AppState &app)
{
    if (!app.rom_loaded || app.rewind_snapshots.empty()) return false;
    RewindSnapshot snap = std::move(app.rewind_snapshots.back());
    app.rewind_snapshots.pop_back();
    if (!snap.data.empty())
    {
        if (system_load_state_buffer(snap.data.data(), static_cast<uint32_t>(snap.data.size())))
        {
            app.frame_counter = snap.frame;
            render_loaded_state_preview_frame();
            app.status = "Rewinding... frame " + std::to_string(static_cast<unsigned long long>(snap.frame));
            return true;
        }
    }
    return false;
}

static bool rewind_active(const AppState &app)
{
    return app.rewind_enabled && app.rom_loaded && (app.rewind_hold || app.rewind_ui_hold);
}

static void run_rewind(AppState &app)
{
    if (!rewind_active(app))
    {
        if (app.rewind_was_active)
        {
            app.rewind_display_counter = 0;
            app.status = "Rewind released; resumed from frame " +
                         std::to_string(static_cast<unsigned long long>(app.frame_counter));
        }
        app.rewind_was_active = false;
        return;
    }

    if (!app.rewind_was_active)
    {
        app.rewind_was_active = true;
        app.rewind_display_counter = 0;
        app.status = "Rewind: hold to move backward, release to resume.";
    }

    if (app.rewind_snapshots.empty())
    {
        app.status = "Rewind buffer empty.";
        return;
    }

    if ((app.rewind_display_counter++ % std::max(1, app.rewind_step_display_frames)) == 0)
        rewind_one_snapshot(app);
}

static void maybe_autosave(AppState &app)
{
    if (!app.autosave_enabled || !app.rom_loaded) return;
    if (app.frame_counter - app.last_autosave_frame < static_cast<uint64_t>(app.autosave_interval_frames)) return;
    save_state_file(app, autosave_path(app));
    app.last_autosave_frame = app.frame_counter;
}

static void maybe_capture_rewind(AppState &app)
{
    if (!app.rewind_enabled || !app.rom_loaded || rewind_active(app)) return;
    if (app.rewind_snapshots.empty() ||
        app.frame_counter - app.last_rewind_capture_frame >= static_cast<uint64_t>(app.rewind_interval_frames))
        capture_rewind_snapshot(app);
}


extern "C" void smsp_state(uint8_t slot_number, uint8_t mode)
{
    std::filesystem::path dir = user_smsplus_dir() / "states";
    std::filesystem::path path = state_path_for_slot(option.game_name, dir, slot_number);
    ensure_state_dir(dir);
    if (mode == 0)
        system_save_state_file_ex(path.string().c_str(), nullptr, 0, 0, 0);
    else if (mode == 1)
        system_load_state_file(path.string().c_str());
}

extern "C" void system_manage_sram(uint8_t *sram, uint8_t slot_number, uint8_t mode)
{
    (void)slot_number;
    if (g_sram_path.empty())
    {
        if (mode == SRAM_LOAD) std::memset(sram, 0, 0x8000);
        return;
    }
    if (mode == SRAM_LOAD)
    {
        FILE *fp = std::fopen(g_sram_path.c_str(), "rb");
        if (fp)
        {
            std::fread(sram, 1, 0x8000, fp);
            std::fclose(fp);
            sms.save = 1;
        }
        else std::memset(sram, 0, 0x8000);
    }
    else if (mode == SRAM_SAVE && sms.save)
    {
        FILE *fp = std::fopen(g_sram_path.c_str(), "wb");
        if (fp)
        {
            std::fwrite(sram, 1, 0x8000, fp);
            std::fclose(fp);
        }
    }
}

static bool load_game(AppState &app, const std::string &path)
{
    if (path.empty()) return false;
    if (app.rom_loaded)
    {
        system_poweroff();
        app.rom_loaded = false;
    }

    set_defaults();
    if (app.requested_console) option.console = app.requested_console;
    else set_console_from_path(path);
    std::snprintf(option.game_name, sizeof(option.game_name), "%s", path.c_str());
    if (!load_rom(const_cast<char *>(path.c_str())))
    {
        app.status = "Failed to load ROM: " + path;
        return false;
    }
    if (!init_bitmap())
    {
        app.status = "Failed to initialize bitmap";
        return false;
    }
    if (!init_bios(app))
    {
        if (app.status.empty()) app.status = "Failed to initialize BIOS";
        return false;
    }
    system_poweron();
    app.ui.db_lightgun = (sms.device[0] == DEVICE_LIGHTGUN);
    if (app.ui.force_lightgun) sms.device[0] = DEVICE_LIGHTGUN;
    app.rom_path = path;
    if (g_sram_path.empty())
    {
        ensure_state_dir(app.save_dir);
        std::filesystem::path rp(app.rom_path.empty() ? "cart" : app.rom_path);
        char suffix[64];
        std::snprintf(suffix, sizeof(suffix), "_%08X.sav", cart.crc);
        g_sram_path = (app.save_dir / (sanitize_component(rp.stem().string()) + suffix)).string();
    }
    app.rom_loaded = true;
    app.paused = false;
    app.frame_counter = 0;
    app.last_autosave_frame = 0;
    app.last_rewind_capture_frame = 0;
    app.rewind_hold = false;
    app.rewind_ui_hold = false;
    app.rewind_ui_hold_next = false;
    app.rewind_was_active = false;
    app.rewind_display_counter = 0;
    app.rewind_snapshots.clear();
    ensure_state_dir(app.state_dir);
    invalidate_state_thumbnail(app);
    app.status = "Loaded " + path;
    return true;
}


static void set_m5_key(uint8_t row, uint8_t mask, bool down)
{
    if (row >= SORDM5_KEY_ROWS) return;
    if (down) input.m5_key[row] |= mask;
    else input.m5_key[row] &= static_cast<uint8_t>(~mask);
}

static bool m5_key_from_sdl(SDL_Keycode key, SDL_Scancode scan, bool down)
{
    bool handled = false;
    for (const M5Key &mk : kM5Keys)
    {
        if ((key != SDLK_UNKNOWN && key == mk.key) || (scan != SDL_SCANCODE_UNKNOWN && scan == mk.scan))
        {
            set_m5_key(mk.row, mk.mask, down);
            handled = true;
        }
    }
    return handled;
}

static bool m5_text_char_is_bound_action(unsigned char c)
{
    if (c >= 'a' && c <= 'z') c = static_cast<unsigned char>(std::toupper(c));
    for (int i = 0; i < ACT_COUNT; i++)
    {
        if (i == ACT_M5_1 || i == ACT_M5_2 || i == ACT_MENU || i == ACT_VKBD)
            continue;
        SDL_Scancode sc = g_bindings[i].scan;
        if ((c >= 'A' && c <= 'Z') && sc == static_cast<SDL_Scancode>(SDL_SCANCODE_A + (c - 'A')))
            return true;
        if ((c >= '0' && c <= '9') && sc == static_cast<SDL_Scancode>(SDL_SCANCODE_0 + (c - '0')))
            return true;
        if (c == ' ' && sc == SDL_SCANCODE_SPACE)
            return true;
        if ((c == '\r' || c == '\n') && sc == SDL_SCANCODE_RETURN)
            return true;
    }
    return false;
}

static void m5_key_from_text(const char *txt)
{
    if (!txt || !txt[0]) return;
    unsigned char c = static_cast<unsigned char>(txt[0]);
    if (c >= 'a' && c <= 'z') c = static_cast<unsigned char>(std::toupper(c));
    if (m5_text_char_is_bound_action(c)) return;
    for (const M5Key &mk : kM5Keys)
    {
        if (mk.label[0] && !mk.label[1] && static_cast<unsigned char>(mk.label[0]) == c)
        {
            set_m5_key(mk.row, mk.mask, true);
            g_m5_text_pulse[mk.row] |= mk.mask;
        }
    }
}

static void apply_action(Action action, bool down)
{
    switch (action)
    {
        case ACT_UP:    if (down) input.pad[0] |= INPUT_UP; else input.pad[0] &= ~INPUT_UP; break;
        case ACT_DOWN:  if (down) input.pad[0] |= INPUT_DOWN; else input.pad[0] &= ~INPUT_DOWN; break;
        case ACT_LEFT:  if (down) input.pad[0] |= INPUT_LEFT; else input.pad[0] &= ~INPUT_LEFT; break;
        case ACT_RIGHT: if (down) input.pad[0] |= INPUT_RIGHT; else input.pad[0] &= ~INPUT_RIGHT; break;
        case ACT_A:
            if (down) input.pad[0] |= INPUT_BUTTON1; else input.pad[0] &= ~INPUT_BUTTON1;
            set_m5_key(0, 0x40, down); // Space
            break;
        case ACT_B:
            if (down) input.pad[0] |= INPUT_BUTTON2; else input.pad[0] &= ~INPUT_BUTTON2;
            set_m5_key(0, 0x80, down); // Enter
            break;
        case ACT_M5_1:
            set_m5_key(1, 0x01, down);
            if (down) input.system |= INPUT_START; else input.system &= ~INPUT_START;
            break;
        case ACT_M5_2:
            set_m5_key(1, 0x02, down);
            if (down) input.system |= INPUT_PAUSE; else input.system &= ~INPUT_PAUSE;
            break;
        case ACT_PAUSE:
            if (down) input.system |= (IS_GG ? INPUT_START : INPUT_PAUSE);
            else input.system &= static_cast<uint8_t>(~(IS_GG ? INPUT_START : INPUT_PAUSE));
            break;
        default: break;
    }
}

static void update_keyboard_state_from_bindings()
{
    int nkeys = 0;
    const bool *state = SDL_GetKeyboardState(&nkeys);
    if (!state) return;
    for (int i = 0; i < ACT_COUNT; i++)
    {
        SDL_Scancode sc = g_bindings[i].scan;
        if (sc > SDL_SCANCODE_UNKNOWN && sc < nkeys)
            apply_action(static_cast<Action>(i), state[sc]);
    }
}

static bool keyboard_scancode_down(const bool *state, int nkeys, SDL_Scancode sc)
{
    return state && sc > SDL_SCANCODE_UNKNOWN && sc < nkeys && state[sc];
}

static void update_coleco_keypad_from_inputs(AppState &app)
{
    if (sms.console != CONSOLE_COLECO) return;

    int nkeys = 0;
    const bool *state = SDL_GetKeyboardState(&nkeys);
    coleco.keypad[0] = 0xff;
    coleco.keypad[1] = 0xff;

    for (const ColecoKeyBinding &binding : g_coleco_key_bindings)
    {
        bool pressed = keyboard_scancode_down(state, nkeys, binding.scan) ||
                       keyboard_scancode_down(state, nkeys, binding.alt_scan);
        if (!pressed && app.gamepad && binding.button != SDL_GAMEPAD_BUTTON_INVALID)
            pressed = SDL_GetGamepadButton(app.gamepad, binding.button);
        if (pressed)
        {
            coleco.keypad[0] = binding.value;
            return;
        }
    }
}

static SDL_FRect compute_dest_rect(const UiSettings &ui, int active_w, int active_h, int win_w, int win_h);

static bool lightgun_active()
{
    return sms.device[0] == DEVICE_LIGHTGUN || sms.device[1] == DEVICE_LIGHTGUN;
}

static int lightgun_port()
{
    if (sms.device[0] == DEVICE_LIGHTGUN) return 0;
    if (sms.device[1] == DEVICE_LIGHTGUN) return 1;
    return 0;
}

static void set_lightgun_trigger(int port, bool down)
{
    if (down) input.pad[port] |= INPUT_BUTTON1;
    else input.pad[port] &= static_cast<uint8_t>(~INPUT_BUTTON1);
}

static bool window_point_to_lightgun(AppState &app, float wx, float wy, bool trigger)
{
    if (!app.rom_loaded || !lightgun_active()) return false;
    int win_w = 0, win_h = 0;
    SDL_GetWindowSize(app.window, &win_w, &win_h);
    int active_w = bitmap.viewport.w > 0 ? bitmap.viewport.w : 256;
    int active_h = bitmap.viewport.h > 0 ? bitmap.viewport.h : vdp.height;
    SDL_FRect dst = compute_dest_rect(app.ui, active_w, active_h, win_w, win_h);
    if (dst.w <= 0.0f || dst.h <= 0.0f) return false;
    if (wx < dst.x || wy < dst.y || wx >= dst.x + dst.w || wy >= dst.y + dst.h)
    {
        set_lightgun_trigger(lightgun_port(), trigger);
        return false;
    }

    int port = lightgun_port();
    float nx = (wx - dst.x) / dst.w;
    float ny = (wy - dst.y) / dst.h;
    int x = static_cast<int>(nx * active_w + 0.5f);
    int y = static_cast<int>(ny * active_h + 0.5f);
    if (x < 0) x = 0;
    if (x > 255) x = 255;
    if (y < 0) y = 0;
    if (y >= active_h) y = active_h - 1;
    input.analog[port][0] = x;
    input.analog[port][1] = y;
    set_lightgun_trigger(port, trigger);
    return true;
}

static void update_lightgun_mouse(AppState &app)
{
    if (!app.ui.mouse_lightgun || !app.rom_loaded || !lightgun_active()) return;
    float mx = 0.0f, my = 0.0f;
    uint32_t buttons = static_cast<uint32_t>(SDL_GetMouseState(&mx, &my));
    window_point_to_lightgun(app, mx, my, (buttons & SDL_BUTTON_LMASK) != 0);
}

static void open_first_gamepad(AppState &app)
{
    int count = 0;
    SDL_JoystickID *ids = SDL_GetGamepads(&count);
    if (ids && count > 0)
        app.gamepad = SDL_OpenGamepad(ids[0]);
    SDL_free(ids);
}

static bool handle_action_command(AppState &app, Action action, bool down)
{
    if (action == ACT_SAVE_STATE)
    {
        if (down) save_slot(app, app.save_slot);
        return true;
    }
    if (action == ACT_LOAD_STATE)
    {
        if (down) load_slot(app, app.save_slot);
        return true;
    }
    if (action == ACT_REWIND)
    {
        app.rewind_hold = down;
        if (down && !app.rewind_enabled)
            app.status = "Rewind is disabled.";
        return true;
    }
    return false;
}

static const char *gamepad_button_name(SDL_GamepadButton button)
{
    switch (button)
    {
        case SDL_GAMEPAD_BUTTON_DPAD_UP: return "D-pad Up";
        case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return "D-pad Down";
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return "D-pad Left";
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return "D-pad Right";
        case SDL_GAMEPAD_BUTTON_SOUTH: return "South";
        case SDL_GAMEPAD_BUTTON_EAST: return "East";
        case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: return "L Shoulder";
        case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return "R Shoulder";
        case SDL_GAMEPAD_BUTTON_START: return "Start";
        case SDL_GAMEPAD_BUTTON_BACK: return "Back";
        case SDL_GAMEPAD_BUTTON_NORTH: return "North";
        default: return "Unbound";
    }
}

static void handle_gamepad_button(AppState &app, SDL_GamepadButton button, bool down)
{
    if (app.capture_gamepad_binding >= 0 && down)
    {
        g_bindings[app.capture_gamepad_binding].button = button;
        app.capture_gamepad_binding = -1;
        return;
    }
    if (app.capture_coleco_gamepad_binding >= 0 && down)
    {
        g_coleco_key_bindings[app.capture_coleco_gamepad_binding].button = button;
        app.capture_coleco_gamepad_binding = -1;
        return;
    }

    for (int i = 0; i < ACT_COUNT; i++)
    {
        if (g_bindings[i].button == button)
        {
            Action action = static_cast<Action>(i);
            if (handle_action_command(app, action, down)) continue;
            if (i == ACT_MENU && down) app.ui.show_menu = !app.ui.show_menu;
            else if (i == ACT_VKBD && down) app.ui.show_keyboard = !app.ui.show_keyboard;
            else apply_action(action, down);
        }
    }
}

static void parse_args(AppState &app, int argc, char **argv)
{
    for (int i = 1; i < argc; i++)
    {
        const char *a = argv[i];
        auto need = [&](const char *opt) -> const char * {
            if (i + 1 >= argc) { std::fprintf(stderr, "Missing value for %s\n", opt); return nullptr; }
            return argv[++i];
        };
        if (!std::strcmp(a, "--rom")) { if (const char *v = need(a)) app.rom_path = v; }
        else if (!std::strcmp(a, "--bios")) { if (const char *v = need(a)) app.bios_path = v; }
        else if (!std::strcmp(a, "--coleco-bios")) { if (const char *v = need(a)) app.coleco_bios_path = v; }
        else if (!std::strcmp(a, "--sram")) { if (const char *v = need(a)) g_sram_path = v; }
        else if (!std::strcmp(a, "--state-dir")) { if (const char *v = need(a)) app.state_dir = v; }
        else if (!std::strcmp(a, "--console"))
        {
            if (const char *v = need(a))
            {
                app.requested_console = console_option_from_name(v);
                if (!app.requested_console)
                    std::fprintf(stderr, "Unknown console '%s'\n", v);
                else
                    option.console = app.requested_console;
            }
        }
        else if (!std::strcmp(a, "--fullscreen")) app.ui.fullscreen = true;
        else if (!std::strcmp(a, "--stretch")) { app.ui.stretch = true; app.ui.keep_aspect = false; app.ui.pixel_perfect = false; }
        else if (!std::strcmp(a, "--linear")) app.ui.linear_filter = true;
        else if (!std::strcmp(a, "--nearest") || !std::strcmp(a, "--pixel-perfect")) { app.ui.linear_filter = false; app.ui.pixel_perfect = true; }
        else if (!std::strcmp(a, "--lcd-persistence")) option.lcd_persistence = 1;
        else if (!std::strcmp(a, "--no-lcd-persistence")) option.lcd_persistence = 0;
        else if (!std::strcmp(a, "--no-audio")) app.ui.audio = false;
        else if (!std::strcmp(a, "--no-autosave")) app.autosave_enabled = false;
        else if (!std::strcmp(a, "--no-rewind")) app.rewind_enabled = false;
        else if (!std::strcmp(a, "--rewind-interval")) { if (const char *v = need(a)) app.rewind_interval_frames = std::max(1, std::atoi(v)); }
        else if (!std::strcmp(a, "--rewind-steps")) { if (const char *v = need(a)) app.rewind_max_snapshots = static_cast<size_t>(std::max(1, std::atoi(v))); }
        else if (!std::strcmp(a, "--lightgun")) app.ui.force_lightgun = true;
        else if (!std::strcmp(a, "--no-mouse-lightgun")) app.ui.mouse_lightgun = false;
        else if (!std::strcmp(a, "--lightgun-cursor")) { app.ui.show_lightgun_cursor = true; option.lightgun_cursor = 1; }
        else if (!std::strcmp(a, "--no-lightgun-cursor")) { app.ui.show_lightgun_cursor = false; option.lightgun_cursor = 0; }
        else if (!std::strcmp(a, "--hide-menu")) app.ui.show_menu = false;
        else if (a[0] != '-') app.rom_path = a;
    }
}

static SDL_FRect compute_dest_rect(const UiSettings &ui, int active_w, int active_h, int win_w, int win_h)
{
    SDL_FRect dst{0, 0, static_cast<float>(win_w), static_cast<float>(win_h)};
    if (ui.stretch || !ui.keep_aspect) return dst;

    float sx = static_cast<float>(win_w) / static_cast<float>(active_w);
    float sy = static_cast<float>(win_h) / static_cast<float>(active_h);
    float s = std::min(sx, sy);
    if (ui.pixel_perfect) s = std::max(1.0f, std::floor(s));
    dst.w = active_w * s;
    dst.h = active_h * s;
    dst.x = (win_w - dst.w) * 0.5f;
    dst.y = (win_h - dst.h) * 0.5f;
    return dst;
}

static void draw_lightgun_cursor_overlay(AppState &app, const SDL_FRect &dst, int active_w, int active_h)
{
    if (!app.ui.show_lightgun_cursor || !lightgun_active()) return;
    int port = lightgun_port();
    float x = dst.x + (static_cast<float>(input.analog[port][0]) / static_cast<float>(active_w)) * dst.w;
    float y = dst.y + (static_cast<float>(input.analog[port][1]) / static_cast<float>(active_h)) * dst.h;
    SDL_SetRenderDrawColor(app.renderer, 0, 0, 0, 255);
    SDL_RenderLine(app.renderer, x - 9.0f, y - 1.0f, x + 9.0f, y - 1.0f);
    SDL_RenderLine(app.renderer, x - 1.0f, y - 9.0f, x - 1.0f, y + 9.0f);
    SDL_SetRenderDrawColor(app.renderer, 255, 255, 255, 255);
    SDL_RenderLine(app.renderer, x - 8.0f, y, x + 8.0f, y);
    SDL_RenderLine(app.renderer, x, y - 8.0f, x, y + 8.0f);
}

static void render_core(AppState &app)
{
    SDL_SetTextureScaleMode(app.texture, app.ui.linear_filter ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
    SDL_UpdateTexture(app.texture, nullptr, bitmap.data, bitmap.pitch);
    int win_w = 0, win_h = 0;
    SDL_GetWindowSize(app.window, &win_w, &win_h);
    int active_w = bitmap.viewport.w > 0 ? bitmap.viewport.w : 256;
    int active_h = bitmap.viewport.h > 0 ? bitmap.viewport.h : vdp.height;
    SDL_FRect src{static_cast<float>(std::max(0, bitmap.viewport.x)), 0.0f,
                  static_cast<float>(active_w), static_cast<float>(active_h)};
    SDL_FRect dst = compute_dest_rect(app.ui, active_w, active_h, win_w, win_h);
    SDL_SetRenderDrawColor(app.renderer, 0, 0, 0, 255);
    SDL_RenderClear(app.renderer);
    SDL_RenderTexture(app.renderer, app.texture, &src, &dst);
    draw_lightgun_cursor_overlay(app, dst, active_w, active_h);
}

static std::vector<std::filesystem::directory_entry> list_browser(const std::filesystem::path &dir)
{
    std::vector<std::filesystem::directory_entry> entries;
    std::error_code ec;
    for (auto &e : std::filesystem::directory_iterator(dir, ec))
    {
        if (e.is_directory() || e.path().extension() == ".sms" || e.path().extension() == ".gg" ||
            e.path().extension() == ".sg" || e.path().extension() == ".col" || e.path().extension() == ".m5" || e.path().extension() == ".bin")
            entries.push_back(e);
    }
    std::sort(entries.begin(), entries.end(), [](const auto &a, const auto &b) {
        if (a.is_directory() != b.is_directory()) return a.is_directory() > b.is_directory();
        return a.path().filename().string() < b.path().filename().string();
    });
    return entries;
}

static void draw_browser(AppState &app)
{
    ImGui::TextUnformatted("ROM Browser");
    ImGui::SameLine();
    if (ImGui::Button("Up") && app.ui.browser_dir.has_parent_path()) app.ui.browser_dir = app.ui.browser_dir.parent_path();
    ImGui::TextWrapped("%s", app.ui.browser_dir.string().c_str());
    ImGui::BeginChild("rom_browser", ImVec2(0, 210), true);
    auto entries = list_browser(app.ui.browser_dir);
    for (int i = 0; i < static_cast<int>(entries.size()); i++)
    {
        const auto &e = entries[i];
        std::string label = std::string(e.is_directory() ? "[DIR]  " : "[CART] ") + e.path().filename().string();
        if (ImGui::Selectable(label.c_str(), app.ui.browser_index == i, ImGuiSelectableFlags_AllowDoubleClick))
        {
            app.ui.browser_index = i;
            if (ImGui::IsMouseDoubleClicked(0))
            {
                if (e.is_directory()) app.ui.browser_dir = e.path();
                else load_game(app, e.path().string());
            }
        }
    }
    ImGui::EndChild();
}

static void draw_keyboard_controls(AppState &app)
{
    ImGui::TextUnformatted("Keyboard remapping");
    for (int i = 0; i < ACT_COUNT; i++)
    {
        ImGui::PushID(i);
        ImGui::Text("%-20s", g_bindings[i].name);
        ImGui::SameLine(220);
        const char *name = SDL_GetScancodeName(g_bindings[i].scan);
        if (ImGui::Button(app.capture_binding == i ? "press a key..." : (name && name[0] ? name : "Unbound"), ImVec2(160, 0)))
            app.capture_binding = i;
        ImGui::PopID();
    }
    ImGui::Separator();
    ImGui::TextUnformatted("ColecoVision keypad remapping");
    ImGui::TextUnformatted("Each keypad entry has a primary key and an alternate key. Defaults are number-row plus PC numpad.");
    for (int i = 0; i < static_cast<int>(g_coleco_key_bindings.size()); i++)
    {
        ColecoKeyBinding &binding = g_coleco_key_bindings[i];
        ImGui::PushID(2000 + i);
        ImGui::Text("%-12s", binding.name);
        ImGui::SameLine(150);
        const char *primary = SDL_GetScancodeName(binding.scan);
        if (ImGui::Button(app.capture_coleco_key_binding == i ? "press primary..." : (primary && primary[0] ? primary : "Unbound"), ImVec2(150, 0)))
            app.capture_coleco_key_binding = i;
        ImGui::SameLine();
        const char *alt = SDL_GetScancodeName(binding.alt_scan);
        if (ImGui::Button(app.capture_coleco_alt_key_binding == i ? "press alternate..." : (alt && alt[0] ? alt : "Unbound"), ImVec2(150, 0)))
            app.capture_coleco_alt_key_binding = i;
        ImGui::PopID();
    }
    ImGui::TextWrapped("M5 keyboard is direct: PC number keys 1/2 drive the M5 rows used by Pooyan. Text events are also accepted so non-QWERTY layouts can enter logical characters.");
}

static void draw_controller_controls(AppState &app)
{
    ImGui::TextUnformatted("Game controller remapping");
    ImGui::Text("Gamepad: %s", app.gamepad ? "connected" : "not connected");
    for (int i = 0; i < ACT_COUNT; i++)
    {
        ImGui::PushID(1000 + i);
        ImGui::Text("%-20s", g_bindings[i].name);
        ImGui::SameLine(220);
        if (ImGui::Button(app.capture_gamepad_binding == i ? "press a button..." : gamepad_button_name(g_bindings[i].button), ImVec2(160, 0)))
            app.capture_gamepad_binding = i;
        ImGui::PopID();
    }

    ImGui::Separator();
    ImGui::TextUnformatted("ColecoVision keypad");
    ImGui::TextUnformatted("Optional controller mappings for keypad digits, star, and pound.");
    for (int i = 0; i < static_cast<int>(g_coleco_key_bindings.size()); i++)
    {
        ColecoKeyBinding &binding = g_coleco_key_bindings[i];
        ImGui::PushID(3000 + i);
        ImGui::Text("%-12s", binding.name);
        ImGui::SameLine(220);
        if (ImGui::Button(app.capture_coleco_gamepad_binding == i ? "press a button..." : gamepad_button_name(binding.button), ImVec2(160, 0)))
            app.capture_coleco_gamepad_binding = i;
        ImGui::PopID();
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Light Phaser");
    bool force = app.ui.force_lightgun;
    if (ImGui::Checkbox("Force Light Phaser on port 1", &force))
    {
        app.ui.force_lightgun = force;
        if (app.rom_loaded)
            sms.device[0] = (app.ui.force_lightgun || app.ui.db_lightgun) ? DEVICE_LIGHTGUN : DEVICE_PAD2B;
    }
    if (ImGui::Checkbox("Mouse aims Light Phaser", &app.ui.mouse_lightgun)) {}
    bool cursor = app.ui.show_lightgun_cursor;
    if (ImGui::Checkbox("Show on-screen cursor", &cursor))
    {
        app.ui.show_lightgun_cursor = cursor;
        option.lightgun_cursor = cursor ? 1 : 0;
    }
    ImGui::Text("Detected: %s  X:%d Y:%d Trigger:%s", lightgun_active() ? "yes" : "no",
                input.analog[lightgun_port()][0], input.analog[lightgun_port()][1],
                (input.pad[lightgun_port()] & INPUT_BUTTON1) ? "down" : "up");
}

static void draw_states(AppState &app)
{
    ImGui::TextUnformatted("Save states");
    ImGui::Text("Directory: %s", app.state_dir.string().c_str());
    if (ImGui::Button("Slot -")) app.save_slot = std::max(0, app.save_slot - 1);
    ImGui::SameLine();
    ImGui::Text("Slot %d", app.save_slot);
    ImGui::SameLine();
    if (ImGui::Button("Slot +")) app.save_slot = std::min(9, app.save_slot + 1);

    std::filesystem::path slot_path = state_path_for_slot(app.rom_path, app.state_dir, app.save_slot);
    load_state_thumbnail(app, slot_path);
    if (app.state_thumb_texture)
    {
        ImGui::Image((ImTextureID)app.state_thumb_texture, ImVec2((float)app.state_thumb_w, (float)app.state_thumb_h));
        ImGui::Text("%s", slot_path.filename().string().c_str());
    }
    else
    {
        ImGui::TextUnformatted("No thumbnail for this slot yet.");
    }

    if (ImGui::Button("Save state")) { save_slot(app, app.save_slot); invalidate_state_thumbnail(app); }
    ImGui::SameLine();
    if (ImGui::Button("Load state")) load_slot(app, app.save_slot);
    ImGui::SameLine();
    if (ImGui::Button("Load autosave")) load_state_file(app, autosave_path(app));

    ImGui::Separator();
    ImGui::Checkbox("Auto-save", &app.autosave_enabled);
    ImGui::SameLine();
    ImGui::Text("every %d frames", app.autosave_interval_frames);
    if (ImGui::Button("Autosave -")) app.autosave_interval_frames = std::max(60, app.autosave_interval_frames - 60);
    ImGui::SameLine();
    if (ImGui::Button("Autosave +")) app.autosave_interval_frames = std::min(3600, app.autosave_interval_frames + 60);

    ImGui::Checkbox("Rewind", &app.rewind_enabled);
    ImGui::SameLine();
    ImGui::Text("snapshots: %d / %d", static_cast<int>(app.rewind_snapshots.size()), static_cast<int>(app.rewind_max_snapshots));
    if (ImGui::Button("Hold to rewind", ImVec2(150, 0))) {}
    if (ImGui::IsItemActive()) app.rewind_ui_hold_next = true;
    ImGui::SameLine();
    ImGui::Text("%s", rewind_active(app) ? "rewinding" : "ready");
    ImGui::Text("Capture every %d emulated frames; playback step every %d display frames.",
                app.rewind_interval_frames, app.rewind_step_display_frames);
    ImGui::TextWrapped("Hotkeys: F5 saves, F8 loads, hold F6 or the controller Rewind binding to scrub backward continuously. Release to resume from the selected point. State files are PNG images with an embedded compressed save-state chunk, so the thumbnail is visible outside the emulator too.");
}

static void draw_virtual_keyboard(AppState &app)
{
    if (!app.ui.show_keyboard) return;
    ImGui::SetNextWindowBgAlpha(0.88f);
    ImGui::Begin("Sord M5 Virtual Keyboard", &app.ui.show_keyboard, ImGuiWindowFlags_AlwaysAutoResize);
    const int cols = 8;
    for (int i = 0; i < static_cast<int>(std::size(kM5Keys)); i++)
    {
        ImGui::PushID(i);
        bool selected = (i == app.vk_index);
        if (selected) ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.0f);
        if (ImGui::Button(kM5Keys[i].label, ImVec2(58, 30)))
        {
            set_m5_key(kM5Keys[i].row, kM5Keys[i].mask, true);
        }
        if (ImGui::IsItemDeactivated()) set_m5_key(kM5Keys[i].row, kM5Keys[i].mask, false);
        if (selected) ImGui::PopStyleVar();
        if ((i % cols) != cols - 1) ImGui::SameLine();
        ImGui::PopID();
    }
    ImGui::TextUnformatted("Gamepad: D-pad moves, South presses, East closes.");
    ImGui::End();
}

static void draw_menu(AppState &app)
{
    if (!app.ui.show_menu) return;
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(520, 620), ImGuiCond_FirstUseEver);
    ImGui::Begin("SMS Plus GX SDL3", &app.ui.show_menu);
    if (ImGui::BeginTabBar("tabs"))
    {
        if (ImGui::BeginTabItem("Games")) { draw_browser(app); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("States")) { draw_states(app); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Video"))
        {
            ImGui::Checkbox("Keep aspect ratio", &app.ui.keep_aspect);
            ImGui::Checkbox("Stretch", &app.ui.stretch);
            ImGui::Checkbox("Pixel perfect", &app.ui.pixel_perfect);
            ImGui::Checkbox("Linear filtering", &app.ui.linear_filter);
            bool lcd = option.lcd_persistence != 0;
            if (ImGui::Checkbox("Game Gear LCD persistence", &lcd)) option.lcd_persistence = lcd ? 1 : 0;
            if (ImGui::Checkbox("Fullscreen", &app.ui.fullscreen))
                SDL_SetWindowFullscreen(app.window, app.ui.fullscreen);
            ImGui::Text("Active viewport: %d x %d", bitmap.viewport.w, bitmap.viewport.h);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Keyboard")) { draw_keyboard_controls(app); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Controllers")) { draw_controller_controls(app); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Machine"))
        {
            ImGui::Text("ROM: %s", app.rom_path.empty() ? "<none>" : app.rom_path.c_str());
            const char *bios_text = !app.resolved_bios_path.empty() ? app.resolved_bios_path.c_str() :
                                    (!app.coleco_bios_path.empty() ? app.coleco_bios_path.c_str() :
                                     (!app.bios_path.empty() ? app.bios_path.c_str() : "<auto/default>"));
            ImGui::Text("BIOS: %s", bios_text);
            ImGui::Text("Console: %u", sms.console);
            if (ImGui::Button(app.paused ? "Resume" : "Pause")) app.paused = !app.paused;
            ImGui::SameLine();
            if (ImGui::Button("Reset") && app.rom_loaded) system_reset();
            ImGui::Checkbox("Sord M5 virtual keyboard", &app.ui.show_keyboard);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::Separator();
    ImGui::TextWrapped("%s", app.status.c_str());
    ImGui::End();
}

static void handle_event(AppState &app, const SDL_Event &e)
{
    ImGui_ImplSDL3_ProcessEvent(&e);
    switch (e.type)
    {
        case SDL_EVENT_QUIT:
            app.running = false;
            break;
        case SDL_EVENT_DROP_FILE:
        {
            /*
             * SDL3 exposes drop.data as const in current headers, but the
             * allocation is still owned by the caller and must be released
             * with SDL_free().  Copy the path into std::string before freeing
             * it so load_game() never observes a dangling pointer.
             */
            if (e.drop.data)
            {
                const char *dropped_path = static_cast<const char *>(e.drop.data);
                std::string dropped(dropped_path);
                SDL_free(const_cast<char *>(dropped_path));
                load_game(app, dropped);
            }
            break;
        }
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
        {
            bool down = e.type == SDL_EVENT_KEY_DOWN;
            if (app.capture_binding >= 0 && down)
            {
                g_bindings[app.capture_binding].scan = e.key.scancode;
                app.capture_binding = -1;
                break;
            }
            if (app.capture_coleco_key_binding >= 0 && down)
            {
                g_coleco_key_bindings[app.capture_coleco_key_binding].scan = e.key.scancode;
                app.capture_coleco_key_binding = -1;
                break;
            }
            if (app.capture_coleco_alt_key_binding >= 0 && down)
            {
                g_coleco_key_bindings[app.capture_coleco_alt_key_binding].alt_scan = e.key.scancode;
                app.capture_coleco_alt_key_binding = -1;
                break;
            }
            for (int i = 0; i < ACT_COUNT; i++)
            {
                if (g_bindings[i].scan == e.key.scancode)
                    handle_action_command(app, static_cast<Action>(i), down);
            }
            if (e.key.key == SDLK_ESCAPE && down) app.ui.show_menu = !app.ui.show_menu;
            if (e.key.key == SDLK_TAB && down) app.ui.show_keyboard = !app.ui.show_keyboard;
            m5_key_from_sdl(e.key.key, e.key.scancode, down);
            if (e.key.key == SDLK_ESCAPE) input.m5_reset = down ? SORDM5_KEY_RESET : 0;
            break;
        }
        case SDL_EVENT_TEXT_INPUT:
            m5_key_from_text(e.text.text);
            break;
#if defined(SDL_EVENT_FINGER_DOWN) && defined(SDL_EVENT_FINGER_MOTION) && defined(SDL_EVENT_FINGER_UP)
        case SDL_EVENT_FINGER_DOWN:
        case SDL_EVENT_FINGER_MOTION:
        case SDL_EVENT_FINGER_UP:
        {
            int win_w = 0, win_h = 0;
            SDL_GetWindowSize(app.window, &win_w, &win_h);
            bool down = e.type != SDL_EVENT_FINGER_UP;
            window_point_to_lightgun(app, e.tfinger.x * static_cast<float>(win_w), e.tfinger.y * static_cast<float>(win_h), down);
            break;
        }
#endif
        case SDL_EVENT_GAMEPAD_ADDED:
            if (!app.gamepad) app.gamepad = SDL_OpenGamepad(e.gdevice.which);
            break;
        case SDL_EVENT_GAMEPAD_REMOVED:
            if (app.gamepad && SDL_GetGamepadID(app.gamepad) == e.gdevice.which)
            {
                SDL_CloseGamepad(app.gamepad);
                app.gamepad = nullptr;
            }
            break;
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
            handle_gamepad_button(app, static_cast<SDL_GamepadButton>(e.gbutton.button), e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN);
            break;
    }
}

static void update_virtual_keyboard_gamepad(AppState &app)
{
    if (!app.gamepad || !app.ui.show_keyboard) return;
    if (SDL_GetGamepadButton(app.gamepad, SDL_GAMEPAD_BUTTON_EAST)) app.ui.show_keyboard = false;
    int cols = 8;
    int count = static_cast<int>(std::size(kM5Keys));
    // Edge detection is deliberately omitted here; the ImGui window remains usable with mouse/keyboard,
    // while gamepad navigation is basic and predictable.
    if (SDL_GetGamepadButton(app.gamepad, SDL_GAMEPAD_BUTTON_DPAD_LEFT))  app.vk_index = std::max(0, app.vk_index - 1);
    if (SDL_GetGamepadButton(app.gamepad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) app.vk_index = std::min(count - 1, app.vk_index + 1);
    if (SDL_GetGamepadButton(app.gamepad, SDL_GAMEPAD_BUTTON_DPAD_UP))    app.vk_index = std::max(0, app.vk_index - cols);
    if (SDL_GetGamepadButton(app.gamepad, SDL_GAMEPAD_BUTTON_DPAD_DOWN))  app.vk_index = std::min(count - 1, app.vk_index + cols);
    bool press = SDL_GetGamepadButton(app.gamepad, SDL_GAMEPAD_BUTTON_SOUTH);
    const M5Key &mk = kM5Keys[app.vk_index];
    set_m5_key(mk.row, mk.mask, press);
}

static bool init_sdl(AppState &app)
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) return false;
    SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");
    app.window = SDL_CreateWindow("SMS Plus GX SDL3", HOST_WIDTH_RESOLUTION, HOST_HEIGHT_RESOLUTION, SDL_WINDOW_RESIZABLE);
    if (!app.window) return false;
    app.renderer = SDL_CreateRenderer(app.window, nullptr);
    if (!app.renderer) return false;
    #ifdef SMSPLUS_RENDER_32BPP
    /*
     * Do not hide this behind #if defined(SDL_PIXELFORMAT_XRGB8888).  In SDL3
     * some pixel formats may be enum constants rather than preprocessor macros,
     * and the old conditional could silently fall back to an RGB565 texture while
     * the core was uploading 32 bpp XRGB8888 pixels.  That produces the yellow
     * vertical-stripe screen reported on real SDL3 builds.
     */
    app.texture = SDL_CreateTexture(app.renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, BITMAP_W, BITMAP_H);
#else
    app.texture = SDL_CreateTexture(app.renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, BITMAP_W, BITMAP_H);
#endif
    if (!app.texture) return false;
    SDL_SetTextureScaleMode(app.texture, SDL_SCALEMODE_NEAREST);
    if (app.ui.fullscreen) SDL_SetWindowFullscreen(app.window, true);
    SDL_StartTextInput(app.window);

    if (app.ui.audio)
    {
        SDL_AudioSpec spec{};
        spec.format = SDL_AUDIO_S16LE;
        spec.channels = 2;
        spec.freq = SOUND_FREQUENCY;
        app.audio_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
        if (app.audio_stream) SDL_ResumeAudioStreamDevice(app.audio_stream);
    }
    open_first_gamepad(app);
    return true;
}

static void init_imgui(AppState &app)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    ImGui::StyleColorsDark();
    ImGui_ImplSDL3_InitForSDLRenderer(app.window, app.renderer);
    ImGui_ImplSDLRenderer3_Init(app.renderer);
}

static void shutdown_app(AppState &app)
{
    if (app.rom_loaded) system_poweroff();
    system_shutdown();
    if (bios.rom) { std::free(bios.rom); bios.rom = nullptr; }
    if (app.gamepad) SDL_CloseGamepad(app.gamepad);
    if (app.audio_stream) SDL_DestroyAudioStream(app.audio_stream);
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    if (app.state_thumb_texture) SDL_DestroyTexture(app.state_thumb_texture);
    if (app.texture) SDL_DestroyTexture(app.texture);
    if (app.renderer) SDL_DestroyRenderer(app.renderer);
    if (app.window) SDL_DestroyWindow(app.window);
    SDL_Quit();
    std::free(g_pixels);
    g_pixels = nullptr;
}

int main(int argc, char **argv)
{
    AppState app;
    set_defaults();
    init_user_paths(app);
    parse_args(app, argc, argv);
    ensure_state_dir(app.state_dir);
    ensure_state_dir(app.save_dir);
    if (!init_bitmap()) return 1;
    if (!init_sdl(app))
    {
        std::fprintf(stderr, "SDL3 initialization failed: %s\n", SDL_GetError());
        return 1;
    }
    system_init();
    init_imgui(app);

    if (!app.rom_path.empty()) load_game(app, app.rom_path);
    else app.status = "Drop a ROM, pass one on the command line, or choose it from the browser.";

    while (app.running)
    {
        app.rewind_ui_hold = app.rewind_ui_hold_next;
        app.rewind_ui_hold_next = false;

        SDL_Event e;
        while (SDL_PollEvent(&e)) handle_event(app, e);
        update_keyboard_state_from_bindings();
        update_coleco_keypad_from_inputs(app);
        update_virtual_keyboard_gamepad(app);
        update_lightgun_mouse(app);

        if (app.rom_loaded && !app.paused)
        {
            if (rewind_active(app))
            {
                run_rewind(app);
            }
            else
            {
                run_rewind(app); /* clears one-shot rewind state after release */
                system_frame(0);
                app.frame_counter++;
                for (size_t i = 0; i < SORDM5_KEY_ROWS; i++)
                {
                    input.m5_key[i] &= static_cast<uint8_t>(~g_m5_text_pulse[i]);
                    g_m5_text_pulse[i] = 0;
                }
                maybe_capture_rewind(app);
                maybe_autosave(app);
                if (app.audio_stream && snd.output && snd.sample_count > 0)
                    SDL_PutAudioStreamData(app.audio_stream, snd.output, snd.sample_count * 2 * sizeof(int16_t));
            }
        }

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        if (app.rom_loaded) render_core(app);
        else { SDL_SetRenderDrawColor(app.renderer, 0, 0, 0, 255); SDL_RenderClear(app.renderer); }
        draw_menu(app);
        draw_virtual_keyboard(app);
        ImGui::Render();
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), app.renderer);
        SDL_RenderPresent(app.renderer);
    }

    shutdown_app(app);
    return 0;
}
