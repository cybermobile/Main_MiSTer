// theme.h
// Theme management system for MiSTer modern frontend
// Provides JSON theme loading, saving, and built-in presets
// 2024

#ifndef __THEME_H__
#define __THEME_H__

#include <inttypes.h>
#include "gfx_menu.h"

// Maximum themes that can be loaded
#define THEME_MAX_LOADED 16

// Theme file paths
#define THEME_DIR "/media/fat/config/themes"
#define THEME_USER_FILE "/media/fat/config/gfx_theme.json"

// Built-in theme names
#define THEME_BUILTIN_DARK "Dark"
#define THEME_BUILTIN_LIGHT "Light"
#define THEME_BUILTIN_RETRO "Retro"
#define THEME_BUILTIN_NEON "Neon"
#define THEME_BUILTIN_MINIMAL "Minimal"

// Theme metadata
typedef struct {
	char name[64];
	char author[64];
	char version[16];
	char description[256];
	char preview_image[256];  // Path to preview screenshot
} theme_metadata_t;

// Full theme definition (extends gfx_theme_t)
typedef struct {
	theme_metadata_t meta;
	gfx_theme_t theme;
	char filepath[256];       // Source file path
	uint8_t is_builtin;       // Is a built-in theme
	uint8_t is_modified;      // Has unsaved changes
} theme_entry_t;

// Theme list for selection
typedef struct {
	theme_entry_t entries[THEME_MAX_LOADED];
	int count;
	int selected_index;
	int preview_index;        // Theme being previewed (may differ from active)
} theme_list_t;

//// Core Functions ////

// Initialize theme system
void theme_init(void);

// Shutdown theme system
void theme_shutdown(void);

// Scan for available themes
void theme_scan(void);

// Get theme list
theme_list_t* theme_get_list(void);

// Get current active theme
theme_entry_t* theme_get_current(void);

//// Theme Selection ////

// Select theme by index
void theme_select(int index);

// Select theme by name
void theme_select_by_name(const char *name);

// Apply selected theme (makes it active)
void theme_apply(int index);

// Apply current selection
void theme_apply_current(void);

// Preview theme without applying
void theme_preview(int index);

// Cancel preview (revert to active theme)
void theme_cancel_preview(void);

//// Theme Loading/Saving ////

// Load theme from JSON file
int theme_load_json(const char *filepath, theme_entry_t *out_theme);

// Save theme to JSON file
int theme_save_json(const char *filepath, theme_entry_t *theme);

// Save current theme
int theme_save_current(void);

// Export theme to file
int theme_export(int index, const char *filepath);

// Import theme from file
int theme_import(const char *filepath);

//// Built-in Themes ////

// Load built-in theme by name
int theme_load_builtin(const char *name, theme_entry_t *out_theme);

// Get dark theme (default)
void theme_get_dark(gfx_theme_t *out_theme);

// Get light theme
void theme_get_light(gfx_theme_t *out_theme);

// Get retro theme (CRT-inspired)
void theme_get_retro(gfx_theme_t *out_theme);

// Get neon theme (cyberpunk)
void theme_get_neon(gfx_theme_t *out_theme);

// Get minimal theme
void theme_get_minimal(gfx_theme_t *out_theme);

//// Theme Editing ////

// Create new theme based on current
theme_entry_t* theme_create_new(const char *name);

// Duplicate theme
theme_entry_t* theme_duplicate(int index, const char *new_name);

// Delete user theme
int theme_delete(int index);

// Set theme color
void theme_set_color(theme_entry_t *theme, const char *color_name, uint32_t color);

// Get theme color
uint32_t theme_get_color(theme_entry_t *theme, const char *color_name);

// Set theme font size
void theme_set_font_size(theme_entry_t *theme, const char *size_name, int size);

// Set theme layout value
void theme_set_layout(theme_entry_t *theme, const char *layout_name, int value);

//// Color Utilities ////

// Parse color from hex string "#RRGGBB" or "#AARRGGBB"
uint32_t theme_parse_color(const char *hex_str);

// Format color to hex string
void theme_format_color(uint32_t color, char *out_str, int include_alpha);

// Blend two colors
uint32_t theme_blend_colors(uint32_t color1, uint32_t color2, float factor);

// Lighten color
uint32_t theme_lighten(uint32_t color, float amount);

// Darken color
uint32_t theme_darken(uint32_t color, float amount);

// Adjust saturation
uint32_t theme_saturate(uint32_t color, float amount);

//// Settings Integration ////

// Get color names for UI
const char** theme_get_color_names(int *count);

// Get layout setting names for UI
const char** theme_get_layout_names(int *count);

// Validate theme
int theme_validate(theme_entry_t *theme);

// Reset theme to defaults
void theme_reset_to_defaults(theme_entry_t *theme);

#endif // __THEME_H__
