// scraper.h
// Artwork scraper for MiSTer graphical frontend
// Uses libretro-thumbnails (no API key required)
// 2024

#ifndef __SCRAPER_H__
#define __SCRAPER_H__

#include <inttypes.h>

// Scraper status
typedef enum {
	SCRAPER_IDLE = 0,
	SCRAPER_RUNNING,
	SCRAPER_COMPLETE,
	SCRAPER_ERROR
} scraper_status_t;

// Per-game scrape result
typedef struct {
	char game_name[256];
	char game_path[1024];
	int found;                         // Artwork was found
	int downloaded;                    // Artwork was downloaded
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
	uint32_t start_time;               // When scraping started
} scraper_progress_t;

//// Core Functions ////

// Initialize scraper system
void scraper_init(void);

// Shutdown scraper
void scraper_shutdown(void);

//// Scraping Operations ////

// Scrape a single game (blocking)
// Returns: 1 on success (artwork found), 0 on failure
int scraper_scrape_game(const char *game_name, const char *game_path,
                        const char *core_name, scraper_result_t *result);

// Scrape all games in a system directory (non-blocking)
// Returns: 1 on success (scraping started), 0 on error
int scraper_scrape_system(const char *system_name);

// Stop current scraping operation
void scraper_stop(void);

//// Status Functions ////

// Get current scraper status
scraper_status_t scraper_get_status(void);

// Get scraping progress
scraper_progress_t* scraper_get_progress(void);

// Check if a specific game has artwork
int scraper_has_artwork(const char *game_path, const char *core_name);

//// Platform Mapping ////

// Get libretro-thumbnails repository name for a MiSTer core
const char* scraper_get_libretro_repo(const char *core_name);

#endif // __SCRAPER_H__
