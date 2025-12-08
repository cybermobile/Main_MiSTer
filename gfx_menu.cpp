// gfx_menu.cpp
// Graphical menu framework for MiSTer modern frontend
// 2024

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "gfx_menu.h"
#include "video.h"
#include "cfg.h"
#include "boxart.h"
#include "file_io.h"
#include "hardware.h"
#include "animator.h"

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

// Forward declarations
static void render_list_view(Imlib_Image canvas);
static void render_grid_view(Imlib_Image canvas);
static void render_header(Imlib_Image canvas);
static void render_footer(Imlib_Image canvas);
static void render_preview_panel(Imlib_Image canvas);
static void render_scrollbar(Imlib_Image canvas, gfx_rect_t bounds);
static void apply_blur_background(Imlib_Image canvas, Imlib_Image boxart);

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

// Initialize default theme with modern dark style
static void init_default_theme(void)
{
	memset(&default_theme, 0, sizeof(default_theme));
	strcpy(default_theme.name, "Default Dark");

	// Dark theme colors
	default_theme.colors.background = gfx_color_hex(0xFF1a1a2e);      // Dark blue-black
	default_theme.colors.panel_bg = gfx_color_hex(0xE016213e);        // Slightly lighter, semi-transparent
	default_theme.colors.panel_border = gfx_color_hex(0xFF0f3460);    // Blue accent border
	default_theme.colors.text_primary = gfx_color_hex(0xFFe0e0e0);    // Light gray
	default_theme.colors.text_secondary = gfx_color_hex(0xFF808080);  // Medium gray
	default_theme.colors.text_highlight = gfx_color_hex(0xFFe94560);  // Pink/red accent
	default_theme.colors.selection_bg = gfx_color_hex(0xCC0f3460);    // Blue selection
	default_theme.colors.selection_border = gfx_color_hex(0xFFe94560); // Pink border
	default_theme.colors.scrollbar_bg = gfx_color_hex(0x40ffffff);    // Faint white
	default_theme.colors.scrollbar_fg = gfx_color_hex(0xFFe94560);    // Pink accent

	// Font settings
	strcpy(default_theme.font_name, "");  // Use default
	default_theme.font_size_title = 24;
	default_theme.font_size_item = 18;
	default_theme.font_size_info = 14;

	// Layout settings
	default_theme.thumbnail_width = 80;
	default_theme.thumbnail_height = 80;
	default_theme.item_spacing = 8;
	default_theme.panel_padding = 16;
	default_theme.corner_radius = 8;
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
	item->thumbnail = NULL;

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

	// Title text (using simple rectangle as placeholder for text)
	// Real text rendering would use imlib_text_draw() with a loaded font
	if (menu_state.title[0])
	{
		// Placeholder: draw a small indicator where title would be
		gfx_rect_t title_indicator = { 20, 20, 200, 24 };
		draw_filled_rect(canvas, title_indicator, theme->colors.text_primary);
	}
}

// Render footer bar with controls hint
static void render_footer(Imlib_Image canvas)
{
	gfx_theme_t *theme = menu_state.theme;
	int width = fb_width;
	int height = fb_height;

	// Footer background
	gfx_rect_t footer_rect = { 0, height - FOOTER_HEIGHT, width, FOOTER_HEIGHT };
	gfx_color_t footer_bg = theme->colors.panel_bg;
	footer_bg.a = 200;
	draw_filled_rect(canvas, footer_rect, footer_bg);

	// Top border
	gfx_rect_t border_rect = { 0, height - FOOTER_HEIGHT, width, 2 };
	draw_filled_rect(canvas, border_rect, theme->colors.panel_border);
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

		// Thumbnail
		if (item->thumbnail)
		{
			imlib_context_set_image(canvas);
			imlib_context_set_blend(1);
			imlib_blend_image_onto_image(item->thumbnail, 1,
				0, 0, thumb_size, thumb_size,
				item_rect.x + 4, item_rect.y + 4,
				thumb_size - 8, thumb_size - 8);
		}
		else
		{
			// Placeholder for missing thumbnail
			gfx_rect_t thumb_rect = { item_rect.x + 4, item_rect.y + 4, thumb_size - 8, thumb_size - 8 };
			gfx_color_t placeholder = theme->colors.panel_border;
			placeholder.a = 100;
			draw_filled_rect(canvas, thumb_rect, placeholder);
		}

		// Item type indicator (folder icon, etc.)
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

		// Name text placeholder (real text would use font rendering)
		gfx_rect_t name_area = { item_rect.x + thumb_size + 8, item_rect.y + 8,
		                         item_rect.w - thumb_size - 40, 20 };
		gfx_color_t text_color = (item_idx == menu_state.selected_index) ?
		                         theme->colors.text_highlight : theme->colors.text_primary;
		// Placeholder bar representing text
		name_area.w = strlen(item->name) * 6;  // Approximate width
		if (name_area.w > item_rect.w - thumb_size - 40) name_area.w = item_rect.w - thumb_size - 40;
		draw_filled_rect(canvas, name_area, text_color);

		y += item_height;
	}

	// Scrollbar
	render_scrollbar(canvas, list_bounds);
}

// Render preview panel (right side with large boxart)
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

	// Render large boxart preview
	Imlib_Image boxart = boxart_get_preview_image();
	if (boxart)
	{
		imlib_context_set_image(boxart);
		int src_w = imlib_image_get_width();
		int src_h = imlib_image_get_height();

		// Calculate scaled size maintaining aspect ratio
		int max_w = preview_bounds.w - panel_padding * 2;
		int max_h = (int)(preview_bounds.h * 0.7f);  // Leave room for info

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

		// Border around boxart
		gfx_rect_t border = { dst_x - 2, dst_y - 2, dst_w + 4, dst_h + 4 };
		draw_rect_border(canvas, border, theme->colors.panel_border, 2);
	}

	// Game info area
	int info_y = preview_bounds.y + (int)(preview_bounds.h * 0.75f);
	gfx_rect_t info_area = { preview_bounds.x + panel_padding, info_y,
	                         preview_bounds.w - panel_padding * 2, preview_bounds.h - (info_y - preview_bounds.y) };

	// Game title placeholder
	gfx_rect_t title_bar = { info_area.x, info_area.y, info_area.w, 24 };
	draw_filled_rect(canvas, title_bar, theme->colors.text_primary);

	// Description placeholder
	if (selected->description[0])
	{
		gfx_rect_t desc_bar = { info_area.x, info_area.y + 32, info_area.w, 16 };
		draw_filled_rect(canvas, desc_bar, theme->colors.text_secondary);
	}
}

// Render grid view
static void render_grid_view(Imlib_Image canvas)
{
	gfx_theme_t *theme = menu_state.theme;
	int panel_padding = theme->panel_padding;

	// Grid parameters
	int cell_size = 160;
	int cell_spacing = 16;
	int cols = (fb_width - panel_padding * 2) / (cell_size + cell_spacing);
	if (cols < 1) cols = 1;

	int content_y = HEADER_HEIGHT + panel_padding;
	int content_height = fb_height - HEADER_HEIGHT - FOOTER_HEIGHT - panel_padding * 2;
	int rows_visible = content_height / (cell_size + cell_spacing);
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

		// Thumbnail
		if (item->thumbnail)
		{
			imlib_context_set_image(canvas);
			imlib_context_set_blend(1);
			imlib_blend_image_onto_image(item->thumbnail, 1,
				0, 0, cell_size, cell_size,
				cell.x, cell.y, cell.w, cell.h - 24);
		}

		// Name bar at bottom
		gfx_rect_t name_bar = { cell.x, cell.y + cell.h - 24, cell.w, 24 };
		gfx_color_t name_bg = theme->colors.panel_border;
		name_bg.a = 200;
		draw_filled_rect(canvas, name_bar, name_bg);

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

// Main render function
void gfx_menu_render(void)
{
	if (!menu_state.enabled) return;
	if (!menu_state.needs_redraw) return;
	if (!fb_base || fb_width <= 0 || fb_height <= 0) return;

	// Create canvas image from framebuffer
	Imlib_Image canvas = imlib_create_image_using_data(fb_width, fb_height,
		(uint32_t*)(fb_base + (1920*1080 * 1)));  // Use first background buffer

	if (!canvas) return;

	imlib_context_set_image(canvas);
	imlib_image_set_has_alpha(1);

	// Fill background
	gfx_theme_t *theme = menu_state.theme;
	gfx_rect_t full_screen = { 0, 0, fb_width, fb_height };
	draw_filled_rect(canvas, full_screen, theme->colors.background);

	// Render blurred boxart as background (if available)
	Imlib_Image boxart = boxart_get_preview_image();
	if (boxart)
	{
		apply_blur_background(canvas, boxart);
	}

	// Render UI elements based on view type
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
			// TODO: Implement wheel view
			render_list_view(canvas);
			break;
		default:
			render_list_view(canvas);
			break;
	}

	render_header(canvas);
	render_footer(canvas);

	menu_state.needs_redraw = 0;
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
		default:
			break;
	}

	return 0;
}

void gfx_draw_text(Imlib_Image img, const char *text, int x, int y, gfx_color_t color)
{
	// TODO: Implement proper text rendering using Imlib2 fonts
	// For now, this is a placeholder that draws a colored bar
	if (!text || !img) return;

	int len = strlen(text);
	gfx_rect_t text_rect = { x, y, len * 8, 16 };

	imlib_context_set_image(img);
	set_imlib_color(color);
	imlib_image_fill_rectangle(text_rect.x, text_rect.y, text_rect.w, text_rect.h);
}
