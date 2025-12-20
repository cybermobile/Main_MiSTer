// playtime.h
// Playtime tracking system for MiSTer modern frontend
// Inspired by Analogue 3D Library feature
// 2024

#ifndef __PLAYTIME_H__
#define __PLAYTIME_H__

#include <inttypes.h>
#include <time.h>

// Maximum number of tracked games
#define PLAYTIME_MAX_ENTRIES 500

// Playtime data file
#define PLAYTIME_FILE "/media/fat/config/playtime.dat"

// Game history entry
typedef struct {
	char game_path[512];          // Full path to game file
	char game_name[256];          // Display name
	char core_name[64];           // Core used to play
	uint32_t play_count;          // Number of times played
	uint32_t total_seconds;       // Total playtime in seconds
	time_t first_played;          // Unix timestamp of first play
	time_t last_played;           // Unix timestamp of last play
	uint32_t last_session_seconds; // Duration of last session
} playtime_entry_t;

// Playtime database
typedef struct {
	playtime_entry_t entries[PLAYTIME_MAX_ENTRIES];
	int count;
	int current_index;            // Index of currently playing game (-1 if none)
	time_t session_start;         // When current session started
} playtime_db_t;

//// Core Functions ////

// Initialize playtime system
void playtime_init(void);

// Shutdown and save playtime data
void playtime_shutdown(void);

// Load playtime database from file
int playtime_load(void);

// Save playtime database to file
int playtime_save(void);

//// Session Tracking ////

// Start tracking a game session
void playtime_start_session(const char *game_path, const char *game_name, const char *core_name);

// End current game session
void playtime_end_session(void);

// Update session (call periodically to save progress)
void playtime_update(void);

// Check if a session is active
int playtime_session_active(void);

// Get current session duration in seconds
uint32_t playtime_get_session_duration(void);

//// Query Functions ////

// Get entry for a game (NULL if not found)
playtime_entry_t* playtime_get_entry(const char *game_path);

// Get entry by index
playtime_entry_t* playtime_get_entry_by_index(int index);

// Get total playtime for a game in seconds
uint32_t playtime_get_total(const char *game_path);

// Get play count for a game
uint32_t playtime_get_play_count(const char *game_path);

// Get last played timestamp
time_t playtime_get_last_played(const char *game_path);

// Get first played timestamp
time_t playtime_get_first_played(const char *game_path);

// Get total number of tracked games
int playtime_get_count(void);

//// Recently Played ////

// Get recently played games (sorted by last_played, newest first)
// Returns number of entries filled
int playtime_get_recent(playtime_entry_t **out_entries, int max_count);

// Get most played games (sorted by total_seconds, highest first)
int playtime_get_most_played(playtime_entry_t **out_entries, int max_count);

//// Formatting Utilities ////

// Format playtime as string (e.g., "12h 34m" or "45m" or "< 1m")
void playtime_format_duration(uint32_t seconds, char *out_str, int out_size);

// Format last played as relative string (e.g., "Today", "Yesterday", "3 days ago")
void playtime_format_relative_time(time_t timestamp, char *out_str, int out_size);

// Format date as string (e.g., "Dec 12, 2024")
void playtime_format_date(time_t timestamp, char *out_str, int out_size);

//// Database Management ////

// Clear all playtime data
void playtime_clear_all(void);

// Remove entry for a game
int playtime_remove_entry(const char *game_path);

// Get database pointer (for advanced access)
playtime_db_t* playtime_get_db(void);

#endif // __PLAYTIME_H__
