// test_ui_preview.cpp
// Standalone test utility to preview the graphical menu UI
// Build: g++ -o test_ui_preview test_ui_preview.cpp gfx_menu.cpp theme.cpp -lImlib2 -I. -I./lib
// Run: ./test_ui_preview [output.png]

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "gfx_menu.h"
#include "theme.h"
#include "animator.h"
#include "search.h"
#include "core_settings.h"
#include "gamedb.h"
#include "playtime.h"
#include "zaparoo.h"
#include "scraper.h"

// Framebuffer stubs - used by gfx_menu.cpp
static uint32_t fake_fb[1920 * 1080];
volatile uint32_t *fb_base = fake_fb;
int fb_width = 1920;
int fb_height = 1080;

// Character font - provided by charrom.cpp (link with charrom.o)

// Config stub - must match cfg_t structure from cfg.h
#include "cfg.h"
cfg_t cfg = {0};

// Stub for FileLoad (used by charrom.cpp's LoadFont)
int FileLoad(const char* name, void* data, int size) { (void)name; (void)data; (void)size; return 0; }

// C++ stubs for hardware functions (these have C++ linkage in headers)
void video_fb_enable(int enable, int buffer) { (void)enable; (void)buffer; }
int video_fb_state(void) { return 0; }
int video_chvt(int num) { (void)num; return 0; }
void OsdDisable(void) {}
void user_io_osd_key_enable(char enable) { (void)enable; }

// Stubs for network and controller display (new UI features)
static char fake_eth_ip[] = "";  // No ethernet in test
static char fake_wifi_ip[] = "192.168.1.100";  // WiFi connected
char* getNet(int spec) {
    // spec 1 = ethernet, spec 2 = wifi
    if (spec == 2) return fake_wifi_ip;  // Simulate WiFi connected
    return NULL;
}

const char* get_player_controller_name(int player) {
    // Simulate controllers connected for players 1 and 2
    static const char* names[] = { "8BitDo Pro 2", "Xbox Controller", NULL, NULL };
    if (player >= 1 && player <= 4) return names[player - 1];
    return NULL;
}
unsigned long GetTimer(unsigned long offset) { return 1000 + offset; }
int menu_use_graphical(void) { return 1; }

// Create a placeholder boxart image for preview
static Imlib_Image placeholder_boxart = NULL;
Imlib_Image boxart_get_preview_image(void) {
    if (!placeholder_boxart) {
        // Create a 300x400 placeholder image (typical boxart aspect ratio)
        placeholder_boxart = imlib_create_image(300, 400);
        if (placeholder_boxart) {
            imlib_context_set_image(placeholder_boxart);

            // Fill with a gradient-like game cover placeholder
            imlib_context_set_color(40, 60, 100, 255);  // Dark blue background
            imlib_image_fill_rectangle(0, 0, 300, 400);

            // Add a border
            imlib_context_set_color(80, 120, 180, 255);  // Lighter blue border
            imlib_image_fill_rectangle(0, 0, 300, 8);    // Top
            imlib_image_fill_rectangle(0, 392, 300, 8);  // Bottom
            imlib_image_fill_rectangle(0, 0, 8, 400);    // Left
            imlib_image_fill_rectangle(292, 0, 8, 400);  // Right

            // Add center decoration (game icon placeholder)
            imlib_context_set_color(60, 90, 140, 255);
            imlib_image_fill_rectangle(100, 150, 100, 100);

            // Inner highlight
            imlib_context_set_color(100, 140, 200, 255);
            imlib_image_fill_rectangle(120, 170, 60, 60);
        }
    }
    return placeholder_boxart;
}

// C++ stubs for search
int search_is_active(void) { return 0; }
const char* search_get_query(void) { return ""; }
search_result_t* search_get_results(int *count) { if (count) *count = 0; return NULL; }
search_result_t* search_get_selected(void) { return NULL; }
int search_keyboard_visible(void) { return 0; }
search_keyboard_t* search_get_keyboard(void) { return NULL; }

// C++ stubs for playtime
playtime_entry_t* playtime_get_entry(const char *path) { (void)path; return NULL; }
void playtime_format_duration(uint32_t seconds, char *buf, int len) {
    (void)seconds;
    snprintf(buf, len, "0:00");
}
void playtime_format_relative_time(time_t timestamp, char *buf, int len) {
    (void)timestamp;
    snprintf(buf, len, "Never");
}

// C++ stubs for gamedb
gamedb_entry_t* gamedb_lookup_filename(const char *filename) { (void)filename; return NULL; }
const char* gamedb_genre_name(gamedb_genre_t genre) {
    switch (genre) {
        case GENRE_ACTION: return "Action";
        case GENRE_ADVENTURE: return "Adventure";
        case GENRE_ARCADE: return "Arcade";
        case GENRE_BOARD: return "Board";
        case GENRE_EDUCATIONAL: return "Educational";
        case GENRE_RPG: return "Role-Playing";
        case GENRE_PLATFORMER: return "Platformer";
        case GENRE_PUZZLE: return "Puzzle";
        case GENRE_SHOOTER: return "Shooter";
        case GENRE_SPORTS: return "Sports";
        case GENRE_RACING: return "Racing";
        case GENRE_FIGHTING: return "Fighting";
        case GENRE_SIMULATION: return "Simulation";
        case GENRE_STRATEGY: return "Strategy";
        case GENRE_OTHER: return "Other";
        default: return "Unknown";
    }
}
const char* gamedb_region_name(gamedb_region_t region) {
    switch (region) {
        case REGION_USA: return "USA";
        case REGION_EUROPE: return "Europe";
        case REGION_JAPAN: return "Japan";
        case REGION_WORLD: return "World";
        case REGION_OTHER: return "Other";
        default: return "Unknown";
    }
}

// C++ stubs for animation
void anim_init(void) {}
void anim_shutdown(void) {}
void anim_update(float delta_time) { (void)delta_time; }
int anim_is_running(uint32_t id) { (void)id; return 0; }
void anim_cancel(uint32_t id) { (void)id; }
void anim_start(uint32_t id) { (void)id; }
uint32_t anim_create_to(float *target, float to, float duration, anim_easing_t easing) {
    (void)duration; (void)easing;
    if (target) *target = to;
    return 1;
}
uint32_t anim_fade_in(float *opacity, float duration) {
    (void)duration;
    if (opacity) *opacity = 1.0f;
    return 1;
}
uint32_t anim_fade_out(float *opacity, float duration) {
    (void)duration;
    if (opacity) *opacity = 0.0f;
    return 1;
}

// C++ stubs for core settings
int core_settings_is_loaded(void) { return 0; }
settings_menu_state_t* core_settings_get_menu_state(void) { return NULL; }
const char* core_settings_category_name(setting_category_t category) {
    (void)category;
    return "Unknown";
}
int core_settings_get_by_category(setting_category_t category, core_setting_t **out, int max) {
    (void)category; (void)out; (void)max;
    return 0;
}
core_setting_t* core_settings_get_by_index(int index) { (void)index; return NULL; }
void core_settings_format_value(core_setting_t *setting, char *buf, int len) {
    (void)setting;
    if (len > 0) buf[0] = '\0';
}
const char* core_settings_get_option_name(core_setting_t *setting) {
    (void)setting;
    return "";
}

// Zaparoo state for testing
static zaparoo_status_t test_zaparoo_status = ZAPAROO_IDLE;
static zaparoo_overlay_t test_zaparoo_overlay = {0};

// C++ stubs for zaparoo
void zaparoo_init(void) {
    test_zaparoo_status = ZAPAROO_IDLE;
    memset(&test_zaparoo_overlay, 0, sizeof(test_zaparoo_overlay));
}
void zaparoo_shutdown(void) {}
void zaparoo_poll(void) {}
zaparoo_status_t zaparoo_get_status(void) { return test_zaparoo_status; }
int zaparoo_is_reader_connected(void) { return test_zaparoo_status != ZAPAROO_DISCONNECTED; }
const char* zaparoo_get_reader_name(void) { return "PN532 (Test)"; }
const zaparoo_card_t* zaparoo_get_last_card(void) { return NULL; }
void zaparoo_clear_card(void) {}
void zaparoo_simulate_scan(const char *game_name, const char *game_path, const char *core_name) {
    (void)core_name;
    strncpy(test_zaparoo_overlay.card.game_name, game_name, sizeof(test_zaparoo_overlay.card.game_name) - 1);
    strncpy(test_zaparoo_overlay.card.game_path, game_path, sizeof(test_zaparoo_overlay.card.game_path) - 1);
    test_zaparoo_overlay.active = 1;
    test_zaparoo_overlay.opacity = 1.0f;
    test_zaparoo_status = ZAPAROO_CARD_DETECTED;
}
zaparoo_overlay_t* zaparoo_get_overlay(void) { return &test_zaparoo_overlay; }
void zaparoo_show_overlay(const char *game_name, const char *game_path) {
    zaparoo_simulate_scan(game_name, game_path, "SNES");
}
void zaparoo_hide_overlay(void) {
    test_zaparoo_overlay.active = 0;
    // Don't reset status - let test control it
}
int zaparoo_overlay_active(void) { return test_zaparoo_overlay.active; }
void zaparoo_overlay_update(void) {}
void zaparoo_set_card_callback(zaparoo_card_callback_t callback) { (void)callback; }

// Helper to set zaparoo status for testing
void test_set_zaparoo_status(zaparoo_status_t status) {
    test_zaparoo_status = status;
}

// C++ stubs for scraper (simplified - libretro only)
static scraper_progress_t test_scraper_progress = {0};

void scraper_init(void) {}
void scraper_shutdown(void) {}
int scraper_scrape_game(const char *game_name, const char *game_path,
                        const char *core_name, scraper_result_t *result) {
    (void)game_name; (void)game_path; (void)core_name; (void)result;
    return 0;
}
int scraper_scrape_system(const char *system_name) { (void)system_name; return 0; }
void scraper_stop(void) {}
scraper_status_t scraper_get_status(void) { return SCRAPER_IDLE; }
scraper_progress_t* scraper_get_progress(void) { return &test_scraper_progress; }
int scraper_has_artwork(const char *game_path, const char *core_name) {
    (void)game_path; (void)core_name;
    return 0;
}
const char* scraper_get_libretro_repo(const char *core_name) { (void)core_name; return NULL; }

int main(int argc, char *argv[])
{
    const char *output_file = (argc > 1) ? argv[1] : "ui_preview/preview.png";

    printf("MiSTer UI Preview Generator\n");
    printf("===========================\n\n");

    // Initialize systems
    printf("Initializing theme system...\n");
    theme_init();

    printf("Initializing graphical menu...\n");
    gfx_menu_init();
    gfx_menu_set_enabled(1);

    // Apply a theme
    printf("Applying Analogue theme...\n");
    theme_select_by_name("Analogue");
    theme_entry_t *theme = theme_get_current();
    if (theme) {
        gfx_menu_set_theme(&theme->theme);
        printf("  Theme: %s\n", theme->meta.name);
    }

    // Test Systems Grid (new simplified UI)
    printf("\n--- Testing Systems Grid ---\n");
    
    // Clear and add system items
    gfx_menu_clear_items();
    gfx_menu_add_item("SNES", "/media/fat/_Console/SNES", GFX_ITEM_CORE);
    gfx_menu_add_item("NES", "/media/fat/_Console/NES", GFX_ITEM_CORE);
    gfx_menu_add_item("Genesis", "/media/fat/_Console/Genesis", GFX_ITEM_CORE);
    gfx_menu_add_item("TurboGrafx-16", "/media/fat/_Console/TurboGrafx16", GFX_ITEM_CORE);
    gfx_menu_add_item("PlayStation", "/media/fat/_Console/PSX", GFX_ITEM_CORE);
    gfx_menu_add_item("Neo Geo", "/media/fat/_Console/NeoGeo", GFX_ITEM_CORE);
    gfx_menu_add_item("Game Boy", "/media/fat/_Console/Gameboy", GFX_ITEM_CORE);
    gfx_menu_add_item("GBA", "/media/fat/_Console/GBA", GFX_ITEM_CORE);
    gfx_menu_add_item("N64", "/media/fat/_Console/N64", GFX_ITEM_CORE);
    gfx_menu_add_item("Atari 2600", "/media/fat/_Console/Atari2600", GFX_ITEM_CORE);
    gfx_menu_add_item("Commodore 64", "/media/fat/_Computer/C64", GFX_ITEM_CORE);
    gfx_menu_add_item("Amiga", "/media/fat/_Computer/Minimig", GFX_ITEM_CORE);
    
    gfx_menu_show_systems();
    gfx_menu_select_index(0);  // Select SNES
    
    char systems_filename[256];
    snprintf(systems_filename, sizeof(systems_filename), "ui_preview/preview_systems.png");
    printf("Rendering Systems Grid to %s...\n", systems_filename);
    int systems_result = gfx_menu_save_preview(systems_filename);
    if (systems_result == 0) {
        printf("  Success!\n");
    } else {
        printf("  Failed with error %d\n", systems_result);
    }

    // Test Games Grid
    printf("\n--- Testing Games Grid ---\n");
    
    // Clear and add game items for SNES
    gfx_menu_clear_items();
    gfx_menu_add_item("Super Mario World", "/media/fat/games/SNES/Super Mario World.mgl", GFX_ITEM_GAME);
    gfx_menu_add_item("Zelda - A Link to the Past", "/media/fat/games/SNES/Zelda ALTTP.mgl", GFX_ITEM_GAME);
    gfx_menu_add_item("Super Metroid", "/media/fat/games/SNES/Super Metroid.mgl", GFX_ITEM_GAME);
    gfx_menu_add_item("Chrono Trigger", "/media/fat/games/SNES/Chrono Trigger.mgl", GFX_ITEM_GAME);
    gfx_menu_add_item("Final Fantasy VI", "/media/fat/games/SNES/Final Fantasy VI.mgl", GFX_ITEM_GAME);
    gfx_menu_add_item("EarthBound", "/media/fat/games/SNES/EarthBound.mgl", GFX_ITEM_GAME);
    gfx_menu_add_item("Secret of Mana", "/media/fat/games/SNES/Secret of Mana.mgl", GFX_ITEM_GAME);
    gfx_menu_add_item("Donkey Kong Country", "/media/fat/games/SNES/DKC.mgl", GFX_ITEM_GAME);
    gfx_menu_add_item("Super Mario Kart", "/media/fat/games/SNES/Super Mario Kart.mgl", GFX_ITEM_GAME);
    gfx_menu_add_item("Star Fox", "/media/fat/games/SNES/Star Fox.mgl", GFX_ITEM_GAME);
    gfx_menu_add_item("F-Zero", "/media/fat/games/SNES/F-Zero.mgl", GFX_ITEM_GAME);
    gfx_menu_add_item("Mega Man X", "/media/fat/games/SNES/Mega Man X.mgl", GFX_ITEM_GAME);
    
    gfx_menu_show_games("SNES");
    gfx_menu_select_index(0);  // Select Super Mario World
    
    printf("Rendering Games Grid to %s...\n", output_file);
    int games_result = gfx_menu_save_preview(output_file);
    if (games_result == 0) {
        printf("  Success!\n");
    } else {
        printf("  Failed with error %d\n", games_result);
    }

    // Test Zaparoo NFC status icons in header
    printf("\n--- Testing Zaparoo NFC Status Icons ---\n");

    // Test different NFC status states
    const char *status_names[] = { "Disconnected", "Idle", "Scanning", "CardDetected" };
    zaparoo_status_t statuses[] = { ZAPAROO_DISCONNECTED, ZAPAROO_IDLE, ZAPAROO_SCANNING, ZAPAROO_CARD_DETECTED };

    for (int i = 0; i < 4; i++) {
        test_set_zaparoo_status(statuses[i]);
        zaparoo_hide_overlay();  // No overlay for status tests

        char filename[256];
        snprintf(filename, sizeof(filename), "ui_preview/preview_nfc_%s.png", status_names[i]);

        printf("Rendering NFC status '%s' to %s...\n", status_names[i], filename);
        int result = gfx_menu_save_preview(filename);

        if (result == 0) {
            printf("  Success!\n");
        } else {
            printf("  Failed with error %d\n", result);
        }
    }

    // Test Zaparoo card scan overlay
    printf("\n--- Testing Zaparoo Card Scan Overlay ---\n");

    test_set_zaparoo_status(ZAPAROO_CARD_DETECTED);
    zaparoo_show_overlay("Super Mario World", "/media/fat/games/SNES/Super Mario World.sfc");

    char overlay_filename[256];
    snprintf(overlay_filename, sizeof(overlay_filename), "ui_preview/preview_zaparoo_overlay.png");

    printf("Rendering Zaparoo card scan overlay to %s...\n", overlay_filename);
    int overlay_result = gfx_menu_save_preview(overlay_filename);

    if (overlay_result == 0) {
        printf("  Success!\n");
    } else {
        printf("  Failed with error %d\n", overlay_result);
    }

    printf("\nDone! Check the generated PNG files.\n");
    printf("\nGenerated files:\n");
    printf("  - ui_preview/preview_systems.png (Systems Grid)\n");
    printf("  - ui_preview/preview.png (Games Grid)\n");
    printf("  - ui_preview/preview_nfc_*.png (NFC status icons)\n");
    printf("  - ui_preview/preview_zaparoo_overlay.png (Card scan overlay)\n");

    // Cleanup
    gfx_menu_shutdown();

    return 0;
}
