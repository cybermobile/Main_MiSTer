// playtime.cpp
// Playtime tracking system for MiSTer modern frontend
// Inspired by Analogue 3D Library feature
// 2024

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#include "playtime.h"
#include "file_io.h"

// Global playtime database
static playtime_db_t playtime_db;
static int initialized = 0;
static time_t last_save_time = 0;

// Auto-save interval (seconds)
#define AUTOSAVE_INTERVAL 60

// Forward declarations
static int find_entry(const char *game_path);
static int add_entry(const char *game_path, const char *game_name, const char *core_name);
static void sort_by_last_played(playtime_entry_t **entries, int count);
static void sort_by_total_time(playtime_entry_t **entries, int count);

void playtime_init(void)
{
	memset(&playtime_db, 0, sizeof(playtime_db));
	playtime_db.current_index = -1;
	playtime_db.session_start = 0;

	playtime_load();
	initialized = 1;

	printf("Playtime system initialized with %d entries\n", playtime_db.count);
}

void playtime_shutdown(void)
{
	// End any active session
	playtime_end_session();

	// Save data
	playtime_save();

	initialized = 0;
	printf("Playtime system shutdown\n");
}

int playtime_load(void)
{
	FILE *f = fopen(PLAYTIME_FILE, "rb");
	if (!f)
	{
		printf("Playtime: No existing data file\n");
		return 0;
	}

	// Read header (magic + version + count)
	char magic[8];
	uint32_t version, count;

	if (fread(magic, 1, 8, f) != 8 || memcmp(magic, "MSTRPLAY", 8) != 0)
	{
		printf("Playtime: Invalid file format\n");
		fclose(f);
		return -1;
	}

	if (fread(&version, sizeof(version), 1, f) != 1)
	{
		fclose(f);
		return -1;
	}

	if (version != 1)
	{
		printf("Playtime: Unsupported version %u\n", version);
		fclose(f);
		return -1;
	}

	if (fread(&count, sizeof(count), 1, f) != 1)
	{
		fclose(f);
		return -1;
	}

	if (count > PLAYTIME_MAX_ENTRIES)
	{
		count = PLAYTIME_MAX_ENTRIES;
	}

	// Read entries
	for (uint32_t i = 0; i < count; i++)
	{
		playtime_entry_t *entry = &playtime_db.entries[i];

		if (fread(entry, sizeof(playtime_entry_t), 1, f) != 1)
		{
			break;
		}

		playtime_db.count++;
	}

	fclose(f);
	printf("Playtime: Loaded %d entries\n", playtime_db.count);
	return 0;
}

int playtime_save(void)
{
	FILE *f = fopen(PLAYTIME_FILE, "wb");
	if (!f)
	{
		printf("Playtime: Cannot create save file\n");
		return -1;
	}

	// Write header
	const char magic[] = "MSTRPLAY";
	uint32_t version = 1;
	uint32_t count = playtime_db.count;

	fwrite(magic, 1, 8, f);
	fwrite(&version, sizeof(version), 1, f);
	fwrite(&count, sizeof(count), 1, f);

	// Write entries
	for (int i = 0; i < playtime_db.count; i++)
	{
		fwrite(&playtime_db.entries[i], sizeof(playtime_entry_t), 1, f);
	}

	fclose(f);
	last_save_time = time(NULL);
	return 0;
}

void playtime_start_session(const char *game_path, const char *game_name, const char *core_name)
{
	if (!game_path) return;

	// End any existing session
	playtime_end_session();

	// Find or create entry
	int idx = find_entry(game_path);
	if (idx < 0)
	{
		idx = add_entry(game_path, game_name, core_name);
	}

	if (idx < 0) return;

	playtime_entry_t *entry = &playtime_db.entries[idx];

	// Update play count and timestamps
	entry->play_count++;
	entry->last_played = time(NULL);
	if (entry->first_played == 0)
	{
		entry->first_played = entry->last_played;
	}

	// Update core name if provided
	if (core_name && core_name[0])
	{
		strncpy(entry->core_name, core_name, sizeof(entry->core_name) - 1);
	}

	// Start session tracking
	playtime_db.current_index = idx;
	playtime_db.session_start = time(NULL);

	printf("Playtime: Started session for '%s'\n", game_name ? game_name : game_path);
}

void playtime_end_session(void)
{
	if (playtime_db.current_index < 0 || playtime_db.session_start == 0)
	{
		return;
	}

	playtime_entry_t *entry = &playtime_db.entries[playtime_db.current_index];

	// Calculate session duration
	time_t now = time(NULL);
	uint32_t session_duration = (uint32_t)(now - playtime_db.session_start);

	// Update entry
	entry->total_seconds += session_duration;
	entry->last_session_seconds = session_duration;
	entry->last_played = now;

	printf("Playtime: Ended session, duration: %u seconds, total: %u seconds\n",
	       session_duration, entry->total_seconds);

	// Reset session
	playtime_db.current_index = -1;
	playtime_db.session_start = 0;

	// Save immediately
	playtime_save();
}

void playtime_update(void)
{
	if (!initialized) return;

	// Auto-save periodically during active session
	if (playtime_db.current_index >= 0)
	{
		time_t now = time(NULL);
		if (now - last_save_time >= AUTOSAVE_INTERVAL)
		{
			// Update current session time before saving
			playtime_entry_t *entry = &playtime_db.entries[playtime_db.current_index];
			uint32_t session_duration = (uint32_t)(now - playtime_db.session_start);

			// Temporarily update for save
			uint32_t old_total = entry->total_seconds;
			entry->total_seconds = old_total + session_duration;

			playtime_save();

			// Restore (will be finalized on end_session)
			entry->total_seconds = old_total;
		}
	}
}

int playtime_session_active(void)
{
	return playtime_db.current_index >= 0 && playtime_db.session_start > 0;
}

uint32_t playtime_get_session_duration(void)
{
	if (!playtime_session_active()) return 0;
	return (uint32_t)(time(NULL) - playtime_db.session_start);
}

static int find_entry(const char *game_path)
{
	if (!game_path) return -1;

	for (int i = 0; i < playtime_db.count; i++)
	{
		if (strcmp(playtime_db.entries[i].game_path, game_path) == 0)
		{
			return i;
		}
	}
	return -1;
}

static int add_entry(const char *game_path, const char *game_name, const char *core_name)
{
	if (playtime_db.count >= PLAYTIME_MAX_ENTRIES)
	{
		// Remove oldest entry to make room
		// Find entry with oldest last_played
		int oldest_idx = 0;
		time_t oldest_time = playtime_db.entries[0].last_played;

		for (int i = 1; i < playtime_db.count; i++)
		{
			if (playtime_db.entries[i].last_played < oldest_time)
			{
				oldest_time = playtime_db.entries[i].last_played;
				oldest_idx = i;
			}
		}

		// Shift entries to remove oldest
		for (int i = oldest_idx; i < playtime_db.count - 1; i++)
		{
			playtime_db.entries[i] = playtime_db.entries[i + 1];
		}
		playtime_db.count--;
	}

	int idx = playtime_db.count++;
	playtime_entry_t *entry = &playtime_db.entries[idx];

	memset(entry, 0, sizeof(playtime_entry_t));
	strncpy(entry->game_path, game_path, sizeof(entry->game_path) - 1);

	if (game_name && game_name[0])
	{
		strncpy(entry->game_name, game_name, sizeof(entry->game_name) - 1);
	}
	else
	{
		// Extract name from path
		const char *name = strrchr(game_path, '/');
		name = name ? name + 1 : game_path;
		strncpy(entry->game_name, name, sizeof(entry->game_name) - 1);

		// Remove extension
		char *dot = strrchr(entry->game_name, '.');
		if (dot) *dot = '\0';
	}

	if (core_name && core_name[0])
	{
		strncpy(entry->core_name, core_name, sizeof(entry->core_name) - 1);
	}

	return idx;
}

playtime_entry_t* playtime_get_entry(const char *game_path)
{
	int idx = find_entry(game_path);
	if (idx < 0) return NULL;
	return &playtime_db.entries[idx];
}

playtime_entry_t* playtime_get_entry_by_index(int index)
{
	if (index < 0 || index >= playtime_db.count) return NULL;
	return &playtime_db.entries[index];
}

uint32_t playtime_get_total(const char *game_path)
{
	playtime_entry_t *entry = playtime_get_entry(game_path);
	if (!entry) return 0;

	uint32_t total = entry->total_seconds;

	// Add current session time if this is the active game
	if (playtime_db.current_index >= 0 &&
	    &playtime_db.entries[playtime_db.current_index] == entry)
	{
		total += playtime_get_session_duration();
	}

	return total;
}

uint32_t playtime_get_play_count(const char *game_path)
{
	playtime_entry_t *entry = playtime_get_entry(game_path);
	return entry ? entry->play_count : 0;
}

time_t playtime_get_last_played(const char *game_path)
{
	playtime_entry_t *entry = playtime_get_entry(game_path);
	return entry ? entry->last_played : 0;
}

time_t playtime_get_first_played(const char *game_path)
{
	playtime_entry_t *entry = playtime_get_entry(game_path);
	return entry ? entry->first_played : 0;
}

int playtime_get_count(void)
{
	return playtime_db.count;
}

// Comparison function for sorting by last played (newest first)
static int compare_last_played(const void *a, const void *b)
{
	playtime_entry_t *ea = *(playtime_entry_t**)a;
	playtime_entry_t *eb = *(playtime_entry_t**)b;

	if (eb->last_played > ea->last_played) return 1;
	if (eb->last_played < ea->last_played) return -1;
	return 0;
}

// Comparison function for sorting by total time (most played first)
static int compare_total_time(const void *a, const void *b)
{
	playtime_entry_t *ea = *(playtime_entry_t**)a;
	playtime_entry_t *eb = *(playtime_entry_t**)b;

	if (eb->total_seconds > ea->total_seconds) return 1;
	if (eb->total_seconds < ea->total_seconds) return -1;
	return 0;
}

int playtime_get_recent(playtime_entry_t **out_entries, int max_count)
{
	if (!out_entries || max_count <= 0) return 0;

	// Create array of pointers
	playtime_entry_t *sorted[PLAYTIME_MAX_ENTRIES];
	int count = playtime_db.count < max_count ? playtime_db.count : max_count;

	for (int i = 0; i < playtime_db.count; i++)
	{
		sorted[i] = &playtime_db.entries[i];
	}

	// Sort by last played
	qsort(sorted, playtime_db.count, sizeof(playtime_entry_t*), compare_last_played);

	// Copy to output
	for (int i = 0; i < count; i++)
	{
		out_entries[i] = sorted[i];
	}

	return count;
}

int playtime_get_most_played(playtime_entry_t **out_entries, int max_count)
{
	if (!out_entries || max_count <= 0) return 0;

	// Create array of pointers
	playtime_entry_t *sorted[PLAYTIME_MAX_ENTRIES];
	int count = playtime_db.count < max_count ? playtime_db.count : max_count;

	for (int i = 0; i < playtime_db.count; i++)
	{
		sorted[i] = &playtime_db.entries[i];
	}

	// Sort by total time
	qsort(sorted, playtime_db.count, sizeof(playtime_entry_t*), compare_total_time);

	// Copy to output
	for (int i = 0; i < count; i++)
	{
		out_entries[i] = sorted[i];
	}

	return count;
}

void playtime_format_duration(uint32_t seconds, char *out_str, int out_size)
{
	if (!out_str || out_size <= 0) return;

	if (seconds < 60)
	{
		snprintf(out_str, out_size, "< 1m");
	}
	else if (seconds < 3600)
	{
		snprintf(out_str, out_size, "%um", seconds / 60);
	}
	else
	{
		uint32_t hours = seconds / 3600;
		uint32_t mins = (seconds % 3600) / 60;
		if (mins > 0)
		{
			snprintf(out_str, out_size, "%uh %um", hours, mins);
		}
		else
		{
			snprintf(out_str, out_size, "%uh", hours);
		}
	}
}

void playtime_format_relative_time(time_t timestamp, char *out_str, int out_size)
{
	if (!out_str || out_size <= 0) return;

	if (timestamp == 0)
	{
		snprintf(out_str, out_size, "Never");
		return;
	}

	time_t now = time(NULL);
	time_t diff = now - timestamp;

	// Get local time info for "today" comparison
	struct tm *now_tm = localtime(&now);
	int now_day = now_tm->tm_yday;
	int now_year = now_tm->tm_year;

	struct tm *ts_tm = localtime(&timestamp);
	int ts_day = ts_tm->tm_yday;
	int ts_year = ts_tm->tm_year;

	if (ts_year == now_year && ts_day == now_day)
	{
		snprintf(out_str, out_size, "Today");
	}
	else if (ts_year == now_year && ts_day == now_day - 1)
	{
		snprintf(out_str, out_size, "Yesterday");
	}
	else if (diff < 7 * 24 * 3600)
	{
		int days = diff / (24 * 3600);
		snprintf(out_str, out_size, "%d days ago", days);
	}
	else if (diff < 30 * 24 * 3600)
	{
		int weeks = diff / (7 * 24 * 3600);
		if (weeks == 1)
			snprintf(out_str, out_size, "1 week ago");
		else
			snprintf(out_str, out_size, "%d weeks ago", weeks);
	}
	else
	{
		playtime_format_date(timestamp, out_str, out_size);
	}
}

void playtime_format_date(time_t timestamp, char *out_str, int out_size)
{
	if (!out_str || out_size <= 0) return;

	if (timestamp == 0)
	{
		out_str[0] = '\0';
		return;
	}

	struct tm *tm = localtime(&timestamp);
	strftime(out_str, out_size, "%b %d, %Y", tm);
}

void playtime_clear_all(void)
{
	memset(&playtime_db, 0, sizeof(playtime_db));
	playtime_db.current_index = -1;
	playtime_save();
	printf("Playtime: Cleared all data\n");
}

int playtime_remove_entry(const char *game_path)
{
	int idx = find_entry(game_path);
	if (idx < 0) return -1;

	// Shift entries
	for (int i = idx; i < playtime_db.count - 1; i++)
	{
		playtime_db.entries[i] = playtime_db.entries[i + 1];
	}
	playtime_db.count--;

	// Adjust current index if needed
	if (playtime_db.current_index == idx)
	{
		playtime_db.current_index = -1;
		playtime_db.session_start = 0;
	}
	else if (playtime_db.current_index > idx)
	{
		playtime_db.current_index--;
	}

	playtime_save();
	return 0;
}

playtime_db_t* playtime_get_db(void)
{
	return &playtime_db;
}
