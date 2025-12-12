// gamedb.h
// Game database for MiSTer modern frontend
// Provides metadata lookup for games (title, year, genre, etc.)
// 2024

#ifndef __GAMEDB_H__
#define __GAMEDB_H__

#include <inttypes.h>

// Maximum entries in memory
#define GAMEDB_MAX_ENTRIES 8192

// Maximum string lengths
#define GAMEDB_NAME_MAX 256
#define GAMEDB_DESC_MAX 512
#define GAMEDB_PATH_MAX 1024

// Game regions
typedef enum {
	REGION_UNKNOWN = 0,
	REGION_USA,
	REGION_EUROPE,
	REGION_JAPAN,
	REGION_WORLD,
	REGION_OTHER
} gamedb_region_t;

// Game genres
typedef enum {
	GENRE_UNKNOWN = 0,
	GENRE_ACTION,
	GENRE_ADVENTURE,
	GENRE_ARCADE,
	GENRE_BOARD,
	GENRE_EDUCATIONAL,
	GENRE_FIGHTING,
	GENRE_PLATFORMER,
	GENRE_PUZZLE,
	GENRE_RACING,
	GENRE_RPG,
	GENRE_SHOOTER,
	GENRE_SIMULATION,
	GENRE_SPORTS,
	GENRE_STRATEGY,
	GENRE_OTHER
} gamedb_genre_t;

// Game entry structure
typedef struct {
	char name[GAMEDB_NAME_MAX];           // Display name (cleaned up)
	char filename[GAMEDB_NAME_MAX];       // Original filename for matching
	char description[GAMEDB_DESC_MAX];    // Game description
	char developer[128];                   // Developer name
	char publisher[128];                   // Publisher name
	uint16_t year;                         // Release year
	gamedb_genre_t genre;                  // Game genre
	gamedb_region_t region;                // Game region
	uint8_t players_min;                   // Minimum players
	uint8_t players_max;                   // Maximum players
	uint32_t crc32;                        // ROM CRC32 for matching
	uint32_t size;                         // ROM size in bytes
	char serial[32];                       // Game serial/product code
	uint8_t is_favorite;                   // User favorite flag
	uint32_t play_count;                   // Times played
	uint32_t play_time;                    // Total play time in seconds
	uint32_t last_played;                  // Unix timestamp of last play
} gamedb_entry_t;

// Database state
typedef struct {
	char core_name[64];                    // Current core
	gamedb_entry_t *entries;               // Array of entries
	int entry_count;                       // Number of entries
	int capacity;                          // Array capacity
	uint8_t loaded;                        // Database loaded flag
	char db_path[GAMEDB_PATH_MAX];         // Path to database file
} gamedb_state_t;

// Favorites list
typedef struct {
	char paths[1024][GAMEDB_PATH_MAX];     // Favorite game paths
	int count;
} gamedb_favorites_t;

// Search/filter result
typedef struct {
	gamedb_entry_t *entry;
	int original_index;
	int score;  // Match score for fuzzy search
} gamedb_result_t;

//// Core Functions ////

// Initialize/shutdown the database system
void gamedb_init(void);
void gamedb_shutdown(void);

// Load database for a specific core
// Searches for: /media/fat/gamedb/{core}.xml, .json, .dat
int gamedb_load(const char *core_name);

// Unload current database
void gamedb_unload(void);

// Check if database is loaded
int gamedb_is_loaded(void);

// Get current core name
const char* gamedb_get_core(void);

//// Lookup Functions ////

// Look up game by filename (with fuzzy matching)
gamedb_entry_t* gamedb_lookup_filename(const char *filename);

// Look up game by CRC32
gamedb_entry_t* gamedb_lookup_crc32(uint32_t crc32);

// Look up game by serial/product code
gamedb_entry_t* gamedb_lookup_serial(const char *serial);

// Get entry by index
gamedb_entry_t* gamedb_get_entry(int index);

// Get total entry count
int gamedb_get_count(void);

//// Search Functions ////

// Search games by name (fuzzy)
// Returns number of results, fills results array
int gamedb_search(const char *query, gamedb_result_t *results, int max_results);

// Filter by genre
int gamedb_filter_genre(gamedb_genre_t genre, gamedb_result_t *results, int max_results);

// Filter by year range
int gamedb_filter_year(uint16_t min_year, uint16_t max_year, gamedb_result_t *results, int max_results);

// Filter by region
int gamedb_filter_region(gamedb_region_t region, gamedb_result_t *results, int max_results);

// Filter by player count
int gamedb_filter_players(uint8_t min_players, uint8_t max_players, gamedb_result_t *results, int max_results);

//// Favorites Functions ////

// Load favorites for current core
int gamedb_load_favorites(void);

// Save favorites for current core
int gamedb_save_favorites(void);

// Add game to favorites
int gamedb_add_favorite(const char *path);

// Remove game from favorites
int gamedb_remove_favorite(const char *path);

// Check if game is favorite
int gamedb_is_favorite(const char *path);

// Toggle favorite status
int gamedb_toggle_favorite(const char *path);

// Get favorites list
gamedb_favorites_t* gamedb_get_favorites(void);

//// Play Statistics ////

// Record game play session
void gamedb_record_play(const char *path, uint32_t duration_seconds);

// Get play count for a game
uint32_t gamedb_get_play_count(const char *path);

// Get total play time for a game
uint32_t gamedb_get_play_time(const char *path);

// Get last played timestamp
uint32_t gamedb_get_last_played(const char *path);

// Load/save play statistics
int gamedb_load_stats(void);
int gamedb_save_stats(void);

//// Utility Functions ////

// Parse region from filename (e.g., "(USA)", "(Europe)")
gamedb_region_t gamedb_parse_region(const char *filename);

// Parse year from filename or string
uint16_t gamedb_parse_year(const char *str);

// Clean up game name (remove tags, extensions)
void gamedb_clean_name(const char *input, char *output, size_t output_size);

// Get genre name string
const char* gamedb_genre_name(gamedb_genre_t genre);

// Get region name string
const char* gamedb_region_name(gamedb_region_t region);

// Calculate fuzzy match score between two strings
int gamedb_fuzzy_score(const char *query, const char *target);

//// Database Import Functions ////

// Import from No-Intro DAT/XML file
int gamedb_import_nointro(const char *xml_path);

// Import from custom JSON format
int gamedb_import_json(const char *json_path);

// Export database to JSON
int gamedb_export_json(const char *json_path);

// Debug: print database contents
void gamedb_debug_print(void);

#endif // __GAMEDB_H__
