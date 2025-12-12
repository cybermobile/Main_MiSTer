// boxart.cpp
// Boxart and game artwork management for MiSTer frontend
// 2024

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>

#include "boxart.h"
#include "file_io.h"
#include "cfg.h"
#include "video.h"
#include "hardware.h"
#include "shmem.h"

// Artwork subdirectory names for each type
static const char* artwork_dirs[] = {
	"boxart",      // ARTWORK_BOXART
	"snap",        // ARTWORK_SNAP
	"title",       // ARTWORK_TITLE
	"marquee",     // ARTWORK_MARQUEE
	"wheel"        // ARTWORK_WHEEL
};

// Supported image extensions
static const char* image_extensions[] = {
	".png",
	".jpg",
	".jpeg",
	".bmp",
	NULL
};

// Global boxart state
static boxart_state_t boxart_state;

// Timer for cache management
static uint32_t cache_access_counter = 0;

// Forward declarations
static int find_cache_slot(void);
static int cache_lookup(const char *path);
static void cache_evict_oldest(void);
static void normalize_game_name(const char *input, char *output, size_t output_size);
static int file_exists(const char *path);

// Get framebuffer info from video.cpp
extern volatile uint32_t *fb_base;
extern int fb_width;
extern int fb_height;

void boxart_init(void)
{
	memset(&boxart_state, 0, sizeof(boxart_state));

	// Set default base path
	snprintf(boxart_state.base_path, BOXART_PATH_MAX, "%s/media", getRootDir());

	boxart_state.enabled = 1;
	boxart_state.show_preview = 1;
	boxart_state.current_preview = NULL;
	boxart_state.fallback_image = NULL;

	printf("Boxart system initialized. Base path: %s\n", boxart_state.base_path);
}

void boxart_shutdown(void)
{
	boxart_cache_clear();
	boxart_clear_preview();

	if (boxart_state.fallback_image)
	{
		imlib_context_set_image(boxart_state.fallback_image);
		imlib_free_image();
		boxart_state.fallback_image = NULL;
	}

	printf("Boxart system shutdown\n");
}

void boxart_set_core(const char *core_name)
{
	if (!core_name)
	{
		boxart_state.core_name[0] = '\0';
		return;
	}

	// Copy and clean up core name
	strncpy(boxart_state.core_name, core_name, sizeof(boxart_state.core_name) - 1);
	boxart_state.core_name[sizeof(boxart_state.core_name) - 1] = '\0';

	// Remove datecode suffix (e.g., "NES_20231015" -> "NES")
	char *p = strstr(boxart_state.core_name, "_20");
	if (p) *p = '\0';

	// Remove .rbf extension if present
	size_t len = strlen(boxart_state.core_name);
	if (len > 4 && strcasecmp(boxart_state.core_name + len - 4, ".rbf") == 0)
	{
		boxart_state.core_name[len - 4] = '\0';
	}

	printf("Boxart: Core set to '%s'\n", boxart_state.core_name);
}

const char* boxart_get_core(void)
{
	return boxart_state.core_name;
}

// Clean game name by removing region codes and version tags
// "Donkey Kong Country (USA) (Rev 1).zip" -> "Donkey Kong Country"
static void clean_game_name(const char *input, char *output, size_t output_size)
{
	if (!input || !output || output_size == 0) return;

	// Copy input first
	strncpy(output, input, output_size - 1);
	output[output_size - 1] = '\0';

	// Remove file extension
	char *dot = strrchr(output, '.');
	if (dot) *dot = '\0';

	// Remove parenthetical tags like (USA), (Rev 1), (Europe), etc.
	// Keep removing until no more are found
	char *paren;
	while ((paren = strrchr(output, '(')) != NULL)
	{
		// Check if there's a closing paren
		char *close = strchr(paren, ')');
		if (close)
		{
			// Remove the whole tag including leading spaces
			while (paren > output && paren[-1] == ' ') paren--;
			*paren = '\0';
		}
		else
		{
			break; // No closing paren, stop
		}
	}

	// Remove bracket tags like [!], [b], etc.
	char *bracket;
	while ((bracket = strrchr(output, '[')) != NULL)
	{
		char *close = strchr(bracket, ']');
		if (close)
		{
			while (bracket > output && bracket[-1] == ' ') bracket--;
			*bracket = '\0';
		}
		else
		{
			break;
		}
	}

	// Trim trailing spaces
	size_t len = strlen(output);
	while (len > 0 && output[len - 1] == ' ')
	{
		output[--len] = '\0';
	}
}

// Normalize a game name for matching (remove extension, special chars)
static void normalize_game_name(const char *input, char *output, size_t output_size)
{
	if (!input || !output || output_size == 0) return;

	size_t len = strlen(input);
	size_t out_idx = 0;

	// Find the last dot for extension removal
	const char *ext = strrchr(input, '.');
	size_t name_len = ext ? (size_t)(ext - input) : len;

	for (size_t i = 0; i < name_len && out_idx < output_size - 1; i++)
	{
		char c = input[i];

		// Convert to lowercase
		if (c >= 'A' && c <= 'Z')
		{
			c = c - 'A' + 'a';
		}

		// Keep alphanumeric chars, replace others with underscore
		if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
		{
			output[out_idx++] = c;
		}
		else if (out_idx > 0 && output[out_idx - 1] != '_')
		{
			output[out_idx++] = '_';
		}
	}

	// Remove trailing underscores
	while (out_idx > 0 && output[out_idx - 1] == '_')
	{
		out_idx--;
	}

	output[out_idx] = '\0';
}

static int file_exists(const char *path)
{
	struct stat st;
	return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

const char* boxart_get_path(const char *game_name, artwork_type_t type)
{
	static char path_buf[BOXART_PATH_MAX];
	char clean_name[256];
	char normalized_name[256];

	if (!game_name || !boxart_state.enabled) return NULL;
	if (type >= ARTWORK_COUNT) return NULL;
	if (boxart_state.core_name[0] == '\0') return NULL;

	// Get base name (without path)
	const char *base_name = strrchr(game_name, '/');
	base_name = base_name ? base_name + 1 : game_name;

	// Get clean name (without region codes, version tags, extension)
	// e.g., "Donkey Kong Country (USA) (Rev 1).zip" -> "Donkey Kong Country"
	clean_game_name(base_name, clean_name, sizeof(clean_name));

	// Get name without extension only
	char name_no_ext[256];
	strncpy(name_no_ext, base_name, sizeof(name_no_ext) - 1);
	name_no_ext[sizeof(name_no_ext) - 1] = '\0';
	char *dot = strrchr(name_no_ext, '.');
	if (dot) *dot = '\0';

	printf("Boxart: Looking for '%s' in core '%s'\n", game_name, boxart_state.core_name);
	printf("Boxart: Clean name: '%s'\n", clean_name);

	// Try each image extension
	for (int ext_idx = 0; image_extensions[ext_idx]; ext_idx++)
	{
		// Try 1: Clean name (most likely to match artwork)
		// e.g., /media/fat/media/SNES/boxart/Donkey Kong Country.png
		snprintf(path_buf, sizeof(path_buf), "%s/%s/%s/%s%s",
			boxart_state.base_path,
			boxart_state.core_name,
			artwork_dirs[type],
			clean_name,
			image_extensions[ext_idx]);

		printf("Boxart: Trying path: %s\n", path_buf);

		if (file_exists(path_buf))
		{
			printf("Boxart: FOUND!\n");
			return path_buf;
		}

		// Try 2: Exact filename without extension
		snprintf(path_buf, sizeof(path_buf), "%s/%s/%s/%s%s",
			boxart_state.base_path,
			boxart_state.core_name,
			artwork_dirs[type],
			name_no_ext,
			image_extensions[ext_idx]);

		if (file_exists(path_buf))
		{
			return path_buf;
		}

		// Try 3: Normalized name (lowercase, underscores)
		normalize_game_name(base_name, normalized_name, sizeof(normalized_name));
		snprintf(path_buf, sizeof(path_buf), "%s/%s/%s/%s%s",
			boxart_state.base_path,
			boxart_state.core_name,
			artwork_dirs[type],
			normalized_name,
			image_extensions[ext_idx]);

		if (file_exists(path_buf))
		{
			return path_buf;
		}
	}

	// Also try alternate path: /media/fat/boxart/{core}/{game}.png
	for (int ext_idx = 0; image_extensions[ext_idx]; ext_idx++)
	{
		snprintf(path_buf, sizeof(path_buf), "%s/boxart/%s/%s%s",
			getRootDir(),
			boxart_state.core_name,
			name_no_ext,
			image_extensions[ext_idx]);

		if (file_exists(path_buf))
		{
			return path_buf;
		}
	}

	return NULL;
}

int boxart_exists(const char *game_name, artwork_type_t type)
{
	return boxart_get_path(game_name, type) != NULL;
}

static int cache_lookup(const char *path)
{
	for (int i = 0; i < boxart_state.cache_count; i++)
	{
		if (boxart_state.cache[i].loaded &&
		    strcmp(boxart_state.cache[i].path, path) == 0)
		{
			boxart_state.cache[i].last_access = cache_access_counter++;
			return i;
		}
	}
	return -1;
}

static int find_cache_slot(void)
{
	// Find empty slot
	for (int i = 0; i < BOXART_CACHE_SIZE; i++)
	{
		if (!boxart_state.cache[i].loaded)
		{
			return i;
		}
	}

	// Cache is full, evict oldest
	cache_evict_oldest();
	return find_cache_slot();
}

static void cache_evict_oldest(void)
{
	if (boxart_state.cache_count == 0) return;

	int oldest_idx = -1;
	uint32_t oldest_access = UINT32_MAX;

	for (int i = 0; i < BOXART_CACHE_SIZE; i++)
	{
		if (boxart_state.cache[i].loaded &&
		    boxart_state.cache[i].last_access < oldest_access)
		{
			oldest_access = boxart_state.cache[i].last_access;
			oldest_idx = i;
		}
	}

	if (oldest_idx >= 0)
	{
		boxart_cache_entry_t *entry = &boxart_state.cache[oldest_idx];
		if (entry->image)
		{
			imlib_context_set_image(entry->image);
			imlib_free_image();
		}
		entry->image = NULL;
		entry->loaded = 0;
		entry->path[0] = '\0';
		boxart_state.cache_count--;
	}
}

int boxart_load(const char *game_name, artwork_type_t type, boxart_result_t *result)
{
	if (!result) return 0;
	memset(result, 0, sizeof(boxart_result_t));

	if (!boxart_state.enabled || !game_name) return 0;

	const char *path = boxart_get_path(game_name, type);
	if (!path) return 0;

	// Check cache first
	int cache_idx = cache_lookup(path);
	if (cache_idx >= 0)
	{
		boxart_cache_entry_t *entry = &boxart_state.cache[cache_idx];
		result->image = entry->image;
		result->width = entry->width;
		result->height = entry->height;
		result->type = type;
		result->from_cache = 1;
		return 1;
	}

	// Load the image
	Imlib_Load_Error error = IMLIB_LOAD_ERROR_NONE;
	Imlib_Image img = imlib_load_image_with_error_return(path, &error);

	if (!img)
	{
		if (error != IMLIB_LOAD_ERROR_FILE_DOES_NOT_EXIST)
		{
			printf("Boxart: Failed to load '%s', error %d\n", path, error);
		}
		return 0;
	}

	// Get image dimensions
	imlib_context_set_image(img);
	int width = imlib_image_get_width();
	int height = imlib_image_get_height();

	// Add to cache
	int slot = find_cache_slot();
	if (slot >= 0)
	{
		boxart_cache_entry_t *entry = &boxart_state.cache[slot];
		strncpy(entry->path, path, BOXART_PATH_MAX - 1);
		entry->path[BOXART_PATH_MAX - 1] = '\0';
		entry->image = img;
		entry->width = width;
		entry->height = height;
		entry->last_access = cache_access_counter++;
		entry->loaded = 1;
		boxart_state.cache_count++;
	}

	result->image = img;
	result->width = width;
	result->height = height;
	result->type = type;
	result->from_cache = 0;

	return 1;
}

int boxart_load_any(const char *game_name, boxart_result_t *result)
{
	// Try artwork types in priority order
	artwork_type_t types[] = {
		ARTWORK_BOXART,
		ARTWORK_TITLE,
		ARTWORK_SNAP,
		ARTWORK_MARQUEE,
		ARTWORK_WHEEL
	};

	for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++)
	{
		if (boxart_load(game_name, types[i], result))
		{
			return 1;
		}
	}

	return 0;
}

void boxart_cache_free(const char *path)
{
	if (!path) return;

	int idx = cache_lookup(path);
	if (idx >= 0)
	{
		boxart_cache_entry_t *entry = &boxart_state.cache[idx];
		if (entry->image)
		{
			imlib_context_set_image(entry->image);
			imlib_free_image();
		}
		entry->image = NULL;
		entry->loaded = 0;
		entry->path[0] = '\0';
		boxart_state.cache_count--;
	}
}

void boxart_cache_clear(void)
{
	for (int i = 0; i < BOXART_CACHE_SIZE; i++)
	{
		if (boxart_state.cache[i].loaded && boxart_state.cache[i].image)
		{
			imlib_context_set_image(boxart_state.cache[i].image);
			imlib_free_image();
		}
		boxart_state.cache[i].image = NULL;
		boxart_state.cache[i].loaded = 0;
		boxart_state.cache[i].path[0] = '\0';
	}
	boxart_state.cache_count = 0;
	cache_access_counter = 0;
}

int boxart_render(Imlib_Image image, int x, int y, int max_width, int max_height)
{
	if (!image || max_width <= 0 || max_height <= 0) return 0;

	imlib_context_set_image(image);
	int src_w = imlib_image_get_width();
	int src_h = imlib_image_get_height();

	if (src_w <= 0 || src_h <= 0) return 0;

	// Calculate scaled dimensions maintaining aspect ratio
	float scale_x = (float)max_width / (float)src_w;
	float scale_y = (float)max_height / (float)src_h;
	float scale = (scale_x < scale_y) ? scale_x : scale_y;

	int dst_w = (int)(src_w * scale);
	int dst_h = (int)(src_h * scale);

	// Center within the max bounds
	int dst_x = x + (max_width - dst_w) / 2;
	int dst_y = y + (max_height - dst_h) / 2;

	// Get the menu framebuffer image (we need to blend onto it)
	// This function should be called from video_boxart_render() which has access to the FB

	return 1;  // Rendering will be done by video.cpp
}

void boxart_set_preview(const char *game_name)
{
	boxart_clear_preview();

	if (!game_name || !boxart_state.enabled) return;

	boxart_result_t result;
	if (boxart_load_any(game_name, &result))
	{
		boxart_state.current_preview = result.image;
	}
}

void boxart_clear_preview(void)
{
	// Don't free - it's in the cache
	boxart_state.current_preview = NULL;
}

int boxart_render_preview(int x, int y, int max_width, int max_height)
{
	if (!boxart_state.current_preview) return 0;
	return boxart_render(boxart_state.current_preview, x, y, max_width, max_height);
}

int boxart_cache_count(void)
{
	return boxart_state.cache_count;
}

size_t boxart_cache_memory(void)
{
	size_t total = 0;
	for (int i = 0; i < BOXART_CACHE_SIZE; i++)
	{
		if (boxart_state.cache[i].loaded && boxart_state.cache[i].image)
		{
			// Each pixel is 32-bit ARGB
			total += boxart_state.cache[i].width * boxart_state.cache[i].height * 4;
		}
	}
	return total;
}

void boxart_cache_debug(void)
{
	printf("=== Boxart Cache Debug ===\n");
	printf("Cache entries: %d / %d\n", boxart_state.cache_count, BOXART_CACHE_SIZE);
	printf("Memory usage: %zu bytes\n", boxart_cache_memory());
	printf("Core: %s\n", boxart_state.core_name);
	printf("Base path: %s\n", boxart_state.base_path);

	for (int i = 0; i < BOXART_CACHE_SIZE; i++)
	{
		if (boxart_state.cache[i].loaded)
		{
			printf("  [%d] %s (%dx%d, access=%u)\n",
				i,
				boxart_state.cache[i].path,
				boxart_state.cache[i].width,
				boxart_state.cache[i].height,
				boxart_state.cache[i].last_access);
		}
	}
	printf("==========================\n");
}

// Get the current preview image (for external rendering)
Imlib_Image boxart_get_preview_image(void)
{
	return boxart_state.current_preview;
}

// Set base path for artwork
void boxart_set_base_path(const char *path)
{
	if (path)
	{
		strncpy(boxart_state.base_path, path, BOXART_PATH_MAX - 1);
		boxart_state.base_path[BOXART_PATH_MAX - 1] = '\0';
	}
}

// Enable/disable boxart system
void boxart_set_enabled(int enabled)
{
	boxart_state.enabled = enabled ? 1 : 0;
}

int boxart_is_enabled(void)
{
	return boxart_state.enabled;
}
