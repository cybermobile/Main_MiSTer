// boxart.h
// Boxart and game artwork management for MiSTer frontend
// 2024

#ifndef __BOXART_H__
#define __BOXART_H__

#include <inttypes.h>
#include "lib/imlib2/Imlib2.h"

// Maximum number of cached images
#define BOXART_CACHE_SIZE 32

// Maximum path length for artwork
#define BOXART_PATH_MAX 1024

// Artwork types
typedef enum {
	ARTWORK_BOXART = 0,    // Box/cover art
	ARTWORK_SNAP,          // In-game screenshot
	ARTWORK_TITLE,         // Title screen
	ARTWORK_MARQUEE,       // Arcade marquee
	ARTWORK_WHEEL,         // Wheel/logo art
	ARTWORK_COUNT
} artwork_type_t;

// Cached image entry
typedef struct {
	char path[BOXART_PATH_MAX];
	Imlib_Image image;
	int width;
	int height;
	uint32_t last_access;
	uint8_t loaded;
} boxart_cache_entry_t;

// Boxart system state
typedef struct {
	char base_path[BOXART_PATH_MAX];     // Base artwork directory
	char core_name[256];                  // Current core name
	boxart_cache_entry_t cache[BOXART_CACHE_SIZE];
	int cache_count;
	uint8_t enabled;
	uint8_t show_preview;
	Imlib_Image current_preview;
	Imlib_Image fallback_image;           // Generic fallback image
} boxart_state_t;

// Result from boxart lookup
typedef struct {
	Imlib_Image image;
	int width;
	int height;
	artwork_type_t type;
	uint8_t from_cache;
} boxart_result_t;

//// Functions ////

// Initialize the boxart system
void boxart_init(void);

// Shutdown and free all resources
void boxart_shutdown(void);

// Set the current core name (affects artwork path resolution)
void boxart_set_core(const char *core_name);

// Get the current core name
const char* boxart_get_core(void);

// Load artwork for a game
// Returns: 1 on success, 0 on failure
// result: filled with image data on success
int boxart_load(const char *game_name, artwork_type_t type, boxart_result_t *result);

// Load artwork with automatic type fallback (tries boxart -> snap -> title)
int boxart_load_any(const char *game_name, boxart_result_t *result);

// Free a specific cached image
void boxart_cache_free(const char *path);

// Clear entire cache
void boxart_cache_clear(void);

// Render boxart to the menu framebuffer
// x, y: position on screen
// max_width, max_height: maximum dimensions (image will be scaled to fit)
// Returns: 1 on success, 0 on failure
int boxart_render(Imlib_Image image, int x, int y, int max_width, int max_height);

// Render current preview (if any)
int boxart_render_preview(int x, int y, int max_width, int max_height);

// Set/get preview image for current selection
void boxart_set_preview(const char *game_name);
void boxart_clear_preview(void);

// Get artwork path for a game
// Returns: full path to artwork file, or NULL if not found
const char* boxart_get_path(const char *game_name, artwork_type_t type);

// Check if artwork exists for a game
int boxart_exists(const char *game_name, artwork_type_t type);

// Get cache statistics
int boxart_cache_count(void);
size_t boxart_cache_memory(void);

// Debug: print cache contents
void boxart_cache_debug(void);

// Get the current preview image (for external rendering)
Imlib_Image boxart_get_preview_image(void);

// Set base path for artwork
void boxart_set_base_path(const char *path);

// Enable/disable boxart system
void boxart_set_enabled(int enabled);
int boxart_is_enabled(void);

#endif // __BOXART_H__
