// search.h
// Search and filter system for MiSTer modern frontend
// Provides text search, filtering, and quick navigation
// 2024

#ifndef __SEARCH_H__
#define __SEARCH_H__

#include <inttypes.h>
#include "gamedb.h"

// Maximum search query length
#define SEARCH_MAX_QUERY 128

// Maximum search results
#define SEARCH_MAX_RESULTS 256

// Search modes
typedef enum {
	SEARCH_MODE_NAME = 0,      // Search by game name
	SEARCH_MODE_DEVELOPER,     // Search by developer
	SEARCH_MODE_YEAR,          // Search by year
	SEARCH_MODE_ALL            // Search all fields
} search_mode_t;

// Filter state
typedef struct {
	gamedb_genre_t genre;          // Filter by genre (0 = any)
	gamedb_region_t region;        // Filter by region (0 = any)
	uint16_t year_min;             // Minimum year (0 = any)
	uint16_t year_max;             // Maximum year (0 = any)
	uint8_t players_min;           // Minimum players (0 = any)
	uint8_t players_max;           // Maximum players (0 = any)
	uint8_t favorites_only;        // Show only favorites
	uint8_t recently_played;       // Show recently played
} search_filter_t;

// Search result entry
typedef struct {
	int index;                     // Original index in list
	char name[256];                // Display name
	char path[1024];               // File path
	int score;                     // Match score (higher = better)
	void *item_data;               // Pointer to original item data
} search_result_t;

// Search state
typedef struct {
	char query[SEARCH_MAX_QUERY];  // Current search query
	int query_length;              // Query length
	search_mode_t mode;            // Search mode
	search_filter_t filter;        // Active filters
	search_result_t results[SEARCH_MAX_RESULTS];
	int result_count;              // Number of results
	int selected_result;           // Currently selected result
	uint8_t active;                // Search is active
	uint8_t dirty;                 // Results need refresh
} search_state_t;

// Keyboard state for virtual keyboard
typedef struct {
	int cursor_x;                  // Cursor X position in keyboard
	int cursor_y;                  // Cursor Y position in keyboard
	uint8_t caps_lock;             // Caps lock state
	uint8_t shift;                 // Shift state
	uint8_t visible;               // Keyboard visible
} search_keyboard_t;

//// Core Functions ////

// Initialize search system
void search_init(void);

// Shutdown search system
void search_shutdown(void);

// Reset search state
void search_reset(void);

//// Search Operations ////

// Start search mode (shows search UI)
void search_start(void);

// End search mode
void search_end(void);

// Toggle search mode on/off
void search_toggle(void);

// Check if search is active
int search_is_active(void);

// Set search query
void search_set_query(const char *query);

// Get current query
const char* search_get_query(void);

// Append character to query
void search_append_char(char c);

// Delete last character from query
void search_delete_char(void);

// Clear query
void search_clear_query(void);

// Execute search with current query and filters
void search_execute(void);

// Get search results
search_result_t* search_get_results(int *count);

// Get selected result
search_result_t* search_get_selected(void);

// Select next result
void search_select_next(void);

// Select previous result
void search_select_prev(void);

// Confirm selection (returns selected result or NULL)
search_result_t* search_confirm(void);

//// Filter Operations ////

// Set genre filter
void search_set_genre(gamedb_genre_t genre);

// Set region filter
void search_set_region(gamedb_region_t region);

// Set year range filter
void search_set_year_range(uint16_t min_year, uint16_t max_year);

// Set player count filter
void search_set_players(uint8_t min_players, uint8_t max_players);

// Set favorites only filter
void search_set_favorites_only(int enabled);

// Set recently played filter
void search_set_recently_played(int enabled);

// Clear all filters
void search_clear_filters(void);

// Get current filter state
search_filter_t* search_get_filters(void);

// Check if any filters are active
int search_has_active_filters(void);

//// Virtual Keyboard ////

// Show virtual keyboard
void search_show_keyboard(void);

// Hide virtual keyboard
void search_hide_keyboard(void);

// Check if keyboard is visible
int search_keyboard_visible(void);

// Get keyboard state
search_keyboard_t* search_get_keyboard(void);

// Move keyboard cursor
void search_keyboard_move(int dx, int dy);

// Press current keyboard key
void search_keyboard_press(void);

// Toggle shift
void search_keyboard_shift(void);

// Toggle caps lock
void search_keyboard_caps(void);

// Get character at keyboard position
char search_keyboard_char_at(int x, int y);

//// Quick Navigation ////

// Jump to letter (A-Z, 0-9)
void search_jump_to_letter(char letter);

// Jump to next letter group
void search_next_letter(void);

// Jump to previous letter group
void search_prev_letter(void);

// Get current letter position
char search_get_current_letter(void);

//// Utility Functions ////

// Calculate fuzzy match score between query and target
int search_fuzzy_score(const char *query, const char *target);

// Check if item passes current filters
int search_item_passes_filter(gamedb_entry_t *entry, search_filter_t *filter);

// Sort results by score
void search_sort_results(void);

// Debug: print search state
void search_debug_print(void);

#endif // __SEARCH_H__
