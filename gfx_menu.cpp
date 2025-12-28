// gfx_menu.cpp
// Graphical menu framework for MiSTer modern frontend
// 2024

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/kd.h>
#include <linux/fb.h>

#include "gfx_menu.h"
#include "video.h"
#include "cfg.h"
#include "boxart.h"
#include "file_io.h"
#include "hardware.h"
#include "animator.h"
#include "search.h"
#include "user_io.h"
#include "osd.h"
#include "charrom.h"
#include "playtime.h"
#include "gamedb.h"
#include "menu.h"
#include "zaparoo.h"

// Maximum items in menu
#define GFX_MAX_ITEMS 1024

// Layout constants
#define LIST_PANEL_WIDTH_RATIO   0.45f   // Left panel takes 45% of screen
#define PREVIEW_PANEL_WIDTH_RATIO 0.50f  // Right panel takes 50%
#define HEADER_HEIGHT 60
#define FOOTER_HEIGHT 50
#define SCROLLBAR_WIDTH 8

// Animation constants
#define SCROLL_SMOOTH_FACTOR 0.15f
#define SELECTION_ANIM_SPEED 8.0f
#define SCROLL_ANIM_DURATION 0.25f
#define SELECT_ANIM_DURATION 0.15f

// Global state
static gfx_menu_state_t menu_state;
static gfx_theme_t default_theme;
static gfx_menu_item_t items_storage[GFX_MAX_ITEMS];
static gfx_animation_t scroll_anim;
static gfx_animation_t selection_anim;

// Animation state for smooth scrolling and selection
static float anim_scroll_offset = 0.0f;
static float anim_selection_y = 0.0f;
static float anim_selection_scale = 1.0f;
static float anim_preview_opacity = 1.0f;
static uint32_t scroll_anim_id = 0;
static uint32_t selection_anim_id = 0;
static uint32_t preview_fade_id = 0;
static uint32_t selection_pulse_id = 0;

// Framebuffer access
extern volatile uint32_t *fb_base;
extern int fb_width;
extern int fb_height;

#define FB_SIZE (1920*1080)
// Framebuffer double-buffering for gfx UI.
// Avoid buffer 0 (Linux fbcon/getty). Use buffers 1 and 2.
#define GFX_FB_A 1
#define GFX_FB_B 2
static int gfx_fb_front = GFX_FB_A;

static void gfx_console_set_graphics_mode(int enable)
{
	// /dev/tty0 refers to the currently active VT
	int fd = open("/dev/tty0", O_RDWR | O_CLOEXEC);
	if (fd < 0) return;
	ioctl(fd, KDSETMODE, enable ? KD_GRAPHICS : KD_TEXT);
	close(fd);
}

// Forward declarations
static void render_grid_view(Imlib_Image canvas);
static void render_game_details_overlay(Imlib_Image canvas);
static void render_systems_grid(Imlib_Image canvas);
static void render_settings_menu(Imlib_Image canvas);
static void render_search_overlay(Imlib_Image canvas);
static void render_zaparoo_overlay(Imlib_Image canvas);
static void render_header(Imlib_Image canvas);
static void render_footer(Imlib_Image canvas);
static void render_scrollbar(Imlib_Image canvas, gfx_rect_t bounds);
static void draw_nfc_icon(Imlib_Image canvas, int x, int y, int size, gfx_color_t color);
static void draw_wifi_icon(Imlib_Image canvas, int x, int y, int size, gfx_color_t color);
static void draw_ethernet_icon(Imlib_Image canvas, int x, int y, int size, gfx_color_t color);
static void draw_controller_icon(Imlib_Image canvas, int x, int y, int size, gfx_color_t color);

// Settings menu state
static int settings_selected = 0;
static gfx_menu_mode_t settings_previous_mode = GFX_MODE_SYSTEMS;
#define SETTINGS_COUNT 7  // Theme, Boxart, Animations, Rescan, Scripts, Update All, Back

// Theme selection
static int current_theme_index = 0;
#define THEME_COUNT 4
static const char *theme_names[THEME_COUNT] = {
	"Analogue Dark",
	"Analogue Light", 
	"Classic MiSTer",
	"Retro CRT"
};

// Text scrolling state for long game names
static int scroll_text_offset = 0;
static uint32_t scroll_text_timer = 0;
static int scroll_text_item = -1;  // Which item is being scrolled
static int scroll_text_direction = 1;  // 1 = scroll left, -1 = scroll right
#define SCROLL_TEXT_DELAY_MS 100
#define SCROLL_TEXT_PAUSE_MS 1500  // Pause at ends

// External function for network status
extern char* getNet(int spec);

// Helper: Create color from RGBA
gfx_color_t gfx_color_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	gfx_color_t c = { a, r, g, b };
	return c;
}

// Helper: Create color from hex (0xAARRGGBB)
gfx_color_t gfx_color_hex(uint32_t hex)
{
	gfx_color_t c;
	c.a = (hex >> 24) & 0xFF;
	c.r = (hex >> 16) & 0xFF;
	c.g = (hex >> 8) & 0xFF;
	c.b = hex & 0xFF;
	return c;
}

// Helper: Set Imlib2 color from gfx_color
static void set_imlib_color(gfx_color_t c)
{
	imlib_context_set_color(c.r, c.g, c.b, c.a);
}

// Helper: Draw text with max width, truncating with "..." if needed
// Font is 8x8 scaled 2x = 16 pixels per character
static void gfx_draw_text_truncated(Imlib_Image img, const char *text, int x, int y, int max_width, gfx_color_t color)
{
	if (!text || !img || max_width <= 0) return;

	const int char_width = 16; // 8 * 2 (scale factor)
	int max_chars = max_width / char_width;
	int text_len = strlen(text);

	if (text_len <= max_chars) {
		// Text fits, draw normally
		gfx_draw_text(img, text, x, y, color);
	} else if (max_chars > 3) {
		// Truncate with ellipsis
		char truncated[256];
		int copy_len = max_chars - 3; // Leave room for "..."
		if (copy_len > 255 - 3) copy_len = 255 - 3;
		strncpy(truncated, text, copy_len);
		truncated[copy_len] = '\0';
		strcat(truncated, "...");
		gfx_draw_text(img, truncated, x, y, color);
	}
}

// Helper: Draw scrolling text for selected item (marquee effect)
// Returns the visible portion of text with scrolling animation
static void gfx_draw_text_scrolling(Imlib_Image img, const char *text, int x, int y, int max_width, gfx_color_t color, int is_selected)
{
	if (!text || !img || max_width <= 0) return;

	const int char_width = 16;
	int max_chars = max_width / char_width;
	int text_len = strlen(text);

	if (text_len <= max_chars || !is_selected) {
		// Text fits or not selected, use normal truncation
		gfx_draw_text_truncated(img, text, x, y, max_width, color);
		return;
	}

	// For selected item with long text, scroll it
	uint32_t now = GetTimer(0);
	
	// Reset scroll if selection changed
	if (scroll_text_item != menu_state.selected_index) {
		scroll_text_item = menu_state.selected_index;
		scroll_text_offset = 0;
		scroll_text_direction = 1;
		scroll_text_timer = now + SCROLL_TEXT_PAUSE_MS;  // Initial pause
	}

	// Update scroll position
	if (now >= scroll_text_timer) {
		int max_offset = text_len - max_chars;
		
		if (scroll_text_direction > 0) {
			scroll_text_offset++;
			if (scroll_text_offset >= max_offset) {
				scroll_text_offset = max_offset;
				scroll_text_direction = -1;
				scroll_text_timer = now + SCROLL_TEXT_PAUSE_MS;  // Pause at end
			} else {
				scroll_text_timer = now + SCROLL_TEXT_DELAY_MS;
			}
		} else {
			scroll_text_offset--;
			if (scroll_text_offset <= 0) {
				scroll_text_offset = 0;
				scroll_text_direction = 1;
				scroll_text_timer = now + SCROLL_TEXT_PAUSE_MS;  // Pause at start
			} else {
				scroll_text_timer = now + SCROLL_TEXT_DELAY_MS;
			}
		}
		menu_state.needs_redraw = 1;  // Keep redrawing during scroll
	}

	// Draw the visible portion
	char visible[256];
	int copy_len = max_chars;
	if (copy_len > 255) copy_len = 255;
	strncpy(visible, text + scroll_text_offset, copy_len);
	visible[copy_len] = '\0';
	gfx_draw_text(img, visible, x, y, color);
}

// Apply theme by index
static void apply_theme(int theme_index)
{
	memset(&default_theme, 0, sizeof(default_theme));
	
	// Common settings for all themes
	strcpy(default_theme.font_name, "");
	default_theme.font_size_title = 28;
	default_theme.font_size_item = 20;
	default_theme.font_size_info = 16;
	default_theme.thumbnail_width = 100;
	default_theme.thumbnail_height = 100;
	default_theme.item_spacing = 12;
	default_theme.panel_padding = 20;
	default_theme.corner_radius = 4;
	default_theme.background_image = NULL;
	
	switch (theme_index)
	{
		case 0:  // Analogue Dark (default)
			strcpy(default_theme.name, "Analogue Dark");
			default_theme.colors.background = gfx_color_hex(0xFF222222);
			default_theme.colors.panel_bg = gfx_color_hex(0xE0181818);
			default_theme.colors.panel_border = gfx_color_hex(0xFF333333);
			default_theme.colors.text_primary = gfx_color_hex(0xFFcccccc);
			default_theme.colors.text_secondary = gfx_color_hex(0xFF888888);
			default_theme.colors.text_highlight = gfx_color_hex(0xFFffffff);
			default_theme.colors.selection_bg = gfx_color_hex(0x30ffffff);
			default_theme.colors.selection_border = gfx_color_hex(0xFFcccccc);
			default_theme.colors.scrollbar_bg = gfx_color_hex(0x20ffffff);
			default_theme.colors.scrollbar_fg = gfx_color_hex(0xFFcccccc);
			break;
			
		case 1:  // Analogue Light
			strcpy(default_theme.name, "Analogue Light");
			default_theme.colors.background = gfx_color_hex(0xFFf5f5f5);
			default_theme.colors.panel_bg = gfx_color_hex(0xE0ffffff);
			default_theme.colors.panel_border = gfx_color_hex(0xFFdddddd);
			default_theme.colors.text_primary = gfx_color_hex(0xFF333333);
			default_theme.colors.text_secondary = gfx_color_hex(0xFF666666);
			default_theme.colors.text_highlight = gfx_color_hex(0xFF000000);
			default_theme.colors.selection_bg = gfx_color_hex(0x30000000);
			default_theme.colors.selection_border = gfx_color_hex(0xFF333333);
			default_theme.colors.scrollbar_bg = gfx_color_hex(0x20000000);
			default_theme.colors.scrollbar_fg = gfx_color_hex(0xFF666666);
			break;
			
		case 2:  // Classic MiSTer (blue theme)
			strcpy(default_theme.name, "Classic MiSTer");
			default_theme.colors.background = gfx_color_hex(0xFF1a1a2e);
			default_theme.colors.panel_bg = gfx_color_hex(0xE016213e);
			default_theme.colors.panel_border = gfx_color_hex(0xFF0f3460);
			default_theme.colors.text_primary = gfx_color_hex(0xFFe0e0e0);
			default_theme.colors.text_secondary = gfx_color_hex(0xFF94a3b8);
			default_theme.colors.text_highlight = gfx_color_hex(0xFF00d4ff);
			default_theme.colors.selection_bg = gfx_color_hex(0x400f3460);
			default_theme.colors.selection_border = gfx_color_hex(0xFF00d4ff);
			default_theme.colors.scrollbar_bg = gfx_color_hex(0x200f3460);
			default_theme.colors.scrollbar_fg = gfx_color_hex(0xFF00d4ff);
			break;
			
		case 3:  // Retro CRT (green phosphor)
			strcpy(default_theme.name, "Retro CRT");
			default_theme.colors.background = gfx_color_hex(0xFF0a0a0a);
			default_theme.colors.panel_bg = gfx_color_hex(0xE0101010);
			default_theme.colors.panel_border = gfx_color_hex(0xFF1a3a1a);
			default_theme.colors.text_primary = gfx_color_hex(0xFF33ff33);
			default_theme.colors.text_secondary = gfx_color_hex(0xFF228822);
			default_theme.colors.text_highlight = gfx_color_hex(0xFF66ff66);
			default_theme.colors.selection_bg = gfx_color_hex(0x3033ff33);
			default_theme.colors.selection_border = gfx_color_hex(0xFF33ff33);
			default_theme.colors.scrollbar_bg = gfx_color_hex(0x2033ff33);
			default_theme.colors.scrollbar_fg = gfx_color_hex(0xFF33ff33);
			break;
			
		default:
			// Fall through to Analogue Dark
			apply_theme(0);
			return;
	}
	
	current_theme_index = theme_index;
	menu_state.theme = &default_theme;
	menu_state.needs_redraw = 1;
}

// Initialize default theme with Analogue-inspired minimalist style
static void init_default_theme(void)
{
	apply_theme(0);  // Analogue Dark is the default
}

void gfx_menu_init(void)
{
	memset(&menu_state, 0, sizeof(menu_state));
	memset(&items_storage, 0, sizeof(items_storage));
	memset(&scroll_anim, 0, sizeof(scroll_anim));
	memset(&selection_anim, 0, sizeof(selection_anim));

	menu_state.items = items_storage;
	menu_state.item_count = 0;
	menu_state.selected_index = 0;
	menu_state.scroll_offset = 0;
	menu_state.mode = GFX_MODE_SYSTEMS;  // Start with system selection
	menu_state.enabled = 0;  // Disabled by default, use classic OSD
	menu_state.needs_redraw = 1;
	menu_state.current_system[0] = '\0';

	// Initialize animation state
	anim_scroll_offset = 0.0f;
	anim_selection_y = 0.0f;
	anim_selection_scale = 1.0f;
	scroll_anim_id = 0;
	selection_anim_id = 0;

	init_default_theme();
	menu_state.theme = &default_theme;

	printf("GFX Menu initialized\n");
}

void gfx_menu_shutdown(void)
{
	gfx_menu_clear_items();

	if (default_theme.background_image)
	{
		imlib_context_set_image(default_theme.background_image);
		imlib_free_image();
		default_theme.background_image = NULL;
	}

	printf("GFX Menu shutdown\n");
}

void gfx_menu_set_enabled(int enabled)
{
	menu_state.enabled = enabled ? 1 : 0;
	menu_state.needs_redraw = 1;
	
	if (menu_state.enabled)
	{
		// Keep menu input routing enabled even though we disable OSD hardware overlay
		user_io_osd_key_enable(1);

		// Switch away from tty1 (which runs agetty/login) so the Linux console text
		// doesn't draw over our framebuffer UI. Avoid tty2 (used by doc viewer / scripts).
		video_chvt(3);
		// Put the active VT into graphics mode to prevent fbcon/getty text/cursor drawing.
		gfx_console_set_graphics_mode(1);

		// Enable gfx framebuffer and start on front buffer
		gfx_fb_front = GFX_FB_A;
		video_fb_enable(1, gfx_fb_front);

		// Disable OSD hardware overlay
		OsdDisable();
	}
	else
	{
		user_io_osd_key_enable(0);
		// Restore text mode first (non-blocking)
		gfx_console_set_graphics_mode(0);
		// Disable our framebuffer - let core/system take over
		video_fb_enable(0, 0);
		// Return to default console (may block briefly, do last)
		video_chvt(1);
	}
}

int gfx_menu_is_enabled(void)
{
	return menu_state.enabled;
}

void gfx_menu_load_theme(const char *theme_name)
{
	// TODO: Load theme from JSON file
	// For now, just use default theme
	(void)theme_name;
	gfx_menu_apply_default_theme();
}

void gfx_menu_set_theme(gfx_theme_t *theme)
{
	if (theme)
	{
		menu_state.theme = theme;
		menu_state.needs_redraw = 1;
	}
}

gfx_theme_t* gfx_menu_get_theme(void)
{
	return menu_state.theme;
}

void gfx_menu_apply_default_theme(void)
{
	init_default_theme();
	menu_state.theme = &default_theme;
	menu_state.needs_redraw = 1;
}

void gfx_menu_set_title(const char *title)
{
	if (title)
	{
		strncpy(menu_state.title, title, sizeof(menu_state.title) - 1);
		menu_state.title[sizeof(menu_state.title) - 1] = '\0';
	}
	else
	{
		menu_state.title[0] = '\0';
	}
	menu_state.needs_redraw = 1;
}

// Breadcrumb removed - not used in simplified grid UI

void gfx_menu_clear_items(void)
{
	for (int i = 0; i < menu_state.item_count; i++)
	{
		if (items_storage[i].thumbnail)
		{
			// Don't free - thumbnails come from boxart cache
			items_storage[i].thumbnail = NULL;
		}
	}
	memset(items_storage, 0, sizeof(items_storage));
	menu_state.item_count = 0;
	menu_state.selected_index = 0;
	menu_state.scroll_offset = 0;
	menu_state.needs_redraw = 1;
}

int gfx_menu_add_item(const char *name, const char *path, gfx_item_type_t type)
{
	if (menu_state.item_count >= GFX_MAX_ITEMS) return -1;

	gfx_menu_item_t *item = &items_storage[menu_state.item_count];
	memset(item, 0, sizeof(gfx_menu_item_t));

	if (name)
	{
		strncpy(item->name, name, sizeof(item->name) - 1);
	}
	if (path)
	{
		strncpy(item->path, path, sizeof(item->path) - 1);
	}
	item->type = type;
	item->is_selected = 0;
	item->is_favorite = 0;
	item->thumbnail = NULL;  // Thumbnails loaded separately via gfx_menu_set_item_thumbnail

	menu_state.item_count++;
	menu_state.needs_redraw = 1;

	return menu_state.item_count - 1;
}

void gfx_menu_set_item_thumbnail(int index, Imlib_Image thumbnail)
{
	if (index >= 0 && index < menu_state.item_count)
	{
		items_storage[index].thumbnail = thumbnail;
		menu_state.needs_redraw = 1;
	}
}

void gfx_menu_set_item_description(int index, const char *description)
{
	if (index >= 0 && index < menu_state.item_count && description)
	{
		strncpy(items_storage[index].description, description,
			sizeof(items_storage[index].description) - 1);
		menu_state.needs_redraw = 1;
	}
}

void gfx_menu_set_item_favorite(int index, int is_favorite)
{
	if (index >= 0 && index < menu_state.item_count)
	{
		items_storage[index].is_favorite = is_favorite ? 1 : 0;
		menu_state.needs_redraw = 1;
	}
}

void gfx_menu_select_next(void)
{
	if (menu_state.item_count == 0) return;

	int prev_index = menu_state.selected_index;
	menu_state.selected_index++;
	if (menu_state.selected_index >= menu_state.item_count)
	{
		menu_state.selected_index = 0;  // Wrap around
	}

	// Cancel any existing selection animation
	if (selection_anim_id && anim_is_running(selection_anim_id))
	{
		anim_cancel(selection_anim_id);
	}

	// Selection animation (skip if animations disabled)
	if (cfg.anim_enable)
	{
		anim_selection_scale = 0.95f;
		selection_anim_id = anim_create_to(&anim_selection_scale, 1.0f,
		                                   SELECT_ANIM_DURATION, EASE_OUT_BACK);
		if (selection_anim_id)
		{
			anim_start(selection_anim_id);
		}

		// Fade preview panel for new selection
		if (preview_fade_id && anim_is_running(preview_fade_id))
		{
			anim_cancel(preview_fade_id);
		}
		anim_preview_opacity = 0.7f;
		preview_fade_id = anim_fade_in(&anim_preview_opacity, 0.2f);
	}
	else
	{
		anim_selection_scale = 1.0f;
		anim_preview_opacity = 1.0f;
	}

	// Update scroll to keep selection visible
	gfx_menu_scroll_to(menu_state.selected_index);
	menu_state.needs_redraw = 1;

	(void)prev_index;  // Mark as used
}

void gfx_menu_select_prev(void)
{
	if (menu_state.item_count == 0) return;

	int prev_index = menu_state.selected_index;
	menu_state.selected_index--;
	if (menu_state.selected_index < 0)
	{
		menu_state.selected_index = menu_state.item_count - 1;  // Wrap around
	}

	// Cancel any existing selection animation
	if (selection_anim_id && anim_is_running(selection_anim_id))
	{
		anim_cancel(selection_anim_id);
	}

	// Selection animation (skip if animations disabled)
	if (cfg.anim_enable)
	{
		anim_selection_scale = 0.95f;
		selection_anim_id = anim_create_to(&anim_selection_scale, 1.0f,
		                                   SELECT_ANIM_DURATION, EASE_OUT_BACK);
		if (selection_anim_id)
		{
			anim_start(selection_anim_id);
		}

		// Fade preview panel for new selection
		if (preview_fade_id && anim_is_running(preview_fade_id))
		{
			anim_cancel(preview_fade_id);
		}
		anim_preview_opacity = 0.7f;
		preview_fade_id = anim_fade_in(&anim_preview_opacity, 0.2f);
	}
	else
	{
		anim_selection_scale = 1.0f;
		anim_preview_opacity = 1.0f;
	}

	gfx_menu_scroll_to(menu_state.selected_index);
	menu_state.needs_redraw = 1;

	(void)prev_index;  // Mark as used
}

void gfx_menu_select_index(int index)
{
	if (index >= 0 && index < menu_state.item_count)
	{
		menu_state.selected_index = index;
		gfx_menu_scroll_to(index);
		menu_state.needs_redraw = 1;
	}
}

void gfx_menu_page_up(void)
{
	int page_size = menu_state.visible_count > 0 ? menu_state.visible_count : 10;
	menu_state.selected_index -= page_size;
	if (menu_state.selected_index < 0)
	{
		menu_state.selected_index = 0;
	}
	gfx_menu_scroll_to(menu_state.selected_index);
	menu_state.needs_redraw = 1;
}

void gfx_menu_page_down(void)
{
	int page_size = menu_state.visible_count > 0 ? menu_state.visible_count : 10;
	menu_state.selected_index += page_size;
	if (menu_state.selected_index >= menu_state.item_count)
	{
		menu_state.selected_index = menu_state.item_count - 1;
	}
	gfx_menu_scroll_to(menu_state.selected_index);
	menu_state.needs_redraw = 1;
}

void gfx_menu_scroll_to(int index)
{
	if (menu_state.visible_count <= 0) return;

	// Calculate target scroll offset to center the selection
	int half_visible = menu_state.visible_count / 2;
	int target_offset = index - half_visible;

	// Clamp scroll offset
	int max_offset = menu_state.item_count - menu_state.visible_count;
	if (target_offset < 0) target_offset = 0;
	if (target_offset > max_offset) target_offset = max_offset;
	if (max_offset < 0) target_offset = 0;

	// Cancel any existing scroll animation
	if (scroll_anim_id && anim_is_running(scroll_anim_id))
	{
		anim_cancel(scroll_anim_id);
	}

	// Update scroll offset - immediately if animations disabled
	if (!cfg.anim_enable)
	{
		anim_scroll_offset = (float)target_offset;
		menu_state.scroll_offset = target_offset;
	}
	else
	{
		// Create smooth scroll animation using animator system
		scroll_anim_id = anim_create_to(&anim_scroll_offset, (float)target_offset,
		                                SCROLL_ANIM_DURATION, EASE_OUT_CUBIC);
		if (scroll_anim_id)
		{
			anim_start(scroll_anim_id);
		}
		menu_state.scroll_offset = target_offset;
	}
}

int gfx_menu_get_selected_index(void)
{
	return menu_state.selected_index;
}

gfx_menu_item_t* gfx_menu_get_selected_item(void)
{
	if (menu_state.selected_index >= 0 && menu_state.selected_index < menu_state.item_count)
	{
		return &items_storage[menu_state.selected_index];
	}
	return NULL;
}

// Menu mode (screen) management - simplified for grid-only UI
void gfx_menu_set_mode(gfx_menu_mode_t mode)
{
	if (mode < GFX_MODE_COUNT)
	{
		// Save previous mode when switching to settings
		if (mode == GFX_MODE_SETTINGS && menu_state.mode != GFX_MODE_SETTINGS)
		{
			settings_previous_mode = menu_state.mode;
			settings_selected = 0;
		}
		menu_state.mode = mode;
		menu_state.needs_redraw = 1;
	}
}

gfx_menu_mode_t gfx_menu_get_mode(void)
{
	return menu_state.mode;
}

void gfx_menu_show_systems(void)
{
	menu_state.mode = GFX_MODE_SYSTEMS;
	menu_state.current_system[0] = '\0';
	gfx_menu_set_title("Select System");
	menu_state.selected_index = 0;
	menu_state.scroll_offset = 0;
	menu_state.needs_redraw = 1;
}

void gfx_menu_show_games(const char *system_name)
{
	if (system_name && system_name[0])
	{
		strncpy(menu_state.current_system, system_name, sizeof(menu_state.current_system) - 1);
		menu_state.current_system[sizeof(menu_state.current_system) - 1] = '\0';
	}
	menu_state.mode = GFX_MODE_GAMES;
	gfx_menu_set_title(menu_state.current_system);
	menu_state.selected_index = 0;
	menu_state.scroll_offset = 0;
	menu_state.needs_redraw = 1;
}

const char* gfx_menu_get_current_system(void)
{
	return menu_state.current_system;
}

void gfx_menu_invalidate(void)
{
	menu_state.needs_redraw = 1;
}

int gfx_menu_needs_redraw(void)
{
	return menu_state.needs_redraw;
}

void gfx_menu_update_animations(float delta_time)
{
	(void)delta_time;  // Not needed - anim_update called from main loop

	// Check if any animations are active and request redraw
	int animations_active = 0;

	if (scroll_anim_id && anim_is_running(scroll_anim_id))
	{
		animations_active = 1;
	}

	if (selection_anim_id && anim_is_running(selection_anim_id))
	{
		animations_active = 1;
	}

	if (preview_fade_id && anim_is_running(preview_fade_id))
	{
		animations_active = 1;
	}

	if (selection_pulse_id && anim_is_running(selection_pulse_id))
	{
		animations_active = 1;
	}

	if (animations_active)
	{
		menu_state.needs_redraw = 1;
	}
}

// Draw a filled rectangle
static void draw_filled_rect(Imlib_Image img, gfx_rect_t rect, gfx_color_t color)
{
	imlib_context_set_image(img);
	set_imlib_color(color);
	imlib_image_fill_rectangle(rect.x, rect.y, rect.w, rect.h);
}

// Draw a rectangle border
static void draw_rect_border(Imlib_Image img, gfx_rect_t rect, gfx_color_t color, int thickness)
{
	imlib_context_set_image(img);
	set_imlib_color(color);

	// Top
	imlib_image_fill_rectangle(rect.x, rect.y, rect.w, thickness);
	// Bottom
	imlib_image_fill_rectangle(rect.x, rect.y + rect.h - thickness, rect.w, thickness);
	// Left
	imlib_image_fill_rectangle(rect.x, rect.y, thickness, rect.h);
	// Right
	imlib_image_fill_rectangle(rect.x + rect.w - thickness, rect.y, thickness, rect.h);
}

void gfx_draw_rounded_rect(Imlib_Image img, gfx_rect_t rect, int radius, gfx_color_t color)
{
	// Simplified: just draw a regular rectangle for now
	// TODO: Implement proper rounded corners with circles
	draw_filled_rect(img, rect, color);
}

// Draw NFC icon (card shape with radio waves)
static void draw_nfc_icon(Imlib_Image canvas, int x, int y, int size, gfx_color_t color)
{
	imlib_context_set_image(canvas);
	set_imlib_color(color);

	// Card body (rectangle)
	int card_w = size;
	int card_h = (int)(size * 0.7f);
	int card_x = x;
	int card_y = y + (size - card_h) / 2;

	// Draw card outline
	int thickness = 2;
	// Top
	imlib_image_fill_rectangle(card_x, card_y, card_w, thickness);
	// Bottom
	imlib_image_fill_rectangle(card_x, card_y + card_h - thickness, card_w, thickness);
	// Left
	imlib_image_fill_rectangle(card_x, card_y, thickness, card_h);
	// Right
	imlib_image_fill_rectangle(card_x + card_w - thickness, card_y, thickness, card_h);

	// Radio wave arcs (simplified as concentric partial rectangles/lines)
	// Draw 3 curved lines emanating from top-right corner
	int wave_x = card_x + card_w - 8;
	int wave_y = card_y + 4;

	// Wave 1 (innermost)
	imlib_image_fill_rectangle(wave_x - 2, wave_y, 4, 2);
	imlib_image_fill_rectangle(wave_x + 2, wave_y, 2, 4);

	// Wave 2 (middle)
	imlib_image_fill_rectangle(wave_x - 5, wave_y - 2, 6, 2);
	imlib_image_fill_rectangle(wave_x + 4, wave_y - 2, 2, 6);

	// Wave 3 (outermost)
	imlib_image_fill_rectangle(wave_x - 8, wave_y - 4, 8, 2);
	imlib_image_fill_rectangle(wave_x + 6, wave_y - 4, 2, 8);

	// Small chip rectangle inside card
	int chip_size = 6;
	int chip_x = card_x + 6;
	int chip_y = card_y + (card_h - chip_size) / 2;
	imlib_image_fill_rectangle(chip_x, chip_y, chip_size, chip_size);
}

// Draw WiFi icon (signal waves)
static void draw_wifi_icon(Imlib_Image canvas, int x, int y, int size, gfx_color_t color)
{
	imlib_context_set_image(canvas);
	set_imlib_color(color);

	// WiFi icon: 3 arcs above a dot
	int center_x = x + size / 2;
	int base_y = y + size - 4;
	int thickness = 2;

	// Base dot
	imlib_image_fill_rectangle(center_x - 2, base_y - 2, 4, 4);

	// Arc 1 (smallest, innermost)
	int arc_w = 10;
	int arc_h = 6;
	imlib_image_fill_rectangle(center_x - arc_w/2, base_y - arc_h - 2, arc_w, thickness);
	imlib_image_fill_rectangle(center_x - arc_w/2, base_y - arc_h - 2, thickness, arc_h/2);
	imlib_image_fill_rectangle(center_x + arc_w/2 - thickness, base_y - arc_h - 2, thickness, arc_h/2);

	// Arc 2 (medium)
	arc_w = 18;
	arc_h = 10;
	imlib_image_fill_rectangle(center_x - arc_w/2, base_y - arc_h - 6, arc_w, thickness);
	imlib_image_fill_rectangle(center_x - arc_w/2, base_y - arc_h - 6, thickness, arc_h/2);
	imlib_image_fill_rectangle(center_x + arc_w/2 - thickness, base_y - arc_h - 6, thickness, arc_h/2);

	// Arc 3 (largest, outermost)
	arc_w = 26;
	arc_h = 14;
	imlib_image_fill_rectangle(center_x - arc_w/2, base_y - arc_h - 10, arc_w, thickness);
	imlib_image_fill_rectangle(center_x - arc_w/2, base_y - arc_h - 10, thickness, arc_h/2);
	imlib_image_fill_rectangle(center_x + arc_w/2 - thickness, base_y - arc_h - 10, thickness, arc_h/2);
}

// Draw Ethernet icon (RJ45 plug shape)
static void draw_ethernet_icon(Imlib_Image canvas, int x, int y, int size, gfx_color_t color)
{
	imlib_context_set_image(canvas);
	set_imlib_color(color);

	int plug_w = (int)(size * 0.7f);
	int plug_h = (int)(size * 0.9f);
	int plug_x = x + (size - plug_w) / 2;
	int plug_y = y + (size - plug_h) / 2;
	int thickness = 2;

	// Main plug body outline
	imlib_image_fill_rectangle(plug_x, plug_y, plug_w, thickness);  // Top
	imlib_image_fill_rectangle(plug_x, plug_y + plug_h - thickness, plug_w, thickness);  // Bottom
	imlib_image_fill_rectangle(plug_x, plug_y, thickness, plug_h);  // Left
	imlib_image_fill_rectangle(plug_x + plug_w - thickness, plug_y, thickness, plug_h);  // Right

	// Connector pins (4 vertical lines inside)
	int pin_spacing = plug_w / 5;
	int pin_height = plug_h / 3;
	int pin_y = plug_y + plug_h / 4;
	for (int i = 1; i <= 4; i++)
	{
		int pin_x = plug_x + i * pin_spacing - 1;
		imlib_image_fill_rectangle(pin_x, pin_y, 2, pin_height);
	}

	// Cable coming out bottom
	int cable_w = plug_w / 3;
	int cable_x = plug_x + (plug_w - cable_w) / 2;
	imlib_image_fill_rectangle(cable_x, plug_y + plug_h, cable_w, 4);
}

// Draw controller/gamepad icon
static void draw_controller_icon(Imlib_Image canvas, int x, int y, int size, gfx_color_t color)
{
	imlib_context_set_image(canvas);
	set_imlib_color(color);

	int body_w = size;
	int body_h = (int)(size * 0.6f);
	int body_x = x;
	int body_y = y + (size - body_h) / 2;
	int thickness = 2;

	// Main body outline (rounded rectangle approximation)
	imlib_image_fill_rectangle(body_x + 3, body_y, body_w - 6, thickness);  // Top
	imlib_image_fill_rectangle(body_x + 3, body_y + body_h - thickness, body_w - 6, thickness);  // Bottom
	imlib_image_fill_rectangle(body_x, body_y + 3, thickness, body_h - 6);  // Left
	imlib_image_fill_rectangle(body_x + body_w - thickness, body_y + 3, thickness, body_h - 6);  // Right

	// Rounded corners
	imlib_image_fill_rectangle(body_x + 1, body_y + 1, 2, 2);
	imlib_image_fill_rectangle(body_x + body_w - 3, body_y + 1, 2, 2);
	imlib_image_fill_rectangle(body_x + 1, body_y + body_h - 3, 2, 2);
	imlib_image_fill_rectangle(body_x + body_w - 3, body_y + body_h - 3, 2, 2);

	// D-pad (left side)
	int dpad_x = body_x + 5;
	int dpad_y = body_y + body_h / 2 - 2;
	imlib_image_fill_rectangle(dpad_x, dpad_y, 6, 4);      // Horizontal
	imlib_image_fill_rectangle(dpad_x + 1, dpad_y - 2, 4, 8);  // Vertical

	// Buttons (right side, 2 dots)
	int btn_x = body_x + body_w - 10;
	int btn_y = body_y + body_h / 2 - 3;
	imlib_image_fill_rectangle(btn_x, btn_y, 3, 3);
	imlib_image_fill_rectangle(btn_x + 4, btn_y + 2, 3, 3);
}

// Render header bar with title and status icons
static void render_header(Imlib_Image canvas)
{
	gfx_theme_t *theme = menu_state.theme;
	int width = fb_width;

	// Header background
	gfx_rect_t header_rect = { 0, 0, width, HEADER_HEIGHT };
	gfx_color_t header_bg = theme->colors.panel_bg;
	header_bg.a = 200;  // Slightly transparent
	draw_filled_rect(canvas, header_rect, header_bg);

	// Bottom border
	gfx_rect_t border_rect = { 0, HEADER_HEIGHT - 2, width, 2 };
	draw_filled_rect(canvas, border_rect, theme->colors.panel_border);

	if (menu_state.title[0])
	{
		gfx_draw_text(canvas, menu_state.title, 20, 18, theme->colors.text_primary);
	}

	int icon_y = 15;
	int icon_size = 30;

	// Network status icon (right side of header, left of Zaparoo)
	int net_icon_x = width - 120;
	char *eth_ip = getNet(1);   // Check ethernet
	char *wifi_ip = getNet(2);  // Check wifi
	gfx_color_t net_color;

	if (wifi_ip)
	{
		// WiFi connected - green
		net_color = gfx_color_rgba(80, 220, 120, 255);
		draw_wifi_icon(canvas, net_icon_x, icon_y, icon_size, net_color);
	}
	else if (eth_ip)
	{
		// Ethernet connected - blue
		net_color = gfx_color_rgba(100, 150, 255, 255);
		draw_ethernet_icon(canvas, net_icon_x, icon_y, icon_size, net_color);
	}
	else
	{
		// No network - gray
		net_color = gfx_color_rgba(80, 80, 80, 100);
		draw_wifi_icon(canvas, net_icon_x, icon_y, icon_size, net_color);
	}

	// Zaparoo NFC status icon (rightmost)
	int nfc_icon_x = width - 70;

	zaparoo_status_t zap_status = zaparoo_get_status();
	gfx_color_t icon_color;

	switch (zap_status)
	{
	case ZAPAROO_DISCONNECTED:
		// Dark gray with low opacity - no reader connected
		icon_color = gfx_color_rgba(80, 80, 80, 100);
		break;
	case ZAPAROO_IDLE:
		// Cyan/teal - reader connected and ready
		icon_color = gfx_color_rgba(80, 180, 220, 255);
		break;
	case ZAPAROO_SCANNING:
		// Yellow/orange pulse - actively scanning
		icon_color = gfx_color_rgba(255, 200, 80, 255);
		break;
	case ZAPAROO_CARD_DETECTED:
		// Bright green - card detected successfully
		icon_color = gfx_color_rgba(80, 220, 120, 255);
		break;
	default:
		icon_color = gfx_color_rgba(80, 80, 80, 100);
		break;
	}

	draw_nfc_icon(canvas, nfc_icon_x, icon_y, icon_size, icon_color);
}

// External function for controller names
extern const char* get_player_controller_name(int player);

// Render footer bar with controls hint and controller icons
static void render_footer(Imlib_Image canvas)
{
	gfx_theme_t *theme = menu_state.theme;
	int width = fb_width;
	int height = fb_height;

	// Footer background (fully opaque to cover any overlapping content)
	gfx_rect_t footer_rect = { 0, height - FOOTER_HEIGHT, width, FOOTER_HEIGHT };
	gfx_color_t footer_bg = theme->colors.panel_bg;
	footer_bg.a = 255;
	draw_filled_rect(canvas, footer_rect, footer_bg);

	// Top border
	gfx_rect_t border_rect = { 0, height - FOOTER_HEIGHT, width, 2 };
	draw_filled_rect(canvas, border_rect, theme->colors.panel_border);

	// Controls hint text varies by mode - simplified for grid-only UI
	const char *controls;
	switch (menu_state.mode)
	{
		case GFX_MODE_SYSTEMS:
			controls = "[A] Select  [Sel] Settings  [X] Refresh";
			break;
		case GFX_MODE_GAMES:
			controls = "[A] Preview  [B] Back  [Sel] Settings  [Y] Fav";
			break;
		case GFX_MODE_PREVIEW:
			controls = "[A] Launch  [B] Back  [Y] Fav";
			break;
		case GFX_MODE_SETTINGS:
			controls = "[A] Toggle  [B] Close";
			break;
		default:
			controls = "[A] Select  [B] Back";
			break;
	}

	int text_height = 16; // 8 * 2 (scale factor)
	int text_y = height - FOOTER_HEIGHT + (FOOTER_HEIGHT - text_height) / 2;
	int text_x = 20; // Left padding

	// Use primary text color for visibility
	gfx_draw_text(canvas, controls, text_x, text_y, theme->colors.text_primary);

	// Controller icons on right side
	int icon_size = 24;
	int icon_spacing = 45;
	int icon_x = width - 40;  // Start from right
	int icon_y = height - FOOTER_HEIGHT + (FOOTER_HEIGHT - icon_size) / 2;

	// Show connected controllers (players 1-4)
	for (int player = 4; player >= 1; player--)
	{
		const char *controller_name = get_player_controller_name(player);
		if (controller_name && controller_name[0])
		{
			// Draw controller icon
			draw_controller_icon(canvas, icon_x - icon_size, icon_y, icon_size, theme->colors.text_primary);

			// Draw player number
			char num[4];
			snprintf(num, sizeof(num), "P%d", player);
			gfx_draw_text(canvas, num, icon_x - icon_size - 32, icon_y + 4, theme->colors.text_highlight);

			icon_x -= icon_spacing;
		}
	}
}

// Render scrollbar
static void render_scrollbar(Imlib_Image canvas, gfx_rect_t bounds)
{
	if (menu_state.item_count <= menu_state.visible_count) return;

	gfx_theme_t *theme = menu_state.theme;

	// Scrollbar background
	gfx_rect_t sb_bg = { bounds.x + bounds.w - SCROLLBAR_WIDTH, bounds.y,
	                     SCROLLBAR_WIDTH, bounds.h };
	draw_filled_rect(canvas, sb_bg, theme->colors.scrollbar_bg);

	// Calculate thumb size and position
	float visible_ratio = (float)menu_state.visible_count / (float)menu_state.item_count;
	int thumb_height = (int)(bounds.h * visible_ratio);
	if (thumb_height < 30) thumb_height = 30;

	float scroll_ratio = (float)menu_state.scroll_offset /
	                     (float)(menu_state.item_count - menu_state.visible_count);
	int thumb_y = bounds.y + (int)((bounds.h - thumb_height) * scroll_ratio);

	gfx_rect_t thumb = { sb_bg.x + 2, thumb_y, SCROLLBAR_WIDTH - 4, thumb_height };
	draw_filled_rect(canvas, thumb, theme->colors.scrollbar_fg);
}

// Render systems grid (system selection screen)
static void render_systems_grid(Imlib_Image canvas)
{
	gfx_theme_t *theme = menu_state.theme;
	int panel_padding = theme->panel_padding;

	// Grid parameters for system selection - fewer, larger cells
	int content_y = HEADER_HEIGHT + panel_padding;
	int content_height = fb_height - HEADER_HEIGHT - FOOTER_HEIGHT - panel_padding * 2;
	int content_width = fb_width - panel_padding * 2;

	// Larger cells for system icons (4 columns)
	int target_cols = 4;
	int cell_spacing = (int)(fb_width * 0.012f);  // ~24px at 1920
	if (cell_spacing < 16) cell_spacing = 16;

	int cell_size = (content_width - (target_cols + 1) * cell_spacing) / target_cols;
	int cols = target_cols;

	int rows_visible = content_height / (cell_size + cell_spacing);
	if (rows_visible < 1) rows_visible = 1;
	menu_state.visible_count = cols * rows_visible;

	int x = panel_padding + cell_spacing;
	int y = content_y;
	int col = 0;

	for (int i = 0; i < menu_state.visible_count && (menu_state.scroll_offset + i) < menu_state.item_count; i++)
	{
		int item_idx = menu_state.scroll_offset + i;
		gfx_menu_item_t *item = &items_storage[item_idx];

		gfx_rect_t cell = { x, y, cell_size, cell_size };

		// Selection highlight
		if (item_idx == menu_state.selected_index)
		{
			gfx_rect_t sel_rect = { cell.x - 6, cell.y - 6, cell.w + 12, cell.h + 12 };
			draw_filled_rect(canvas, sel_rect, theme->colors.selection_bg);
			draw_rect_border(canvas, sel_rect, theme->colors.selection_border, 4);
		}

		// Cell background
		draw_filled_rect(canvas, cell, theme->colors.panel_bg);
		draw_rect_border(canvas, cell, theme->colors.panel_border, 2);

		// System icon/thumbnail (if available)
		Imlib_Image thumb = item->thumbnail;
		if (thumb)
		{
			imlib_context_set_image(thumb);
			int src_w = imlib_image_get_width();
			int src_h = imlib_image_get_height();

			imlib_context_set_image(canvas);
			imlib_context_set_blend(1);
			imlib_blend_image_onto_image(thumb, 1,
				0, 0, src_w, src_h,
				cell.x + 10, cell.y + 10, cell.w - 20, cell.h - 50);
		}

		// System name bar at bottom
		gfx_rect_t name_bar = { cell.x, cell.y + cell.h - 36, cell.w, 36 };
		gfx_color_t name_bg = theme->colors.panel_border;
		name_bg.a = 220;
		draw_filled_rect(canvas, name_bar, name_bg);

		// System name text (centered, with truncation for long names)
		gfx_color_t name_color = (item_idx == menu_state.selected_index) ?
		                         theme->colors.text_highlight : theme->colors.text_primary;
		int text_max_width = cell.w - 16;  // Padding on both sides
		gfx_draw_text_truncated(canvas, item->name, cell.x + 8, cell.y + cell.h - 28, text_max_width, name_color);

		// Move to next cell
		col++;
		x += cell_size + cell_spacing;
		if (col >= cols)
		{
			col = 0;
			x = panel_padding + cell_spacing;
			y += cell_size + cell_spacing;
		}
	}
}

// Run a script in background
static void run_script(const char *script_path)
{
	char cmd[512];
	snprintf(cmd, sizeof(cmd), "%s &", script_path);
	system(cmd);
}

// Render settings menu
static void render_settings_menu(Imlib_Image canvas)
{
	gfx_theme_t *theme = menu_state.theme;
	
	// Center the settings menu
	int menu_width = 500;
	int menu_height = 420;
	int menu_x = (fb_width - menu_width) / 2;
	int menu_y = (fb_height - menu_height) / 2;
	
	// Dark overlay
	gfx_rect_t overlay = { 0, 0, fb_width, fb_height };
	gfx_color_t overlay_color = { 180, 0, 0, 0 };  // Semi-transparent black
	draw_filled_rect(canvas, overlay, overlay_color);
	
	// Menu panel
	gfx_rect_t panel = { menu_x, menu_y, menu_width, menu_height };
	draw_filled_rect(canvas, panel, theme->colors.panel_bg);
	draw_rect_border(canvas, panel, theme->colors.panel_border, 3);
	
	// Title
	gfx_draw_text(canvas, "Settings", menu_x + 20, menu_y + 20, theme->colors.text_primary);
	
	// Settings items - with theme selection
	const char *labels[] = {
		"Theme",
		"Boxart",
		"Animations", 
		"Rescan Library",
		"Run Update All",
		"WiFi Setup",
		"Back"
	};
	
	// Build values array with theme name
	char theme_value[64];
	snprintf(theme_value, sizeof(theme_value), "< %s >", theme_names[current_theme_index]);
	
	const char *values[SETTINGS_COUNT];
	values[0] = theme_value;
	values[1] = cfg.boxart_enable ? "ON" : "OFF";
	values[2] = cfg.anim_enable ? "ON" : "OFF";
	values[3] = "";
	values[4] = "";
	values[5] = "";
	values[6] = "";
	
	int item_height = 48;
	int item_y = menu_y + 60;
	
	for (int i = 0; i < SETTINGS_COUNT; i++)
	{
		gfx_rect_t item_rect = { menu_x + 20, item_y, menu_width - 40, item_height - 10 };
		
		// Highlight selected item
		if (i == settings_selected)
		{
			draw_filled_rect(canvas, item_rect, theme->colors.selection_bg);
			draw_rect_border(canvas, item_rect, theme->colors.selection_border, 2);
		}
		
		// Label
		gfx_draw_text(canvas, labels[i], menu_x + 30, item_y + 12, theme->colors.text_primary);
		
		// Value (right-aligned)
		if (values[i][0])
		{
			int val_x = menu_x + menu_width - 180;
			gfx_color_t val_color;
			if (i == 0) {
				// Theme name - use highlight color
				val_color = theme->colors.text_highlight;
			} else if (strcmp(values[i], "ON") == 0) {
				val_color = gfx_color_rgba(100, 255, 100, 255);
			} else if (strcmp(values[i], "OFF") == 0) {
				val_color = gfx_color_rgba(255, 100, 100, 255);
			} else {
				val_color = theme->colors.text_secondary;
			}
			gfx_draw_text(canvas, values[i], val_x, item_y + 12, val_color);
		}
		
		item_y += item_height;
	}
	
	// Instructions
	gfx_draw_text(canvas, "[A] Select  [B] Close  [</>] Change", menu_x + 20, menu_y + menu_height - 35, theme->colors.text_secondary);
}

// Render full-screen game details overlay
static void render_game_details_overlay(Imlib_Image canvas)
{
	gfx_theme_t *theme = menu_state.theme;
	
	// Dark background overlay
	gfx_rect_t overlay = { 0, 0, fb_width, fb_height };
	gfx_color_t overlay_color = { 230, 0, 0, 0 };  // Semi-transparent black
	draw_filled_rect(canvas, overlay, overlay_color);
	
	// Get selected item
	gfx_menu_item_t *selected = gfx_menu_get_selected_item();
	if (!selected) return;
	
	// Layout: Left side = large boxart, Right side = info
	int content_y = HEADER_HEIGHT + 30;
	int content_height = fb_height - HEADER_HEIGHT - FOOTER_HEIGHT - 60;
	int content_width = fb_width - 60;
	int content_x = 30;
	
	// Boxart area (left 45%)
	int boxart_area_width = (int)(content_width * 0.45f);
	int boxart_x = content_x;
	
	// Info area (right 50%)
	int info_x = content_x + boxart_area_width + 40;
	int info_width = content_width - boxart_area_width - 40;
	
	// Render large boxart
	Imlib_Image boxart = boxart_get_preview_image();
	if (boxart)
	{
		imlib_context_set_image(boxart);
		int src_w = imlib_image_get_width();
		int src_h = imlib_image_get_height();
		
		// Calculate scaled size maintaining aspect ratio
		int max_w = boxart_area_width - 20;
		int max_h = content_height - 40;
		
		float scale_x = (float)max_w / (float)src_w;
		float scale_y = (float)max_h / (float)src_h;
		float scale = (scale_x < scale_y) ? scale_x : scale_y;
		
		int dst_w = (int)(src_w * scale);
		int dst_h = (int)(src_h * scale);
		int dst_x = boxart_x + (boxart_area_width - dst_w) / 2;
		int dst_y = content_y + (content_height - dst_h) / 2;
		
		// Shadow
		gfx_rect_t shadow = { dst_x + 6, dst_y + 6, dst_w, dst_h };
		draw_filled_rect(canvas, shadow, gfx_color_hex(0x60000000));
		
		// Blend boxart
		imlib_context_set_image(canvas);
		imlib_context_set_blend(1);
		imlib_blend_image_onto_image(boxart, 1,
			0, 0, src_w, src_h,
			dst_x, dst_y, dst_w, dst_h);
		
		// Border
		gfx_rect_t border = { dst_x - 2, dst_y - 2, dst_w + 4, dst_h + 4 };
		draw_rect_border(canvas, border, theme->colors.selection_border, 3);
	}
	else
	{
		// No boxart placeholder
		gfx_rect_t placeholder = { boxart_x + 20, content_y + 40, boxart_area_width - 40, content_height - 80 };
		draw_filled_rect(canvas, placeholder, theme->colors.panel_bg);
		draw_rect_border(canvas, placeholder, theme->colors.panel_border, 2);
		
		int text_x = placeholder.x + (placeholder.w - 12 * 16) / 2;
		gfx_draw_text(canvas, "No Artwork", text_x, placeholder.y + placeholder.h / 2, theme->colors.text_secondary);
	}
	
	// Right side: Game info
	int info_y = content_y;
	int line_height = 24;
	
	// Game title (large)
	gfx_draw_text(canvas, selected->name, info_x, info_y, theme->colors.text_highlight);
	info_y += line_height + 20;
	
	// Separator line
	gfx_rect_t sep1 = { info_x, info_y, info_width, 2 };
	draw_filled_rect(canvas, sep1, theme->colors.panel_border);
	info_y += 20;
	
	// Try to get game metadata from database
	gamedb_entry_t *game_info = NULL;
	if (cfg.gamedb_enable && selected->path[0])
	{
		game_info = gamedb_lookup_filename(selected->path);
	}
	
	// Developer / Year
	if (game_info && (game_info->developer[0] || game_info->year > 0))
	{
		char info_line[256] = "";
		if (game_info->developer[0])
		{
			snprintf(info_line, sizeof(info_line), "Developer: %s", game_info->developer);
			gfx_draw_text(canvas, info_line, info_x, info_y, theme->colors.text_primary);
			info_y += line_height;
		}
		if (game_info->year > 0)
		{
			snprintf(info_line, sizeof(info_line), "Year: %d", game_info->year);
			gfx_draw_text(canvas, info_line, info_x, info_y, theme->colors.text_primary);
			info_y += line_height;
		}
		info_y += 10;
	}
	
	// Genre
	if (game_info && game_info->genre != GENRE_UNKNOWN)
	{
		char genre_line[256];
		snprintf(genre_line, sizeof(genre_line), "Genre: %s", gamedb_genre_name(game_info->genre));
		gfx_draw_text(canvas, genre_line, info_x, info_y, theme->colors.text_primary);
		info_y += line_height + 10;
	}
	
	// Description
	if (game_info && game_info->description[0])
	{
		gfx_draw_text(canvas, "Description:", info_x, info_y, theme->colors.text_secondary);
		info_y += line_height;
		
		// Word-wrap description (simple approach: truncate per line)
		const char *desc = game_info->description;
		int max_chars = info_width / 16;
		int lines_drawn = 0;
		int max_lines = 6;
		
		while (*desc && lines_drawn < max_lines)
		{
			char line_buf[256];
			int i = 0;
			while (*desc && i < max_chars - 1 && i < 255)
			{
				if (*desc == '\n') { desc++; break; }
				line_buf[i++] = *desc++;
			}
			line_buf[i] = '\0';
			
			// Try to break at word boundary
			if (*desc && i == max_chars - 1)
			{
				int j = i - 1;
				while (j > 0 && line_buf[j] != ' ') j--;
				if (j > 0)
				{
					desc -= (i - j - 1);
					line_buf[j] = '\0';
				}
			}
			
			gfx_draw_text(canvas, line_buf, info_x, info_y, theme->colors.text_primary);
			info_y += line_height - 4;
			lines_drawn++;
		}
		
		if (*desc)
		{
			gfx_draw_text(canvas, "...", info_x, info_y, theme->colors.text_secondary);
		}
	}
	else
	{
		// No description available
		gfx_draw_text(canvas, "No description available", info_x, info_y, theme->colors.text_secondary);
	}
	
	// Bottom: Launch hint
	int hint_y = fb_height - FOOTER_HEIGHT - 50;
	gfx_rect_t hint_bg = { info_x - 10, hint_y - 10, info_width + 20, 40 };
	gfx_color_t hint_bg_color = theme->colors.selection_bg;
	hint_bg_color.a = 200;
	draw_filled_rect(canvas, hint_bg, hint_bg_color);
	draw_rect_border(canvas, hint_bg, theme->colors.selection_border, 2);
	
	gfx_draw_text(canvas, "Press [A] to Launch Game", info_x + 10, hint_y, theme->colors.text_highlight);
	
	// Favorite indicator
	if (selected->is_favorite)
	{
		gfx_color_t star_color = gfx_color_rgba(255, 215, 0, 255);  // Gold
		gfx_draw_text(canvas, "* Favorite", info_x + info_width - 150, content_y, star_color);
	}
	
	// Render header and footer
	render_header(canvas);
	render_footer(canvas);
}

// Render grid view (games)
static void render_grid_view(Imlib_Image canvas)
{
	gfx_theme_t *theme = menu_state.theme;
	int panel_padding = theme->panel_padding;

	// Grid parameters - dynamically sized for resolution
	// Target: 6 columns at 1080p, scale proportionally
	int content_y = HEADER_HEIGHT + panel_padding;
	int content_height = fb_height - HEADER_HEIGHT - FOOTER_HEIGHT - panel_padding * 2;
	int content_width = fb_width - panel_padding * 2;

	// Calculate cell size based on target columns (6 for 1080p)
	int target_cols = 6;
	int cell_spacing = (int)(fb_width * 0.006f);  // ~12px at 1920
	if (cell_spacing < 8) cell_spacing = 8;

	int cell_size = (content_width - (target_cols + 1) * cell_spacing) / target_cols;
	int cols = target_cols;

	int rows_visible = content_height / (cell_size + cell_spacing);
	if (rows_visible < 1) rows_visible = 1;
	menu_state.visible_count = cols * rows_visible;

	int x = panel_padding;
	int y = content_y;
	int col = 0;

	for (int i = 0; i < menu_state.visible_count && (menu_state.scroll_offset + i) < menu_state.item_count; i++)
	{
		int item_idx = menu_state.scroll_offset + i;
		gfx_menu_item_t *item = &items_storage[item_idx];

		gfx_rect_t cell = { x, y, cell_size, cell_size };

		// Selection highlight
		if (item_idx == menu_state.selected_index)
		{
			gfx_rect_t sel_rect = { cell.x - 4, cell.y - 4, cell.w + 8, cell.h + 8 };
			draw_filled_rect(canvas, sel_rect, theme->colors.selection_bg);
			draw_rect_border(canvas, sel_rect, theme->colors.selection_border, 3);
		}

		// Cell background
		draw_filled_rect(canvas, cell, theme->colors.panel_bg);

		// Thumbnail or placeholder boxart
		Imlib_Image thumb = item->thumbnail ? item->thumbnail : boxart_get_preview_image();
		if (thumb)
		{
			imlib_context_set_image(thumb);
			int src_w = imlib_image_get_width();
			int src_h = imlib_image_get_height();

			imlib_context_set_image(canvas);
			imlib_context_set_blend(1);
			imlib_blend_image_onto_image(thumb, 1,
				0, 0, src_w, src_h,
				cell.x, cell.y, cell.w, cell.h - 24);
		}

		// Favorite star indicator (top-right of cell)
		if (item->is_favorite)
		{
			// Draw a gold/yellow star in top-right corner
			int star_size = 20;
			int star_x = cell.x + cell.w - star_size - 4;
			int star_y = cell.y + 4;
			gfx_color_t star_color = gfx_color_rgba(255, 215, 0, 255);  // Gold
			
			// Simple star shape (filled square for now, could be actual star)
			gfx_rect_t star_bg = { star_x - 2, star_y - 2, star_size + 4, star_size + 4 };
			gfx_color_t star_bg_color = gfx_color_rgba(0, 0, 0, 150);
			draw_filled_rect(canvas, star_bg, star_bg_color);
			
			// Draw star character or simple shape
			gfx_draw_text(canvas, "*", star_x + 2, star_y, star_color);
		}

		// Name bar at bottom
		gfx_rect_t name_bar = { cell.x, cell.y + cell.h - 24, cell.w, 24 };
		gfx_color_t name_bg = theme->colors.panel_border;
		name_bg.a = 200;
		draw_filled_rect(canvas, name_bar, name_bg);

		// Game name text - use scrolling for selected item if text is long
		gfx_color_t name_color = (item_idx == menu_state.selected_index) ?
		                         theme->colors.text_highlight : theme->colors.text_primary;
		int is_selected = (item_idx == menu_state.selected_index);
		gfx_draw_text_scrolling(canvas, item->name, cell.x + 4, cell.y + cell.h - 20, cell.w - 8, name_color, is_selected);

		// Move to next cell
		col++;
		x += cell_size + cell_spacing;
		if (col >= cols)
		{
			col = 0;
			x = panel_padding;
			y += cell_size + cell_spacing;
		}
	}
}

// Render wheel/carousel view
static void render_wheel_view(Imlib_Image canvas)
{
	gfx_theme_t *theme = menu_state.theme;

	// Wheel parameters - dynamically sized for resolution
	// Scale based on screen height (reference: 720p)
	float scale = (float)fb_height / 720.0f;

	int center_x = fb_width / 2;
	int center_y = fb_height / 2 - (int)(40 * scale);  // Slightly above center
	int wheel_radius = (int)(280 * scale);              // Distance from center to items
	int item_size_center = (int)(280 * scale);          // Size of center (selected) item
	int item_size_side = (int)(100 * scale);            // Size of side items
	int visible_items = 7;                              // Number of visible items in wheel

	// Calculate positions for wheel items
	int half_visible = visible_items / 2;

	// Render back to front: outer items first, then inner, then center
	// This creates proper z-ordering (items behind render first)
	int render_order[] = { -3, 3, -2, 2, -1, 1, 0 }; // Outside to center
	int render_count = sizeof(render_order) / sizeof(render_order[0]);

	for (int r = 0; r < render_count; r++)
	{
		int offset = render_order[r];
		if (offset < -half_visible || offset > half_visible) continue;

		int item_idx = menu_state.selected_index + offset;

		// Wrap around
		if (item_idx < 0) item_idx += menu_state.item_count;
		if (item_idx >= menu_state.item_count) item_idx -= menu_state.item_count;

		if (menu_state.item_count == 0) break;
		if (item_idx < 0 || item_idx >= menu_state.item_count) continue;

		gfx_menu_item_t *item = &items_storage[item_idx];

		// Calculate position on wheel arc
		float angle = (float)offset * 0.35f;  // Spread angle
		float cos_a = cosf(angle);
		float sin_a = sinf(angle);

		// Position (arc layout)
		int x = center_x + (int)(sin_a * wheel_radius);
		int y = center_y + (int)((1.0f - cos_a) * wheel_radius * 0.3f);

		// Size based on distance from center
		float scale = 1.0f - fabsf((float)offset) * 0.15f;
		if (scale < 0.5f) scale = 0.5f;

		int item_size = (offset == 0) ? item_size_center : (int)(item_size_side * scale);
		if (offset == 0)
		{
			// Apply selection animation scale
			item_size = (int)(item_size * anim_selection_scale);
		}

		// Adjust position for item size
		x -= item_size / 2;
		y -= item_size / 2;

		// Alpha based on distance from center
		uint8_t alpha = 255;
		if (offset != 0)
		{
			alpha = (uint8_t)(255 * (1.0f - fabsf((float)offset) * 0.2f));
		}

		gfx_rect_t item_rect = { x, y, item_size, item_size };

		// Selection glow for center item
		if (offset == 0)
		{
			// Outer glow
			gfx_rect_t glow_rect = { x - 8, y - 8, item_size + 16, item_size + 16 };
			gfx_color_t glow = theme->colors.selection_border;
			glow.a = 100;
			draw_filled_rect(canvas, glow_rect, glow);

			// Selection border
			draw_rect_border(canvas, glow_rect, theme->colors.selection_border, 3);
		}

		// Item background
		gfx_color_t bg = theme->colors.panel_bg;
		bg.a = alpha;
		draw_filled_rect(canvas, item_rect, bg);

		// Thumbnail or placeholder boxart
		Imlib_Image thumb = item->thumbnail ? item->thumbnail : boxart_get_preview_image();
		if (thumb)
		{
			imlib_context_set_image(thumb);
			int src_w = imlib_image_get_width();
			int src_h = imlib_image_get_height();

			imlib_context_set_image(canvas);
			imlib_context_set_blend(1);
			imlib_blend_image_onto_image(thumb, 1,
				0, 0, src_w, src_h,
				item_rect.x, item_rect.y, item_rect.w, item_rect.h - 20);
		}

		// Name bar at bottom
		gfx_rect_t name_bar = { item_rect.x, item_rect.y + item_rect.h - 20,
		                        item_rect.w, 20 };
		gfx_color_t name_bg = theme->colors.panel_border;
		name_bg.a = (uint8_t)(200 * alpha / 255);
		draw_filled_rect(canvas, name_bar, name_bg);
	}

	// Title of selected item at bottom
	gfx_menu_item_t *selected = gfx_menu_get_selected_item();
	if (selected)
	{
		int title_y = center_y + wheel_radius / 2 + (int)(100 * scale);
		int title_w = (int)(400 * scale);
		int title_h = (int)(30 * scale);
		gfx_rect_t title_area = { center_x - title_w / 2, title_y, title_w, title_h };
		gfx_color_t title_bg = theme->colors.panel_bg;
		title_bg.a = 200;
		draw_filled_rect(canvas, title_area, title_bg);

		// Title text centered
		int text_width = strlen(selected->name) * 16; // 16 pixels per char
		int text_x = center_x - text_width / 2;
		if (text_x < title_area.x + 10) text_x = title_area.x + 10;
		gfx_draw_text_truncated(canvas, selected->name, text_x, title_y + 6, title_w - 20, theme->colors.text_highlight);
	}

	// Update visible count for page navigation
	menu_state.visible_count = visible_items;
}


// Render search overlay
static void render_search_overlay(Imlib_Image canvas)
{
	if (!search_is_active()) return;

	gfx_theme_t *theme = menu_state.theme;

	// Semi-transparent overlay
	gfx_rect_t overlay = { 0, 0, fb_width, fb_height };
	gfx_color_t overlay_color = gfx_color_hex(0xD0000000);
	draw_filled_rect(canvas, overlay, overlay_color);

	// Search box area
	int box_width = 600;
	int box_height = 400;
	int box_x = (fb_width - box_width) / 2;
	int box_y = (fb_height - box_height) / 2 - 50;

	gfx_rect_t search_box = { box_x, box_y, box_width, box_height };
	draw_filled_rect(canvas, search_box, theme->colors.panel_bg);
	draw_rect_border(canvas, search_box, theme->colors.panel_border, 2);

	// Search title bar
	gfx_rect_t title_bar = { box_x, box_y, box_width, 40 };
	gfx_color_t title_bg = theme->colors.panel_border;
	draw_filled_rect(canvas, title_bar, title_bg);

	// "Search" title placeholder
	gfx_rect_t title_text = { box_x + 20, box_y + 12, 80, 16 };
	draw_filled_rect(canvas, title_text, theme->colors.text_primary);

	// Query input field
	int input_y = box_y + 60;
	gfx_rect_t input_bg = { box_x + 20, input_y, box_width - 40, 36 };
	gfx_color_t input_color = gfx_color_hex(0xFF1a1a1a);
	draw_filled_rect(canvas, input_bg, input_color);
	draw_rect_border(canvas, input_bg, theme->colors.selection_border, 2);

	// Query text placeholder
	const char *query = search_get_query();
	if (query && query[0])
	{
		int query_width = strlen(query) * 10;
		if (query_width > box_width - 60) query_width = box_width - 60;
		gfx_rect_t query_text = { box_x + 30, input_y + 10, query_width, 16 };
		draw_filled_rect(canvas, query_text, theme->colors.text_primary);
	}

	// Blinking cursor
	static int cursor_blink = 0;
	cursor_blink = (cursor_blink + 1) % 60;
	if (cursor_blink < 30)
	{
		int cursor_x = box_x + 30 + (query ? strlen(query) * 10 : 0);
		gfx_rect_t cursor = { cursor_x, input_y + 8, 2, 20 };
		draw_filled_rect(canvas, cursor, theme->colors.text_highlight);
	}

	// Results area
	int results_y = input_y + 50;
	int results_height = box_height - 130;
	gfx_rect_t results_area = { box_x + 20, results_y, box_width - 40, results_height };
	gfx_color_t results_bg = gfx_color_hex(0x40000000);
	draw_filled_rect(canvas, results_area, results_bg);

	// Render search results
	int result_count = 0;
	search_result_t *results = search_get_results(&result_count);
	int max_visible = results_height / 32;
	int selected_idx = 0;
	search_result_t *selected = search_get_selected();

	// Find selected index
	for (int i = 0; i < result_count; i++)
	{
		if (&results[i] == selected)
		{
			selected_idx = i;
			break;
		}
	}

	// Calculate scroll offset for results
	int scroll_start = 0;
	if (selected_idx >= max_visible)
	{
		scroll_start = selected_idx - max_visible + 1;
	}

	int y = results_y + 4;
	for (int i = scroll_start; i < result_count && (i - scroll_start) < max_visible; i++)
	{
		gfx_rect_t result_rect = { box_x + 24, y, box_width - 48, 28 };

		// Highlight selected
		if (&results[i] == selected)
		{
			draw_filled_rect(canvas, result_rect, theme->colors.selection_bg);
			draw_rect_border(canvas, result_rect, theme->colors.selection_border, 1);
		}

		// Result name placeholder
		int name_width = strlen(results[i].name) * 7;
		if (name_width > result_rect.w - 20) name_width = result_rect.w - 20;
		gfx_rect_t name_text = { result_rect.x + 8, result_rect.y + 8, name_width, 12 };
		gfx_color_t text_col = (&results[i] == selected) ?
		                        theme->colors.text_highlight : theme->colors.text_primary;
		draw_filled_rect(canvas, name_text, text_col);

		y += 32;
	}

	// Result count indicator
	char count_str[32];
	snprintf(count_str, sizeof(count_str), "%d results", result_count);
	int count_width = strlen(count_str) * 7;
	gfx_rect_t count_text = { box_x + box_width - count_width - 30,
	                          box_y + box_height - 30, count_width, 14 };
	draw_filled_rect(canvas, count_text, theme->colors.text_secondary);

	// Virtual keyboard (if visible)
	if (search_keyboard_visible())
	{
		int kb_y = box_y + box_height + 20;
		int kb_width = 500;
		int kb_x = (fb_width - kb_width) / 2;
		int key_size = 40;
		int key_spacing = 8;

		gfx_rect_t kb_bg = { kb_x - 10, kb_y - 10, kb_width + 20, 200 };
		draw_filled_rect(canvas, kb_bg, theme->colors.panel_bg);
		draw_rect_border(canvas, kb_bg, theme->colors.panel_border, 2);

		search_keyboard_t *kb = search_get_keyboard();

		// Render keyboard rows
		const char* rows[] = { "1234567890", "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM" };
		int row_offsets[] = { 0, 20, 40, 70 };  // X offsets for each row

		for (int row = 0; row < 4; row++)
		{
			int row_len = strlen(rows[row]);
			int rx = kb_x + row_offsets[row];
			int ry = kb_y + row * (key_size + key_spacing);

			for (int col = 0; col < row_len; col++)
			{
				gfx_rect_t key = { rx + col * (key_size + key_spacing), ry,
				                   key_size, key_size };

				// Highlight current key
				if (row == kb->cursor_y && col == kb->cursor_x)
				{
					draw_filled_rect(canvas, key, theme->colors.selection_bg);
					draw_rect_border(canvas, key, theme->colors.selection_border, 2);
				}
				else
				{
					gfx_color_t key_bg = gfx_color_hex(0xFF2a2a2a);
					draw_filled_rect(canvas, key, key_bg);
				}

				// Key label placeholder
				gfx_rect_t label = { key.x + key.w/2 - 5, key.y + key.h/2 - 6, 10, 12 };
				draw_filled_rect(canvas, label, theme->colors.text_primary);
			}
		}

		// Special keys (Space, Backspace, Enter)
		int special_y = kb_y + 4 * (key_size + key_spacing);

		// Space bar
		gfx_rect_t space_key = { kb_x + 100, special_y, 200, key_size };
		gfx_color_t key_bg = gfx_color_hex(0xFF2a2a2a);
		draw_filled_rect(canvas, space_key, key_bg);

		// Backspace
		gfx_rect_t back_key = { kb_x + 320, special_y, 80, key_size };
		draw_filled_rect(canvas, back_key, key_bg);

		// Enter/Search
		gfx_rect_t enter_key = { kb_x + 410, special_y, 80, key_size };
		draw_filled_rect(canvas, enter_key, theme->colors.selection_bg);
	}
}

// Render Zaparoo card scan overlay
static void render_zaparoo_overlay(Imlib_Image canvas)
{
	if (!zaparoo_overlay_active()) return;

	gfx_theme_t *theme = menu_state.theme;
	zaparoo_overlay_t *overlay = zaparoo_get_overlay();

	// Full-screen semi-transparent background
	gfx_rect_t bg_overlay = { 0, 0, fb_width, fb_height };
	gfx_color_t bg_color = gfx_color_hex(0xE0000000);
	draw_filled_rect(canvas, bg_overlay, bg_color);

	// Dynamic scaling based on screen height (reference: 720p)
	float scale = (float)fb_height / 720.0f;

	// Center card dimensions - scaled for resolution
	int card_width = (int)(420 * scale);
	int card_height = (int)(520 * scale);
	int card_x = (fb_width - card_width) / 2;
	int card_y = (fb_height - card_height) / 2 - (int)(20 * scale);

	// Card background with selection border
	gfx_rect_t card = { card_x, card_y, card_width, card_height };
	draw_filled_rect(canvas, card, theme->colors.panel_bg);
	draw_rect_border(canvas, card, theme->colors.selection_border, (int)(3 * scale));

	// Header bar with "NFC DETECTED" or "ZAPAROO"
	int header_h = (int)(50 * scale);
	gfx_rect_t header = { card_x, card_y, card_width, header_h };
	draw_filled_rect(canvas, header, theme->colors.selection_bg);

	// NFC icon in header
	int icon_size = (int)(30 * scale);
	draw_nfc_icon(canvas, card_x + (int)(15 * scale), card_y + (int)(10 * scale), icon_size, theme->colors.text_highlight);

	// "ZAPAROO" title text
	gfx_draw_text(canvas, "ZAPAROO", card_x + (int)(55 * scale), card_y + (int)(17 * scale), theme->colors.text_highlight);

	// Boxart area (centered in card)
	int art_w = (int)(280 * scale);
	int art_h = (int)(280 * scale);
	int art_x = card_x + (card_width - art_w) / 2;
	int art_y = card_y + header_h + (int)(30 * scale);

	// Try to get boxart for the game
	Imlib_Image boxart = boxart_get_preview_image();
	gfx_rect_t art_rect = { art_x, art_y, art_w, art_h };

	if (boxart)
	{
		imlib_context_set_image(boxart);
		int src_w = imlib_image_get_width();
		int src_h = imlib_image_get_height();

		imlib_context_set_image(canvas);
		imlib_context_set_blend(1);
		imlib_blend_image_onto_image(boxart, 1,
			0, 0, src_w, src_h,
			art_x, art_y, art_w, art_h);
	}
	else
	{
		// Fallback placeholder
		gfx_color_t art_bg = gfx_color_hex(0xFF2a2a3a);
		draw_filled_rect(canvas, art_rect, art_bg);
	}
	draw_rect_border(canvas, art_rect, theme->colors.panel_border, (int)(2 * scale));

	// Game title area
	int title_y = art_y + art_h + (int)(25 * scale);
	const char *game_name = overlay->card.game_name;
	if (game_name[0])
	{
		// Draw game name text
		gfx_draw_text(canvas, game_name, card_x + (int)(20 * scale), title_y, theme->colors.text_primary);
	}
	else
	{
		// Placeholder
		gfx_rect_t name_placeholder = { card_x + (int)(30 * scale), title_y, (int)(250 * scale), (int)(20 * scale) };
		draw_filled_rect(canvas, name_placeholder, theme->colors.text_primary);
	}

	// Loading indicator / progress bar
	int progress_y = card_y + card_height - (int)(40 * scale);
	int progress_w = card_width - (int)(80 * scale);
	int progress_h = (int)(8 * scale);
	if (progress_h < 4) progress_h = 4;
	int progress_x = card_x + (int)(40 * scale);

	// Progress background
	gfx_rect_t progress_bg = { progress_x, progress_y, progress_w, progress_h };
	gfx_color_t progress_bg_color = gfx_color_hex(0xFF1a1a2a);
	draw_filled_rect(canvas, progress_bg, progress_bg_color);

	// Animated progress fill (simple pulse effect)
	static int progress_phase = 0;
	progress_phase = (progress_phase + 3) % 100;
	int fill_w = (progress_w * progress_phase) / 100;
	gfx_rect_t progress_fill = { progress_x, progress_y, fill_w, progress_h };
	draw_filled_rect(canvas, progress_fill, theme->colors.selection_border);

	// "Loading..." text
	int loading_text_x = card_x + (card_width - 10 * 16) / 2; // Center "Loading..." (10 chars * 16px)
	gfx_draw_text(canvas, "Loading...", loading_text_x, progress_y + progress_h + (int)(10 * scale), theme->colors.text_secondary);
}

// Main render function
void gfx_menu_render(void)
{
	static Imlib_Image render_buffer = NULL;
	static int last_width = 0, last_height = 0;

	if (!menu_state.enabled) return;

	// Check if we should show graphical menu or let OSD show
	// (e.g., when in System Settings, OSD should be visible)
	if (!menu_use_graphical())
	{
		// Let OSD show instead - disable our framebuffer
		video_fb_enable(0, 0);
		return;
	}

	// If framebuffer isn't ready yet, keep needs_redraw true so we try again later
	if (!fb_base || fb_width <= 0 || fb_height <= 0) {
		menu_state.needs_redraw = 1;  // Try again next frame
		return;
	}

	// Skip rendering if nothing changed - but don't use artificial timing
	if (!menu_state.needs_redraw) {
		video_fb_enable(1, gfx_fb_front);
		OsdDisable();
		return;
	}
	// Create or recreate render buffer if size changed
	if (!render_buffer || last_width != fb_width || last_height != fb_height) {
		if (render_buffer) {
			imlib_context_set_image(render_buffer);
			imlib_free_image();
		}
		render_buffer = imlib_create_image(fb_width, fb_height);
		if (!render_buffer) return;
		last_width = fb_width;
		last_height = fb_height;
		menu_state.needs_redraw = 1;  // Force redraw on size change
	}
	
	// Only do expensive re-render when content changed
	if (menu_state.needs_redraw)
	{
		Imlib_Image canvas = render_buffer;

		imlib_context_set_image(canvas);
		imlib_image_set_has_alpha(1);

		// Fill background
		gfx_theme_t *theme = menu_state.theme;
		gfx_rect_t full_screen = { 0, 0, fb_width, fb_height };
		draw_filled_rect(canvas, full_screen, theme->colors.background);

		// Render UI elements based on menu mode - simplified grid-only UI
		switch (menu_state.mode)
		{
			case GFX_MODE_SYSTEMS:
				render_systems_grid(canvas);
				break;
			case GFX_MODE_GAMES:
				render_grid_view(canvas);
				break;
			case GFX_MODE_PREVIEW:
				// Show full-screen game preview with boxart and details
				render_game_details_overlay(canvas);
				break;
			case GFX_MODE_SETTINGS:
				// Render underlying view first, then settings overlay
				render_grid_view(canvas);
				render_settings_menu(canvas);
				break;
			default:
				render_grid_view(canvas);
				break;
		}

		render_header(canvas);
		render_footer(canvas);

		// Render search overlay on top of everything
		render_search_overlay(canvas);

		// Render Zaparoo card scan overlay (topmost)
		render_zaparoo_overlay(canvas);

		// Copy rendered image to framebuffer (back buffer)
		imlib_context_set_image(canvas);
		uint32_t *src_data = imlib_image_get_data_for_reading_only();
		const int back = (gfx_fb_front == GFX_FB_A) ? GFX_FB_B : GFX_FB_A;
		volatile uint32_t *dst_data = fb_base + (FB_SIZE * back);
		if (src_data && dst_data) {
			memcpy((void*)dst_data, src_data, fb_width * fb_height * 4);
		}

		menu_state.needs_redraw = 0;
		
		// Wait for vsync before buffer swap to prevent tearing/flickering
		int fb_fd = open("/dev/fb0", O_RDWR);
		if (fb_fd >= 0) {
			int zero = 0;
			ioctl(fb_fd, FBIO_WAITFORVSYNC, &zero);
			close(fb_fd);
		}
		
		// Swap buffers
		video_fb_enable(1, back);
		gfx_fb_front = back;
	}
	else
	{
		// Just keep our buffer displayed (no re-render needed)
		video_fb_enable(1, gfx_fb_front);
	}
	
	OsdDisable();
}

// Save current graphical menu to PNG file for testing/preview
int gfx_menu_save_preview(const char *filename)
{
	if (!menu_state.enabled) return -1;

	// Create a test image if framebuffer not available
	int width = (fb_width > 0) ? fb_width : 1920;
	int height = (fb_height > 0) ? fb_height : 1080;

	Imlib_Image preview = imlib_create_image(width, height);
	if (!preview) return -2;

	imlib_context_set_image(preview);
	imlib_image_set_has_alpha(1);

	// Fill background
	gfx_theme_t *theme = menu_state.theme;
	gfx_rect_t full_screen = { 0, 0, width, height };
	draw_filled_rect(preview, full_screen, theme->colors.background);

	// Temporarily set dimensions for rendering
	int old_width = fb_width;
	int old_height = fb_height;
	fb_width = width;
	fb_height = height;

	// Render UI elements based on menu mode - simplified grid-only UI
	switch (menu_state.mode)
	{
		case GFX_MODE_SYSTEMS:
			render_systems_grid(preview);
			break;
		case GFX_MODE_GAMES:
		default:
			render_grid_view(preview);
			break;
	}

	render_header(preview);
	render_footer(preview);

	// Render overlays
	render_search_overlay(preview);
	render_zaparoo_overlay(preview);

	// Restore dimensions
	fb_width = old_width;
	fb_height = old_height;

	// Save to file
	imlib_context_set_image(preview);
	Imlib_Load_Error err;
	imlib_save_image_with_error_return(filename, &err);
	imlib_free_image();

	return (err == IMLIB_LOAD_ERROR_NONE) ? 0 : -3;
}

// Apply blurred boxart as background
static void apply_blur_background(Imlib_Image canvas, Imlib_Image boxart)
{
	if (!boxart) return;

	imlib_context_set_image(boxart);
	int src_w = imlib_image_get_width();
	int src_h = imlib_image_get_height();

	// Scale to fill screen
	imlib_context_set_image(canvas);
	imlib_context_set_blend(1);

	// Draw scaled boxart as background (will be dimmed by panel overlays)
	float scale_x = (float)fb_width / (float)src_w;
	float scale_y = (float)fb_height / (float)src_h;
	float scale = (scale_x > scale_y) ? scale_x : scale_y;  // Cover entire screen

	int dst_w = (int)(src_w * scale);
	int dst_h = (int)(src_h * scale);
	int dst_x = (fb_width - dst_w) / 2;
	int dst_y = (fb_height - dst_h) / 2;

	imlib_blend_image_onto_image(boxart, 0,
		0, 0, src_w, src_h,
		dst_x, dst_y, dst_w, dst_h);

	// Apply dark overlay for readability
	gfx_rect_t overlay = { 0, 0, fb_width, fb_height };
	gfx_color_t dark = gfx_color_hex(0xC0000000);
	draw_filled_rect(canvas, overlay, dark);
}

// Grid navigation helper - get number of columns based on current mode
static int get_grid_columns(void)
{
	// Systems grid uses larger cells (4 columns), games grid uses 6 columns
	if (menu_state.mode == GFX_MODE_SYSTEMS) return 4;
	return 6;  // Default for games grid
}

// Grid navigation: move left within grid
static void grid_move_left(void)
{
	if (menu_state.selected_index > 0)
	{
		menu_state.selected_index--;
		menu_state.needs_redraw = 1;
		
		// Adjust scroll if needed
		if (menu_state.selected_index < menu_state.scroll_offset)
		{
			int cols = get_grid_columns();
			menu_state.scroll_offset -= cols;
			if (menu_state.scroll_offset < 0) menu_state.scroll_offset = 0;
		}
	}
}

// Grid navigation: move right within grid
static void grid_move_right(void)
{
	if (menu_state.selected_index < menu_state.item_count - 1)
	{
		menu_state.selected_index++;
		menu_state.needs_redraw = 1;
		
		// Adjust scroll if needed
		int cols = get_grid_columns();
		int visible_end = menu_state.scroll_offset + menu_state.visible_count;
		if (menu_state.selected_index >= visible_end)
		{
			menu_state.scroll_offset += cols;
		}
	}
}

// Grid navigation: move up one row
static void grid_move_up(void)
{
	int cols = get_grid_columns();
	int new_index = menu_state.selected_index - cols;
	
	if (new_index >= 0)
	{
		menu_state.selected_index = new_index;
		menu_state.needs_redraw = 1;
		
		// Adjust scroll if needed
		if (menu_state.selected_index < menu_state.scroll_offset)
		{
			menu_state.scroll_offset -= cols;
			if (menu_state.scroll_offset < 0) menu_state.scroll_offset = 0;
		}
	}
}

// Grid navigation: move down one row
static void grid_move_down(void)
{
	int cols = get_grid_columns();
	int new_index = menu_state.selected_index + cols;
	
	if (new_index < menu_state.item_count)
	{
		menu_state.selected_index = new_index;
		menu_state.needs_redraw = 1;
		
		// Adjust scroll if needed
		int visible_end = menu_state.scroll_offset + menu_state.visible_count;
		if (menu_state.selected_index >= visible_end)
		{
			menu_state.scroll_offset += cols;
		}
	}
	else if (menu_state.item_count > 0)
	{
		// Move to last item in last row if we can't go a full row down
		int current_col = menu_state.selected_index % cols;
		int last_row_start = ((menu_state.item_count - 1) / cols) * cols;
		int target = last_row_start + current_col;
		if (target >= menu_state.item_count) target = menu_state.item_count - 1;
		
		if (target != menu_state.selected_index)
		{
			menu_state.selected_index = target;
			menu_state.needs_redraw = 1;
			
			// Adjust scroll if needed
			int visible_end = menu_state.scroll_offset + menu_state.visible_count;
			while (menu_state.selected_index >= visible_end)
			{
				menu_state.scroll_offset += cols;
				visible_end = menu_state.scroll_offset + menu_state.visible_count;
			}
		}
	}
}

// Grid navigation: page up (L shoulder - skip one visible page)
static void grid_page_up(void)
{
	int cols = get_grid_columns();
	int page_items = menu_state.visible_count;
	if (page_items < cols) page_items = cols;
	
	int new_index = menu_state.selected_index - page_items;
	if (new_index < 0) new_index = 0;
	
	if (new_index != menu_state.selected_index)
	{
		menu_state.selected_index = new_index;
		
		// Adjust scroll to show new selection
		if (menu_state.selected_index < menu_state.scroll_offset)
		{
			menu_state.scroll_offset = (menu_state.selected_index / cols) * cols;
			if (menu_state.scroll_offset < 0) menu_state.scroll_offset = 0;
		}
		menu_state.needs_redraw = 1;
	}
}

// Grid navigation: page down (R shoulder - skip one visible page)
static void grid_page_down(void)
{
	int cols = get_grid_columns();
	int page_items = menu_state.visible_count;
	if (page_items < cols) page_items = cols;
	
	int new_index = menu_state.selected_index + page_items;
	if (new_index >= menu_state.item_count) new_index = menu_state.item_count - 1;
	if (new_index < 0) new_index = 0;
	
	if (new_index != menu_state.selected_index)
	{
		menu_state.selected_index = new_index;
		
		// Adjust scroll to show new selection
		int visible_end = menu_state.scroll_offset + menu_state.visible_count;
		while (menu_state.selected_index >= visible_end && visible_end < menu_state.item_count)
		{
			menu_state.scroll_offset += cols;
			visible_end = menu_state.scroll_offset + menu_state.visible_count;
		}
		menu_state.needs_redraw = 1;
	}
}

// Toggle favorite status for current selection
static void toggle_favorite(void)
{
	if (menu_state.selected_index >= 0 && menu_state.selected_index < menu_state.item_count)
	{
		gfx_menu_item_t *item = &items_storage[menu_state.selected_index];
		item->is_favorite = !item->is_favorite;
		menu_state.needs_redraw = 1;
		
		// TODO: Persist favorite status to disk
		// This would call a function like: favorites_save(item->path, item->is_favorite);
	}
}

// Toggle favorites filter (show only favorites)
static void toggle_favorites_filter(void)
{
	menu_state.show_favorites_only = !menu_state.show_favorites_only;
	menu_state.selected_index = 0;
	menu_state.scroll_offset = 0;
	menu_state.needs_redraw = 1;
	
	// Update title to indicate filter status
	if (menu_state.show_favorites_only)
	{
		char new_title[256];
		snprintf(new_title, sizeof(new_title), "%s [Favorites]", menu_state.current_system);
		gfx_menu_set_title(new_title);
	}
	else
	{
		gfx_menu_set_title(menu_state.current_system);
	}
}

// Request library refresh
static void request_refresh(void)
{
	menu_state.refresh_requested = 1;
	menu_state.needs_redraw = 1;
}

// Public: Check if showing favorites only
int gfx_menu_is_favorites_filter(void)
{
	return menu_state.show_favorites_only;
}

// Public: Check and clear refresh request flag
int gfx_menu_check_refresh_request(void)
{
	if (menu_state.refresh_requested)
	{
		menu_state.refresh_requested = 0;
		return 1;
	}
	return 0;
}

// Handle input and return 1 if consumed - simplified grid-only UI
int gfx_menu_handle_input(int key)
{
	if (!menu_state.enabled) return 0;

	// Define key codes (Linux keycodes used by menu system)
	#define KEY_UP_LOCAL     103   // KEY_UP
	#define KEY_DOWN_LOCAL   108   // KEY_DOWN
	#define KEY_LEFT_LOCAL   105   // KEY_LEFT
	#define KEY_RIGHT_LOCAL  106   // KEY_RIGHT
	#define KEY_ENTER_LOCAL  28    // KEY_ENTER (A button)
	#define KEY_ESC_LOCAL    1     // KEY_ESC
	#define KEY_BACK_LOCAL   158   // KEY_BACK (B button)
	#define KEY_BACKSPACE_LOCAL 14 // KEY_BACKSPACE
	#define KEY_PGUP_LOCAL   104   // KEY_PAGEUP (L shoulder)
	#define KEY_PGDN_LOCAL   109   // KEY_PAGEDOWN (R shoulder)
	#define KEY_Y_LOCAL      21    // KEY_Y
	#define KEY_X_LOCAL      45    // KEY_X
	#define KEY_TAB_LOCAL    15    // KEY_TAB (Select button)
	#define KEY_GRAVE_LOCAL  41    // KEY_GRAVE (Select on controller)

	// Handle input based on current mode
	switch (menu_state.mode)
	{
		case GFX_MODE_SYSTEMS:
			// System selection grid - proper 2D navigation
			switch (key)
			{
				case KEY_UP_LOCAL:
					grid_move_up();
					return 1;
				case KEY_DOWN_LOCAL:
					grid_move_down();
					return 1;
				case KEY_LEFT_LOCAL:
					grid_move_left();
					return 1;
				case KEY_RIGHT_LOCAL:
					grid_move_right();
					return 1;
				case KEY_PGUP_LOCAL:
					grid_page_up();
					return 1;
				case KEY_PGDN_LOCAL:
					grid_page_down();
					return 1;
				case KEY_ENTER_LOCAL:
					// A button: select system and show games
					if (menu_state.selected_index >= 0 &&
					    menu_state.selected_index < menu_state.item_count)
					{
						gfx_menu_item_t *item = &items_storage[menu_state.selected_index];
						gfx_menu_show_games(item->name);
						return 1;
					}
					break;
				case KEY_X_LOCAL:
					// X button: refresh library
					request_refresh();
					return 1;
				case KEY_TAB_LOCAL:
				case KEY_GRAVE_LOCAL:
					// Select/Tab button: open settings menu
					settings_selected = 0;
					settings_previous_mode = menu_state.mode;
					menu_state.mode = GFX_MODE_SETTINGS;
					menu_state.needs_redraw = 1;
					return 1;
				case KEY_ESC_LOCAL:
				case KEY_BACK_LOCAL:
				case KEY_BACKSPACE_LOCAL:
					// B button in systems view: consume but don't exit
					// We're already at the top level, don't go back to wallpaper
					return 1;
				default:
					break;
			}
			break;

		case GFX_MODE_GAMES:
			// Game selection grid - proper 2D navigation
			switch (key)
			{
				case KEY_UP_LOCAL:
					grid_move_up();
					return 1;
				case KEY_DOWN_LOCAL:
					grid_move_down();
					return 1;
				case KEY_LEFT_LOCAL:
					grid_move_left();
					return 1;
				case KEY_RIGHT_LOCAL:
					grid_move_right();
					return 1;
				case KEY_PGUP_LOCAL:
					grid_page_up();
					return 1;
				case KEY_PGDN_LOCAL:
					grid_page_down();
					return 1;
				case KEY_Y_LOCAL:
					// Y button: toggle favorite
					toggle_favorite();
					return 1;
				case KEY_X_LOCAL:
					// X button: refresh library
					request_refresh();
					return 1;
				case KEY_TAB_LOCAL:
				case KEY_GRAVE_LOCAL:
					// Select/Tab button: open settings menu
					settings_selected = 0;
					settings_previous_mode = menu_state.mode;
					menu_state.mode = GFX_MODE_SETTINGS;
					menu_state.needs_redraw = 1;
					return 1;
				case KEY_ESC_LOCAL:
				case KEY_BACK_LOCAL:
				case KEY_BACKSPACE_LOCAL:
					// B button: go back to system selection
					menu_state.show_favorites_only = 0;  // Clear filter when going back
					gfx_menu_show_systems();
					return 1;
				case KEY_ENTER_LOCAL:
					// A button: launch game directly (no preview step)
					// Disable graphical menu so core takes over
					gfx_menu_set_enabled(0);
					return 0;  // Let menu.cpp process the file selection
				default:
					break;
			}
			break;
			
		case GFX_MODE_PREVIEW:
			// Game preview mode - shows details, second A launches
			switch (key)
			{
				case KEY_ENTER_LOCAL:
					// A button: actually launch the game
					// Disable graphical menu so old MiSTer OSD takes over after core loads
					gfx_menu_set_enabled(0);
					menu_state.mode = GFX_MODE_GAMES;
					return 0;  // Let menu.cpp process the launch
				case KEY_ESC_LOCAL:
				case KEY_BACK_LOCAL:
				case KEY_BACKSPACE_LOCAL:
					// B button: go back to games grid
					menu_state.mode = GFX_MODE_GAMES;
					menu_state.needs_redraw = 1;
					return 1;
				case KEY_Y_LOCAL:
					// Y button: toggle favorite
					toggle_favorite();
					return 1;
				case KEY_TAB_LOCAL:
				case KEY_GRAVE_LOCAL:
					// Select/Tab button: open settings menu
					settings_selected = 0;
					settings_previous_mode = GFX_MODE_GAMES;  // Return to games, not preview
					menu_state.mode = GFX_MODE_SETTINGS;
					menu_state.needs_redraw = 1;
					return 1;
				default:
					// Consume other keys
					return 1;
			}
			break;
			
		case GFX_MODE_SETTINGS:
			// Settings menu navigation
			switch (key)
			{
				case KEY_UP_LOCAL:
					settings_selected--;
					if (settings_selected < 0) settings_selected = SETTINGS_COUNT - 1;
					menu_state.needs_redraw = 1;
					return 1;
				case KEY_DOWN_LOCAL:
					settings_selected++;
					if (settings_selected >= SETTINGS_COUNT) settings_selected = 0;
					menu_state.needs_redraw = 1;
					return 1;
				case KEY_LEFT_LOCAL:
					// Left: cycle theme backward (only on theme row)
					if (settings_selected == 0) {
						current_theme_index--;
						if (current_theme_index < 0) current_theme_index = THEME_COUNT - 1;
						apply_theme(current_theme_index);
					}
					return 1;
				case KEY_RIGHT_LOCAL:
					// Right: cycle theme forward (only on theme row)
					if (settings_selected == 0) {
						current_theme_index++;
						if (current_theme_index >= THEME_COUNT) current_theme_index = 0;
						apply_theme(current_theme_index);
					}
					return 1;
				case KEY_ENTER_LOCAL:
					// A button: toggle setting or execute action
					switch (settings_selected)
					{
						case 0: // Theme - cycle forward on A press too
							current_theme_index++;
							if (current_theme_index >= THEME_COUNT) current_theme_index = 0;
							apply_theme(current_theme_index);
							break;
						case 1: // Boxart
							cfg.boxart_enable = !cfg.boxart_enable;
							break;
						case 2: // Animations
							cfg.anim_enable = !cfg.anim_enable;
							break;
						case 3: // Rescan Library
							request_refresh();
							menu_state.mode = settings_previous_mode;
							break;
						case 4: // Run Update All
							run_script("/media/fat/Scripts/update_all.sh");
							menu_state.mode = settings_previous_mode;
							break;
						case 5: // WiFi Setup
							run_script("/media/fat/Scripts/wifi.sh");
							menu_state.mode = settings_previous_mode;
							break;
						case 6: // Back
							menu_state.mode = settings_previous_mode;
							break;
					}
					menu_state.needs_redraw = 1;
					return 1;
				case KEY_ESC_LOCAL:
				case KEY_BACK_LOCAL:
				case KEY_BACKSPACE_LOCAL:
				case KEY_TAB_LOCAL:
				case KEY_GRAVE_LOCAL:
					// B, Backspace, Tab, or Grave: close settings
					menu_state.mode = settings_previous_mode;
					menu_state.needs_redraw = 1;
					return 1;
				default:
					// Consume all other keys to prevent background interaction
					return 1;
			}
			break;
			
		default:
			break;
	}

	return 0;
}

void gfx_draw_text(Imlib_Image img, const char *text, int x, int y, gfx_color_t color)
{
	// Render using MiSTer's built-in 8x8 bitmap font (charfont), scaled 2x for readability.
	if (!text || !img) return;

	const int scale = 2;

	imlib_context_set_image(img);
	int w = imlib_image_get_width();
	int h = imlib_image_get_height();
	if (w <= 0 || h <= 0) return;

	uint32_t *data = (uint32_t*)imlib_image_get_data();
	if (!data) return;

	const uint32_t pix = ((uint32_t)color.a << 24) | ((uint32_t)color.r << 16) | ((uint32_t)color.g << 8) | (uint32_t)color.b;
	int cx = x;

	for (const unsigned char *p = (const unsigned char*)text; *p; ++p)
	{
		unsigned char ch = *p;
		if (ch == '\n')
		{
			cx = x;
			y += 8 * scale + 2;
			continue;
		}

		// simple tab
		if (ch == '\t')
		{
			cx += 4 * 8 * scale;
			continue;
		}

		// charfont stores columns, not rows: charfont[ch][col] has bits for rows 0-7
		for (int col = 0; col < 8; col++)
		{
			unsigned char bits = charfont[ch][col];
			for (int row = 0; row < 8; row++)
			{
				if (bits & (1 << row))  // bit N = row N
				{
					int px0 = cx + col * scale;
					int py0 = y + row * scale;
					for (int sy = 0; sy < scale; sy++)
					{
						int py = py0 + sy;
						if (py < 0 || py >= h) continue;
						uint32_t *rowp = data + py * w;
						for (int sx = 0; sx < scale; sx++)
						{
							int px = px0 + sx;
							if (px < 0 || px >= w) continue;
							rowp[px] = pix;
						}
					}
				}
			}
		}

		cx += 8 * scale;
	}

	imlib_image_put_back_data((DATA32*)data);
}

//// Core Settings Menu Rendering ////

#include "core_settings.h"

// Render the unified core settings menu
void gfx_menu_render_settings(void)
{
	if (!core_settings_is_loaded()) return;

	settings_menu_state_t *state = core_settings_get_menu_state();
	if (!state || !state->profile) return;

	gfx_theme_t *theme = menu_state.theme;

	// Create canvas from framebuffer
	Imlib_Image canvas = imlib_create_image_using_data(fb_width, fb_height,
		(uint32_t*)(fb_base + (1920*1080 * 1)));
	if (!canvas) return;

	imlib_context_set_image(canvas);
	imlib_image_set_has_alpha(1);

	// Semi-transparent overlay
	gfx_rect_t overlay = { 0, 0, fb_width, fb_height };
	gfx_color_t overlay_color = gfx_color_hex(0xE0101020);
	draw_filled_rect(canvas, overlay, overlay_color);

	// Settings panel
	int panel_width = 700;
	int panel_height = fb_height - 120;
	int panel_x = (fb_width - panel_width) / 2;
	int panel_y = 60;

	gfx_rect_t panel = { panel_x, panel_y, panel_width, panel_height };
	draw_filled_rect(canvas, panel, theme->colors.panel_bg);
	draw_rect_border(canvas, panel, theme->colors.panel_border, 2);

	// Core name header
	core_profile_t *profile = state->profile;
	gfx_rect_t header = { panel_x, panel_y, panel_width, 50 };
	gfx_color_t header_bg = theme->colors.panel_border;
	draw_filled_rect(canvas, header, header_bg);

	// Core name text placeholder
	int name_width = strlen(profile->core_name) * 10;
	if (name_width > panel_width - 40) name_width = panel_width - 40;
	gfx_rect_t core_name = { panel_x + 20, panel_y + 15, name_width, 20 };
	draw_filled_rect(canvas, core_name, theme->colors.text_primary);

	// Category tabs
	int tabs_y = panel_y + 60;
	gfx_menu_render_settings_category_tabs(panel_x, tabs_y, panel_width);

	// Settings list area
	int list_y = tabs_y + 50;
	int list_height = panel_height - 130;
	gfx_menu_render_settings_list(panel_x + 10, list_y, panel_width - 20, list_height);

	// Footer with controls hint
	int footer_y = panel_y + panel_height - 40;
	gfx_rect_t footer = { panel_x, footer_y, panel_width, 40 };
	gfx_color_t footer_bg = theme->colors.panel_border;
	footer_bg.a = 150;
	draw_filled_rect(canvas, footer, footer_bg);

	// Controls hint placeholder
	gfx_rect_t hint = { panel_x + 20, footer_y + 12, 300, 16 };
	draw_filled_rect(canvas, hint, theme->colors.text_secondary);
}

// Render category tabs at top of settings menu
void gfx_menu_render_settings_category_tabs(int x, int y, int width)
{
	settings_menu_state_t *state = core_settings_get_menu_state();
	gfx_theme_t *theme = menu_state.theme;

	Imlib_Image canvas = imlib_context_get_image();

	int tab_width = width / SETTING_CAT_COUNT;
	int tab_height = 40;

	for (int i = 0; i < SETTING_CAT_COUNT; i++)
	{
		gfx_rect_t tab = { x + i * tab_width, y, tab_width - 2, tab_height };

		// Highlight current category
		if ((int)state->current_category == i)
		{
			draw_filled_rect(canvas, tab, theme->colors.selection_bg);
			gfx_rect_t indicator = { tab.x, y + tab_height - 3, tab.w, 3 };
			draw_filled_rect(canvas, indicator, theme->colors.selection_border);
		}
		else
		{
			gfx_color_t tab_bg = theme->colors.panel_bg;
			tab_bg.a = 100;
			draw_filled_rect(canvas, tab, tab_bg);
		}

		// Category name placeholder
		const char *cat_name = core_settings_category_name((setting_category_t)i);
		int name_width = strlen(cat_name) * 7;
		gfx_rect_t name_rect = { tab.x + (tab.w - name_width) / 2, y + 12, name_width, 14 };
		gfx_color_t text_col = ((int)state->current_category == i) ?
		                        theme->colors.text_highlight : theme->colors.text_secondary;
		draw_filled_rect(canvas, name_rect, text_col);
	}
}

// Render settings list for current category
void gfx_menu_render_settings_list(int x, int y, int width, int height)
{
	settings_menu_state_t *state = core_settings_get_menu_state();
	gfx_theme_t *theme = menu_state.theme;

	Imlib_Image canvas = imlib_context_get_image();

	// Settings list background
	gfx_rect_t list_bg = { x, y, width, height };
	gfx_color_t bg = gfx_color_hex(0x40000000);
	draw_filled_rect(canvas, list_bg, bg);

	// Calculate visible items
	int item_height = 44;
	state->visible_count = height / item_height;

	// Collect settings for current category
	core_setting_t *cat_settings[CORE_SETTINGS_MAX];
	int cat_count = core_settings_get_by_category(state->current_category,
	                                               cat_settings, CORE_SETTINGS_MAX);

	// Calculate scroll offset
	int selected_in_cat = 0;
	for (int i = 0; i < cat_count; i++)
	{
		if (cat_settings[i] == core_settings_get_by_index(state->selected_index))
		{
			selected_in_cat = i;
			break;
		}
	}

	int scroll_offset = 0;
	if (selected_in_cat >= state->visible_count)
	{
		scroll_offset = selected_in_cat - state->visible_count + 1;
	}

	// Render settings
	int render_y = y + 4;
	for (int i = scroll_offset; i < cat_count && (i - scroll_offset) < state->visible_count; i++)
	{
		int selected = (cat_settings[i] == core_settings_get_by_index(state->selected_index));
		gfx_menu_render_setting_item(x + 8, render_y, width - 16, cat_settings[i], selected);
		render_y += item_height;
	}

	// Scrollbar (if needed)
	if (cat_count > state->visible_count)
	{
		float visible_ratio = (float)state->visible_count / (float)cat_count;
		int thumb_height = (int)(height * visible_ratio);
		if (thumb_height < 30) thumb_height = 30;

		float scroll_ratio = (float)scroll_offset / (float)(cat_count - state->visible_count);
		int thumb_y = y + (int)((height - thumb_height) * scroll_ratio);

		gfx_rect_t sb_bg = { x + width - 8, y, 6, height };
		draw_filled_rect(canvas, sb_bg, theme->colors.scrollbar_bg);

		gfx_rect_t thumb = { x + width - 7, thumb_y, 4, thumb_height };
		draw_filled_rect(canvas, thumb, theme->colors.scrollbar_fg);
	}
}

// Render individual setting item
void gfx_menu_render_setting_item(int x, int y, int width, void *setting_ptr, int selected)
{
	core_setting_t *setting = (core_setting_t*)setting_ptr;
	if (!setting) return;

	gfx_theme_t *theme = menu_state.theme;
	Imlib_Image canvas = imlib_context_get_image();

	gfx_rect_t item_rect = { x, y, width, 40 };

	// Handle separators
	if (setting->type == SETTING_TYPE_SEPARATOR)
	{
		// Separator bar
		gfx_rect_t sep_bar = { x + 10, y + 18, width - 20, 2 };
		gfx_color_t sep_color = theme->colors.panel_border;
		draw_filled_rect(canvas, sep_bar, sep_color);

		// Separator label
		int label_width = strlen(setting->name) * 7;
		gfx_rect_t label_bg = { x + 20, y + 8, label_width + 20, 20 };
		draw_filled_rect(canvas, label_bg, theme->colors.panel_bg);

		gfx_rect_t label = { x + 30, y + 12, label_width, 12 };
		draw_filled_rect(canvas, label, theme->colors.text_secondary);
		return;
	}

	// Selection highlight
	if (selected)
	{
		draw_filled_rect(canvas, item_rect, theme->colors.selection_bg);
		draw_rect_border(canvas, item_rect, theme->colors.selection_border, 2);
	}

	// Setting name
	int name_width = strlen(setting->name) * 8;
	if (name_width > width / 2 - 20) name_width = width / 2 - 20;
	gfx_rect_t name_rect = { x + 12, y + 12, name_width, 16 };
	gfx_color_t text_col = selected ? theme->colors.text_highlight : theme->colors.text_primary;
	draw_filled_rect(canvas, name_rect, text_col);

	// Value display (right side)
	int value_x = x + width - 200;
	int value_y = y + 10;

	switch (setting->type)
	{
		case SETTING_TYPE_TOGGLE:
		{
			// Toggle switch
			gfx_rect_t switch_bg = { value_x + 120, value_y, 50, 24 };
			gfx_color_t switch_color = setting->value ?
			                           theme->colors.selection_border :
			                           theme->colors.panel_border;
			draw_filled_rect(canvas, switch_bg, switch_color);

			// Toggle knob
			int knob_x = setting->value ? value_x + 146 : value_x + 122;
			gfx_rect_t knob = { knob_x, value_y + 2, 20, 20 };
			draw_filled_rect(canvas, knob, theme->colors.text_primary);

			// On/Off text
			const char *val_str = setting->value ? "On" : "Off";
			int val_width = strlen(val_str) * 8;
			gfx_rect_t val_text = { value_x + 90 - val_width, value_y + 4, val_width, 14 };
			draw_filled_rect(canvas, val_text, theme->colors.text_secondary);
			break;
		}

		case SETTING_TYPE_CHOICE:
		{
			// Choice dropdown indicator
			gfx_rect_t choice_bg = { value_x, value_y, 170, 24 };
			gfx_color_t choice_color = theme->colors.panel_border;
			draw_filled_rect(canvas, choice_bg, choice_color);

			// Current option name
			const char *opt_name = core_settings_get_option_name(setting);
			if (opt_name)
			{
				int opt_width = strlen(opt_name) * 7;
				if (opt_width > 140) opt_width = 140;
				gfx_rect_t opt_text = { value_x + 8, value_y + 5, opt_width, 14 };
				draw_filled_rect(canvas, opt_text, theme->colors.text_primary);
			}

			// Arrow indicators
			gfx_rect_t left_arrow = { value_x - 20, value_y + 6, 12, 12 };
			gfx_rect_t right_arrow = { value_x + 175, value_y + 6, 12, 12 };
			draw_filled_rect(canvas, left_arrow, theme->colors.text_secondary);
			draw_filled_rect(canvas, right_arrow, theme->colors.text_secondary);
			break;
		}

		case SETTING_TYPE_RANGE:
		{
			// Progress bar background
			gfx_rect_t bar_bg = { value_x, value_y + 8, 120, 8 };
			gfx_color_t bar_color = theme->colors.panel_border;
			draw_filled_rect(canvas, bar_bg, bar_color);

			// Progress bar fill
			float progress = (float)(setting->value - setting->min_value) /
			                 (float)(setting->max_value - setting->min_value);
			int fill_width = (int)(118 * progress);
			gfx_rect_t bar_fill = { value_x + 1, value_y + 9, fill_width, 6 };
			draw_filled_rect(canvas, bar_fill, theme->colors.selection_border);

			// Value text
			char val_str[32];
			core_settings_format_value(setting, val_str, sizeof(val_str));
			int val_width = strlen(val_str) * 7;
			gfx_rect_t val_text = { value_x + 130, value_y + 4, val_width, 14 };
			draw_filled_rect(canvas, val_text, theme->colors.text_primary);
			break;
		}

		case SETTING_TYPE_ACTION:
		{
			// Action button
			gfx_rect_t button = { value_x + 60, value_y, 110, 24 };
			draw_filled_rect(canvas, button, theme->colors.panel_border);
			if (selected)
			{
				draw_rect_border(canvas, button, theme->colors.selection_border, 2);
			}

			// Button text placeholder
			gfx_rect_t btn_text = { button.x + 20, button.y + 5, 70, 14 };
			draw_filled_rect(canvas, btn_text, theme->colors.text_primary);
			break;
		}

		default:
			break;
	}

	// Readonly indicator
	if (setting->readonly)
	{
		gfx_rect_t lock = { x + width - 220, y + 14, 12, 12 };
		draw_filled_rect(canvas, lock, theme->colors.text_secondary);
	}

	// Requires restart indicator
	if (setting->requires_restart)
	{
		gfx_rect_t restart = { x + width - 235, y + 14, 8, 8 };
		draw_filled_rect(canvas, restart, theme->colors.text_highlight);
	}
}
