// scraper.cpp
// Multi-source artwork scraper for MiSTer graphical frontend
// 2024

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <pthread.h>
#include <ctype.h>
#include <time.h>

#include "scraper.h"
#include "file_io.h"
#include "cfg.h"

// Platform ID mappings

// ScreenScraper system IDs
// See: https://screenscraper.fr/webapi2/systemesListe.php
typedef struct {
	const char *core_name;
	int system_id;
} screenscraper_platform_t;

static const screenscraper_platform_t screenscraper_platforms[] = {
	{"SNES",          4},    // Super Nintendo
	{"NES",           3},    // Nintendo Entertainment System
	{"Genesis",       1},    // Sega Mega Drive / Genesis
	{"MegaCD",        20},   // Sega CD
	{"SMS",           2},    // Sega Master System
	{"GameGear",      21},   // Game Gear
	{"TurboGrafx16",  31},   // PC Engine / TurboGrafx-16
	{"TGFX16",        31},   // PC Engine / TurboGrafx-16 (alt)
	{"PCE",           31},   // PC Engine
	{"GBA",           12},   // Game Boy Advance
	{"GB",            9},    // Game Boy
	{"GBC",           10},   // Game Boy Color
	{"N64",           14},   // Nintendo 64
	{"PSX",           57},   // PlayStation
	{"NEOGEO",        142},  // Neo Geo AES
	{"Arcade",        75},   // MAME
	{"ATARI2600",     26},   // Atari 2600
	{"ATARI5200",     40},   // Atari 5200
	{"ATARI7800",     41},   // Atari 7800
	{"Lynx",          28},   // Atari Lynx
	{"Jaguar",        27},   // Atari Jaguar
	{"ColecoVision",  48},   // ColecoVision
	{"Intellivision", 115},  // Intellivision
	{"Vectrex",       102},  // Vectrex
	{"WonderSwan",    45},   // WonderSwan
	{"WSC",           46},   // WonderSwan Color
	{"NGP",           82},   // Neo Geo Pocket
	{"NGPC",          83},   // Neo Geo Pocket Color
	{"32X",           19},   // Sega 32X
	{"Saturn",        22},   // Sega Saturn
	{"Dreamcast",     23},   // Sega Dreamcast
	{"SG1000",        109},  // Sega SG-1000
	{"Amiga",         64},   // Amiga
	{"C64",           66},   // Commodore 64
	{"AO486",         135},  // DOS / PC
	{"ZX81",          77},   // ZX81
	{"ZXSpectrum",    76},   // ZX Spectrum
	{NULL, 0}
};

// TheGamesDB platform IDs
// See: https://api.thegamesdb.net/Platforms
typedef struct {
	const char *core_name;
	int platform_id;
} thegamesdb_platform_t;

static const thegamesdb_platform_t thegamesdb_platforms[] = {
	{"SNES",          6},    // Super Nintendo (SNES)
	{"NES",           7},    // Nintendo Entertainment System (NES)
	{"Genesis",       18},   // Sega Genesis
	{"MegaCD",        21},   // Sega CD
	{"SMS",           35},   // Sega Master System
	{"GameGear",      20},   // Sega Game Gear
	{"TurboGrafx16",  34},   // TurboGrafx 16
	{"GBA",           5},    // Nintendo Game Boy Advance
	{"GB",            4},    // Nintendo Game Boy
	{"GBC",           41},   // Nintendo Game Boy Color
	{"N64",           3},    // Nintendo 64
	{"PSX",           10},   // Sony Playstation
	{"NEOGEO",        24},   // Neo Geo
	{"Arcade",        23},   // Arcade
	{"ATARI2600",     22},   // Atari 2600
	{"ATARI5200",     26},   // Atari 5200
	{"ATARI7800",     27},   // Atari 7800
	{"Lynx",          28},   // Atari Lynx
	{"Jaguar",        29},   // Atari Jaguar
	{"32X",           33},   // Sega 32X
	{"Saturn",        17},   // Sega Saturn
	{"Dreamcast",     16},   // Sega Dreamcast
	{"Amiga",         4911}, // Commodore Amiga
	{"C64",           40},   // Commodore 64
	{NULL, 0}
};

// SteamGridDB platform names
typedef struct {
	const char *core_name;
	const char *platform;
} steamgriddb_platform_t;

static const steamgriddb_platform_t steamgriddb_platforms[] = {
	{"SNES",          "snes"},
	{"NES",           "nes"},
	{"Genesis",       "genesis,megadrive"},
	{"MegaCD",        "segacd"},
	{"SMS",           "mastersystem"},
	{"GameGear",      "gamegear"},
	{"TurboGrafx16",  "turbografx16,pcengine"},
	{"GBA",           "gba"},
	{"GB",            "gameboy"},
	{"GBC",           "gameboycolor"},
	{"N64",           "n64"},
	{"PSX",           "psx"},
	{"NEOGEO",        "neogeo"},
	{"Arcade",        "arcade"},
	{"ATARI2600",     "atari2600"},
	{"ATARI5200",     "atari5200"},
	{"ATARI7800",     "atari7800"},
	{NULL, NULL}
};

// Global scraper state
static scraper_config_t scraper_config;
static scraper_progress_t scraper_progress;
static scraper_status_t scraper_status = SCRAPER_IDLE;
static pthread_t scraper_thread;
static int scraper_thread_active = 0;
static int scraper_stop_requested = 0;

// Auto-scrape queue
#define AUTO_SCRAPE_QUEUE_SIZE 16
typedef struct {
	char game_name[256];
	char game_path[1024];
	char core_name[64];
	int scrape_id;
	int complete;
	scraper_result_t result;
} auto_scrape_entry_t;

static auto_scrape_entry_t auto_scrape_queue[AUTO_SCRAPE_QUEUE_SIZE];
static int auto_scrape_next_id = 1;
static pthread_mutex_t scraper_mutex = PTHREAD_MUTEX_INITIALIZER;

// Config file path
#define SCRAPER_CONFIG_FILE "/media/fat/config/scraper.cfg"

// Forward declarations
static void* scraper_thread_func(void *arg);
static int scrape_from_screenscraper(const char *game_name, const char *core_name,
                                     const char *output_dir, scraper_result_t *result);
static int scrape_from_thegamesdb(const char *game_name, const char *core_name,
                                   const char *output_dir, scraper_result_t *result);
static int scrape_from_steamgriddb(const char *game_name, const char *core_name,
                                    const char *output_dir, scraper_result_t *result);
static void clean_game_name(const char *input, char *output, size_t output_size);
static int download_file(const char *url, const char *output_path, const char *auth_header);
static int file_exists(const char *path);

//// Core Functions ////

void scraper_init(void)
{
	memset(&scraper_config, 0, sizeof(scraper_config));
	memset(&scraper_progress, 0, sizeof(scraper_progress));
	memset(auto_scrape_queue, 0, sizeof(auto_scrape_queue));

	// Enable all sources by default
	for (int i = 0; i < SCRAPER_SOURCE_COUNT; i++) {
		scraper_config.sources_enabled[i] = 1;
	}

	scraper_config.artwork_types = SCRAPE_ART_BOX;  // Box art by default
	scraper_config.request_delay_ms = 200;           // Rate limit
	scraper_config.auto_scrape = 1;                  // Enable auto-scrape

	scraper_load_config();

	printf("Scraper: Initialized\n");
}

void scraper_shutdown(void)
{
	scraper_stop();
	scraper_save_config();
	printf("Scraper: Shutdown\n");
}

int scraper_load_config(void)
{
	FILE *f = fopen(SCRAPER_CONFIG_FILE, "r");
	if (!f) return 0;

	char line[512];
	while (fgets(line, sizeof(line), f)) {
		// Remove newline
		char *nl = strchr(line, '\n');
		if (nl) *nl = '\0';

		// Parse key=value
		char *eq = strchr(line, '=');
		if (!eq) continue;
		*eq = '\0';
		char *key = line;
		char *value = eq + 1;

		// Credentials
		if (strcmp(key, "screenscraper_user") == 0) {
			strncpy(scraper_config.creds.screenscraper_user, value, 63);
		} else if (strcmp(key, "screenscraper_pass") == 0) {
			strncpy(scraper_config.creds.screenscraper_pass, value, 63);
		} else if (strcmp(key, "thegamesdb_key") == 0) {
			strncpy(scraper_config.creds.thegamesdb_key, value, 127);
		} else if (strcmp(key, "steamgriddb_key") == 0) {
			strncpy(scraper_config.creds.steamgriddb_key, value, 127);
		}
		// Options
		else if (strcmp(key, "auto_scrape") == 0) {
			scraper_config.auto_scrape = atoi(value) ? 1 : 0;
		} else if (strcmp(key, "overwrite") == 0) {
			scraper_config.overwrite_existing = atoi(value) ? 1 : 0;
		} else if (strcmp(key, "source_screenscraper") == 0) {
			scraper_config.sources_enabled[SCRAPER_SOURCE_SCREENSCRAPER] = atoi(value) ? 1 : 0;
		} else if (strcmp(key, "source_thegamesdb") == 0) {
			scraper_config.sources_enabled[SCRAPER_SOURCE_THEGAMESDB] = atoi(value) ? 1 : 0;
		} else if (strcmp(key, "source_steamgriddb") == 0) {
			scraper_config.sources_enabled[SCRAPER_SOURCE_STEAMGRIDDB] = atoi(value) ? 1 : 0;
		}
	}

	fclose(f);
	printf("Scraper: Loaded config from %s\n", SCRAPER_CONFIG_FILE);
	return 1;
}

int scraper_save_config(void)
{
	FILE *f = fopen(SCRAPER_CONFIG_FILE, "w");
	if (!f) return 0;

	fprintf(f, "# MiSTer Artwork Scraper Configuration\n\n");

	fprintf(f, "# ScreenScraper.fr credentials (free account at screenscraper.fr)\n");
	fprintf(f, "screenscraper_user=%s\n", scraper_config.creds.screenscraper_user);
	fprintf(f, "screenscraper_pass=%s\n", scraper_config.creds.screenscraper_pass);
	fprintf(f, "\n");

	fprintf(f, "# TheGamesDB API key (free at thegamesdb.net)\n");
	fprintf(f, "thegamesdb_key=%s\n", scraper_config.creds.thegamesdb_key);
	fprintf(f, "\n");

	fprintf(f, "# SteamGridDB API key (free at steamgriddb.com)\n");
	fprintf(f, "steamgriddb_key=%s\n", scraper_config.creds.steamgriddb_key);
	fprintf(f, "\n");

	fprintf(f, "# Options\n");
	fprintf(f, "auto_scrape=%d\n", scraper_config.auto_scrape);
	fprintf(f, "overwrite=%d\n", scraper_config.overwrite_existing);
	fprintf(f, "source_screenscraper=%d\n", scraper_config.sources_enabled[SCRAPER_SOURCE_SCREENSCRAPER]);
	fprintf(f, "source_thegamesdb=%d\n", scraper_config.sources_enabled[SCRAPER_SOURCE_THEGAMESDB]);
	fprintf(f, "source_steamgriddb=%d\n", scraper_config.sources_enabled[SCRAPER_SOURCE_STEAMGRIDDB]);

	fclose(f);
	return 1;
}

scraper_config_t* scraper_get_config(void)
{
	return &scraper_config;
}

//// Helper Functions ////

static void clean_game_name(const char *input, char *output, size_t output_size)
{
	if (!input || !output || output_size == 0) return;

	strncpy(output, input, output_size - 1);
	output[output_size - 1] = '\0';

	// Remove file extension
	char *dot = strrchr(output, '.');
	if (dot) *dot = '\0';

	// Remove parenthetical and bracket tags
	char *p;
	while ((p = strrchr(output, '(')) != NULL) {
		char *close = strchr(p, ')');
		if (close) {
			while (p > output && p[-1] == ' ') p--;
			*p = '\0';
		} else break;
	}
	while ((p = strrchr(output, '[')) != NULL) {
		char *close = strchr(p, ']');
		if (close) {
			while (p > output && p[-1] == ' ') p--;
			*p = '\0';
		} else break;
	}

	// Trim trailing spaces
	size_t len = strlen(output);
	while (len > 0 && output[len - 1] == ' ') output[--len] = '\0';
}

static int file_exists(const char *path)
{
	struct stat st;
	return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

// URL encode a string
static void url_encode(const char *input, char *output, size_t output_size)
{
	static const char *hex = "0123456789ABCDEF";
	size_t out_idx = 0;

	for (size_t i = 0; input[i] && out_idx < output_size - 4; i++) {
		unsigned char c = (unsigned char)input[i];
		if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
			output[out_idx++] = c;
		} else if (c == ' ') {
			output[out_idx++] = '+';
		} else {
			output[out_idx++] = '%';
			output[out_idx++] = hex[c >> 4];
			output[out_idx++] = hex[c & 0x0F];
		}
	}
	output[out_idx] = '\0';
}

// Escape a string for safe inclusion between single quotes in a shell command.
// Each ' is rewritten as '\'' so the value cannot break out of its quoting.
// The caller keeps the surrounding '%s' quotes in the format string.
// This is required because interpolated values (ROM filenames, URLs from the
// scraper API) routinely contain apostrophes -- e.g. "Rock 'n' Roll Racing" --
// which would otherwise terminate the quoting and allow shell injection.
static void shell_sq_escape(const char *in, char *out, size_t out_size)
{
	size_t o = 0;
	if (!in) { if (out_size) out[0] = '\0'; return; }
	for (size_t i = 0; in[i]; i++)
	{
		if (in[i] == '\'')
		{
			if (o + 4 >= out_size) break;
			out[o++] = '\''; out[o++] = '\\'; out[o++] = '\''; out[o++] = '\'';
		}
		else
		{
			if (o + 1 >= out_size) break;
			out[o++] = in[i];
		}
	}
	out[o] = '\0';
}

// Download a file using curl
static int download_file(const char *url, const char *output_path, const char *auth_header)
{
	char cmd[4096];
	char e_out[2048], e_url[2048], e_auth[1024];
	int ret;

	shell_sq_escape(output_path, e_out, sizeof(e_out));
	shell_sq_escape(url, e_url, sizeof(e_url));

	// Build curl command
	if (auth_header && auth_header[0]) {
		shell_sq_escape(auth_header, e_auth, sizeof(e_auth));
		snprintf(cmd, sizeof(cmd),
			"curl -sk -o '%s' -H '%s' '%s' 2>/dev/null",
			e_out, e_auth, e_url);
	} else {
		snprintf(cmd, sizeof(cmd),
			"curl -sk -o '%s' '%s' 2>/dev/null",
			e_out, e_url);
	}

	ret = system(cmd);
	if (ret != 0) return 0;

	// Verify file was created and has content
	struct stat st;
	if (stat(output_path, &st) != 0 || st.st_size < 100) {
		unlink(output_path);  // Remove empty/invalid file
		return 0;
	}

	return 1;
}

// Execute curl and capture output
static int curl_get_json(const char *url, const char *auth_header, char *output, size_t output_size)
{
	char cmd[4096];
	char e_url[2048], e_auth[1024];
	FILE *fp;

	shell_sq_escape(url, e_url, sizeof(e_url));

	if (auth_header && auth_header[0]) {
		shell_sq_escape(auth_header, e_auth, sizeof(e_auth));
		snprintf(cmd, sizeof(cmd),
			"curl -sk -H '%s' '%s' 2>/dev/null",
			e_auth, e_url);
	} else {
		snprintf(cmd, sizeof(cmd),
			"curl -sk '%s' 2>/dev/null",
			e_url);
	}

	fp = popen(cmd, "r");
	if (!fp) return 0;

	size_t bytes_read = fread(output, 1, output_size - 1, fp);
	output[bytes_read] = '\0';
	pclose(fp);

	return bytes_read > 0 ? 1 : 0;
}

// Simple JSON string extraction (no external library needed)
static int json_extract_string(const char *json, const char *key, char *value, size_t value_size)
{
	char search[128];
	snprintf(search, sizeof(search), "\"%s\":", key);

	const char *p = strstr(json, search);
	if (!p) return 0;

	p += strlen(search);
	while (*p == ' ' || *p == '\t') p++;

	if (*p != '"') return 0;
	p++;

	size_t i = 0;
	while (*p && *p != '"' && i < value_size - 1) {
		if (*p == '\\' && *(p + 1)) {
			p++;  // Skip escape
		}
		value[i++] = *p++;
	}
	value[i] = '\0';

	return 1;
}

//// Platform Mapping ////

int scraper_get_screenscraper_system(const char *core_name)
{
	if (!core_name) return 0;

	for (int i = 0; screenscraper_platforms[i].core_name; i++) {
		if (strcasecmp(core_name, screenscraper_platforms[i].core_name) == 0) {
			return screenscraper_platforms[i].system_id;
		}
	}
	return 0;
}

int scraper_get_thegamesdb_platform(const char *core_name)
{
	if (!core_name) return 0;

	for (int i = 0; thegamesdb_platforms[i].core_name; i++) {
		if (strcasecmp(core_name, thegamesdb_platforms[i].core_name) == 0) {
			return thegamesdb_platforms[i].platform_id;
		}
	}
	return 0;
}

const char* scraper_get_steamgriddb_platform(const char *core_name)
{
	if (!core_name) return NULL;

	for (int i = 0; steamgriddb_platforms[i].core_name; i++) {
		if (strcasecmp(core_name, steamgriddb_platforms[i].core_name) == 0) {
			return steamgriddb_platforms[i].platform;
		}
	}
	return NULL;
}

//// Source-Specific Scrapers ////

// ScreenScraper.fr API
static int scrape_from_screenscraper(const char *game_name, const char *core_name,
                                     const char *output_dir, scraper_result_t *result)
{
	if (!scraper_config.creds.screenscraper_user[0] ||
	    !scraper_config.creds.screenscraper_pass[0]) {
		strncpy(result->error_msg, "ScreenScraper credentials not configured", sizeof(result->error_msg));
		return 0;
	}

	int system_id = scraper_get_screenscraper_system(core_name);
	if (system_id == 0) {
		strncpy(result->error_msg, "Unknown system for ScreenScraper", sizeof(result->error_msg));
		return 0;
	}

	char clean_name[256];
	char encoded_name[512];
	clean_game_name(game_name, clean_name, sizeof(clean_name));
	url_encode(clean_name, encoded_name, sizeof(encoded_name));

	// Build API URL
	char url[1024];
	snprintf(url, sizeof(url),
		"https://www.screenscraper.fr/api2/jeuRecherche.php"
		"?devid=%s&devpassword=%s"
		"&softname=MiSTer-Scraper"
		"&ssid=%s&sspassword=%s"
		"&systemeid=%d"
		"&recherche=%s"
		"&output=json",
		scraper_config.creds.screenscraper_devid[0] ? scraper_config.creds.screenscraper_devid : "xxx",
		scraper_config.creds.screenscraper_devpass[0] ? scraper_config.creds.screenscraper_devpass : "yyy",
		scraper_config.creds.screenscraper_user,
		scraper_config.creds.screenscraper_pass,
		system_id,
		encoded_name);

	char response[32768];
	if (!curl_get_json(url, NULL, response, sizeof(response))) {
		strncpy(result->error_msg, "Failed to query ScreenScraper API", sizeof(result->error_msg));
		return 0;
	}

	// Look for box art URL in response
	// ScreenScraper returns: "url_box_us", "url_box_eu", "url_box_wor"
	char box_url[512] = {0};
	if (!json_extract_string(response, "url_box_us", box_url, sizeof(box_url))) {
		if (!json_extract_string(response, "url_box_eu", box_url, sizeof(box_url))) {
			if (!json_extract_string(response, "url_box_wor", box_url, sizeof(box_url))) {
				// Try media_box2d as fallback
				json_extract_string(response, "url_media_box2d", box_url, sizeof(box_url));
			}
		}
	}

	if (!box_url[0]) {
		strncpy(result->error_msg, "No box art found on ScreenScraper", sizeof(result->error_msg));
		return 0;
	}

	// Download the image
	char output_path[1024];
	snprintf(output_path, sizeof(output_path), "%s/%s.png", output_dir, clean_name);

	// Create directory if needed
	char mkdir_cmd[2048];
	char e_dir[1600];
	shell_sq_escape(output_dir, e_dir, sizeof(e_dir));
	snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p '%s'", e_dir);
	system(mkdir_cmd);

	if (!download_file(box_url, output_path, NULL)) {
		strncpy(result->error_msg, "Failed to download image from ScreenScraper", sizeof(result->error_msg));
		return 0;
	}

	result->found = 1;
	result->downloaded = 1;
	result->source = SCRAPER_SOURCE_SCREENSCRAPER;
	strncpy(result->artwork_path, output_path, sizeof(result->artwork_path));

	return 1;
}

// TheGamesDB API
static int scrape_from_thegamesdb(const char *game_name, const char *core_name,
                                   const char *output_dir, scraper_result_t *result)
{
	if (!scraper_config.creds.thegamesdb_key[0]) {
		strncpy(result->error_msg, "TheGamesDB API key not configured", sizeof(result->error_msg));
		return 0;
	}

	int platform_id = scraper_get_thegamesdb_platform(core_name);
	if (platform_id == 0) {
		strncpy(result->error_msg, "Unknown platform for TheGamesDB", sizeof(result->error_msg));
		return 0;
	}

	char clean_name[256];
	char encoded_name[512];
	clean_game_name(game_name, clean_name, sizeof(clean_name));
	url_encode(clean_name, encoded_name, sizeof(encoded_name));

	// Search for game
	char url[1024];
	snprintf(url, sizeof(url),
		"https://api.thegamesdb.net/v1/Games/ByGameName"
		"?apikey=%s"
		"&name=%s"
		"&filter[platform]=%d"
		"&include=boxart",
		scraper_config.creds.thegamesdb_key,
		encoded_name,
		platform_id);

	char response[65536];
	if (!curl_get_json(url, NULL, response, sizeof(response))) {
		strncpy(result->error_msg, "Failed to query TheGamesDB API", sizeof(result->error_msg));
		return 0;
	}

	// Extract base URL and boxart filename
	char base_url[256] = {0};
	char boxart_filename[256] = {0};

	// TheGamesDB returns base_url in data.images, then filename in boxart array
	json_extract_string(response, "base_url", base_url, sizeof(base_url));

	// Look for boxart/front
	const char *boxart_pos = strstr(response, "\"boxart\"");
	if (boxart_pos) {
		const char *front_pos = strstr(boxart_pos, "\"front\"");
		if (front_pos) {
			json_extract_string(front_pos, "filename", boxart_filename, sizeof(boxart_filename));
		}
	}

	if (!base_url[0] || !boxart_filename[0]) {
		strncpy(result->error_msg, "No box art found on TheGamesDB", sizeof(result->error_msg));
		return 0;
	}

	// Build full image URL
	char image_url[512];
	snprintf(image_url, sizeof(image_url), "%s/boxart/%s", base_url, boxart_filename);

	// Download the image
	char output_path[1024];
	const char *ext = strrchr(boxart_filename, '.');
	snprintf(output_path, sizeof(output_path), "%s/%s%s",
		output_dir, clean_name, ext ? ext : ".jpg");

	char mkdir_cmd[2048];
	char e_dir[1600];
	shell_sq_escape(output_dir, e_dir, sizeof(e_dir));
	snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p '%s'", e_dir);
	system(mkdir_cmd);

	if (!download_file(image_url, output_path, NULL)) {
		strncpy(result->error_msg, "Failed to download image from TheGamesDB", sizeof(result->error_msg));
		return 0;
	}

	result->found = 1;
	result->downloaded = 1;
	result->source = SCRAPER_SOURCE_THEGAMESDB;
	strncpy(result->artwork_path, output_path, sizeof(result->artwork_path));

	return 1;
}

// SteamGridDB API
static int scrape_from_steamgriddb(const char *game_name, const char *core_name,
                                    const char *output_dir, scraper_result_t *result)
{
	if (!scraper_config.creds.steamgriddb_key[0]) {
		strncpy(result->error_msg, "SteamGridDB API key not configured", sizeof(result->error_msg));
		return 0;
	}

	char clean_name[256];
	char encoded_name[512];
	clean_game_name(game_name, clean_name, sizeof(clean_name));
	url_encode(clean_name, encoded_name, sizeof(encoded_name));

	char auth_header[256];
	snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s",
		scraper_config.creds.steamgriddb_key);

	// Search for game ID
	char url[512];
	snprintf(url, sizeof(url),
		"https://www.steamgriddb.com/api/v2/search/autocomplete/%s",
		encoded_name);

	char response[8192];
	if (!curl_get_json(url, auth_header, response, sizeof(response))) {
		strncpy(result->error_msg, "Failed to search SteamGridDB", sizeof(result->error_msg));
		return 0;
	}

	// Extract first game ID
	char game_id_str[32] = {0};
	const char *id_pos = strstr(response, "\"id\":");
	if (id_pos) {
		id_pos += 5;
		int i = 0;
		while (*id_pos >= '0' && *id_pos <= '9' && i < 31) {
			game_id_str[i++] = *id_pos++;
		}
		game_id_str[i] = '\0';
	}

	if (!game_id_str[0]) {
		strncpy(result->error_msg, "Game not found on SteamGridDB", sizeof(result->error_msg));
		return 0;
	}

	// Get grids (box art) for this game
	snprintf(url, sizeof(url),
		"https://www.steamgriddb.com/api/v2/grids/game/%s?dimensions=600x900,460x215",
		game_id_str);

	if (!curl_get_json(url, auth_header, response, sizeof(response))) {
		strncpy(result->error_msg, "Failed to get grids from SteamGridDB", sizeof(result->error_msg));
		return 0;
	}

	// Extract first image URL
	char image_url[512] = {0};
	json_extract_string(response, "url", image_url, sizeof(image_url));

	// Unescape JSON slashes
	char *p = image_url;
	while (*p) {
		if (*p == '\\' && *(p + 1) == '/') {
			memmove(p, p + 1, strlen(p));
		}
		p++;
	}

	if (!image_url[0]) {
		strncpy(result->error_msg, "No grid art found on SteamGridDB", sizeof(result->error_msg));
		return 0;
	}

	// Determine extension from URL
	const char *ext = ".png";
	if (strstr(image_url, ".jpg") || strstr(image_url, ".jpeg")) ext = ".jpg";
	else if (strstr(image_url, ".webp")) ext = ".webp";

	char output_path[1024];
	snprintf(output_path, sizeof(output_path), "%s/%s%s", output_dir, clean_name, ext);

	char mkdir_cmd[2048];
	char e_dir[1600];
	shell_sq_escape(output_dir, e_dir, sizeof(e_dir));
	snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p '%s'", e_dir);
	system(mkdir_cmd);

	if (!download_file(image_url, output_path, NULL)) {
		strncpy(result->error_msg, "Failed to download image from SteamGridDB", sizeof(result->error_msg));
		return 0;
	}

	result->found = 1;
	result->downloaded = 1;
	result->source = SCRAPER_SOURCE_STEAMGRIDDB;
	strncpy(result->artwork_path, output_path, sizeof(result->artwork_path));

	return 1;
}

//// Scraping Operations ////

int scraper_scrape_game(const char *game_name, const char *game_path,
                        const char *core_name, scraper_result_t *result)
{
	if (!result) return 0;
	memset(result, 0, sizeof(scraper_result_t));

	if (!game_name || !core_name) return 0;

	strncpy(result->game_name, game_name, sizeof(result->game_name) - 1);
	strncpy(result->game_path, game_path ? game_path : "", sizeof(result->game_path) - 1);

	// Build output directory
	char output_dir[1024];
	snprintf(output_dir, sizeof(output_dir), "%s/media/%s/boxart",
		getRootDir(), core_name);

	// Check if artwork already exists (unless overwrite enabled)
	char clean_name[256];
	clean_game_name(game_name, clean_name, sizeof(clean_name));

	char existing_path[1024];
	const char *exts[] = {".png", ".jpg", ".jpeg", ".bmp", NULL};
	for (int i = 0; exts[i]; i++) {
		snprintf(existing_path, sizeof(existing_path), "%s/%s%s", output_dir, clean_name, exts[i]);
		if (file_exists(existing_path) && !scraper_config.overwrite_existing) {
			result->found = 1;
			strncpy(result->artwork_path, existing_path, sizeof(result->artwork_path));
			return 1;  // Already has artwork
		}
	}

	// Try each enabled source in order
	scraper_source_t sources[] = {
		SCRAPER_SOURCE_SCREENSCRAPER,
		SCRAPER_SOURCE_THEGAMESDB,
		SCRAPER_SOURCE_STEAMGRIDDB
	};

	for (int i = 0; i < SCRAPER_SOURCE_COUNT; i++) {
		if (!scraper_config.sources_enabled[sources[i]]) continue;
		if (!scraper_source_configured(sources[i])) continue;

		int success = 0;
		switch (sources[i]) {
			case SCRAPER_SOURCE_SCREENSCRAPER:
				success = scrape_from_screenscraper(game_name, core_name, output_dir, result);
				break;
			case SCRAPER_SOURCE_THEGAMESDB:
				success = scrape_from_thegamesdb(game_name, core_name, output_dir, result);
				break;
			case SCRAPER_SOURCE_STEAMGRIDDB:
				success = scrape_from_steamgriddb(game_name, core_name, output_dir, result);
				break;
			default:
				break;
		}

		if (success) {
			printf("Scraper: Found '%s' on %s\n", game_name, scraper_source_name(sources[i]));
			return 1;
		}

		// Rate limit between sources
		usleep(scraper_config.request_delay_ms * 1000);
	}

	strncpy(result->error_msg, "Not found on any source", sizeof(result->error_msg));
	return 0;
}

//// Auto-Scrape Integration ////

int scraper_scrape_game_async(const char *game_name, const char *game_path,
                               const char *core_name)
{
	if (!scraper_config.auto_scrape) return 0;

	pthread_mutex_lock(&scraper_mutex);

	// Find empty slot in queue
	int slot = -1;
	for (int i = 0; i < AUTO_SCRAPE_QUEUE_SIZE; i++) {
		if (auto_scrape_queue[i].scrape_id == 0) {
			slot = i;
			break;
		}
	}

	if (slot < 0) {
		pthread_mutex_unlock(&scraper_mutex);
		return 0;  // Queue full
	}

	auto_scrape_entry_t *entry = &auto_scrape_queue[slot];
	strncpy(entry->game_name, game_name, sizeof(entry->game_name) - 1);
	strncpy(entry->game_path, game_path ? game_path : "", sizeof(entry->game_path) - 1);
	strncpy(entry->core_name, core_name, sizeof(entry->core_name) - 1);
	entry->scrape_id = auto_scrape_next_id++;
	entry->complete = 0;
	memset(&entry->result, 0, sizeof(entry->result));

	int scrape_id = entry->scrape_id;

	pthread_mutex_unlock(&scraper_mutex);

	// TODO: Actually start async scraping thread
	// For now, just do it synchronously on next poll

	return scrape_id;
}

int scraper_async_complete(int scrape_id, scraper_result_t *result)
{
	if (scrape_id <= 0) return 0;

	pthread_mutex_lock(&scraper_mutex);

	for (int i = 0; i < AUTO_SCRAPE_QUEUE_SIZE; i++) {
		if (auto_scrape_queue[i].scrape_id == scrape_id) {
			if (auto_scrape_queue[i].complete) {
				if (result) {
					memcpy(result, &auto_scrape_queue[i].result, sizeof(scraper_result_t));
				}
				// Clear the slot
				auto_scrape_queue[i].scrape_id = 0;
				pthread_mutex_unlock(&scraper_mutex);
				return 1;
			}
			pthread_mutex_unlock(&scraper_mutex);
			return 0;  // Not complete yet
		}
	}

	pthread_mutex_unlock(&scraper_mutex);
	return -1;  // Not found
}

int scraper_auto_scrape(const char *game_name, const char *game_path,
                        const char *core_name)
{
	if (!scraper_config.auto_scrape) return 0;
	if (!game_name || !core_name) return 0;

	// Check if any source is configured
	int any_configured = 0;
	for (int i = 0; i < SCRAPER_SOURCE_COUNT; i++) {
		if (scraper_config.sources_enabled[i] && scraper_source_configured((scraper_source_t)i)) {
			any_configured = 1;
			break;
		}
	}
	if (!any_configured) return 0;

	return scraper_scrape_game_async(game_name, game_path, core_name);
}

void scraper_poll(void)
{
	pthread_mutex_lock(&scraper_mutex);

	// Process one pending async scrape per poll
	for (int i = 0; i < AUTO_SCRAPE_QUEUE_SIZE; i++) {
		if (auto_scrape_queue[i].scrape_id > 0 && !auto_scrape_queue[i].complete) {
			auto_scrape_entry_t *entry = &auto_scrape_queue[i];

			pthread_mutex_unlock(&scraper_mutex);

			// Do the scrape (blocking)
			scraper_scrape_game(entry->game_name, entry->game_path,
			                    entry->core_name, &entry->result);

			pthread_mutex_lock(&scraper_mutex);
			entry->complete = 1;
			break;  // Only one per poll
		}
	}

	pthread_mutex_unlock(&scraper_mutex);
}

//// Status Functions ////

scraper_status_t scraper_get_status(void)
{
	return scraper_status;
}

scraper_progress_t* scraper_get_progress(void)
{
	return &scraper_progress;
}

int scraper_has_artwork(const char *game_path, const char *core_name)
{
	if (!game_path || !core_name) return 0;

	// Get just the filename
	const char *filename = strrchr(game_path, '/');
	filename = filename ? filename + 1 : game_path;

	char clean_name[256];
	clean_game_name(filename, clean_name, sizeof(clean_name));

	char check_path[1024];
	const char *exts[] = {".png", ".jpg", ".jpeg", ".bmp", NULL};

	for (int i = 0; exts[i]; i++) {
		snprintf(check_path, sizeof(check_path), "%s/media/%s/boxart/%s%s",
			getRootDir(), core_name, clean_name, exts[i]);
		if (file_exists(check_path)) return 1;
	}

	return 0;
}

void scraper_stop(void)
{
	scraper_stop_requested = 1;

	if (scraper_thread_active) {
		pthread_join(scraper_thread, NULL);
		scraper_thread_active = 0;
	}

	scraper_status = SCRAPER_IDLE;
	scraper_stop_requested = 0;
}

void scraper_pause(void)
{
	if (scraper_status == SCRAPER_RUNNING) {
		scraper_status = SCRAPER_PAUSED;
	}
}

void scraper_resume(void)
{
	if (scraper_status == SCRAPER_PAUSED) {
		scraper_status = SCRAPER_RUNNING;
	}
}

//// Source Info ////

const char* scraper_source_name(scraper_source_t source)
{
	switch (source) {
		case SCRAPER_SOURCE_SCREENSCRAPER: return "ScreenScraper";
		case SCRAPER_SOURCE_THEGAMESDB:    return "TheGamesDB";
		case SCRAPER_SOURCE_STEAMGRIDDB:   return "SteamGridDB";
		default: return "Unknown";
	}
}

int scraper_source_configured(scraper_source_t source)
{
	switch (source) {
		case SCRAPER_SOURCE_SCREENSCRAPER:
			return scraper_config.creds.screenscraper_user[0] &&
			       scraper_config.creds.screenscraper_pass[0];
		case SCRAPER_SOURCE_THEGAMESDB:
			return scraper_config.creds.thegamesdb_key[0] != '\0';
		case SCRAPER_SOURCE_STEAMGRIDDB:
			return scraper_config.creds.steamgriddb_key[0] != '\0';
		default:
			return 0;
	}
}

int scraper_test_source(scraper_source_t source)
{
	// Quick test that the API key/credentials work
	char response[4096];
	int success = 0;

	switch (source) {
		case SCRAPER_SOURCE_SCREENSCRAPER: {
			if (!scraper_source_configured(source)) return 0;
			char url[512];
			snprintf(url, sizeof(url),
				"https://www.screenscraper.fr/api2/ssuserInfos.php"
				"?devid=xxx&devpassword=yyy"
				"&softname=MiSTer-Scraper"
				"&ssid=%s&sspassword=%s"
				"&output=json",
				scraper_config.creds.screenscraper_user,
				scraper_config.creds.screenscraper_pass);
			success = curl_get_json(url, NULL, response, sizeof(response));
			break;
		}
		case SCRAPER_SOURCE_THEGAMESDB: {
			if (!scraper_source_configured(source)) return 0;
			char url[256];
			snprintf(url, sizeof(url),
				"https://api.thegamesdb.net/v1/Platforms?apikey=%s",
				scraper_config.creds.thegamesdb_key);
			success = curl_get_json(url, NULL, response, sizeof(response));
			if (success) {
				success = strstr(response, "\"code\":200") != NULL ||
				          strstr(response, "\"data\"") != NULL;
			}
			break;
		}
		case SCRAPER_SOURCE_STEAMGRIDDB: {
			if (!scraper_source_configured(source)) return 0;
			char auth[256];
			snprintf(auth, sizeof(auth), "Authorization: Bearer %s",
				scraper_config.creds.steamgriddb_key);
			success = curl_get_json(
				"https://www.steamgriddb.com/api/v2/search/autocomplete/mario",
				auth, response, sizeof(response));
			if (success) {
				success = strstr(response, "\"success\":true") != NULL ||
				          strstr(response, "\"data\"") != NULL;
			}
			break;
		}
		default:
			break;
	}

	return success;
}

//// Bulk Scraping ////

static void* scraper_thread_func(void *arg)
{
	const char *system_name = (const char *)arg;

	scraper_status = SCRAPER_RUNNING;
	memset(&scraper_progress, 0, sizeof(scraper_progress));
	scraper_progress.start_time = (uint32_t)time(NULL);

	// Build games directory path
	char games_dir[1024];
	snprintf(games_dir, sizeof(games_dir), "%s/games/%s",
		getRootDir(), system_name);

	// Count games first
	DIR *dir = opendir(games_dir);
	if (!dir) {
		scraper_status = SCRAPER_ERROR;
		return NULL;
	}

	struct dirent *entry;
	while ((entry = readdir(dir)) != NULL) {
		if (entry->d_name[0] == '.') continue;
		scraper_progress.total_games++;
	}
	rewinddir(dir);

	// Process each game
	while ((entry = readdir(dir)) != NULL && !scraper_stop_requested) {
		if (entry->d_name[0] == '.') continue;

		// Skip non-game files
		const char *ext = strrchr(entry->d_name, '.');
		if (ext) {
			if (strcasecmp(ext, ".txt") == 0 ||
			    strcasecmp(ext, ".cfg") == 0 ||
			    strcasecmp(ext, ".xml") == 0) {
				continue;
			}
		}

		while (scraper_status == SCRAPER_PAUSED && !scraper_stop_requested) {
			usleep(100000);  // 100ms
		}

		strncpy(scraper_progress.current_game, entry->d_name,
			sizeof(scraper_progress.current_game) - 1);

		char game_path[1024];
		snprintf(game_path, sizeof(game_path), "%s/%s", games_dir, entry->d_name);

		scraper_result_t result;
		if (scraper_has_artwork(game_path, system_name)) {
			scraper_progress.already_had++;
		} else if (scraper_scrape_game(entry->d_name, game_path, system_name, &result)) {
			if (result.downloaded) {
				scraper_progress.downloaded++;
			}
			scraper_progress.found++;
		} else {
			scraper_progress.failed++;
		}

		scraper_progress.processed++;

		// Update ETA
		if (scraper_progress.processed > 0) {
			uint32_t elapsed = (uint32_t)time(NULL) - scraper_progress.start_time;
			float rate = (float)elapsed / scraper_progress.processed;
			int remaining = scraper_progress.total_games - scraper_progress.processed;
			scraper_progress.eta_seconds = (uint32_t)(rate * remaining);
		}

		usleep(scraper_config.request_delay_ms * 1000);
	}

	closedir(dir);

	scraper_status = scraper_stop_requested ? SCRAPER_IDLE : SCRAPER_COMPLETE;
	scraper_progress.current_game[0] = '\0';

	return NULL;
}

int scraper_scrape_system(const char *system_name)
{
	if (scraper_status == SCRAPER_RUNNING) return 0;
	if (!system_name) return 0;

	static char system_copy[64];
	strncpy(system_copy, system_name, sizeof(system_copy) - 1);

	scraper_stop_requested = 0;

	if (pthread_create(&scraper_thread, NULL, scraper_thread_func, system_copy) != 0) {
		return 0;
	}

	scraper_thread_active = 1;
	return 1;
}

int scraper_scrape_all(void)
{
	// TODO: Iterate through all systems
	// For now, this would need a list of known systems
	return 0;
}
