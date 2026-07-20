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
static void render_list_view(Imlib_Image canvas);
static void render_grid_view(Imlib_Image canvas);
static void render_wheel_view(Imlib_Image canvas);
static void render_game_details(Imlib_Image canvas);
static void render_home_screen(Imlib_Image canvas);
static void render_search_overlay(Imlib_Image canvas);
static void render_zaparoo_overlay(Imlib_Image canvas);
static void render_header(Imlib_Image canvas);
static void render_footer(Imlib_Image canvas);
static void render_preview_panel(Imlib_Image canvas);
static void render_scrollbar(Imlib_Image canvas, gfx_rect_t bounds);
static void apply_blur_background(Imlib_Image canvas, Imlib_Image boxart);
static void draw_nfc_icon(Imlib_Image canvas, int x, int y, int size, gfx_color_t color);

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

// Helper: Draw text with word wrapping over multiple lines
// Returns number of lines rendered
static int gfx_draw_text_wrapped(Imlib_Image img, const char *text, int x, int y, int max_width, int max_lines, int line_height, gfx_color_t color)
{
	if (!text || !img || max_width <= 0 || max_lines <= 0) return 0;

	const int char_width = 16; // 8 * 2 (scale factor)
	int max_chars_per_line = max_width / char_width;
	if (max_chars_per_line < 1) return 0;

	int lines_rendered = 0;
	int current_y = y;
	const char *ptr = text;
	char line_buf[256];

	while (*ptr && lines_rendered < max_lines)
	{
		// Find end of current line (word wrap)
		int line_len = 0;
		int last_space = -1;
		const char *scan = ptr;

		while (*scan && line_len < max_chars_per_line && *scan != '\n')
		{
			if (*scan == ' ') last_space = line_len;
			scan++;
			line_len++;
		}

		// If we hit a newline or end of string, use that length
		if (*scan == '\n' || *scan == '\0')
		{
			// Use exact length
		}
		else if (last_space > 0 && line_len == max_chars_per_line)
		{
			// Word wrap at last space
			line_len = last_space;
		}

		// Copy line to buffer
		int copy_len = (line_len < 255) ? line_len : 255;
		strncpy(line_buf, ptr, copy_len);
		line_buf[copy_len] = '\0';

		// Draw this line (truncate if on last line and more text remains)
		if (lines_rendered == max_lines - 1 && ptr[line_len] != '\0' && ptr[line_len] != '\n')
		{
			// Last line with more text - add ellipsis
			if (copy_len > 3) {
				line_buf[copy_len - 3] = '.';
				line_buf[copy_len - 2] = '.';
				line_buf[copy_len - 1] = '.';
			}
		}

		gfx_draw_text(img, line_buf, x, current_y, color);
		lines_rendered++;
		current_y += line_height;

		// Move to next line
		ptr += line_len;
		if (*ptr == '\n') ptr++; // Skip newline
		while (*ptr == ' ') ptr++; // Skip leading spaces
	}

	return lines_rendered;
}

// Initialize default theme with Analogue-inspired minimalist style
static void init_default_theme(void)
{
	memset(&default_theme, 0, sizeof(default_theme));
	strcpy(default_theme.name, "Analogue Dark");

	// Analogue-inspired color palette - clean, minimalist dark theme
	default_theme.colors.background = gfx_color_hex(0xFF222222);      // Clean dark gray
	default_theme.colors.panel_bg = gfx_color_hex(0xE0181818);        // Subtle darker panel
	default_theme.colors.panel_border = gfx_color_hex(0xFF333333);    // Subtle border
	default_theme.colors.text_primary = gfx_color_hex(0xFFcccccc);    // High contrast light gray
	default_theme.colors.text_secondary = gfx_color_hex(0xFF888888);  // Muted gray
	default_theme.colors.text_highlight = gfx_color_hex(0xFFffffff);  // Pure white for emphasis
	default_theme.colors.selection_bg = gfx_color_hex(0x30ffffff);    // Subtle white selection
	default_theme.colors.selection_border = gfx_color_hex(0xFFcccccc); // Clean light border
	default_theme.colors.scrollbar_bg = gfx_color_hex(0x20ffffff);    // Very subtle
	default_theme.colors.scrollbar_fg = gfx_color_hex(0xFFcccccc);    // Visible but not harsh

	// Font settings - larger for better readability
	strcpy(default_theme.font_name, "");  // Use default
	default_theme.font_size_title = 28;
	default_theme.font_size_item = 20;
	default_theme.font_size_info = 16;

	// Layout settings - more generous spacing
	default_theme.thumbnail_width = 100;
	default_theme.thumbnail_height = 100;
	default_theme.item_spacing = 12;
	default_theme.panel_padding = 20;
	default_theme.corner_radius = 4;  // Subtle rounded corners
	default_theme.background_image = NULL;
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
	menu_state.view_type = GFX_VIEW_LIST;
	menu_state.enabled = 0;  // Disabled by default, use classic OSD
	menu_state.needs_redraw = 1;

	// Initialize animation state
	anim_scroll_offset = 0.0f;
	anim_selection_y = 0.0f;
	anim_selection_scale = 1.0f;
	anim_preview_opacity = 1.0f;
	scroll_anim_id = 0;
	selection_anim_id = 0;
	preview_fade_id = 0;
	selection_pulse_id = 0;

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

void gfx_menu_set_breadcrumb(const char *breadcrumb)
{
	if (breadcrumb)
	{
		strncpy(menu_state.breadcrumb, breadcrumb, sizeof(menu_state.breadcrumb) - 1);
		menu_state.breadcrumb[sizeof(menu_state.breadcrumb) - 1] = '\0';
	}
	else
	{
		menu_state.breadcrumb[0] = '\0';
	}
	menu_state.needs_redraw = 1;
}

void gfx_menu_clear_items(void)
{
	for (int i = 0; i < menu_state.item_count; i++)
	{
		if (items_storage[i].thumbnail)
		{
			// Each item owns a private clone (see gfx_menu_set_item_thumbnail),
			// so free it here rather than leaving it dangling.
			imlib_context_set_image(items_storage[i].thumbnail);
			imlib_free_image();
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
	if (index < 0 || index >= menu_state.item_count) return;

	gfx_menu_item_t *item = &items_storage[index];

	// Free any previously-owned clone before replacing it.
	if (item->thumbnail)
	{
		imlib_context_set_image(item->thumbnail);
		imlib_free_image();
		item->thumbnail = NULL;
	}

	// Own a private clone. The source handle belongs to the boxart cache, which
	// may evict and free it at any time; without a clone the item would be left
	// pointing at freed memory and the render path would use-after-free.
	if (thumbnail)
	{
		imlib_context_set_image(thumbnail);
		item->thumbnail = imlib_clone_image();
	}

	menu_state.needs_redraw = 1;
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

	// Animate selection scale (subtle pop effect)
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

	// Animate selection scale (subtle pop effect)
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

	// Create smooth scroll animation using animator system
	scroll_anim_id = anim_create_to(&anim_scroll_offset, (float)target_offset,
	                                SCROLL_ANIM_DURATION, EASE_OUT_CUBIC);
	if (scroll_anim_id)
	{
		anim_start(scroll_anim_id);
	}

	// Also update immediate value for logic
	menu_state.scroll_offset = target_offset;
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

void gfx_menu_set_view(gfx_view_type_t view)
{
	if (view < GFX_VIEW_COUNT)
	{
		menu_state.view_type = view;
		menu_state.needs_redraw = 1;
	}
}

gfx_view_type_t gfx_menu_get_view(void)
{
	return menu_state.view_type;
}

void gfx_menu_cycle_view(void)
{
	menu_state.view_type = (gfx_view_type_t)((menu_state.view_type + 1) % GFX_VIEW_COUNT);
	menu_state.needs_redraw = 1;
}

// Menu mode (screen) management
void gfx_menu_set_mode(gfx_menu_mode_t mode)
{
	if (mode < GFX_MODE_COUNT)
	{
		menu_state.mode = mode;
		menu_state.needs_redraw = 1;
	}
}

gfx_menu_mode_t gfx_menu_get_mode(void)
{
	return menu_state.mode;
}

void gfx_menu_show_details(int item_index)
{
	if (item_index >= 0 && item_index < menu_state.item_count)
	{
		menu_state.details_item_index = item_index;
		menu_state.mode = GFX_MODE_DETAILS;
		menu_state.needs_redraw = 1;
	}
}

void gfx_menu_show_home(void)
{
	menu_state.mode = GFX_MODE_HOME;
	gfx_menu_set_title("Home");
	menu_state.needs_redraw = 1;
}

void gfx_menu_show_browse(void)
{
	menu_state.mode = GFX_MODE_BROWSE;
	menu_state.needs_redraw = 1;
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

// Render header bar with title
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

	// Zaparoo NFC status icon (right side of header)
	int icon_x = width - 70;
	int icon_y = 15;
	int icon_size = 30;

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

	draw_nfc_icon(canvas, icon_x, icon_y, icon_size, icon_color);
}

// Render footer bar with controls hint
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

	// Controls hint text varies by mode
	// Font is 8x8 scaled 2x = 16px per char, footer is 50px tall
	const char *controls;
	switch (menu_state.mode)
	{
		case GFX_MODE_HOME:
			controls = "[A] Select  [B] Browse Cores  [D-Pad] Navigate";
			break;
		case GFX_MODE_DETAILS:
			controls = "[A] Play  [B] Back  [Y] Favorite";
			break;
		case GFX_MODE_BROWSE:
		default:
			controls = "[A] Details  [B] Back  [D-Pad] Navigate  [Select] View";
			break;
	}

	int text_height = 16; // 8 * 2 (scale factor)
	int text_y = height - FOOTER_HEIGHT + (FOOTER_HEIGHT - text_height) / 2;
	int text_x = 20; // Left padding

	// Use primary text color for visibility
	gfx_draw_text(canvas, controls, text_x, text_y, theme->colors.text_primary);
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

// Render list view (main view mode)
static void render_list_view(Imlib_Image canvas)
{
	gfx_theme_t *theme = menu_state.theme;
	int panel_padding = theme->panel_padding;
	int item_spacing = theme->item_spacing;
	int thumb_size = theme->thumbnail_height;

	// Left panel bounds (file list)
	int list_width = (int)(fb_width * LIST_PANEL_WIDTH_RATIO);
	gfx_rect_t list_bounds = {
		panel_padding,
		HEADER_HEIGHT + panel_padding,
		list_width - panel_padding * 2,
		fb_height - HEADER_HEIGHT - FOOTER_HEIGHT - panel_padding * 2
	};

	// List panel background
	gfx_rect_t panel_rect = { 0, HEADER_HEIGHT, list_width, fb_height - HEADER_HEIGHT - FOOTER_HEIGHT };
	draw_filled_rect(canvas, panel_rect, theme->colors.panel_bg);

	// Calculate visible items
	int item_height = thumb_size + item_spacing;
	menu_state.visible_count = list_bounds.h / item_height;

	// Use animated scroll offset for smooth scrolling
	int display_scroll = (int)anim_scroll_offset;
	float scroll_frac = anim_scroll_offset - (float)display_scroll;
	int y_offset = (int)(scroll_frac * item_height);

	// Render items (render one extra for smooth scrolling)
	int y = list_bounds.y - y_offset;
	int items_to_render = menu_state.visible_count + 1;
	for (int i = 0; i < items_to_render && (display_scroll + i) < menu_state.item_count; i++)
	{
		int item_idx = display_scroll + i;
		if (item_idx < 0) continue;
		gfx_menu_item_t *item = &items_storage[item_idx];

		gfx_rect_t item_rect = { list_bounds.x, y, list_bounds.w - SCROLLBAR_WIDTH - 4, thumb_size };

		// Skip items outside visible bounds
		if (item_rect.y + item_rect.h < list_bounds.y || item_rect.y > list_bounds.y + list_bounds.h)
		{
			y += item_height;
			continue;
		}

		// Selection highlight with animated scale
		if (item_idx == menu_state.selected_index)
		{
			// Apply scale animation to selection box
			int scale_offset = (int)((1.0f - anim_selection_scale) * item_rect.w * 0.5f);
			gfx_rect_t sel_rect = {
				item_rect.x - scale_offset,
				item_rect.y - (int)((1.0f - anim_selection_scale) * item_rect.h * 0.5f),
				item_rect.w + scale_offset * 2,
				item_rect.h + (int)((1.0f - anim_selection_scale) * item_rect.h)
			};
			draw_filled_rect(canvas, sel_rect, theme->colors.selection_bg);
			draw_rect_border(canvas, sel_rect, theme->colors.selection_border, 2);
		}

		// Item type indicator (folder icon for folders/back only)
		if (item->type == GFX_ITEM_FOLDER || item->type == GFX_ITEM_BACK)
		{
			gfx_rect_t folder_icon = { item_rect.x + 8, item_rect.y + thumb_size/2 - 8, 16, 16 };
			draw_filled_rect(canvas, folder_icon, theme->colors.text_secondary);
		}

		// Favorite star indicator
		if (item->is_favorite)
		{
			gfx_rect_t star = { item_rect.x + item_rect.w - 24, item_rect.y + 8, 16, 16 };
			draw_filled_rect(canvas, star, theme->colors.text_highlight);
		}

		// Name text (truncated to fit within list panel)
		gfx_color_t text_color = (item_idx == menu_state.selected_index) ?
		                         theme->colors.text_highlight : theme->colors.text_primary;
		int text_x = item_rect.x + 10; // No thumbnail, start text closer to left
		int text_y = item_rect.y + (thumb_size - 16) / 2; // Vertically center text
		int text_max_width = list_width - text_x - SCROLLBAR_WIDTH - 10; // Leave room for scrollbar
		gfx_draw_text_truncated(canvas, item->name, text_x, text_y, text_max_width, text_color);

		y += item_height;
	}

	// Scrollbar
	render_scrollbar(canvas, list_bounds);
}

// Render preview panel (right side with large boxart and game details)
// Inspired by Analogue 3D Library view
static void render_preview_panel(Imlib_Image canvas)
{
	gfx_theme_t *theme = menu_state.theme;
	int panel_padding = theme->panel_padding;

	int list_width = (int)(fb_width * LIST_PANEL_WIDTH_RATIO);
	int preview_x = list_width + panel_padding;
	int preview_width = fb_width - list_width - panel_padding * 2;
	int preview_height = fb_height - HEADER_HEIGHT - FOOTER_HEIGHT - panel_padding * 2;

	gfx_rect_t preview_bounds = {
		preview_x,
		HEADER_HEIGHT + panel_padding,
		preview_width,
		preview_height
	};

	// Preview panel background with animated opacity
	gfx_rect_t panel_bg = { list_width, HEADER_HEIGHT, fb_width - list_width, fb_height - HEADER_HEIGHT - FOOTER_HEIGHT };
	gfx_color_t bg = theme->colors.panel_bg;
	bg.a = (uint8_t)(180 * anim_preview_opacity);
	draw_filled_rect(canvas, panel_bg, bg);

	// Get selected item
	gfx_menu_item_t *selected = gfx_menu_get_selected_item();
	if (!selected) return;

	// For folders, show folder info
	if (selected->type == GFX_ITEM_FOLDER || selected->type == GFX_ITEM_BACK)
	{
		// Folder icon placeholder
		int icon_size = 80;
		gfx_rect_t folder_icon = {
			preview_bounds.x + (preview_bounds.w - icon_size) / 2,
			preview_bounds.y + preview_bounds.h / 3,
			icon_size, icon_size
		};
		draw_filled_rect(canvas, folder_icon, theme->colors.text_secondary);

		// Folder name
		int name_width = strlen(selected->name) * 10;
		if (name_width > preview_bounds.w - 40) name_width = preview_bounds.w - 40;
		gfx_rect_t folder_name = {
			preview_bounds.x + (preview_bounds.w - name_width) / 2,
			folder_icon.y + icon_size + 20,
			name_width, 20
		};
		draw_filled_rect(canvas, folder_name, theme->colors.text_primary);
		return;
	}

	// Render large boxart preview
	Imlib_Image boxart = boxart_get_preview_image();
	int boxart_bottom = preview_bounds.y + panel_padding;

	if (boxart)
	{
		imlib_context_set_image(boxart);
		int src_w = imlib_image_get_width();
		int src_h = imlib_image_get_height();

		// Calculate scaled size maintaining aspect ratio
		int max_w = preview_bounds.w - panel_padding * 2;
		int max_h = (int)(preview_bounds.h * 0.55f);  // Leave more room for detailed info

		float scale_x = (float)max_w / (float)src_w;
		float scale_y = (float)max_h / (float)src_h;
		float scale = (scale_x < scale_y) ? scale_x : scale_y;

		int dst_w = (int)(src_w * scale);
		int dst_h = (int)(src_h * scale);
		int dst_x = preview_bounds.x + (preview_bounds.w - dst_w) / 2;
		int dst_y = preview_bounds.y + panel_padding;

		// Draw boxart with shadow
		gfx_rect_t shadow = { dst_x + 4, dst_y + 4, dst_w, dst_h };
		gfx_color_t shadow_color = gfx_color_hex(0x40000000);
		draw_filled_rect(canvas, shadow, shadow_color);

		// Blend boxart
		imlib_context_set_image(canvas);
		imlib_context_set_blend(1);
		imlib_blend_image_onto_image(boxart, 1,
			0, 0, src_w, src_h,
			dst_x, dst_y, dst_w, dst_h);

		// Subtle border around boxart
		gfx_rect_t border = { dst_x - 1, dst_y - 1, dst_w + 2, dst_h + 2 };
		draw_rect_border(canvas, border, theme->colors.panel_border, 1);

		boxart_bottom = dst_y + dst_h + panel_padding;
	}

	// Game info area - description below boxart
	int info_y = boxart_bottom + 10;
	int info_x = preview_bounds.x + panel_padding;
	int info_width = preview_bounds.w - panel_padding * 2;
	int line_height = 20; // Line height for description text

	// Try to get game metadata from database
	gamedb_entry_t *game_info = NULL;
	if (cfg.gamedb_enable && selected->path[0])
	{
		game_info = gamedb_lookup_filename(selected->path);
	}

	// Show game description if available
	if (game_info && game_info->description[0])
	{
		// Render description text (word-wrapped would be ideal, but for now truncate per line)
		gfx_draw_text_truncated(canvas, game_info->description, info_x, info_y, info_width, theme->colors.text_secondary);
		info_y += line_height * 2;
	}

	// Developer / Year info on one line
	if (game_info && (game_info->developer[0] || game_info->year > 0))
	{
		char info_line[256] = "";
		if (game_info->developer[0])
		{
			snprintf(info_line, sizeof(info_line), "%s", game_info->developer);
		}
		if (game_info->year > 0)
		{
			char year_str[16];
			snprintf(year_str, sizeof(year_str), info_line[0] ? " (%d)" : "%d", game_info->year);
			strncat(info_line, year_str, sizeof(info_line) - strlen(info_line) - 1);
		}
		if (info_line[0])
		{
			gfx_draw_text_truncated(canvas, info_line, info_x, info_y, info_width, theme->colors.text_secondary);
			info_y += line_height;
		}
	}

	// Playtime section (Analogue Library inspired)
	playtime_entry_t *playtime = playtime_get_entry(selected->path);
	if (playtime && playtime->total_seconds > 0)
	{
		// Separator
		gfx_rect_t sep2 = { info_x, info_y, info_width, 1 };
		draw_filled_rect(canvas, sep2, theme->colors.panel_border);
		info_y += 12;

		// Playtime label and value
		gfx_rect_t pt_label = { info_x, info_y + 4, 70, 16 };
		draw_filled_rect(canvas, pt_label, theme->colors.text_secondary);

		char playtime_str[32];
		playtime_format_duration(playtime->total_seconds, playtime_str, sizeof(playtime_str));
		int pt_width = strlen(playtime_str) * 10;
		gfx_rect_t pt_val = { info_x + 80, info_y + 4, pt_width, 16 };
		draw_filled_rect(canvas, pt_val, theme->colors.text_primary);
		info_y += line_height;

		// Last played
		if (playtime->last_played > 0)
		{
			gfx_rect_t lp_label = { info_x, info_y + 4, 90, 16 };
			draw_filled_rect(canvas, lp_label, theme->colors.text_secondary);

			char last_played_str[64];
			playtime_format_relative_time(playtime->last_played, last_played_str, sizeof(last_played_str));
			int lp_width = strlen(last_played_str) * 8;
			gfx_rect_t lp_val = { info_x + 100, info_y + 4, lp_width, 16 };
			draw_filled_rect(canvas, lp_val, theme->colors.text_primary);
			info_y += line_height;
		}

		// Play count
		if (playtime->play_count > 1)
		{
			gfx_rect_t pc_label = { info_x, info_y + 4, 80, 16 };
			draw_filled_rect(canvas, pc_label, theme->colors.text_secondary);

			char count_str[16];
			snprintf(count_str, sizeof(count_str), "%u times", playtime->play_count);
			int pc_width = strlen(count_str) * 8;
			gfx_rect_t pc_val = { info_x + 90, info_y + 4, pc_width, 16 };
			draw_filled_rect(canvas, pc_val, theme->colors.text_primary);
		}
	}
}

// Render grid view
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

		// Name bar at bottom
		gfx_rect_t name_bar = { cell.x, cell.y + cell.h - 24, cell.w, 24 };
		gfx_color_t name_bg = theme->colors.panel_border;
		name_bg.a = 200;
		draw_filled_rect(canvas, name_bar, name_bg);

		// Game name text
		gfx_color_t name_color = (item_idx == menu_state.selected_index) ?
		                         theme->colors.text_highlight : theme->colors.text_primary;
		gfx_draw_text_truncated(canvas, item->name, cell.x + 4, cell.y + cell.h - 20, cell.w - 8, name_color);

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

// Render full-screen game details page (Polymega-inspired)
static void render_game_details(Imlib_Image canvas)
{
	gfx_theme_t *theme = menu_state.theme;
	int panel_padding = theme->panel_padding;

	// Get the item we're showing details for
	gfx_menu_item_t *item = NULL;
	if (menu_state.details_item_index >= 0 && menu_state.details_item_index < menu_state.item_count)
	{
		item = &items_storage[menu_state.details_item_index];
	}
	if (!item) return;

	// Full-screen background
	gfx_rect_t full_screen = { 0, 0, fb_width, fb_height };
	draw_filled_rect(canvas, full_screen, theme->colors.background);

	// Try to get boxart and use as blurred background
	Imlib_Image boxart = item->thumbnail ? item->thumbnail : boxart_get_preview_image();
	if (boxart)
	{
		apply_blur_background(canvas, boxart);
	}

	// Main content area (below header, above footer)
	int content_y = HEADER_HEIGHT + panel_padding;
	int content_height = fb_height - HEADER_HEIGHT - FOOTER_HEIGHT - panel_padding * 2;

	// Left side: Large boxart (40% of width)
	int left_width = (int)(fb_width * 0.40f);
	int boxart_x = panel_padding * 2;
	int boxart_max_w = left_width - panel_padding * 3;
	int boxart_max_h = (int)(content_height * 0.70f);

	if (boxart)
	{
		imlib_context_set_image(boxart);
		int src_w = imlib_image_get_width();
		int src_h = imlib_image_get_height();

		// Scale maintaining aspect ratio
		float scale_x = (float)boxart_max_w / (float)src_w;
		float scale_y = (float)boxart_max_h / (float)src_h;
		float scale = (scale_x < scale_y) ? scale_x : scale_y;

		int dst_w = (int)(src_w * scale);
		int dst_h = (int)(src_h * scale);
		int dst_x = boxart_x + (boxart_max_w - dst_w) / 2;
		int dst_y = content_y + panel_padding;

		// Drop shadow
		gfx_rect_t shadow = { dst_x + 6, dst_y + 6, dst_w, dst_h };
		draw_filled_rect(canvas, shadow, gfx_color_hex(0x60000000));

		// Draw boxart
		imlib_context_set_image(canvas);
		imlib_context_set_blend(1);
		imlib_blend_image_onto_image(boxart, 1,
			0, 0, src_w, src_h,
			dst_x, dst_y, dst_w, dst_h);

		// Border
		gfx_rect_t border = { dst_x - 2, dst_y - 2, dst_w + 4, dst_h + 4 };
		draw_rect_border(canvas, border, theme->colors.selection_border, 2);
	}

	// Right side: Game information panel (55% of width)
	int info_x = left_width + panel_padding;
	int info_width = fb_width - left_width - panel_padding * 3;
	int info_y = content_y;

	// Info panel background
	gfx_rect_t info_panel = { info_x, info_y, info_width, content_height };
	gfx_color_t panel_bg = theme->colors.panel_bg;
	panel_bg.a = 220;
	draw_filled_rect(canvas, info_panel, panel_bg);

	int text_x = info_x + panel_padding;
	int text_y = info_y + panel_padding;
	int text_max_width = info_width - panel_padding * 2;
	int line_height = 24;

	// Game title (large)
	gfx_draw_text_truncated(canvas, item->name, text_x, text_y, text_max_width, theme->colors.text_highlight);
	text_y += line_height + 10;

	// Separator line
	gfx_rect_t sep1 = { text_x, text_y, text_max_width, 2 };
	draw_filled_rect(canvas, sep1, theme->colors.panel_border);
	text_y += 15;

	// Try to get game metadata from database
	gamedb_entry_t *game_info = NULL;
	if (cfg.gamedb_enable && item->path[0])
	{
		game_info = gamedb_lookup_filename(item->path);
	}

	// Metadata section
	if (game_info)
	{
		// Genre
		if (game_info->genre != GENRE_UNKNOWN)
		{
			gfx_draw_text(canvas, "Genre:", text_x, text_y, theme->colors.text_secondary);
			const char *genre_name = gamedb_genre_name(game_info->genre);
			gfx_draw_text(canvas, genre_name, text_x + 130, text_y, theme->colors.text_primary);
			text_y += line_height;
		}

		// Year
		if (game_info->year > 0)
		{
			gfx_draw_text(canvas, "Year:", text_x, text_y, theme->colors.text_secondary);
			char year_str[16];
			snprintf(year_str, sizeof(year_str), "%d", game_info->year);
			gfx_draw_text(canvas, year_str, text_x + 130, text_y, theme->colors.text_primary);
			text_y += line_height;
		}

		// Region
		if (game_info->region != REGION_UNKNOWN)
		{
			gfx_draw_text(canvas, "Region:", text_x, text_y, theme->colors.text_secondary);
			const char *region_name = gamedb_region_name(game_info->region);
			gfx_draw_text(canvas, region_name, text_x + 130, text_y, theme->colors.text_primary);
			text_y += line_height;
		}

		// Players
		if (game_info->players_max > 0)
		{
			gfx_draw_text(canvas, "Players:", text_x, text_y, theme->colors.text_secondary);
			char players_str[16];
			if (game_info->players_min == game_info->players_max)
				snprintf(players_str, sizeof(players_str), "%d", game_info->players_max);
			else
				snprintf(players_str, sizeof(players_str), "%d-%d", game_info->players_min, game_info->players_max);
			gfx_draw_text(canvas, players_str, text_x + 130, text_y, theme->colors.text_primary);
			text_y += line_height;
		}

		// Developer
		if (game_info->developer[0])
		{
			gfx_draw_text(canvas, "Developer:", text_x, text_y, theme->colors.text_secondary);
			gfx_draw_text_truncated(canvas, game_info->developer, text_x + 130, text_y, text_max_width - 140, theme->colors.text_primary);
			text_y += line_height;
		}

		// Publisher
		if (game_info->publisher[0])
		{
			gfx_draw_text(canvas, "Publisher:", text_x, text_y, theme->colors.text_secondary);
			gfx_draw_text_truncated(canvas, game_info->publisher, text_x + 130, text_y, text_max_width - 140, theme->colors.text_primary);
			text_y += line_height;
		}

		// Separator before description
		text_y += 10;
		gfx_rect_t sep2 = { text_x, text_y, text_max_width, 1 };
		draw_filled_rect(canvas, sep2, theme->colors.panel_border);
		text_y += 15;

		// Description (word-wrapped)
		if (game_info->description[0])
		{
			gfx_draw_text(canvas, "Description:", text_x, text_y, theme->colors.text_secondary);
			text_y += line_height;

			int desc_lines = gfx_draw_text_wrapped(canvas, game_info->description,
				text_x, text_y, text_max_width, 6, line_height - 4, theme->colors.text_primary);
			text_y += desc_lines * (line_height - 4) + 10;
		}
	}
	else
	{
		// No metadata available - show filename info
		gfx_draw_text(canvas, "Path:", text_x, text_y, theme->colors.text_secondary);
		text_y += line_height;
		gfx_draw_text_truncated(canvas, item->path, text_x, text_y, text_max_width, theme->colors.text_primary);
		text_y += line_height * 2;
	}

	// Playtime section
	text_y += 10;
	gfx_rect_t sep3 = { text_x, text_y, text_max_width, 2 };
	draw_filled_rect(canvas, sep3, theme->colors.panel_border);
	text_y += 15;

	gfx_draw_text(canvas, "PLAY STATISTICS", text_x, text_y, theme->colors.text_highlight);
	text_y += line_height + 5;

	playtime_entry_t *playtime = playtime_get_entry(item->path);
	if (playtime && playtime->total_seconds > 0)
	{
		// Total playtime
		char playtime_str[64];
		playtime_format_duration(playtime->total_seconds, playtime_str, sizeof(playtime_str));
		gfx_draw_text(canvas, "Total Time:", text_x, text_y, theme->colors.text_secondary);
		gfx_draw_text(canvas, playtime_str, text_x + 180, text_y, theme->colors.text_primary);
		text_y += line_height;

		// Last played
		if (playtime->last_played > 0)
		{
			char last_played_str[64];
			playtime_format_relative_time(playtime->last_played, last_played_str, sizeof(last_played_str));
			gfx_draw_text(canvas, "Last Played:", text_x, text_y, theme->colors.text_secondary);
			gfx_draw_text(canvas, last_played_str, text_x + 180, text_y, theme->colors.text_primary);
			text_y += line_height;
		}

		// Play count
		if (playtime->play_count > 0)
		{
			char count_str[32];
			snprintf(count_str, sizeof(count_str), "%u time%s", playtime->play_count,
				playtime->play_count == 1 ? "" : "s");
			gfx_draw_text(canvas, "Sessions:", text_x, text_y, theme->colors.text_secondary);
			gfx_draw_text(canvas, count_str, text_x + 180, text_y, theme->colors.text_primary);
		}
	}
	else
	{
		gfx_draw_text(canvas, "Never played", text_x, text_y, theme->colors.text_secondary);
	}

	// Favorite indicator (top-right of info panel)
	if (item->is_favorite)
	{
		int star_x = info_x + info_width - 40;
		int star_y = info_y + 10;
		gfx_rect_t star = { star_x, star_y, 24, 24 };
		draw_filled_rect(canvas, star, theme->colors.text_highlight);
	}
}

// Home screen section names
static const char* home_section_names[] = {
	"CONTINUE PLAYING",
	"RECENTLY ADDED",
	"FAVORITES",
	"CORES"
};

// Home screen card data for each section (for display)
typedef struct {
	char name[128];
	char path[256];
	Imlib_Image thumbnail;
} home_card_t;

#define HOME_MAX_CARDS_PER_SECTION 10
static home_card_t home_cards[HOME_SECTION_COUNT][HOME_MAX_CARDS_PER_SECTION];
static int home_card_counts[HOME_SECTION_COUNT] = {0};

// Populate home screen with test data (called from test preview)
void gfx_menu_home_populate_test_data(void)
{
	// Continue Playing section
	home_card_counts[HOME_SECTION_CONTINUE] = 4;
	strcpy(home_cards[HOME_SECTION_CONTINUE][0].name, "Super Mario World");
	strcpy(home_cards[HOME_SECTION_CONTINUE][1].name, "Zelda: ALTTP");
	strcpy(home_cards[HOME_SECTION_CONTINUE][2].name, "Super Metroid");
	strcpy(home_cards[HOME_SECTION_CONTINUE][3].name, "Chrono Trigger");

	// Recently Added section
	home_card_counts[HOME_SECTION_RECENT] = 5;
	strcpy(home_cards[HOME_SECTION_RECENT][0].name, "Sonic the Hedgehog");
	strcpy(home_cards[HOME_SECTION_RECENT][1].name, "Streets of Rage 2");
	strcpy(home_cards[HOME_SECTION_RECENT][2].name, "Gunstar Heroes");
	strcpy(home_cards[HOME_SECTION_RECENT][3].name, "Castlevania");
	strcpy(home_cards[HOME_SECTION_RECENT][4].name, "Mega Man X");

	// Favorites section
	home_card_counts[HOME_SECTION_FAVORITES] = 3;
	strcpy(home_cards[HOME_SECTION_FAVORITES][0].name, "Final Fantasy VI");
	strcpy(home_cards[HOME_SECTION_FAVORITES][1].name, "EarthBound");
	strcpy(home_cards[HOME_SECTION_FAVORITES][2].name, "Secret of Mana");

	// Cores section
	home_card_counts[HOME_SECTION_CORES] = 6;
	strcpy(home_cards[HOME_SECTION_CORES][0].name, "SNES");
	strcpy(home_cards[HOME_SECTION_CORES][1].name, "Genesis");
	strcpy(home_cards[HOME_SECTION_CORES][2].name, "NES");
	strcpy(home_cards[HOME_SECTION_CORES][3].name, "Game Boy");
	strcpy(home_cards[HOME_SECTION_CORES][4].name, "TurboGrafx-16");
	strcpy(home_cards[HOME_SECTION_CORES][5].name, "Neo Geo");

	// Update state counts
	for (int i = 0; i < HOME_SECTION_COUNT; i++) {
		menu_state.home.section_counts[i] = home_card_counts[i];
	}
}

// Render a single game card for home screen
static void render_home_card(Imlib_Image canvas, int x, int y, int width, int height,
                             const char *name, Imlib_Image thumbnail, int selected)
{
	gfx_theme_t *theme = menu_state.theme;

	// Card background
	gfx_rect_t card = { x, y, width, height };
	gfx_color_t card_bg = theme->colors.panel_bg;
	card_bg.a = selected ? 255 : 200;
	draw_filled_rect(canvas, card, card_bg);

	// Selection highlight
	if (selected)
	{
		gfx_rect_t sel = { x - 3, y - 3, width + 6, height + 6 };
		draw_rect_border(canvas, sel, theme->colors.selection_border, 3);
	}

	// Thumbnail area (top portion of card)
	int thumb_height = height - 30;
	gfx_rect_t thumb_area = { x, y, width, thumb_height };

	if (thumbnail)
	{
		imlib_context_set_image(thumbnail);
		int src_w = imlib_image_get_width();
		int src_h = imlib_image_get_height();

		imlib_context_set_image(canvas);
		imlib_context_set_blend(1);
		imlib_blend_image_onto_image(thumbnail, 1,
			0, 0, src_w, src_h,
			x + 2, y + 2, width - 4, thumb_height - 4);
	}
	else
	{
		// Placeholder gradient
		gfx_color_t placeholder = gfx_color_hex(0xFF2a3a4a);
		draw_filled_rect(canvas, thumb_area, placeholder);

		// Center decoration
		int dec_size = width / 3;
		gfx_rect_t dec = { x + width/2 - dec_size/2, y + thumb_height/2 - dec_size/2, dec_size, dec_size };
		gfx_color_t dec_color = gfx_color_hex(0xFF3a4a5a);
		draw_filled_rect(canvas, dec, dec_color);
	}

	// Name bar at bottom
	gfx_rect_t name_bar = { x, y + thumb_height, width, 30 };
	gfx_color_t name_bg = theme->colors.panel_border;
	name_bg.a = 220;
	draw_filled_rect(canvas, name_bar, name_bg);

	// Name text (centered, truncated)
	if (name && name[0])
	{
		gfx_color_t text_color = selected ? theme->colors.text_highlight : theme->colors.text_primary;
		gfx_draw_text_truncated(canvas, name, x + 6, y + thumb_height + 7, width - 12, text_color);
	}
}

// Render home screen with sections (Polymega-inspired)
static void render_home_screen(Imlib_Image canvas)
{
	gfx_theme_t *theme = menu_state.theme;
	int panel_padding = theme->panel_padding;

	// Full screen background
	gfx_rect_t full_screen = { 0, 0, fb_width, fb_height };
	draw_filled_rect(canvas, full_screen, theme->colors.background);

	// Section layout parameters - dynamically sized to fit 4 sections on screen
	int content_y = HEADER_HEIGHT + panel_padding;
	int content_width = fb_width - panel_padding * 2;
	int available_height = fb_height - HEADER_HEIGHT - FOOTER_HEIGHT - panel_padding * 2;
	int section_height = available_height / HOME_SECTION_COUNT;
	int card_height = section_height - 45;         // Leave room for header and spacing
	int card_width = (int)(card_height * 0.85f);   // Slightly wider than tall
	int card_spacing = 15;                          // Space between cards
	int section_padding = 25;                       // Padding within section
	int header_height = 32;                         // Section header height

	// Render each section
	for (int section = 0; section < HOME_SECTION_COUNT; section++)
	{
		int section_y = content_y + section * section_height;

		// Skip if section is below visible area
		if (section_y > fb_height - FOOTER_HEIGHT) break;

		// Section background (subtle)
		gfx_rect_t section_bg = { panel_padding, section_y, content_width, section_height - 10 };
		gfx_color_t bg_color = theme->colors.panel_bg;
		bg_color.a = (section == menu_state.home.current_section) ? 180 : 100;
		draw_filled_rect(canvas, section_bg, bg_color);

		// Section header
		int header_y = section_y + 5;
		gfx_color_t header_color = (section == menu_state.home.current_section) ?
		                           theme->colors.text_highlight : theme->colors.text_primary;
		gfx_draw_text(canvas, home_section_names[section], panel_padding + section_padding, header_y + 8, header_color);

		// Item count indicator
		char count_str[32];
		int card_count = home_card_counts[section];
		snprintf(count_str, sizeof(count_str), "(%d)", card_count);
		int count_x = panel_padding + section_padding + strlen(home_section_names[section]) * 16 + 10;
		gfx_draw_text(canvas, count_str, count_x, header_y + 8, theme->colors.text_secondary);

		// Arrow indicator if more items
		if (card_count > 0)
		{
			int arrow_x = panel_padding + content_width - 40;
			gfx_draw_text(canvas, ">", arrow_x, header_y + 8, theme->colors.text_secondary);
		}

		// Render game cards in horizontal row
		int cards_y = section_y + header_height + 10;
		int cards_x = panel_padding + section_padding;
		int scroll = menu_state.home.section_scroll[section];

		// Calculate visible cards
		int visible_cards = (content_width - section_padding * 2) / (card_width + card_spacing);

		for (int i = 0; i < visible_cards && (scroll + i) < card_count; i++)
		{
			int card_idx = scroll + i;
			int card_x = cards_x + i * (card_width + card_spacing);

			// Is this card selected?
			int is_selected = (section == menu_state.home.current_section) && (i == 0);

			render_home_card(canvas, card_x, cards_y, card_width, card_height,
			                 home_cards[section][card_idx].name,
			                 home_cards[section][card_idx].thumbnail,
			                 is_selected);
		}

		// Empty section message
		if (card_count == 0)
		{
			const char *empty_msg = "No items";
			int msg_x = cards_x + 20;
			int msg_y = cards_y + card_height / 2 - 8;
			gfx_draw_text(canvas, empty_msg, msg_x, msg_y, theme->colors.text_secondary);
		}
	}

	// Section indicator (dots on the right side)
	int dot_x = fb_width - panel_padding - 15;
	int dot_y = content_y + 50;
	int dot_spacing = 20;

	for (int i = 0; i < HOME_SECTION_COUNT; i++)
	{
		gfx_rect_t dot = { dot_x, dot_y + i * dot_spacing, 8, 8 };
		gfx_color_t dot_color = (i == menu_state.home.current_section) ?
		                        theme->colors.text_highlight : theme->colors.text_secondary;
		draw_filled_rect(canvas, dot, dot_color);
	}
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
	static unsigned long last_render_time = 0;

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

	// Frame rate limiter: max ~30fps (33ms between frames) to reduce CPU load
	unsigned long now = GetTimer(0);
	if (now - last_render_time < 33 && !menu_state.needs_redraw) {
		// Just keep current buffer displayed, skip expensive work
		video_fb_enable(1, gfx_fb_front);
		OsdDisable();
		return;
	}
	last_render_time = now;
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

		// Render blurred boxart as background (if available, and not in special modes)
		if (menu_state.mode == GFX_MODE_BROWSE)
		{
			Imlib_Image boxart = boxart_get_preview_image();
			if (boxart)
			{
				apply_blur_background(canvas, boxart);
			}
		}

		// Render UI elements based on menu mode
		switch (menu_state.mode)
		{
			case GFX_MODE_HOME:
				render_home_screen(canvas);
				break;
			case GFX_MODE_DETAILS:
				render_game_details(canvas);
				break;
			case GFX_MODE_BROWSE:
			default:
				// Normal browse mode - render based on view type
				switch (menu_state.view_type)
				{
					case GFX_VIEW_LIST:
						render_list_view(canvas);
						render_preview_panel(canvas);
						break;
					case GFX_VIEW_GRID:
						render_grid_view(canvas);
						break;
					case GFX_VIEW_WHEEL:
						render_wheel_view(canvas);
						break;
					default:
						render_list_view(canvas);
						break;
				}
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

	// Render UI elements based on menu mode
	switch (menu_state.mode)
	{
		case GFX_MODE_HOME:
			render_home_screen(preview);
			break;
		case GFX_MODE_DETAILS:
			render_game_details(preview);
			break;
		case GFX_MODE_BROWSE:
		default:
			// Normal browse mode - render based on view type
			switch (menu_state.view_type)
			{
				case GFX_VIEW_LIST:
					render_list_view(preview);
					render_preview_panel(preview);
					break;
				case GFX_VIEW_GRID:
					render_grid_view(preview);
					break;
				case GFX_VIEW_WHEEL:
					render_wheel_view(preview);
					break;
				default:
					render_list_view(preview);
					break;
			}
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

// Handle input and return 1 if consumed
int gfx_menu_handle_input(int key)
{
	if (!menu_state.enabled) return 0;

	// Define key codes (these should match input.h definitions)
	#define KEY_UP_LOCAL     0x48
	#define KEY_DOWN_LOCAL   0x50
	#define KEY_LEFT_LOCAL   0x4B
	#define KEY_RIGHT_LOCAL  0x4D
	#define KEY_ENTER_LOCAL  0x1C
	#define KEY_ESC_LOCAL    0x01
	#define KEY_Y_LOCAL      0x15  // Y button for favorite toggle

	// Handle input based on current mode
	switch (menu_state.mode)
	{
		case GFX_MODE_DETAILS:
			// Details view: B goes back, A would launch (handled elsewhere), Y toggles favorite
			switch (key)
			{
				case KEY_ESC_LOCAL:
					// Go back to browse mode
					gfx_menu_show_browse();
					return 1;
				case KEY_Y_LOCAL:
					// Toggle favorite for current item
					if (menu_state.details_item_index >= 0 &&
					    menu_state.details_item_index < menu_state.item_count)
					{
						gfx_menu_item_t *item = &items_storage[menu_state.details_item_index];
						item->is_favorite = !item->is_favorite;
						menu_state.needs_redraw = 1;
					}
					return 1;
				default:
					break;
			}
			return 0;

		case GFX_MODE_HOME:
			// Home screen: up/down moves between sections, left/right scrolls items
			switch (key)
			{
				case KEY_UP_LOCAL:
					if (menu_state.home.current_section > 0)
					{
						menu_state.home.current_section = (home_section_t)(menu_state.home.current_section - 1);
						menu_state.needs_redraw = 1;
					}
					return 1;
				case KEY_DOWN_LOCAL:
					if (menu_state.home.current_section < HOME_SECTION_COUNT - 1)
					{
						menu_state.home.current_section = (home_section_t)(menu_state.home.current_section + 1);
						menu_state.needs_redraw = 1;
					}
					return 1;
				case KEY_LEFT_LOCAL:
					if (menu_state.home.section_scroll[menu_state.home.current_section] > 0)
					{
						menu_state.home.section_scroll[menu_state.home.current_section]--;
						menu_state.needs_redraw = 1;
					}
					return 1;
				case KEY_RIGHT_LOCAL:
					menu_state.home.section_scroll[menu_state.home.current_section]++;
					menu_state.needs_redraw = 1;
					return 1;
				case KEY_ESC_LOCAL:
					// B goes to browse mode
					gfx_menu_show_browse();
					return 1;
				default:
					break;
			}
			return 0;

		case GFX_MODE_BROWSE:
		default:
			// Normal browse mode
			switch (key)
			{
				case KEY_UP_LOCAL:
					gfx_menu_select_prev();
					return 1;
				case KEY_DOWN_LOCAL:
					gfx_menu_select_next();
					return 1;
				case KEY_LEFT_LOCAL:
					gfx_menu_page_up();
					return 1;
				case KEY_RIGHT_LOCAL:
					gfx_menu_page_down();
					return 1;
				case KEY_ENTER_LOCAL:
					// A button: open details view for selected game
					if (menu_state.selected_index >= 0 &&
					    menu_state.selected_index < menu_state.item_count)
					{
						gfx_menu_item_t *item = &items_storage[menu_state.selected_index];
						// Only show details for games, not folders
						if (item->type == GFX_ITEM_GAME)
						{
							gfx_menu_show_details(menu_state.selected_index);
							return 1;
						}
					}
					break;
				default:
					break;
			}
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
