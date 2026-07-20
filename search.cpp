// search.cpp
// Search and filter system for MiSTer modern frontend
// 2024

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "search.h"
#include "gamedb.h"

// Virtual keyboard layout (4 rows)
static const char* keyboard_rows[] = {
	"1234567890",
	"QWERTYUIOP",
	"ASDFGHJKL",
	"ZXCVBNM"
};
static const int keyboard_row_count = 4;

// Global search state
static search_state_t search_state;
static search_keyboard_t keyboard_state;
static int initialized = 0;

// Forward declarations
static void update_results(void);
static int compare_results(const void *a, const void *b);

void search_init(void)
{
	memset(&search_state, 0, sizeof(search_state));
	memset(&keyboard_state, 0, sizeof(keyboard_state));

	search_state.mode = SEARCH_MODE_NAME;
	search_state.active = 0;
	search_state.dirty = 0;

	keyboard_state.cursor_x = 0;
	keyboard_state.cursor_y = 1;  // Start on Q row
	keyboard_state.caps_lock = 0;
	keyboard_state.shift = 0;
	keyboard_state.visible = 0;

	initialized = 1;
	printf("Search system initialized\n");
}

void search_shutdown(void)
{
	memset(&search_state, 0, sizeof(search_state));
	memset(&keyboard_state, 0, sizeof(keyboard_state));
	initialized = 0;
	printf("Search system shutdown\n");
}

void search_reset(void)
{
	search_state.query[0] = '\0';
	search_state.query_length = 0;
	search_state.result_count = 0;
	search_state.selected_result = 0;
	search_state.dirty = 1;

	// Clear filters
	memset(&search_state.filter, 0, sizeof(search_filter_t));
}

void search_start(void)
{
	search_state.active = 1;
	search_state.dirty = 1;
	keyboard_state.visible = 1;

	// Execute initial search (show all if query is empty)
	search_execute();
}

void search_end(void)
{
	search_state.active = 0;
	keyboard_state.visible = 0;
}

void search_toggle(void)
{
	if (search_state.active) search_end();
	else search_start();
}

int search_is_active(void)
{
	return search_state.active;
}

void search_set_query(const char *query)
{
	if (query)
	{
		strncpy(search_state.query, query, SEARCH_MAX_QUERY - 1);
		search_state.query[SEARCH_MAX_QUERY - 1] = '\0';
		search_state.query_length = strlen(search_state.query);
	}
	else
	{
		search_state.query[0] = '\0';
		search_state.query_length = 0;
	}
	search_state.dirty = 1;
}

const char* search_get_query(void)
{
	return search_state.query;
}

void search_append_char(char c)
{
	if (search_state.query_length < SEARCH_MAX_QUERY - 1)
	{
		search_state.query[search_state.query_length++] = c;
		search_state.query[search_state.query_length] = '\0';
		search_state.dirty = 1;

		// Auto-execute search as user types
		search_execute();
	}
}

void search_delete_char(void)
{
	if (search_state.query_length > 0)
	{
		search_state.query_length--;
		search_state.query[search_state.query_length] = '\0';
		search_state.dirty = 1;

		// Auto-execute search
		search_execute();
	}
}

void search_clear_query(void)
{
	search_state.query[0] = '\0';
	search_state.query_length = 0;
	search_state.dirty = 1;
	search_execute();
}

// Calculate fuzzy match score
int search_fuzzy_score(const char *query, const char *target)
{
	if (!query || !target) return 0;
	if (!query[0]) return 1;  // Empty query matches everything with low score

	int score = 0;
	int query_len = strlen(query);
	int target_len = strlen(target);

	// Convert to lowercase for comparison
	char query_lower[SEARCH_MAX_QUERY];
	char target_lower[256];

	for (int i = 0; i < query_len && i < SEARCH_MAX_QUERY - 1; i++)
	{
		query_lower[i] = tolower(query[i]);
	}
	query_lower[query_len] = '\0';

	for (int i = 0; i < target_len && i < 255; i++)
	{
		target_lower[i] = tolower(target[i]);
	}
	target_lower[target_len < 255 ? target_len : 255] = '\0';

	// Exact match (highest score)
	if (strcmp(query_lower, target_lower) == 0)
	{
		return 10000;
	}

	// Prefix match (high score)
	if (strncmp(query_lower, target_lower, query_len) == 0)
	{
		score = 5000 + (query_len * 100);
	}

	// Substring match
	char *found = strstr(target_lower, query_lower);
	if (found)
	{
		int pos = found - target_lower;
		score += 2000 - pos * 10;  // Earlier matches score higher
	}

	// Sequential character matching
	int query_idx = 0;
	int consecutive = 0;
	int prev_match_pos = -2;

	for (int i = 0; i < target_len && query_idx < query_len; i++)
	{
		if (target_lower[i] == query_lower[query_idx])
		{
			score += 100;

			// Bonus for consecutive matches
			if (i == prev_match_pos + 1)
			{
				consecutive++;
				score += consecutive * 50;
			}
			else
			{
				consecutive = 0;
			}

			// Bonus for word boundary matches
			if (i == 0 || target[i-1] == ' ' || target[i-1] == '_' || target[i-1] == '-')
			{
				score += 200;
			}

			prev_match_pos = i;
			query_idx++;
		}
	}

	// All characters found?
	if (query_idx == query_len)
	{
		score += 500;
	}
	else
	{
		// Penalize incomplete matches heavily
		score = score / 4;
	}

	return score;
}

// Check if item passes filters
int search_item_passes_filter(gamedb_entry_t *entry, search_filter_t *filter)
{
	if (!entry || !filter) return 1;

	// Genre filter
	if (filter->genre != GENRE_UNKNOWN && entry->genre != filter->genre)
	{
		return 0;
	}

	// Region filter
	if (filter->region != REGION_UNKNOWN && entry->region != filter->region)
	{
		return 0;
	}

	// Year range filter
	if (filter->year_min > 0 && entry->year < filter->year_min)
	{
		return 0;
	}
	if (filter->year_max > 0 && entry->year > filter->year_max)
	{
		return 0;
	}

	// Player count filter
	if (filter->players_min > 0 && entry->players_max < filter->players_min)
	{
		return 0;
	}
	if (filter->players_max > 0 && entry->players_min > filter->players_max)
	{
		return 0;
	}

	// Favorites only
	if (filter->favorites_only && !entry->is_favorite)
	{
		return 0;
	}

	// Recently played (has play count > 0)
	if (filter->recently_played && entry->play_count == 0)
	{
		return 0;
	}

	return 1;
}

void search_execute(void)
{
	search_state.result_count = 0;
	search_state.selected_result = 0;

	// Get items from game database
	int db_count = gamedb_get_count();

	for (int i = 0; i < db_count && search_state.result_count < SEARCH_MAX_RESULTS; i++)
	{
		gamedb_entry_t *entry = gamedb_get_entry(i);
		if (!entry) continue;

		// Check filters first
		if (!search_item_passes_filter(entry, &search_state.filter))
		{
			continue;
		}

		// Calculate match score
		int score = 0;

		if (search_state.query_length == 0)
		{
			// No query - all items match with base score
			score = 1;
		}
		else
		{
			// Calculate fuzzy match score
			switch (search_state.mode)
			{
				case SEARCH_MODE_NAME:
					score = search_fuzzy_score(search_state.query, entry->name);
					break;
				case SEARCH_MODE_DEVELOPER:
					score = search_fuzzy_score(search_state.query, entry->developer);
					break;
				case SEARCH_MODE_YEAR:
					{
						char year_str[8];
						snprintf(year_str, sizeof(year_str), "%d", entry->year);
						score = search_fuzzy_score(search_state.query, year_str);
					}
					break;
				case SEARCH_MODE_ALL:
					// Search all fields, take highest score
					score = search_fuzzy_score(search_state.query, entry->name);
					int dev_score = search_fuzzy_score(search_state.query, entry->developer);
					if (dev_score > score) score = dev_score;
					break;
			}
		}

		// Add to results if score > 0
		if (score > 0)
		{
			search_result_t *result = &search_state.results[search_state.result_count];
			result->index = i;
			strncpy(result->name, entry->name, sizeof(result->name) - 1);
			strncpy(result->path, entry->filename, sizeof(result->path) - 1);
			result->score = score;
			result->item_data = entry;
			search_state.result_count++;
		}
	}

	// Sort results by score
	search_sort_results();

	search_state.dirty = 0;
}

void search_sort_results(void)
{
	if (search_state.result_count > 1)
	{
		qsort(search_state.results, search_state.result_count,
		      sizeof(search_result_t), compare_results);
	}
}

static int compare_results(const void *a, const void *b)
{
	const search_result_t *ra = (const search_result_t*)a;
	const search_result_t *rb = (const search_result_t*)b;

	// Sort by score descending
	if (rb->score != ra->score)
	{
		return rb->score - ra->score;
	}

	// Then alphabetically by name
	return strcasecmp(ra->name, rb->name);
}

search_result_t* search_get_results(int *count)
{
	if (count)
	{
		*count = search_state.result_count;
	}
	return search_state.results;
}

search_result_t* search_get_selected(void)
{
	if (search_state.selected_result >= 0 &&
	    search_state.selected_result < search_state.result_count)
	{
		return &search_state.results[search_state.selected_result];
	}
	return NULL;
}

void search_select_next(void)
{
	if (search_state.result_count > 0)
	{
		search_state.selected_result++;
		if (search_state.selected_result >= search_state.result_count)
		{
			search_state.selected_result = 0;  // Wrap
		}
	}
}

void search_select_prev(void)
{
	if (search_state.result_count > 0)
	{
		search_state.selected_result--;
		if (search_state.selected_result < 0)
		{
			search_state.selected_result = search_state.result_count - 1;  // Wrap
		}
	}
}

search_result_t* search_confirm(void)
{
	search_result_t *selected = search_get_selected();
	if (selected)
	{
		search_end();
	}
	return selected;
}

// Filter operations
void search_set_genre(gamedb_genre_t genre)
{
	search_state.filter.genre = genre;
	search_state.dirty = 1;
}

void search_set_region(gamedb_region_t region)
{
	search_state.filter.region = region;
	search_state.dirty = 1;
}

void search_set_year_range(uint16_t min_year, uint16_t max_year)
{
	search_state.filter.year_min = min_year;
	search_state.filter.year_max = max_year;
	search_state.dirty = 1;
}

void search_set_players(uint8_t min_players, uint8_t max_players)
{
	search_state.filter.players_min = min_players;
	search_state.filter.players_max = max_players;
	search_state.dirty = 1;
}

void search_set_favorites_only(int enabled)
{
	search_state.filter.favorites_only = enabled ? 1 : 0;
	search_state.dirty = 1;
}

void search_set_recently_played(int enabled)
{
	search_state.filter.recently_played = enabled ? 1 : 0;
	search_state.dirty = 1;
}

void search_clear_filters(void)
{
	memset(&search_state.filter, 0, sizeof(search_filter_t));
	search_state.dirty = 1;
}

search_filter_t* search_get_filters(void)
{
	return &search_state.filter;
}

int search_has_active_filters(void)
{
	search_filter_t *f = &search_state.filter;
	return f->genre != GENRE_UNKNOWN ||
	       f->region != REGION_UNKNOWN ||
	       f->year_min > 0 ||
	       f->year_max > 0 ||
	       f->players_min > 0 ||
	       f->players_max > 0 ||
	       f->favorites_only ||
	       f->recently_played;
}

// Virtual keyboard operations
void search_show_keyboard(void)
{
	keyboard_state.visible = 1;
}

void search_hide_keyboard(void)
{
	keyboard_state.visible = 0;
}

int search_keyboard_visible(void)
{
	return keyboard_state.visible;
}

search_keyboard_t* search_get_keyboard(void)
{
	return &keyboard_state;
}

void search_keyboard_move(int dx, int dy)
{
	keyboard_state.cursor_x += dx;
	keyboard_state.cursor_y += dy;

	// Clamp Y
	if (keyboard_state.cursor_y < 0) keyboard_state.cursor_y = 0;
	if (keyboard_state.cursor_y >= keyboard_row_count)
		keyboard_state.cursor_y = keyboard_row_count - 1;

	// Clamp X to current row length
	int row_len = strlen(keyboard_rows[keyboard_state.cursor_y]);
	if (keyboard_state.cursor_x < 0) keyboard_state.cursor_x = row_len - 1;
	if (keyboard_state.cursor_x >= row_len) keyboard_state.cursor_x = 0;
}

void search_keyboard_press(void)
{
	char c = search_keyboard_char_at(keyboard_state.cursor_x, keyboard_state.cursor_y);
	if (c)
	{
		// Apply shift/caps
		if (!keyboard_state.caps_lock && !keyboard_state.shift)
		{
			c = tolower(c);
		}

		search_append_char(c);

		// Clear shift after press
		if (keyboard_state.shift)
		{
			keyboard_state.shift = 0;
		}
	}
}

void search_keyboard_shift(void)
{
	keyboard_state.shift = !keyboard_state.shift;
}

void search_keyboard_caps(void)
{
	keyboard_state.caps_lock = !keyboard_state.caps_lock;
}

char search_keyboard_char_at(int x, int y)
{
	if (y < 0 || y >= keyboard_row_count) return 0;

	int row_len = strlen(keyboard_rows[y]);
	if (x < 0 || x >= row_len) return 0;

	return keyboard_rows[y][x];
}

// Quick navigation
void search_jump_to_letter(char letter)
{
	letter = toupper(letter);

	// Find first result starting with this letter
	for (int i = 0; i < search_state.result_count; i++)
	{
		if (toupper(search_state.results[i].name[0]) == letter)
		{
			search_state.selected_result = i;
			return;
		}
	}
}

void search_next_letter(void)
{
	if (search_state.result_count == 0) return;

	char current = toupper(search_state.results[search_state.selected_result].name[0]);

	// Find next item with different starting letter
	for (int i = search_state.selected_result + 1; i < search_state.result_count; i++)
	{
		if (toupper(search_state.results[i].name[0]) != current)
		{
			search_state.selected_result = i;
			return;
		}
	}

	// Wrap to beginning
	search_state.selected_result = 0;
}

void search_prev_letter(void)
{
	if (search_state.result_count == 0) return;

	char current = toupper(search_state.results[search_state.selected_result].name[0]);

	// Find previous item with different starting letter
	for (int i = search_state.selected_result - 1; i >= 0; i--)
	{
		if (toupper(search_state.results[i].name[0]) != current)
		{
			// Go to first item of this letter group
			char target = toupper(search_state.results[i].name[0]);
			while (i > 0 && toupper(search_state.results[i-1].name[0]) == target)
			{
				i--;
			}
			search_state.selected_result = i;
			return;
		}
	}

	// Wrap to end
	search_state.selected_result = search_state.result_count - 1;
}

char search_get_current_letter(void)
{
	if (search_state.selected_result >= 0 &&
	    search_state.selected_result < search_state.result_count)
	{
		return toupper(search_state.results[search_state.selected_result].name[0]);
	}
	return '\0';
}

void search_debug_print(void)
{
	printf("=== Search Debug ===\n");
	printf("Active: %d\n", search_state.active);
	printf("Query: '%s' (len=%d)\n", search_state.query, search_state.query_length);
	printf("Results: %d\n", search_state.result_count);
	printf("Selected: %d\n", search_state.selected_result);
	printf("Filters active: %d\n", search_has_active_filters());

	if (search_state.result_count > 0)
	{
		printf("Top 5 results:\n");
		for (int i = 0; i < 5 && i < search_state.result_count; i++)
		{
			printf("  %d. %s (score=%d)\n", i+1,
				search_state.results[i].name,
				search_state.results[i].score);
		}
	}
	printf("====================\n");
}
