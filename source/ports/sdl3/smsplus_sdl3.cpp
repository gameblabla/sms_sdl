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
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

extern "C" { t_config option; }

static uint16_t *g_pixels = nullptr;
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
    {"Space", 0, 0x40, SDLK_SPACE, SDL_SCANCODE_SPACE},
    {"Enter", 0, 0x80, SDLK_RETURN, SDL_SCANCODE_RETURN},
    {"1", 1, 0x01, SDLK_1, SDL_SCANCODE_1}, {"2", 1, 0x02, SDLK_2, SDL_SCANCODE_2},
    {"3", 1, 0x04, SDLK_3, SDL_SCANCODE_3}, {"4", 1, 0x08, SDLK_4, SDL_SCANCODE_4},
    {"5", 1, 0x10, SDLK_5, SDL_SCANCODE_5}, {"6", 1, 0x20, SDLK_6, SDL_SCANCODE_6},
    {"7", 1, 0x40, SDLK_7, SDL_SCANCODE_7}, {"8", 1, 0x80, SDLK_8, SDL_SCANCODE_8},
    {"9", 2, 0x01, SDLK_9, SDL_SCANCODE_9}, {"0", 2, 0x02, SDLK_0, SDL_SCANCODE_0},
    {"-", 2, 0x04, SDLK_MINUS, SDL_SCANCODE_MINUS}, {"^", 2, 0x08, SDLK_EQUALS, SDL_SCANCODE_EQUALS},
    {"Q", 2, 0x10, SDLK_Q, SDL_SCANCODE_Q}, {"W", 2, 0x20, SDLK_W, SDL_SCANCODE_W},
    {"E", 2, 0x40, SDLK_E, SDL_SCANCODE_E}, {"R", 2, 0x80, SDLK_R, SDL_SCANCODE_R},
    {"T", 3, 0x01, SDLK_T, SDL_SCANCODE_T}, {"Y", 3, 0x02, SDLK_Y, SDL_SCANCODE_Y},
    {"U", 3, 0x04, SDLK_U, SDL_SCANCODE_U}, {"I", 3, 0x08, SDLK_I, SDL_SCANCODE_I},
    {"A", 3, 0x10, SDLK_A, SDL_SCANCODE_A}, {"S", 3, 0x20, SDLK_S, SDL_SCANCODE_S},
    {"D", 3, 0x40, SDLK_D, SDL_SCANCODE_D}, {"F", 3, 0x80, SDLK_F, SDL_SCANCODE_F},
    {"G", 4, 0x01, SDLK_G, SDL_SCANCODE_G}, {"H", 4, 0x02, SDLK_H, SDL_SCANCODE_H},
    {"J", 4, 0x04, SDLK_J, SDL_SCANCODE_J}, {"K", 4, 0x08, SDLK_K, SDL_SCANCODE_K},
    {"Z", 4, 0x10, SDLK_Z, SDL_SCANCODE_Z}, {"X", 4, 0x20, SDLK_X, SDL_SCANCODE_X},
    {"C", 4, 0x40, SDLK_C, SDL_SCANCODE_C}, {"V", 4, 0x80, SDLK_V, SDL_SCANCODE_V},
    {"B", 5, 0x01, SDLK_B, SDL_SCANCODE_B}, {"N", 5, 0x02, SDLK_N, SDL_SCANCODE_N},
    {"M", 5, 0x04, SDLK_M, SDL_SCANCODE_M}, {",", 5, 0x08, SDLK_COMMA, SDL_SCANCODE_COMMA},
    {".", 5, 0x10, SDLK_PERIOD, SDL_SCANCODE_PERIOD}, {"Down", 5, 0x20, SDLK_DOWN, SDL_SCANCODE_DOWN},
    {"O", 6, 0x01, SDLK_O, SDL_SCANCODE_O}, {"P", 6, 0x02, SDLK_P, SDL_SCANCODE_P},
    {"Up", 6, 0x04, SDLK_UP, SDL_SCANCODE_UP}, {"[", 6, 0x08, SDLK_LEFTBRACKET, SDL_SCANCODE_LEFTBRACKET},
    {"L", 6, 0x10, SDLK_L, SDL_SCANCODE_L}, {"Left", 6, 0x20, SDLK_LEFT, SDL_SCANCODE_LEFT},
    {"Right", 6, 0x40, SDLK_RIGHT, SDL_SCANCODE_RIGHT}, {"]", 6, 0x80, SDLK_RIGHTBRACKET, SDL_SCANCODE_RIGHTBRACKET},
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
    std::string bios_path;
    std::string status;
    bool running = true;
    bool rom_loaded = false;
    bool paused = false;
    int capture_binding = -1;
    int vk_index = 0;
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
    option.soundlevel = 2;
    option.use_bios = 1;
}

static const char *ext_of(const std::string &path)
{
    const char *dot = std::strrchr(path.c_str(), '.');
    return dot ? dot : "";
}

static void set_console_from_path(const std::string &path)
{
    const char *ext = ext_of(path);
    if (!SDL_strcasecmp(ext, ".m5")) option.console = 7;
    else if (!SDL_strcasecmp(ext, ".col")) option.console = 6;
    else if (!SDL_strcasecmp(ext, ".gg")) option.console = 3;
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

static bool init_bios(const AppState &app)
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

    if (sms.console == CONSOLE_COLECO || sms.console == CONSOLE_SORDM5)
    {
        std::string path = app.bios_path;
        if (path.empty() && sms.console == CONSOLE_SORDM5) path = "sordm5bios.bin";
        if (!path.empty() && !load_exact(path, coleco.rom, sizeof(coleco.rom), 0x2000)) return false;
    }
    return true;
}

static bool init_bitmap()
{
    if (!g_pixels)
        g_pixels = static_cast<uint16_t *>(std::calloc(BITMAP_W * BITMAP_H, sizeof(uint16_t)));
    if (!g_pixels) return false;
    bitmap.width = BITMAP_W;
    bitmap.height = BITMAP_H;
    bitmap.depth = 16;
    bitmap.data = reinterpret_cast<uint8_t *>(g_pixels);
    bitmap.pitch = BITMAP_W * sizeof(uint16_t);
    bitmap.viewport.w = VIDEO_WIDTH_SMS;
    bitmap.viewport.h = VIDEO_HEIGHT_SMS;
    bitmap.viewport.x = 0;
    bitmap.viewport.y = 0;
    return true;
}

extern "C" void smsp_state(uint8_t slot_number, uint8_t mode)
{
    (void)slot_number;
    (void)mode;
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
    set_console_from_path(path);
    std::snprintf(option.game_name, sizeof(option.game_name), "%s", path.c_str());
    if (!load_rom(const_cast<char *>(path.c_str())))
    {
        app.status = "Failed to load ROM: " + path;
        return false;
    }
    if (!init_bitmap() || !init_bios(app))
    {
        app.status = "Failed to initialize bitmap or BIOS";
        return false;
    }
    system_poweron();
    app.rom_path = path;
    app.rom_loaded = true;
    app.paused = false;
    app.status = "Loaded " + path;
    return true;
}

static void clear_m5_keyboard()
{
    for (uint8_t &r : input.m5_key) r = 0;
    input.m5_reset = 0;
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

static void m5_key_from_text(const char *txt)
{
    if (!txt || !txt[0]) return;
    unsigned char c = static_cast<unsigned char>(txt[0]);
    if (c >= 'a' && c <= 'z') c = static_cast<unsigned char>(std::toupper(c));
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

static void open_first_gamepad(AppState &app)
{
    int count = 0;
    SDL_JoystickID *ids = SDL_GetGamepads(&count);
    if (ids && count > 0)
        app.gamepad = SDL_OpenGamepad(ids[0]);
    SDL_free(ids);
}

static void handle_gamepad_button(AppState &app, SDL_GamepadButton button, bool down)
{
    for (int i = 0; i < ACT_COUNT; i++)
    {
        if (g_bindings[i].button == button)
        {
            if (i == ACT_MENU && down) app.ui.show_menu = !app.ui.show_menu;
            else if (i == ACT_VKBD && down) app.ui.show_keyboard = !app.ui.show_keyboard;
            else apply_action(static_cast<Action>(i), down);
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
        else if (!std::strcmp(a, "--sram")) { if (const char *v = need(a)) g_sram_path = v; }
        else if (!std::strcmp(a, "--console"))
        {
            if (const char *v = need(a))
            {
                if (!SDL_strcasecmp(v, "sordm5") || !SDL_strcasecmp(v, "m5")) option.console = 7;
                else if (!SDL_strcasecmp(v, "coleco")) option.console = 6;
                else if (!SDL_strcasecmp(v, "gg")) option.console = 3;
                else if (!SDL_strcasecmp(v, "sms2")) option.console = 2;
                else if (!SDL_strcasecmp(v, "sms")) option.console = 1;
            }
        }
        else if (!std::strcmp(a, "--fullscreen")) app.ui.fullscreen = true;
        else if (!std::strcmp(a, "--stretch")) { app.ui.stretch = true; app.ui.keep_aspect = false; app.ui.pixel_perfect = false; }
        else if (!std::strcmp(a, "--linear")) app.ui.linear_filter = true;
        else if (!std::strcmp(a, "--nearest") || !std::strcmp(a, "--pixel-perfect")) { app.ui.linear_filter = false; app.ui.pixel_perfect = true; }
        else if (!std::strcmp(a, "--no-audio")) app.ui.audio = false;
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
}

static std::vector<std::filesystem::directory_entry> list_browser(const std::filesystem::path &dir)
{
    std::vector<std::filesystem::directory_entry> entries;
    std::error_code ec;
    for (auto &e : std::filesystem::directory_iterator(dir, ec))
    {
        if (e.is_directory() || e.path().extension() == ".sms" || e.path().extension() == ".gg" ||
            e.path().extension() == ".sg" || e.path().extension() == ".col" || e.path().extension() == ".m5")
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

static void draw_controls(AppState &app)
{
    ImGui::TextUnformatted("Controls");
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
    ImGui::TextWrapped("M5 keyboard is direct: PC number keys 1/2 drive the M5 rows used by Pooyan. Text events are also accepted so non-QWERTY layouts can enter logical characters.");
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
        if (ImGui::BeginTabItem("Video"))
        {
            ImGui::Checkbox("Keep aspect ratio", &app.ui.keep_aspect);
            ImGui::Checkbox("Stretch", &app.ui.stretch);
            ImGui::Checkbox("Pixel perfect", &app.ui.pixel_perfect);
            ImGui::Checkbox("Linear filtering", &app.ui.linear_filter);
            if (ImGui::Checkbox("Fullscreen", &app.ui.fullscreen))
                SDL_SetWindowFullscreen(app.window, app.ui.fullscreen);
            ImGui::Text("Active viewport: %d x %d", bitmap.viewport.w, bitmap.viewport.h);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Input")) { draw_controls(app); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Machine"))
        {
            ImGui::Text("ROM: %s", app.rom_path.empty() ? "<none>" : app.rom_path.c_str());
            ImGui::Text("BIOS: %s", app.bios_path.empty() ? "<default>" : app.bios_path.c_str());
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
            if (e.drop.data) { load_game(app, e.drop.data); SDL_free(e.drop.data); }
            break;
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
            if (e.key.key == SDLK_ESCAPE && down) app.ui.show_menu = !app.ui.show_menu;
            if (e.key.key == SDLK_TAB && down) app.ui.show_keyboard = !app.ui.show_keyboard;
            m5_key_from_sdl(e.key.key, e.key.scancode, down);
            if (e.key.key == SDLK_ESCAPE) input.m5_reset = down ? SORDM5_KEY_RESET : 0;
            break;
        }
        case SDL_EVENT_TEXT_INPUT:
            m5_key_from_text(e.text.text);
            break;
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
    app.texture = SDL_CreateTexture(app.renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, BITMAP_W, BITMAP_H);
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
    parse_args(app, argc, argv);
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
        SDL_Event e;
        while (SDL_PollEvent(&e)) handle_event(app, e);
        update_keyboard_state_from_bindings();
        update_virtual_keyboard_gamepad(app);

        if (app.rom_loaded && !app.paused)
        {
            system_frame(0);
            for (size_t i = 0; i < SORDM5_KEY_ROWS; i++)
            {
                input.m5_key[i] &= static_cast<uint8_t>(~g_m5_text_pulse[i]);
                g_m5_text_pulse[i] = 0;
            }
            if (app.audio_stream && snd.output && snd.sample_count > 0)
                SDL_PutAudioStreamData(app.audio_stream, snd.output, snd.sample_count * 2 * sizeof(int16_t));
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
