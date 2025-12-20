// gfx_menu.h
// Graphical menu framework for MiSTer modern frontend
// 2024

#ifndef __GFX_MENU_H__
#define __GFX_MENU_H__

#include <inttypes.h>
#include "lib/imlib2/Imlib2.h"

// Menu view types
typedef enum {
	GFX_VIEW_LIST = 0,      // Traditional list with thumbnails
	GFX_VIEW_GRID,          // Grid of boxart images
	GFX_VIEW_WHEEL,         // Wheel/carousel view
	GFX_VIEW_COUNT
} gfx_view_type_t;

// Menu mode (screen type)
typedef enum {
	GFX_MODE_BROWSE = 0,    // File/folder browsing (default list/grid/wheel)
	GFX_MODE_HOME,          // Home screen with sections
	GFX_MODE_DETAILS,       // Full-screen game details
	GFX_MODE_COUNT
} gfx_menu_mode_t;

// Home screen section types
typedef enum {
	HOME_SECTION_CONTINUE = 0,  // Continue Playing (recently played)
	HOME_SECTION_RECENT,        // Recently Added (by file date)
	HOME_SECTION_FAVORITES,     // User favorites
	HOME_SECTION_CORES,         // Quick access to cores
	HOME_SECTION_COUNT
} home_section_t;

// Menu item types
typedef enum {
	GFX_ITEM_GAME = 0,      // Game/ROM file
	GFX_ITEM_CORE,          // FPGA core
	GFX_ITEM_FOLDER,        // Directory
	GFX_ITEM_SETTING,       // Configuration option
	GFX_ITEM_BACK           // Go back/parent
} gfx_item_type_t;

// Color definition (ARGB)
typedef struct {
	uint8_t a, r, g, b;
} gfx_color_t;

// Rectangle
typedef struct {
	int x, y, w, h;
} gfx_rect_t;

// Menu item
typedef struct {
	char name[256];
	char path[1024];
	char description[512];
	gfx_item_type_t type;
	Imlib_Image thumbnail;
	uint8_t is_selected;
	uint8_t is_favorite;
	void *user_data;
} gfx_menu_item_t;

// Theme colors
typedef struct {
	gfx_color_t background;
	gfx_color_t panel_bg;
	gfx_color_t panel_border;
	gfx_color_t text_primary;
	gfx_color_t text_secondary;
	gfx_color_t text_highlight;
	gfx_color_t selection_bg;
	gfx_color_t selection_border;
	gfx_color_t scrollbar_bg;
	gfx_color_t scrollbar_fg;
} gfx_theme_colors_t;

// Theme configuration
typedef struct {
	char name[64];
	gfx_theme_colors_t colors;
	char font_name[256];
	int font_size_title;
	int font_size_item;
	int font_size_info;
	int thumbnail_width;
	int thumbnail_height;
	int item_spacing;
	int panel_padding;
	int corner_radius;
	Imlib_Image background_image;
} gfx_theme_t;

// Home screen state
typedef struct {
	home_section_t current_section;
	int section_scroll[HOME_SECTION_COUNT];  // Horizontal scroll per section
	int section_counts[HOME_SECTION_COUNT];  // Item count per section
} home_state_t;

// Menu state
typedef struct {
	gfx_view_type_t view_type;
	gfx_menu_mode_t mode;           // Current menu mode (browse/home/details)
	gfx_menu_item_t *items;
	int item_count;
	int selected_index;
	int scroll_offset;
	int visible_count;
	char title[256];
	char breadcrumb[512];
	gfx_theme_t *theme;
	uint8_t enabled;
	uint8_t needs_redraw;
	float scroll_velocity;
	uint32_t last_input_time;
	home_state_t home;              // Home screen state
	int details_item_index;         // Item being viewed in details mode
} gfx_menu_state_t;

// Animation state
typedef struct {
	float progress;         // 0.0 to 1.0
	float speed;
	int from_index;
	int to_index;
	uint8_t active;
} gfx_animation_t;

//// Functions ////

// Initialize/shutdown
void gfx_menu_init(void);
void gfx_menu_shutdown(void);

// Enable/disable graphical menu
void gfx_menu_set_enabled(int enabled);
int gfx_menu_is_enabled(void);

// Theme management
void gfx_menu_load_theme(const char *theme_name);
void gfx_menu_set_theme(gfx_theme_t *theme);
gfx_theme_t* gfx_menu_get_theme(void);
void gfx_menu_apply_default_theme(void);

// Menu content
void gfx_menu_set_title(const char *title);
void gfx_menu_set_breadcrumb(const char *breadcrumb);
void gfx_menu_clear_items(void);
int gfx_menu_add_item(const char *name, const char *path, gfx_item_type_t type);
void gfx_menu_set_item_thumbnail(int index, Imlib_Image thumbnail);
void gfx_menu_set_item_description(int index, const char *description);
void gfx_menu_set_item_favorite(int index, int is_favorite);

// Navigation
void gfx_menu_select_next(void);
void gfx_menu_select_prev(void);
void gfx_menu_select_index(int index);
void gfx_menu_page_up(void);
void gfx_menu_page_down(void);
void gfx_menu_scroll_to(int index);
int gfx_menu_get_selected_index(void);
gfx_menu_item_t* gfx_menu_get_selected_item(void);

// View modes
void gfx_menu_set_view(gfx_view_type_t view);
gfx_view_type_t gfx_menu_get_view(void);
void gfx_menu_cycle_view(void);

// Menu modes (screens)
void gfx_menu_set_mode(gfx_menu_mode_t mode);
gfx_menu_mode_t gfx_menu_get_mode(void);
void gfx_menu_show_details(int item_index);
void gfx_menu_show_home(void);
void gfx_menu_show_browse(void);

// Rendering
void gfx_menu_render(void);
void gfx_menu_invalidate(void);
int gfx_menu_needs_redraw(void);

// Input handling (returns 1 if input was consumed)
int gfx_menu_handle_input(int key);

// Animation
void gfx_menu_update_animations(float delta_time);

// Utilities
gfx_color_t gfx_color_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
gfx_color_t gfx_color_hex(uint32_t hex);
void gfx_draw_rounded_rect(Imlib_Image img, gfx_rect_t rect, int radius, gfx_color_t color);
void gfx_draw_text(Imlib_Image img, const char *text, int x, int y, gfx_color_t color);

// Core Settings Menu Rendering
void gfx_menu_render_settings(void);
void gfx_menu_render_settings_category_tabs(int x, int y, int width);
void gfx_menu_render_settings_list(int x, int y, int width, int height);
void gfx_menu_render_setting_item(int x, int y, int width, void *setting, int selected);

// Testing/Preview
int gfx_menu_save_preview(const char *filename);

// Home screen data population (for testing)
void gfx_menu_home_populate_test_data(void);

#endif // __GFX_MENU_H__
