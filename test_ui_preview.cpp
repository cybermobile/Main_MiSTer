// test_ui_preview.cpp
// Standalone test utility to preview the graphical menu UI
// Build: g++ -o test_ui_preview test_ui_preview.cpp gfx_menu.cpp theme.cpp -lImlib2 -I. -I./lib
// Run: ./test_ui_preview [output.png]

#include <stdio.h>
#include <string.h>
#include "gfx_menu.h"
#include "theme.h"

// Stub out hardware-dependent functions for testing
extern "C" {
    // Video stubs
    void video_fb_enable(int enable, int buffer) { (void)enable; (void)buffer; }
    int video_fb_state(void) { return 0; }

    // OSD stubs
    void OsdDisable(void) {}

    // Config stubs
    struct {
        int gfx_menu_enable;
        int gfx_menu_view;
        char gfx_menu_theme[64];
        int gamedb_enable;
    } cfg = { 1, 0, "Analogue", 0 };

    // Timer stub
    unsigned long GetTimer(unsigned long offset) { return 1000 + offset; }

    // Menu state stub
    int menu_use_graphical(void) { return 1; }

    // Boxart stub
    void* boxart_get_preview_image(void) { return NULL; }

    // Search stubs
    int search_is_active(void) { return 0; }

    // Playtime stub
    void* playtime_get_entry(const char *path) { (void)path; return NULL; }

    // Gamedb stub
    void* gamedb_lookup_filename(const char *filename) { (void)filename; return NULL; }
}

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

    printf("\nDone! Check the generated PNG files.\n");

    // Cleanup
    gfx_menu_shutdown();

    return 0;
}
