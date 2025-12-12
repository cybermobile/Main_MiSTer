// theme.cpp
// Theme management system for MiSTer modern frontend
// 2024

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#include "theme.h"
#include "gfx_menu.h"
#include "file_io.h"

// Color name mappings for UI
static const char* color_names[] = {
	"background",
	"panel_bg",
	"panel_border",
	"text_primary",
	"text_secondary",
	"text_highlight",
	"selection_bg",
	"selection_border",
	"scrollbar_bg",
	"scrollbar_fg"
};
static const int color_name_count = 10;

// Layout setting names
static const char* layout_names[] = {
	"thumbnail_width",
	"thumbnail_height",
	"item_spacing",
	"panel_padding",
	"corner_radius",
	"font_size_title",
	"font_size_item",
	"font_size_info"
};
static const int layout_name_count = 8;

// Global state
static theme_list_t theme_list;
static int active_theme_index = 0;
static int preview_active = 0;
static gfx_theme_t saved_theme;  // For reverting preview
static int initialized = 0;

// Forward declarations
static void add_builtin_themes(void);
static int parse_json_theme(const char *json, theme_entry_t *out_theme);

void theme_init(void)
{
	memset(&theme_list, 0, sizeof(theme_list));
	active_theme_index = 0;
	preview_active = 0;

	// Add built-in themes first
	add_builtin_themes();

	// Scan for user themes
	theme_scan();

	initialized = 1;
	printf("Theme system initialized with %d themes\n", theme_list.count);
}

void theme_shutdown(void)
{
	memset(&theme_list, 0, sizeof(theme_list));
	initialized = 0;
	printf("Theme system shutdown\n");
}

static void add_builtin_themes(void)
{
	// Dark theme (default)
	theme_entry_t *dark = &theme_list.entries[theme_list.count++];
	memset(dark, 0, sizeof(theme_entry_t));
	strcpy(dark->meta.name, THEME_BUILTIN_DARK);
	strcpy(dark->meta.author, "MiSTer");
	strcpy(dark->meta.version, "1.0");
	strcpy(dark->meta.description, "Default dark theme with blue accents");
	dark->is_builtin = 1;
	theme_get_dark(&dark->theme);

	// Light theme
	theme_entry_t *light = &theme_list.entries[theme_list.count++];
	memset(light, 0, sizeof(theme_entry_t));
	strcpy(light->meta.name, THEME_BUILTIN_LIGHT);
	strcpy(light->meta.author, "MiSTer");
	strcpy(light->meta.version, "1.0");
	strcpy(light->meta.description, "Clean light theme");
	light->is_builtin = 1;
	theme_get_light(&light->theme);

	// Retro theme
	theme_entry_t *retro = &theme_list.entries[theme_list.count++];
	memset(retro, 0, sizeof(theme_entry_t));
	strcpy(retro->meta.name, THEME_BUILTIN_RETRO);
	strcpy(retro->meta.author, "MiSTer");
	strcpy(retro->meta.version, "1.0");
	strcpy(retro->meta.description, "CRT-inspired retro theme");
	retro->is_builtin = 1;
	theme_get_retro(&retro->theme);

	// Neon theme
	theme_entry_t *neon = &theme_list.entries[theme_list.count++];
	memset(neon, 0, sizeof(theme_entry_t));
	strcpy(neon->meta.name, THEME_BUILTIN_NEON);
	strcpy(neon->meta.author, "MiSTer");
	strcpy(neon->meta.version, "1.0");
	strcpy(neon->meta.description, "Cyberpunk neon theme");
	neon->is_builtin = 1;
	theme_get_neon(&neon->theme);

	// Minimal theme
	theme_entry_t *minimal = &theme_list.entries[theme_list.count++];
	memset(minimal, 0, sizeof(theme_entry_t));
	strcpy(minimal->meta.name, THEME_BUILTIN_MINIMAL);
	strcpy(minimal->meta.author, "MiSTer");
	strcpy(minimal->meta.version, "1.0");
	strcpy(minimal->meta.description, "Clean minimal interface");
	minimal->is_builtin = 1;
	theme_get_minimal(&minimal->theme);
}

void theme_scan(void)
{
	DIR *dir = opendir(THEME_DIR);
	if (!dir)
	{
		// Create themes directory
		mkdir(THEME_DIR, 0755);
		return;
	}

	struct dirent *entry;
	while ((entry = readdir(dir)) != NULL && theme_list.count < THEME_MAX_LOADED)
	{
		// Check for .json files
		int len = strlen(entry->d_name);
		if (len > 5 && strcmp(entry->d_name + len - 5, ".json") == 0)
		{
			char filepath[512];
			snprintf(filepath, sizeof(filepath), "%s/%s", THEME_DIR, entry->d_name);

			theme_entry_t *theme = &theme_list.entries[theme_list.count];
			if (theme_load_json(filepath, theme) == 0)
			{
				theme->is_builtin = 0;
				strncpy(theme->filepath, filepath, sizeof(theme->filepath) - 1);
				theme_list.count++;
				printf("Loaded theme: %s\n", theme->meta.name);
			}
		}
	}

	closedir(dir);
}

theme_list_t* theme_get_list(void)
{
	return &theme_list;
}

theme_entry_t* theme_get_current(void)
{
	if (active_theme_index >= 0 && active_theme_index < theme_list.count)
	{
		return &theme_list.entries[active_theme_index];
	}
	return &theme_list.entries[0];  // Return default
}

void theme_select(int index)
{
	if (index >= 0 && index < theme_list.count)
	{
		theme_list.selected_index = index;
	}
}

void theme_select_by_name(const char *name)
{
	for (int i = 0; i < theme_list.count; i++)
	{
		if (strcasecmp(theme_list.entries[i].meta.name, name) == 0)
		{
			theme_select(i);
			return;
		}
	}
}

void theme_apply(int index)
{
	if (index >= 0 && index < theme_list.count)
	{
		active_theme_index = index;
		gfx_menu_set_theme(&theme_list.entries[index].theme);
		preview_active = 0;
		printf("Applied theme: %s\n", theme_list.entries[index].meta.name);
	}
}

void theme_apply_current(void)
{
	theme_apply(theme_list.selected_index);
}

void theme_preview(int index)
{
	if (index >= 0 && index < theme_list.count)
	{
		if (!preview_active)
		{
			// Save current theme for reverting
			gfx_theme_t *current = gfx_menu_get_theme();
			if (current)
			{
				memcpy(&saved_theme, current, sizeof(gfx_theme_t));
			}
			preview_active = 1;
		}

		theme_list.preview_index = index;
		gfx_menu_set_theme(&theme_list.entries[index].theme);
	}
}

void theme_cancel_preview(void)
{
	if (preview_active)
	{
		gfx_menu_set_theme(&saved_theme);
		theme_list.preview_index = active_theme_index;
		preview_active = 0;
	}
}

// Simple JSON parser for theme files
static int parse_json_theme(const char *json, theme_entry_t *out_theme)
{
	if (!json || !out_theme) return -1;

	// Initialize with defaults
	theme_get_dark(&out_theme->theme);
	memset(&out_theme->meta, 0, sizeof(theme_metadata_t));

	// Very basic JSON parsing - find key-value pairs
	// Format: "key": "value" or "key": number

	const char *p = json;
	char key[64], value[256];

	while (*p)
	{
		// Find opening quote for key
		while (*p && *p != '"') p++;
		if (!*p) break;
		p++;

		// Read key
		int ki = 0;
		while (*p && *p != '"' && ki < 63)
		{
			key[ki++] = *p++;
		}
		key[ki] = '\0';
		if (!*p) break;
		p++;

		// Find colon
		while (*p && *p != ':') p++;
		if (!*p) break;
		p++;

		// Skip whitespace
		while (*p && (*p == ' ' || *p == '\t' || *p == '\n')) p++;

		// Read value
		int vi = 0;
		if (*p == '"')
		{
			// String value
			p++;
			while (*p && *p != '"' && vi < 255)
			{
				value[vi++] = *p++;
			}
			value[vi] = '\0';
			if (*p == '"') p++;
		}
		else if (*p == '#')
		{
			// Color value (hex)
			while (*p && *p != ',' && *p != '}' && *p != '\n' && vi < 255)
			{
				if (*p != ' ' && *p != '\t')
				{
					value[vi++] = *p;
				}
				p++;
			}
			value[vi] = '\0';
		}
		else
		{
			// Number value
			while (*p && *p != ',' && *p != '}' && *p != '\n' && vi < 255)
			{
				if (*p != ' ' && *p != '\t')
				{
					value[vi++] = *p;
				}
				p++;
			}
			value[vi] = '\0';
		}

		// Apply value based on key
		if (strcmp(key, "name") == 0)
		{
			strncpy(out_theme->meta.name, value, 63);
			strncpy(out_theme->theme.name, value, 63);
		}
		else if (strcmp(key, "author") == 0)
		{
			strncpy(out_theme->meta.author, value, 63);
		}
		else if (strcmp(key, "version") == 0)
		{
			strncpy(out_theme->meta.version, value, 15);
		}
		else if (strcmp(key, "description") == 0)
		{
			strncpy(out_theme->meta.description, value, 255);
		}
		else if (strcmp(key, "background") == 0)
		{
			uint32_t color = theme_parse_color(value);
			out_theme->theme.colors.background = gfx_color_hex(color);
		}
		else if (strcmp(key, "panel_bg") == 0)
		{
			uint32_t color = theme_parse_color(value);
			out_theme->theme.colors.panel_bg = gfx_color_hex(color);
		}
		else if (strcmp(key, "panel_border") == 0)
		{
			uint32_t color = theme_parse_color(value);
			out_theme->theme.colors.panel_border = gfx_color_hex(color);
		}
		else if (strcmp(key, "text_primary") == 0)
		{
			uint32_t color = theme_parse_color(value);
			out_theme->theme.colors.text_primary = gfx_color_hex(color);
		}
		else if (strcmp(key, "text_secondary") == 0)
		{
			uint32_t color = theme_parse_color(value);
			out_theme->theme.colors.text_secondary = gfx_color_hex(color);
		}
		else if (strcmp(key, "text_highlight") == 0)
		{
			uint32_t color = theme_parse_color(value);
			out_theme->theme.colors.text_highlight = gfx_color_hex(color);
		}
		else if (strcmp(key, "selection_bg") == 0)
		{
			uint32_t color = theme_parse_color(value);
			out_theme->theme.colors.selection_bg = gfx_color_hex(color);
		}
		else if (strcmp(key, "selection_border") == 0)
		{
			uint32_t color = theme_parse_color(value);
			out_theme->theme.colors.selection_border = gfx_color_hex(color);
		}
		else if (strcmp(key, "scrollbar_bg") == 0)
		{
			uint32_t color = theme_parse_color(value);
			out_theme->theme.colors.scrollbar_bg = gfx_color_hex(color);
		}
		else if (strcmp(key, "scrollbar_fg") == 0)
		{
			uint32_t color = theme_parse_color(value);
			out_theme->theme.colors.scrollbar_fg = gfx_color_hex(color);
		}
		else if (strcmp(key, "thumbnail_width") == 0)
		{
			out_theme->theme.thumbnail_width = atoi(value);
		}
		else if (strcmp(key, "thumbnail_height") == 0)
		{
			out_theme->theme.thumbnail_height = atoi(value);
		}
		else if (strcmp(key, "item_spacing") == 0)
		{
			out_theme->theme.item_spacing = atoi(value);
		}
		else if (strcmp(key, "panel_padding") == 0)
		{
			out_theme->theme.panel_padding = atoi(value);
		}
		else if (strcmp(key, "corner_radius") == 0)
		{
			out_theme->theme.corner_radius = atoi(value);
		}
		else if (strcmp(key, "font_size_title") == 0)
		{
			out_theme->theme.font_size_title = atoi(value);
		}
		else if (strcmp(key, "font_size_item") == 0)
		{
			out_theme->theme.font_size_item = atoi(value);
		}
		else if (strcmp(key, "font_size_info") == 0)
		{
			out_theme->theme.font_size_info = atoi(value);
		}
	}

	return 0;
}

int theme_load_json(const char *filepath, theme_entry_t *out_theme)
{
	if (!filepath || !out_theme) return -1;

	FILE *f = fopen(filepath, "r");
	if (!f) return -1;

	// Get file size
	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);

	if (size <= 0 || size > 65536)
	{
		fclose(f);
		return -1;
	}

	// Read file
	char *json = (char*)malloc(size + 1);
	if (!json)
	{
		fclose(f);
		return -1;
	}

	size_t read = fread(json, 1, size, f);
	fclose(f);

	json[read] = '\0';

	// Parse
	int result = parse_json_theme(json, out_theme);

	free(json);
	return result;
}

int theme_save_json(const char *filepath, theme_entry_t *theme)
{
	if (!filepath || !theme) return -1;

	FILE *f = fopen(filepath, "w");
	if (!f) return -1;

	char bg[12], panel_bg[12], panel_border[12];
	char text_pri[12], text_sec[12], text_hi[12];
	char sel_bg[12], sel_border[12], scroll_bg[12], scroll_fg[12];

	theme_format_color((theme->theme.colors.background.a << 24) |
	                   (theme->theme.colors.background.r << 16) |
	                   (theme->theme.colors.background.g << 8) |
	                   theme->theme.colors.background.b, bg, 1);

	theme_format_color((theme->theme.colors.panel_bg.a << 24) |
	                   (theme->theme.colors.panel_bg.r << 16) |
	                   (theme->theme.colors.panel_bg.g << 8) |
	                   theme->theme.colors.panel_bg.b, panel_bg, 1);

	theme_format_color((theme->theme.colors.panel_border.a << 24) |
	                   (theme->theme.colors.panel_border.r << 16) |
	                   (theme->theme.colors.panel_border.g << 8) |
	                   theme->theme.colors.panel_border.b, panel_border, 1);

	theme_format_color((theme->theme.colors.text_primary.a << 24) |
	                   (theme->theme.colors.text_primary.r << 16) |
	                   (theme->theme.colors.text_primary.g << 8) |
	                   theme->theme.colors.text_primary.b, text_pri, 1);

	theme_format_color((theme->theme.colors.text_secondary.a << 24) |
	                   (theme->theme.colors.text_secondary.r << 16) |
	                   (theme->theme.colors.text_secondary.g << 8) |
	                   theme->theme.colors.text_secondary.b, text_sec, 1);

	theme_format_color((theme->theme.colors.text_highlight.a << 24) |
	                   (theme->theme.colors.text_highlight.r << 16) |
	                   (theme->theme.colors.text_highlight.g << 8) |
	                   theme->theme.colors.text_highlight.b, text_hi, 1);

	theme_format_color((theme->theme.colors.selection_bg.a << 24) |
	                   (theme->theme.colors.selection_bg.r << 16) |
	                   (theme->theme.colors.selection_bg.g << 8) |
	                   theme->theme.colors.selection_bg.b, sel_bg, 1);

	theme_format_color((theme->theme.colors.selection_border.a << 24) |
	                   (theme->theme.colors.selection_border.r << 16) |
	                   (theme->theme.colors.selection_border.g << 8) |
	                   theme->theme.colors.selection_border.b, sel_border, 1);

	theme_format_color((theme->theme.colors.scrollbar_bg.a << 24) |
	                   (theme->theme.colors.scrollbar_bg.r << 16) |
	                   (theme->theme.colors.scrollbar_bg.g << 8) |
	                   theme->theme.colors.scrollbar_bg.b, scroll_bg, 1);

	theme_format_color((theme->theme.colors.scrollbar_fg.a << 24) |
	                   (theme->theme.colors.scrollbar_fg.r << 16) |
	                   (theme->theme.colors.scrollbar_fg.g << 8) |
	                   theme->theme.colors.scrollbar_fg.b, scroll_fg, 1);

	fprintf(f, "{\n");
	fprintf(f, "  \"name\": \"%s\",\n", theme->meta.name);
	fprintf(f, "  \"author\": \"%s\",\n", theme->meta.author);
	fprintf(f, "  \"version\": \"%s\",\n", theme->meta.version);
	fprintf(f, "  \"description\": \"%s\",\n", theme->meta.description);
	fprintf(f, "\n");
	fprintf(f, "  \"background\": \"%s\",\n", bg);
	fprintf(f, "  \"panel_bg\": \"%s\",\n", panel_bg);
	fprintf(f, "  \"panel_border\": \"%s\",\n", panel_border);
	fprintf(f, "  \"text_primary\": \"%s\",\n", text_pri);
	fprintf(f, "  \"text_secondary\": \"%s\",\n", text_sec);
	fprintf(f, "  \"text_highlight\": \"%s\",\n", text_hi);
	fprintf(f, "  \"selection_bg\": \"%s\",\n", sel_bg);
	fprintf(f, "  \"selection_border\": \"%s\",\n", sel_border);
	fprintf(f, "  \"scrollbar_bg\": \"%s\",\n", scroll_bg);
	fprintf(f, "  \"scrollbar_fg\": \"%s\",\n", scroll_fg);
	fprintf(f, "\n");
	fprintf(f, "  \"thumbnail_width\": %d,\n", theme->theme.thumbnail_width);
	fprintf(f, "  \"thumbnail_height\": %d,\n", theme->theme.thumbnail_height);
	fprintf(f, "  \"item_spacing\": %d,\n", theme->theme.item_spacing);
	fprintf(f, "  \"panel_padding\": %d,\n", theme->theme.panel_padding);
	fprintf(f, "  \"corner_radius\": %d,\n", theme->theme.corner_radius);
	fprintf(f, "  \"font_size_title\": %d,\n", theme->theme.font_size_title);
	fprintf(f, "  \"font_size_item\": %d,\n", theme->theme.font_size_item);
	fprintf(f, "  \"font_size_info\": %d\n", theme->theme.font_size_info);
	fprintf(f, "}\n");

	fclose(f);

	theme->is_modified = 0;
	return 0;
}

int theme_save_current(void)
{
	theme_entry_t *current = theme_get_current();
	if (!current || current->is_builtin) return -1;

	if (current->filepath[0])
	{
		return theme_save_json(current->filepath, current);
	}
	return theme_save_json(THEME_USER_FILE, current);
}

// Built-in theme definitions
void theme_get_dark(gfx_theme_t *out_theme)
{
	memset(out_theme, 0, sizeof(gfx_theme_t));
	strcpy(out_theme->name, "Dark");

	out_theme->colors.background = gfx_color_hex(0xFF1a1a2e);
	out_theme->colors.panel_bg = gfx_color_hex(0xE016213e);
	out_theme->colors.panel_border = gfx_color_hex(0xFF0f3460);
	out_theme->colors.text_primary = gfx_color_hex(0xFFe0e0e0);
	out_theme->colors.text_secondary = gfx_color_hex(0xFF808080);
	out_theme->colors.text_highlight = gfx_color_hex(0xFFe94560);
	out_theme->colors.selection_bg = gfx_color_hex(0xCC0f3460);
	out_theme->colors.selection_border = gfx_color_hex(0xFFe94560);
	out_theme->colors.scrollbar_bg = gfx_color_hex(0x40ffffff);
	out_theme->colors.scrollbar_fg = gfx_color_hex(0xFFe94560);

	out_theme->font_size_title = 24;
	out_theme->font_size_item = 18;
	out_theme->font_size_info = 14;
	out_theme->thumbnail_width = 80;
	out_theme->thumbnail_height = 80;
	out_theme->item_spacing = 8;
	out_theme->panel_padding = 16;
	out_theme->corner_radius = 8;
}

void theme_get_light(gfx_theme_t *out_theme)
{
	memset(out_theme, 0, sizeof(gfx_theme_t));
	strcpy(out_theme->name, "Light");

	out_theme->colors.background = gfx_color_hex(0xFFf0f0f0);
	out_theme->colors.panel_bg = gfx_color_hex(0xF0ffffff);
	out_theme->colors.panel_border = gfx_color_hex(0xFFcccccc);
	out_theme->colors.text_primary = gfx_color_hex(0xFF202020);
	out_theme->colors.text_secondary = gfx_color_hex(0xFF606060);
	out_theme->colors.text_highlight = gfx_color_hex(0xFF0066cc);
	out_theme->colors.selection_bg = gfx_color_hex(0xCC0066cc);
	out_theme->colors.selection_border = gfx_color_hex(0xFF0066cc);
	out_theme->colors.scrollbar_bg = gfx_color_hex(0x40000000);
	out_theme->colors.scrollbar_fg = gfx_color_hex(0xFF0066cc);

	out_theme->font_size_title = 24;
	out_theme->font_size_item = 18;
	out_theme->font_size_info = 14;
	out_theme->thumbnail_width = 80;
	out_theme->thumbnail_height = 80;
	out_theme->item_spacing = 8;
	out_theme->panel_padding = 16;
	out_theme->corner_radius = 8;
}

void theme_get_retro(gfx_theme_t *out_theme)
{
	memset(out_theme, 0, sizeof(gfx_theme_t));
	strcpy(out_theme->name, "Retro");

	// CRT-inspired green/amber on black
	out_theme->colors.background = gfx_color_hex(0xFF0a0a0a);
	out_theme->colors.panel_bg = gfx_color_hex(0xD0101010);
	out_theme->colors.panel_border = gfx_color_hex(0xFF00aa00);
	out_theme->colors.text_primary = gfx_color_hex(0xFF00ff00);
	out_theme->colors.text_secondary = gfx_color_hex(0xFF008800);
	out_theme->colors.text_highlight = gfx_color_hex(0xFFffaa00);
	out_theme->colors.selection_bg = gfx_color_hex(0x80004400);
	out_theme->colors.selection_border = gfx_color_hex(0xFF00ff00);
	out_theme->colors.scrollbar_bg = gfx_color_hex(0x40004400);
	out_theme->colors.scrollbar_fg = gfx_color_hex(0xFF00ff00);

	out_theme->font_size_title = 24;
	out_theme->font_size_item = 18;
	out_theme->font_size_info = 14;
	out_theme->thumbnail_width = 80;
	out_theme->thumbnail_height = 80;
	out_theme->item_spacing = 8;
	out_theme->panel_padding = 16;
	out_theme->corner_radius = 4;
}

void theme_get_neon(gfx_theme_t *out_theme)
{
	memset(out_theme, 0, sizeof(gfx_theme_t));
	strcpy(out_theme->name, "Neon");

	// Cyberpunk neon colors
	out_theme->colors.background = gfx_color_hex(0xFF0d0d1a);
	out_theme->colors.panel_bg = gfx_color_hex(0xD01a1a2e);
	out_theme->colors.panel_border = gfx_color_hex(0xFFff00ff);
	out_theme->colors.text_primary = gfx_color_hex(0xFF00ffff);
	out_theme->colors.text_secondary = gfx_color_hex(0xFF0088aa);
	out_theme->colors.text_highlight = gfx_color_hex(0xFFff00ff);
	out_theme->colors.selection_bg = gfx_color_hex(0x80ff00ff);
	out_theme->colors.selection_border = gfx_color_hex(0xFF00ffff);
	out_theme->colors.scrollbar_bg = gfx_color_hex(0x40ff00ff);
	out_theme->colors.scrollbar_fg = gfx_color_hex(0xFF00ffff);

	out_theme->font_size_title = 24;
	out_theme->font_size_item = 18;
	out_theme->font_size_info = 14;
	out_theme->thumbnail_width = 80;
	out_theme->thumbnail_height = 80;
	out_theme->item_spacing = 8;
	out_theme->panel_padding = 16;
	out_theme->corner_radius = 0;  // Sharp edges for cyberpunk look
}

void theme_get_minimal(gfx_theme_t *out_theme)
{
	memset(out_theme, 0, sizeof(gfx_theme_t));
	strcpy(out_theme->name, "Minimal");

	// Clean minimal with subtle colors
	out_theme->colors.background = gfx_color_hex(0xFF1e1e1e);
	out_theme->colors.panel_bg = gfx_color_hex(0xF02a2a2a);
	out_theme->colors.panel_border = gfx_color_hex(0xFF3a3a3a);
	out_theme->colors.text_primary = gfx_color_hex(0xFFffffff);
	out_theme->colors.text_secondary = gfx_color_hex(0xFF888888);
	out_theme->colors.text_highlight = gfx_color_hex(0xFFffffff);
	out_theme->colors.selection_bg = gfx_color_hex(0x60ffffff);
	out_theme->colors.selection_border = gfx_color_hex(0xFFffffff);
	out_theme->colors.scrollbar_bg = gfx_color_hex(0x20ffffff);
	out_theme->colors.scrollbar_fg = gfx_color_hex(0x80ffffff);

	out_theme->font_size_title = 22;
	out_theme->font_size_item = 16;
	out_theme->font_size_info = 12;
	out_theme->thumbnail_width = 64;
	out_theme->thumbnail_height = 64;
	out_theme->item_spacing = 4;
	out_theme->panel_padding = 12;
	out_theme->corner_radius = 4;
}

// Color utilities
uint32_t theme_parse_color(const char *hex_str)
{
	if (!hex_str) return 0xFF000000;

	// Skip # if present
	if (hex_str[0] == '#') hex_str++;

	uint32_t color = 0;
	int len = strlen(hex_str);

	if (len == 6)
	{
		// RGB format
		color = 0xFF000000 | (uint32_t)strtoul(hex_str, NULL, 16);
	}
	else if (len == 8)
	{
		// ARGB format
		color = (uint32_t)strtoul(hex_str, NULL, 16);
	}

	return color;
}

void theme_format_color(uint32_t color, char *out_str, int include_alpha)
{
	if (!out_str) return;

	if (include_alpha)
	{
		sprintf(out_str, "#%08X", color);
	}
	else
	{
		sprintf(out_str, "#%06X", color & 0xFFFFFF);
	}
}

uint32_t theme_blend_colors(uint32_t color1, uint32_t color2, float factor)
{
	if (factor <= 0.0f) return color1;
	if (factor >= 1.0f) return color2;

	uint8_t a1 = (color1 >> 24) & 0xFF;
	uint8_t r1 = (color1 >> 16) & 0xFF;
	uint8_t g1 = (color1 >> 8) & 0xFF;
	uint8_t b1 = color1 & 0xFF;

	uint8_t a2 = (color2 >> 24) & 0xFF;
	uint8_t r2 = (color2 >> 16) & 0xFF;
	uint8_t g2 = (color2 >> 8) & 0xFF;
	uint8_t b2 = color2 & 0xFF;

	uint8_t a = (uint8_t)(a1 + (a2 - a1) * factor);
	uint8_t r = (uint8_t)(r1 + (r2 - r1) * factor);
	uint8_t g = (uint8_t)(g1 + (g2 - g1) * factor);
	uint8_t b = (uint8_t)(b1 + (b2 - b1) * factor);

	return (a << 24) | (r << 16) | (g << 8) | b;
}

uint32_t theme_lighten(uint32_t color, float amount)
{
	return theme_blend_colors(color, 0xFFFFFFFF, amount);
}

uint32_t theme_darken(uint32_t color, float amount)
{
	uint8_t a = (color >> 24) & 0xFF;
	return theme_blend_colors(color, (a << 24), amount);
}

const char** theme_get_color_names(int *count)
{
	if (count) *count = color_name_count;
	return color_names;
}

const char** theme_get_layout_names(int *count)
{
	if (count) *count = layout_name_count;
	return layout_names;
}

int theme_validate(theme_entry_t *theme)
{
	if (!theme) return 0;
	if (!theme->meta.name[0]) return 0;
	if (theme->theme.thumbnail_width <= 0) return 0;
	if (theme->theme.thumbnail_height <= 0) return 0;
	return 1;
}

void theme_reset_to_defaults(theme_entry_t *theme)
{
	if (theme)
	{
		theme_get_dark(&theme->theme);
		theme->is_modified = 1;
	}
}
