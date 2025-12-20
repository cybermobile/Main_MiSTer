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
int fb_width = 1280;
int fb_height = 720;

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

// C++ stubs for scraper
static scraper_config_t test_scraper_config = {0};
static scraper_progress_t test_scraper_progress = {0};

void scraper_init(void) {
    memset(&test_scraper_config, 0, sizeof(test_scraper_config));
    test_scraper_config.auto_scrape = 1;
}
void scraper_shutdown(void) {}
int scraper_load_config(void) { return 0; }
int scraper_save_config(void) { return 1; }
scraper_config_t* scraper_get_config(void) { return &test_scraper_config; }
int scraper_scrape_game(const char *game_name, const char *game_path,
                        const char *core_name, scraper_result_t *result) {
    (void)game_name; (void)game_path; (void)core_name; (void)result;
    return 0;
}
int scraper_scrape_game_async(const char *game_name, const char *game_path,
                               const char *core_name) {
    (void)game_name; (void)game_path; (void)core_name;
    return 0;
}
int scraper_async_complete(int scrape_id, scraper_result_t *result) {
    (void)scrape_id; (void)result;
    return 0;
}
int scraper_scrape_system(const char *system_name) { (void)system_name; return 0; }
int scraper_scrape_all(void) { return 0; }
void scraper_stop(void) {}
void scraper_pause(void) {}
void scraper_resume(void) {}
scraper_status_t scraper_get_status(void) { return SCRAPER_IDLE; }
scraper_progress_t* scraper_get_progress(void) { return &test_scraper_progress; }
int scraper_has_artwork(const char *game_path, const char *core_name) {
    (void)game_path; (void)core_name;
    return 0;
}
int scraper_auto_scrape(const char *game_name, const char *game_path,
                        const char *core_name) {
    (void)game_name; (void)game_path; (void)core_name;
    return 0;
}
void scraper_poll(void) {}
int scraper_get_screenscraper_system(const char *core_name) { (void)core_name; return 0; }
int scraper_get_thegamesdb_platform(const char *core_name) { (void)core_name; return 0; }
const char* scraper_get_steamgriddb_platform(const char *core_name) { (void)core_name; return NULL; }
const char* scraper_source_name(scraper_source_t source) {
    switch (source) {
        case SCRAPER_SOURCE_SCREENSCRAPER: return "ScreenScraper";
        case SCRAPER_SOURCE_THEGAMESDB: return "TheGamesDB";
        case SCRAPER_SOURCE_STEAMGRIDDB: return "SteamGridDB";
        default: return "Unknown";
    }
}
int scraper_source_configured(scraper_source_t source) { (void)source; return 0; }
int scraper_test_source(scraper_source_t source) { (void)source; return 0; }

int main(int argc, char *argv[])
{
    const char *output_file = (argc > 1) ? argv[1] : "ui_preview.png";

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

    // Add some test menu items
    printf("Adding test menu items...\n");
    gfx_menu_set_title("Games - SNES");
    gfx_menu_set_breadcrumb("/media/fat/games/SNES");

    gfx_menu_add_item("..", "/media/fat/games", GFX_ITEM_BACK);
    gfx_menu_add_item("Super Mario World", "/media/fat/games/SNES/Super Mario World.sfc", GFX_ITEM_GAME);
    gfx_menu_add_item("The Legend of Zelda - A Link to the Past", "/media/fat/games/SNES/Zelda ALTTP.sfc", GFX_ITEM_GAME);
    gfx_menu_add_item("Super Metroid", "/media/fat/games/SNES/Super Metroid.sfc", GFX_ITEM_GAME);
    gfx_menu_add_item("Chrono Trigger", "/media/fat/games/SNES/Chrono Trigger.sfc", GFX_ITEM_GAME);
    gfx_menu_add_item("Final Fantasy VI", "/media/fat/games/SNES/Final Fantasy VI.sfc", GFX_ITEM_GAME);
    gfx_menu_add_item("EarthBound", "/media/fat/games/SNES/EarthBound.sfc", GFX_ITEM_GAME);
    gfx_menu_add_item("Secret of Mana", "/media/fat/games/SNES/Secret of Mana.sfc", GFX_ITEM_GAME);
    gfx_menu_add_item("Donkey Kong Country", "/media/fat/games/SNES/DKC.sfc", GFX_ITEM_GAME);
    gfx_menu_add_item("Super Mario Kart", "/media/fat/games/SNES/Super Mario Kart.sfc", GFX_ITEM_GAME);
    gfx_menu_add_item("Star Fox", "/media/fat/games/SNES/Star Fox.sfc", GFX_ITEM_GAME);

    // Select an item
    gfx_menu_select_index(1);  // Select Super Mario World

    // Try different view modes
    const char *view_names[] = { "List", "Grid", "Wheel" };

    for (int view = 0; view < 3; view++) {
        gfx_menu_set_view((gfx_view_type_t)view);

        char filename[256];
        if (view == 0) {
            strcpy(filename, output_file);
        } else {
            snprintf(filename, sizeof(filename), "%s_%s.png",
                     output_file, view_names[view]);
        }

        printf("Rendering %s view to %s...\n", view_names[view], filename);
        int result = gfx_menu_save_preview(filename);

        if (result == 0) {
            printf("  Success!\n");
        } else {
            printf("  Failed with error %d\n", result);
        }
    }

    // Test Zaparoo NFC status icons in header
    printf("\n--- Testing Zaparoo NFC Status Icons ---\n");

    // Test different NFC status states
    const char *status_names[] = { "Disconnected", "Idle", "Scanning", "CardDetected" };
    zaparoo_status_t statuses[] = { ZAPAROO_DISCONNECTED, ZAPAROO_IDLE, ZAPAROO_SCANNING, ZAPAROO_CARD_DETECTED };

    gfx_menu_set_view(GFX_VIEW_LIST);  // Use list view for status tests

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

    // Test Game Details Page (Polymega-inspired)
    printf("\n--- Testing Game Details Page ---\n");

    zaparoo_hide_overlay();  // No overlay for details test
    test_set_zaparoo_status(ZAPAROO_IDLE);
    gfx_menu_set_view(GFX_VIEW_LIST);

    // Show details for an item
    gfx_menu_show_details(1);  // Show details for Super Mario World

    char details_filename[256];
    snprintf(details_filename, sizeof(details_filename), "ui_preview/preview_details.png");

    printf("Rendering Game Details page to %s...\n", details_filename);
    int details_result = gfx_menu_save_preview(details_filename);

    if (details_result == 0) {
        printf("  Success!\n");
    } else {
        printf("  Failed with error %d\n", details_result);
    }

    // Test Home Screen
    printf("\n--- Testing Home Screen ---\n");

    // Populate home screen with test data
    gfx_menu_home_populate_test_data();
    gfx_menu_show_home();

    char home_filename[256];
    snprintf(home_filename, sizeof(home_filename), "ui_preview/preview_home.png");

    printf("Rendering Home Screen to %s...\n", home_filename);
    int home_result = gfx_menu_save_preview(home_filename);

    if (home_result == 0) {
        printf("  Success!\n");
    } else {
        printf("  Failed with error %d\n", home_result);
    }

    // Return to browse mode for final cleanup
    gfx_menu_show_browse();

    printf("\nDone! Check the generated PNG files.\n");
    printf("\nGenerated files:\n");
    printf("  - ui_preview/preview.png (List view)\n");
    printf("  - ui_preview/preview.png_Grid.png (Grid view)\n");
    printf("  - ui_preview/preview.png_Wheel.png (Wheel view)\n");
    printf("  - ui_preview/preview_nfc_*.png (NFC status icons)\n");
    printf("  - ui_preview/preview_zaparoo_overlay.png (Card scan overlay)\n");
    printf("  - ui_preview/preview_details.png (Game Details page)\n");
    printf("  - ui_preview/preview_home.png (Home Screen)\n");

    // Cleanup
    gfx_menu_shutdown();

    return 0;
}
