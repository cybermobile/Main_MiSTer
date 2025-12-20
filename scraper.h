// scraper.h
// Multi-source artwork scraper for MiSTer graphical frontend
// Supports: ScreenScraper.fr, TheGamesDB, SteamGridDB
// 2024

#ifndef __SCRAPER_H__
#define __SCRAPER_H__

#include <inttypes.h>

// Maximum number of concurrent downloads
#define SCRAPER_MAX_CONCURRENT 4

// Scraper sources (in priority order)
typedef enum {
	SCRAPER_SOURCE_SCREENSCRAPER = 0,  // ScreenScraper.fr (best for retro games)
	SCRAPER_SOURCE_THEGAMESDB,         // TheGamesDB (good metadata)
	SCRAPER_SOURCE_STEAMGRIDDB,        // SteamGridDB (good for box art)
	SCRAPER_SOURCE_COUNT
} scraper_source_t;

// Scraper status
typedef enum {
	SCRAPER_IDLE = 0,
	SCRAPER_RUNNING,
	SCRAPER_PAUSED,
	SCRAPER_COMPLETE,
	SCRAPER_ERROR
} scraper_status_t;

// Artwork types to download
typedef enum {
	SCRAPE_ART_BOX = (1 << 0),        // Box/cover art
	SCRAPE_ART_SNAP = (1 << 1),       // In-game screenshot
	SCRAPE_ART_TITLE = (1 << 2),      // Title screen
	SCRAPE_ART_WHEEL = (1 << 3),      // Wheel/logo art
	SCRAPE_ART_MARQUEE = (1 << 4),    // Arcade marquee
	SCRAPE_ART_ALL = 0xFF             // All types
} scrape_artwork_flags_t;

// Per-game scrape result
typedef struct {
	char game_name[256];
	char game_path[1024];
	int found;                         // Artwork was found
	int downloaded;                    // Artwork was downloaded
	scraper_source_t source;           // Which source provided it
	char artwork_path[1024];           // Path where artwork was saved
	char error_msg[256];               // Error message if failed
} scraper_result_t;

// Scraper progress/stats
typedef struct {
	int total_games;                   // Total games to process
	int processed;                     // Games processed so far
	int found;                         // Games with artwork found
	int downloaded;                    // Artwork files downloaded
	int already_had;                   // Games that already had artwork
	int failed;                        // Games where scraping failed
	char current_game[256];            // Currently processing
	char current_source[64];           // Current source being tried
	uint32_t start_time;               // When scraping started
	uint32_t eta_seconds;              // Estimated time remaining
} scraper_progress_t;

// API credentials structure
typedef struct {
	// ScreenScraper credentials
	char screenscraper_user[64];
	char screenscraper_pass[64];
	char screenscraper_devid[64];
	char screenscraper_devpass[64];

	// TheGamesDB API key
	char thegamesdb_key[128];

	// SteamGridDB API key
	char steamgriddb_key[128];
} scraper_credentials_t;

// Scraper configuration
typedef struct {
	scraper_credentials_t creds;
	uint8_t sources_enabled[SCRAPER_SOURCE_COUNT];  // Which sources to use
	uint8_t artwork_types;                          // scrape_artwork_flags_t
	uint8_t overwrite_existing;                     // Replace existing artwork
	uint8_t auto_scrape;                            // Scrape on-demand when missing
	uint16_t request_delay_ms;                      // Delay between requests (rate limit)
} scraper_config_t;

//// Core Functions ////

// Initialize scraper system
void scraper_init(void);

// Shutdown scraper
void scraper_shutdown(void);

// Load/save configuration
int scraper_load_config(void);
int scraper_save_config(void);

// Get current configuration (for editing)
scraper_config_t* scraper_get_config(void);

//// Scraping Operations ////

// Scrape a single game (blocking)
// Returns: 1 on success (artwork found), 0 on failure
int scraper_scrape_game(const char *game_name, const char *game_path,
                        const char *core_name, scraper_result_t *result);

// Scrape a single game (non-blocking, for auto-scrape)
// Returns: scrape ID (> 0) or 0 on error
int scraper_scrape_game_async(const char *game_name, const char *game_path,
                               const char *core_name);

// Check if async scrape is complete
int scraper_async_complete(int scrape_id, scraper_result_t *result);

// Scrape all games in a system directory (non-blocking)
// Returns: 1 on success (scraping started), 0 on error
int scraper_scrape_system(const char *system_name);

// Scrape all games across all systems (non-blocking)
int scraper_scrape_all(void);

// Stop current scraping operation
void scraper_stop(void);

// Pause/resume scraping
void scraper_pause(void);
void scraper_resume(void);

//// Status Functions ////

// Get current scraper status
scraper_status_t scraper_get_status(void);

// Get scraping progress
scraper_progress_t* scraper_get_progress(void);

// Check if a specific game has artwork
int scraper_has_artwork(const char *game_path, const char *core_name);

//// Auto-Scrape Integration ////

// Try to auto-scrape missing artwork (called by boxart system)
// Returns: 1 if scraping was started, 0 if not needed or error
int scraper_auto_scrape(const char *game_name, const char *game_path,
                        const char *core_name);

// Poll for auto-scrape completions (call periodically)
void scraper_poll(void);

//// Platform Mapping ////

// Get ScreenScraper system ID for a MiSTer core
int scraper_get_screenscraper_system(const char *core_name);

// Get TheGamesDB platform ID for a MiSTer core
int scraper_get_thegamesdb_platform(const char *core_name);

// Get SteamGridDB platform for a MiSTer core
const char* scraper_get_steamgriddb_platform(const char *core_name);

//// Source-Specific Functions ////

// Source names for display
const char* scraper_source_name(scraper_source_t source);

// Check if a source is configured (has valid credentials)
int scraper_source_configured(scraper_source_t source);

// Test API connection for a source
int scraper_test_source(scraper_source_t source);

#endif // __SCRAPER_H__
