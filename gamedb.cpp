// gamedb.cpp
// Game database for MiSTer modern frontend
// 2024

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>

#include "gamedb.h"
#include "file_io.h"
#include "cfg.h"
#include "sxmlc.h"

// Global state
static gamedb_state_t db_state;
static gamedb_entry_t entries_storage[GAMEDB_MAX_ENTRIES];
static gamedb_favorites_t favorites;

// Genre name lookup table
static const char* genre_names[] = {
	"Unknown",
	"Action",
	"Adventure",
	"Arcade",
	"Board Game",
	"Educational",
	"Fighting",
	"Platformer",
	"Puzzle",
	"Racing",
	"RPG",
	"Shooter",
	"Simulation",
	"Sports",
	"Strategy",
	"Other"
};

// Region name lookup table
static const char* region_names[] = {
	"Unknown",
	"USA",
	"Europe",
	"Japan",
	"World",
	"Other"
};

// Forward declarations
static int parse_nointro_xml(const char *xml_path);
static void add_entry(gamedb_entry_t *entry);
static int match_filename(const char *db_name, const char *search_name);

void gamedb_init(void)
{
	memset(&db_state, 0, sizeof(db_state));
	memset(&entries_storage, 0, sizeof(entries_storage));
	memset(&favorites, 0, sizeof(favorites));

	db_state.entries = entries_storage;
	db_state.capacity = GAMEDB_MAX_ENTRIES;
	db_state.entry_count = 0;
	db_state.loaded = 0;

	printf("GameDB initialized\n");
}

void gamedb_shutdown(void)
{
	gamedb_unload();
	printf("GameDB shutdown\n");
}

int gamedb_load(const char *core_name)
{
	if (!core_name || !core_name[0]) return 0;

	// Unload previous database
	gamedb_unload();

	// Store core name
	strncpy(db_state.core_name, core_name, sizeof(db_state.core_name) - 1);
	db_state.core_name[sizeof(db_state.core_name) - 1] = '\0';

	// Clean core name (remove datecode)
	char *p = strstr(db_state.core_name, "_20");
	if (p) *p = '\0';

	// Try loading database files in order of preference
	char path[GAMEDB_PATH_MAX];
	const char *extensions[] = { ".xml", ".dat", ".json", NULL };

	for (int i = 0; extensions[i]; i++)
	{
		// Try /media/fat/{gamedb_path}/{core}.xml (gamedb_path from MiSTer.ini, default "gamedb")
		const char *gamedb_dir = (cfg.gamedb_path[0]) ? cfg.gamedb_path : "gamedb";
		snprintf(path, sizeof(path), "%s/%s/%s%s",
			getRootDir(), gamedb_dir, db_state.core_name, extensions[i]);

		struct stat st;
		if (stat(path, &st) == 0)
		{
			printf("GameDB: Loading %s\n", path);
			strncpy(db_state.db_path, path, sizeof(db_state.db_path) - 1);

			if (strstr(path, ".xml") || strstr(path, ".dat"))
			{
				if (parse_nointro_xml(path))
				{
					db_state.loaded = 1;
					printf("GameDB: Loaded %d entries for %s\n",
						db_state.entry_count, db_state.core_name);

					// Load favorites and stats
					gamedb_load_favorites();
					gamedb_load_stats();

					return 1;
				}
			}
			else if (strstr(path, ".json"))
			{
				if (gamedb_import_json(path))
				{
					db_state.loaded = 1;
					printf("GameDB: Loaded %d entries for %s\n",
						db_state.entry_count, db_state.core_name);

					gamedb_load_favorites();
					gamedb_load_stats();

					return 1;
				}
			}
		}
	}

	printf("GameDB: No database found for %s\n", db_state.core_name);
	return 0;
}

void gamedb_unload(void)
{
	// Save favorites and stats before unloading
	if (db_state.loaded)
	{
		gamedb_save_favorites();
		gamedb_save_stats();
	}

	memset(entries_storage, 0, sizeof(entries_storage));
	db_state.entry_count = 0;
	db_state.loaded = 0;
	db_state.core_name[0] = '\0';
	db_state.db_path[0] = '\0';
}

int gamedb_is_loaded(void)
{
	return db_state.loaded;
}

const char* gamedb_get_core(void)
{
	return db_state.core_name;
}

static void add_entry(gamedb_entry_t *entry)
{
	if (db_state.entry_count >= db_state.capacity) return;

	memcpy(&entries_storage[db_state.entry_count], entry, sizeof(gamedb_entry_t));
	db_state.entry_count++;
}

// Parse No-Intro DAT/XML format
// Format: <game name="..."><rom name="..." size="..." crc="..."/></game>
static int parse_nointro_xml(const char *xml_path)
{
	XMLDoc doc;
	XMLDoc_init(&doc);

	if (XMLDoc_parse_file_DOM(xml_path, &doc) != 1)
	{
		printf("GameDB: Failed to parse XML: %s\n", xml_path);
		XMLDoc_free(&doc);
		return 0;
	}

	// Find root node (usually "datafile" or "dat")
	XMLNode *root = NULL;
	for (int i = 0; i < doc.n_nodes; i++)
	{
		if (doc.nodes[i]->tag_type == TAG_FATHER)
		{
			root = doc.nodes[i];
			break;
		}
	}

	if (!root)
	{
		printf("GameDB: No root node found\n");
		XMLDoc_free(&doc);
		return 0;
	}

	// Iterate through children looking for "game" nodes
	for (int i = 0; i < root->n_children; i++)
	{
		XMLNode *node = root->children[i];
		if (!node || !node->tag) continue;

		if (strcmp(node->tag, "game") == 0)
		{
			gamedb_entry_t entry;
			memset(&entry, 0, sizeof(entry));

			// Get game name from attribute
			for (int a = 0; a < node->n_attributes; a++)
			{
				if (strcmp(node->attributes[a].name, "name") == 0)
				{
					strncpy(entry.name, node->attributes[a].value, GAMEDB_NAME_MAX - 1);
					break;
				}
			}

			// Parse region from name
			entry.region = gamedb_parse_region(entry.name);

			// Look for <rom> child with CRC and size
			for (int c = 0; c < node->n_children; c++)
			{
				XMLNode *child = node->children[c];
				if (!child || !child->tag) continue;

				if (strcmp(child->tag, "rom") == 0)
				{
					for (int a = 0; a < child->n_attributes; a++)
					{
						if (strcmp(child->attributes[a].name, "name") == 0)
						{
							strncpy(entry.filename, child->attributes[a].value, GAMEDB_NAME_MAX - 1);
						}
						else if (strcmp(child->attributes[a].name, "crc") == 0)
						{
							entry.crc32 = strtoul(child->attributes[a].value, NULL, 16);
						}
						else if (strcmp(child->attributes[a].name, "size") == 0)
						{
							entry.size = strtoul(child->attributes[a].value, NULL, 10);
						}
						else if (strcmp(child->attributes[a].name, "serial") == 0)
						{
							strncpy(entry.serial, child->attributes[a].value, sizeof(entry.serial) - 1);
						}
					}
				}
				else if (strcmp(child->tag, "description") == 0 && child->text)
				{
					strncpy(entry.description, child->text, GAMEDB_DESC_MAX - 1);
				}
				else if (strcmp(child->tag, "year") == 0 && child->text)
				{
					entry.year = atoi(child->text);
				}
				else if (strcmp(child->tag, "manufacturer") == 0 && child->text)
				{
					strncpy(entry.developer, child->text, sizeof(entry.developer) - 1);
				}
				else if (strcmp(child->tag, "publisher") == 0 && child->text)
				{
					strncpy(entry.publisher, child->text, sizeof(entry.publisher) - 1);
				}
			}

			// If no filename, use name
			if (!entry.filename[0])
			{
				strncpy(entry.filename, entry.name, GAMEDB_NAME_MAX - 1);
			}

			// Default players
			entry.players_min = 1;
			entry.players_max = 1;

			add_entry(&entry);
		}
	}

	XMLDoc_free(&doc);
	return 1;
}

// Fuzzy matching score between query and target
int gamedb_fuzzy_score(const char *query, const char *target)
{
	if (!query || !target) return 0;

	int query_len = strlen(query);
	int target_len = strlen(target);

	if (query_len == 0) return 0;

	// Convert both to lowercase for comparison
	char query_lower[256], target_lower[256];
	for (int i = 0; i < query_len && i < 255; i++)
	{
		query_lower[i] = tolower(query[i]);
	}
	query_lower[query_len < 255 ? query_len : 255] = '\0';

	for (int i = 0; i < target_len && i < 255; i++)
	{
		target_lower[i] = tolower(target[i]);
	}
	target_lower[target_len < 255 ? target_len : 255] = '\0';

	// Exact match
	if (strcmp(query_lower, target_lower) == 0) return 1000;

	// Prefix match
	if (strncmp(query_lower, target_lower, query_len) == 0) return 900;

	// Substring match
	if (strstr(target_lower, query_lower)) return 800;

	// Word-by-word matching
	int score = 0;
	char *token = strtok(query_lower, " ");
	while (token)
	{
		if (strstr(target_lower, token))
		{
			score += 100;
		}
		token = strtok(NULL, " ");
	}

	return score;
}

// Match a filename from the database against a search name
static int match_filename(const char *db_name, const char *search_name)
{
	if (!db_name || !search_name) return 0;

	// Clean both names for comparison
	char clean_db[256], clean_search[256];
	gamedb_clean_name(db_name, clean_db, sizeof(clean_db));
	gamedb_clean_name(search_name, clean_search, sizeof(clean_search));

	// Try exact match first
	if (strcasecmp(clean_db, clean_search) == 0) return 1000;

	// Try fuzzy matching
	return gamedb_fuzzy_score(clean_search, clean_db);
}

gamedb_entry_t* gamedb_lookup_filename(const char *filename)
{
	if (!filename || !db_state.loaded) return NULL;

	int best_score = 0;
	int best_index = -1;

	for (int i = 0; i < db_state.entry_count; i++)
	{
		int score = match_filename(entries_storage[i].filename, filename);
		if (score > best_score)
		{
			best_score = score;
			best_index = i;
		}

		// Also check against display name
		score = match_filename(entries_storage[i].name, filename);
		if (score > best_score)
		{
			best_score = score;
			best_index = i;
		}
	}

	// Require minimum score threshold
	if (best_score >= 500 && best_index >= 0)
	{
		return &entries_storage[best_index];
	}

	return NULL;
}

gamedb_entry_t* gamedb_lookup_crc32(uint32_t crc32)
{
	if (!db_state.loaded || crc32 == 0) return NULL;

	for (int i = 0; i < db_state.entry_count; i++)
	{
		if (entries_storage[i].crc32 == crc32)
		{
			return &entries_storage[i];
		}
	}

	return NULL;
}

gamedb_entry_t* gamedb_lookup_serial(const char *serial)
{
	if (!serial || !db_state.loaded) return NULL;

	for (int i = 0; i < db_state.entry_count; i++)
	{
		if (entries_storage[i].serial[0] &&
		    strcasecmp(entries_storage[i].serial, serial) == 0)
		{
			return &entries_storage[i];
		}
	}

	return NULL;
}

gamedb_entry_t* gamedb_get_entry(int index)
{
	if (index >= 0 && index < db_state.entry_count)
	{
		return &entries_storage[index];
	}
	return NULL;
}

int gamedb_get_count(void)
{
	return db_state.entry_count;
}

int gamedb_search(const char *query, gamedb_result_t *results, int max_results)
{
	if (!query || !results || max_results <= 0 || !db_state.loaded)
		return 0;

	int count = 0;

	for (int i = 0; i < db_state.entry_count && count < max_results; i++)
	{
		int score = gamedb_fuzzy_score(query, entries_storage[i].name);
		if (score == 0)
		{
			score = gamedb_fuzzy_score(query, entries_storage[i].filename);
		}

		if (score > 0)
		{
			results[count].entry = &entries_storage[i];
			results[count].original_index = i;
			results[count].score = score;
			count++;
		}
	}

	// Sort by score (simple bubble sort for small result sets)
	for (int i = 0; i < count - 1; i++)
	{
		for (int j = 0; j < count - i - 1; j++)
		{
			if (results[j].score < results[j + 1].score)
			{
				gamedb_result_t tmp = results[j];
				results[j] = results[j + 1];
				results[j + 1] = tmp;
			}
		}
	}

	return count;
}

int gamedb_filter_genre(gamedb_genre_t genre, gamedb_result_t *results, int max_results)
{
	if (!results || max_results <= 0 || !db_state.loaded) return 0;

	int count = 0;
	for (int i = 0; i < db_state.entry_count && count < max_results; i++)
	{
		if (entries_storage[i].genre == genre)
		{
			results[count].entry = &entries_storage[i];
			results[count].original_index = i;
			results[count].score = 100;
			count++;
		}
	}

	return count;
}

int gamedb_filter_year(uint16_t min_year, uint16_t max_year, gamedb_result_t *results, int max_results)
{
	if (!results || max_results <= 0 || !db_state.loaded) return 0;

	int count = 0;
	for (int i = 0; i < db_state.entry_count && count < max_results; i++)
	{
		if (entries_storage[i].year >= min_year && entries_storage[i].year <= max_year)
		{
			results[count].entry = &entries_storage[i];
			results[count].original_index = i;
			results[count].score = 100;
			count++;
		}
	}

	return count;
}

int gamedb_filter_region(gamedb_region_t region, gamedb_result_t *results, int max_results)
{
	if (!results || max_results <= 0 || !db_state.loaded) return 0;

	int count = 0;
	for (int i = 0; i < db_state.entry_count && count < max_results; i++)
	{
		if (entries_storage[i].region == region)
		{
			results[count].entry = &entries_storage[i];
			results[count].original_index = i;
			results[count].score = 100;
			count++;
		}
	}

	return count;
}

int gamedb_filter_players(uint8_t min_players, uint8_t max_players, gamedb_result_t *results, int max_results)
{
	if (!results || max_results <= 0 || !db_state.loaded) return 0;

	int count = 0;
	for (int i = 0; i < db_state.entry_count && count < max_results; i++)
	{
		if (entries_storage[i].players_max >= min_players &&
		    entries_storage[i].players_min <= max_players)
		{
			results[count].entry = &entries_storage[i];
			results[count].original_index = i;
			results[count].score = 100;
			count++;
		}
	}

	return count;
}

// Favorites functions
int gamedb_load_favorites(void)
{
	if (!db_state.core_name[0]) return 0;

	char path[GAMEDB_PATH_MAX];
	snprintf(path, sizeof(path), "%s/config/%s_favorites.cfg",
		getRootDir(), db_state.core_name);

	FILE *f = fopen(path, "r");
	if (!f) return 0;

	favorites.count = 0;
	char line[GAMEDB_PATH_MAX];

	while (fgets(line, sizeof(line), f) && favorites.count < 1024)
	{
		// Remove newline
		size_t len = strlen(line);
		if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';
		if (len > 1 && line[len - 2] == '\r') line[len - 2] = '\0';

		if (line[0])
		{
			strncpy(favorites.paths[favorites.count], line, GAMEDB_PATH_MAX - 1);
			favorites.count++;
		}
	}

	fclose(f);
	printf("GameDB: Loaded %d favorites for %s\n", favorites.count, db_state.core_name);
	return 1;
}

int gamedb_save_favorites(void)
{
	if (!db_state.core_name[0]) return 0;

	char path[GAMEDB_PATH_MAX];
	snprintf(path, sizeof(path), "%s/config/%s_favorites.cfg",
		getRootDir(), db_state.core_name);

	FILE *f = fopen(path, "w");
	if (!f) return 0;

	for (int i = 0; i < favorites.count; i++)
	{
		fprintf(f, "%s\n", favorites.paths[i]);
	}

	fclose(f);
	return 1;
}

int gamedb_add_favorite(const char *path)
{
	if (!path || favorites.count >= 1024) return 0;

	// Check if already favorite
	if (gamedb_is_favorite(path)) return 1;

	strncpy(favorites.paths[favorites.count], path, GAMEDB_PATH_MAX - 1);
	favorites.count++;

	return 1;
}

int gamedb_remove_favorite(const char *path)
{
	if (!path) return 0;

	for (int i = 0; i < favorites.count; i++)
	{
		if (strcmp(favorites.paths[i], path) == 0)
		{
			// Shift remaining entries
			for (int j = i; j < favorites.count - 1; j++)
			{
				strcpy(favorites.paths[j], favorites.paths[j + 1]);
			}
			favorites.count--;
			return 1;
		}
	}

	return 0;
}

int gamedb_is_favorite(const char *path)
{
	if (!path) return 0;

	for (int i = 0; i < favorites.count; i++)
	{
		if (strcmp(favorites.paths[i], path) == 0)
		{
			return 1;
		}
	}

	return 0;
}

int gamedb_toggle_favorite(const char *path)
{
	if (gamedb_is_favorite(path))
	{
		return gamedb_remove_favorite(path);
	}
	else
	{
		return gamedb_add_favorite(path);
	}
}

gamedb_favorites_t* gamedb_get_favorites(void)
{
	return &favorites;
}

// Play statistics - stored per-game in config
void gamedb_record_play(const char *path, uint32_t duration_seconds)
{
	if (!path || !db_state.loaded) return;

	// Find matching entry
	gamedb_entry_t *entry = gamedb_lookup_filename(path);
	if (entry)
	{
		entry->play_count++;
		entry->play_time += duration_seconds;
		entry->last_played = (uint32_t)time(NULL);
	}
}

uint32_t gamedb_get_play_count(const char *path)
{
	gamedb_entry_t *entry = gamedb_lookup_filename(path);
	return entry ? entry->play_count : 0;
}

uint32_t gamedb_get_play_time(const char *path)
{
	gamedb_entry_t *entry = gamedb_lookup_filename(path);
	return entry ? entry->play_time : 0;
}

uint32_t gamedb_get_last_played(const char *path)
{
	gamedb_entry_t *entry = gamedb_lookup_filename(path);
	return entry ? entry->last_played : 0;
}

int gamedb_load_stats(void)
{
	if (!db_state.core_name[0]) return 0;

	char path[GAMEDB_PATH_MAX];
	snprintf(path, sizeof(path), "%s/config/%s_playstats.cfg",
		getRootDir(), db_state.core_name);

	FILE *f = fopen(path, "r");
	if (!f) return 0;

	char line[512];
	while (fgets(line, sizeof(line), f))
	{
		char filename[256];
		uint32_t play_count, play_time, last_played;

		if (sscanf(line, "%255[^|]|%u|%u|%u",
			filename, &play_count, &play_time, &last_played) == 4)
		{
			gamedb_entry_t *entry = gamedb_lookup_filename(filename);
			if (entry)
			{
				entry->play_count = play_count;
				entry->play_time = play_time;
				entry->last_played = last_played;
			}
		}
	}

	fclose(f);
	return 1;
}

int gamedb_save_stats(void)
{
	if (!db_state.core_name[0]) return 0;

	char path[GAMEDB_PATH_MAX];
	snprintf(path, sizeof(path), "%s/config/%s_playstats.cfg",
		getRootDir(), db_state.core_name);

	FILE *f = fopen(path, "w");
	if (!f) return 0;

	for (int i = 0; i < db_state.entry_count; i++)
	{
		if (entries_storage[i].play_count > 0)
		{
			fprintf(f, "%s|%u|%u|%u\n",
				entries_storage[i].filename,
				entries_storage[i].play_count,
				entries_storage[i].play_time,
				entries_storage[i].last_played);
		}
	}

	fclose(f);
	return 1;
}

// Parse region from filename tags
gamedb_region_t gamedb_parse_region(const char *filename)
{
	if (!filename) return REGION_UNKNOWN;

	if (strstr(filename, "(USA)") || strstr(filename, "(US)") ||
	    strstr(filename, "(U)"))
		return REGION_USA;

	if (strstr(filename, "(Europe)") || strstr(filename, "(E)") ||
	    strstr(filename, "(EU)"))
		return REGION_EUROPE;

	if (strstr(filename, "(Japan)") || strstr(filename, "(J)") ||
	    strstr(filename, "(JP)"))
		return REGION_JAPAN;

	if (strstr(filename, "(World)") || strstr(filename, "(W)"))
		return REGION_WORLD;

	return REGION_UNKNOWN;
}

uint16_t gamedb_parse_year(const char *str)
{
	if (!str) return 0;

	// Look for 4-digit year pattern
	const char *p = str;
	while (*p)
	{
		if (p[0] >= '1' && p[0] <= '2' &&
		    p[1] >= '0' && p[1] <= '9' &&
		    p[2] >= '0' && p[2] <= '9' &&
		    p[3] >= '0' && p[3] <= '9')
		{
			int year = (p[0] - '0') * 1000 + (p[1] - '0') * 100 +
			           (p[2] - '0') * 10 + (p[3] - '0');
			if (year >= 1970 && year <= 2100)
			{
				return (uint16_t)year;
			}
		}
		p++;
	}

	return 0;
}

void gamedb_clean_name(const char *input, char *output, size_t output_size)
{
	if (!input || !output || output_size == 0) return;

	size_t in_len = strlen(input);
	size_t out_idx = 0;
	int in_bracket = 0;
	int in_paren = 0;

	for (size_t i = 0; i < in_len && out_idx < output_size - 1; i++)
	{
		char c = input[i];

		// Track brackets and parentheses
		if (c == '[') { in_bracket++; continue; }
		if (c == ']') { in_bracket--; continue; }
		if (c == '(') { in_paren++; continue; }
		if (c == ')') { in_paren--; continue; }

		// Skip content in brackets/parentheses
		if (in_bracket > 0 || in_paren > 0) continue;

		// Stop at file extension
		if (c == '.' && i > 0)
		{
			// Check if this looks like an extension
			int looks_like_ext = 1;
			for (size_t j = i + 1; j < in_len && j < i + 5; j++)
			{
				if (!isalnum(input[j])) looks_like_ext = 0;
			}
			if (looks_like_ext) break;
		}

		output[out_idx++] = c;
	}

	// Trim trailing spaces
	while (out_idx > 0 && output[out_idx - 1] == ' ')
	{
		out_idx--;
	}

	output[out_idx] = '\0';
}

const char* gamedb_genre_name(gamedb_genre_t genre)
{
	if (genre < sizeof(genre_names) / sizeof(genre_names[0]))
	{
		return genre_names[genre];
	}
	return "Unknown";
}

const char* gamedb_region_name(gamedb_region_t region)
{
	if (region < sizeof(region_names) / sizeof(region_names[0]))
	{
		return region_names[region];
	}
	return "Unknown";
}

// Import from No-Intro XML (wrapper)
int gamedb_import_nointro(const char *xml_path)
{
	gamedb_unload();
	return parse_nointro_xml(xml_path);
}

// Import from JSON format
int gamedb_import_json(const char *json_path)
{
	// Simple JSON parser for our format
	// Format: [{"name":"...", "filename":"...", "year":1990, ...}, ...]

	FILE *f = fopen(json_path, "r");
	if (!f) return 0;

	// Read entire file
	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);

	if (size <= 0 || size > 10 * 1024 * 1024)  // Max 10MB
	{
		fclose(f);
		return 0;
	}

	char *json = (char*)malloc(size + 1);
	if (!json)
	{
		fclose(f);
		return 0;
	}

	fread(json, 1, size, f);
	json[size] = '\0';
	fclose(f);

	// Very simple JSON parsing (not robust, just for our format)
	char *p = json;
	gamedb_entry_t entry;

	while ((p = strstr(p, "{")) != NULL)
	{
		memset(&entry, 0, sizeof(entry));
		entry.players_min = 1;
		entry.players_max = 1;

		char *end = strchr(p, '}');
		if (!end) break;

		// Parse fields
		char *name_start = strstr(p, "\"name\"");
		if (name_start && name_start < end)
		{
			name_start = strchr(name_start, ':');
			if (name_start)
			{
				name_start = strchr(name_start, '"');
				if (name_start)
				{
					name_start++;
					char *name_end = strchr(name_start, '"');
					if (name_end && name_end < end)
					{
						size_t len = name_end - name_start;
						if (len >= GAMEDB_NAME_MAX) len = GAMEDB_NAME_MAX - 1;
						strncpy(entry.name, name_start, len);
					}
				}
			}
		}

		char *filename_start = strstr(p, "\"filename\"");
		if (filename_start && filename_start < end)
		{
			filename_start = strchr(filename_start, ':');
			if (filename_start)
			{
				filename_start = strchr(filename_start, '"');
				if (filename_start)
				{
					filename_start++;
					char *filename_end = strchr(filename_start, '"');
					if (filename_end && filename_end < end)
					{
						size_t len = filename_end - filename_start;
						if (len >= GAMEDB_NAME_MAX) len = GAMEDB_NAME_MAX - 1;
						strncpy(entry.filename, filename_start, len);
					}
				}
			}
		}

		char *year_start = strstr(p, "\"year\"");
		if (year_start && year_start < end)
		{
			year_start = strchr(year_start, ':');
			if (year_start)
			{
				entry.year = atoi(year_start + 1);
			}
		}

		if (entry.name[0])
		{
			if (!entry.filename[0])
			{
				strncpy(entry.filename, entry.name, GAMEDB_NAME_MAX - 1);
			}
			entry.region = gamedb_parse_region(entry.name);
			add_entry(&entry);
		}

		p = end + 1;
	}

	free(json);
	return db_state.entry_count > 0 ? 1 : 0;
}

int gamedb_export_json(const char *json_path)
{
	FILE *f = fopen(json_path, "w");
	if (!f) return 0;

	fprintf(f, "[\n");

	for (int i = 0; i < db_state.entry_count; i++)
	{
		gamedb_entry_t *e = &entries_storage[i];
		fprintf(f, "  {\"name\":\"%s\", \"filename\":\"%s\", \"year\":%d, \"region\":\"%s\"}%s\n",
			e->name, e->filename, e->year, gamedb_region_name(e->region),
			i < db_state.entry_count - 1 ? "," : "");
	}

	fprintf(f, "]\n");
	fclose(f);

	return 1;
}

void gamedb_debug_print(void)
{
	printf("=== GameDB Debug ===\n");
	printf("Core: %s\n", db_state.core_name);
	printf("Loaded: %d\n", db_state.loaded);
	printf("Entries: %d\n", db_state.entry_count);
	printf("Favorites: %d\n", favorites.count);

	for (int i = 0; i < db_state.entry_count && i < 10; i++)
	{
		printf("  [%d] %s (%d) - %s\n",
			i, entries_storage[i].name,
			entries_storage[i].year,
			gamedb_region_name(entries_storage[i].region));
	}

	if (db_state.entry_count > 10)
	{
		printf("  ... and %d more\n", db_state.entry_count - 10);
	}

	printf("====================\n");
}
