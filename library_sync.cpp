// library_sync.cpp
// Background library synchronization for MiSTer graphical frontend
// Scans ROM folders, generates MGLs, downloads missing boxart
// 2025

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#include "library_sync.h"
#include "mgl_generator.h"
#include "scraper.h"
#include "boxart.h"
#include "file_io.h"

// Sync state
static pthread_t sync_thread;
static int sync_thread_active = 0;
static int sync_stop_requested = 0;
static library_sync_status_t sync_status = SYNC_IDLE;
static library_sync_progress_t sync_progress;
static pthread_mutex_t sync_mutex = PTHREAD_MUTEX_INITIALIZER;

// Forward declaration
static void* sync_thread_func(void *arg);

//// Core Functions ////

void library_sync_init(void)
{
	memset(&sync_progress, 0, sizeof(sync_progress));
	sync_status = SYNC_IDLE;
	sync_thread_active = 0;
	sync_stop_requested = 0;
	printf("Library Sync: Initialized\n");
}

void library_sync_shutdown(void)
{
	library_sync_stop();
	printf("Library Sync: Shutdown\n");
}

int library_sync_start(void)
{
	if (sync_thread_active) {
		printf("Library Sync: Already running\n");
		return 0;
	}

	sync_stop_requested = 0;
	sync_status = SYNC_RUNNING;
	memset(&sync_progress, 0, sizeof(sync_progress));

	if (pthread_create(&sync_thread, NULL, sync_thread_func, NULL) != 0) {
		printf("Library Sync: Failed to create thread\n");
		sync_status = SYNC_ERROR;
		return 0;
	}

	sync_thread_active = 1;
	pthread_detach(sync_thread);

	printf("Library Sync: Started background sync\n");
	return 1;
}

void library_sync_stop(void)
{
	if (!sync_thread_active) return;

	sync_stop_requested = 1;

	// Wait for thread to finish (with timeout)
	for (int i = 0; i < 50 && sync_thread_active; i++) {
		usleep(100000);  // 100ms
	}

	printf("Library Sync: Stopped\n");
}

library_sync_status_t library_sync_get_status(void)
{
	return sync_status;
}

library_sync_progress_t* library_sync_get_progress(void)
{
	return &sync_progress;
}

int library_sync_is_running(void)
{
	return sync_thread_active;
}

//// Sync Thread ////

static void* sync_thread_func(void *arg)
{
	(void)arg;

	// Open debug log file
	FILE *logf = fopen("/media/fat/sync_debug.log", "w");
	if (logf) {
		fprintf(logf, "Library Sync: Thread started\n");
		fflush(logf);
	}

	printf("Library Sync: Thread started\n");
	fflush(stdout);

	int total_systems = mgl_get_system_count();
	
	if (logf) {
		fprintf(logf, "Library Sync: Found %d system configs\n", total_systems);
		fflush(logf);
	}
	printf("Library Sync: Found %d system configs\n", total_systems);
	fflush(stdout);
	
	sync_progress.total_systems = total_systems;
	sync_progress.mgls_created = 0;
	sync_progress.boxart_downloaded = 0;

	// Phase 1: Generate MGLs for all systems
	printf("Library Sync: Phase 1 - Generating MGLs\n");
	fflush(stdout);
	if (logf) { fprintf(logf, "Phase 1: Generating MGLs\n"); fflush(logf); }
	strncpy(sync_progress.current_phase, "Generating MGLs", sizeof(sync_progress.current_phase));

	for (int i = 0; i < total_systems && !sync_stop_requested; i++) {
		const char *system_name = mgl_get_system_name(i);
		if (!system_name) continue;

		if (logf) { fprintf(logf, "Processing system %d: %s\n", i, system_name); fflush(logf); }

		strncpy(sync_progress.current_system, system_name, sizeof(sync_progress.current_system));
		sync_progress.systems_processed = i;

		int created = mgl_generate_for_system(system_name);
		if (logf) { fprintf(logf, "  -> Created %d MGLs\n", created); fflush(logf); }
		if (created > 0) {
			sync_progress.mgls_created += created;
		}
	}

	if (sync_stop_requested) {
		sync_status = SYNC_IDLE;
		sync_thread_active = 0;
		return NULL;
	}

	// Phase 2: Download missing boxart using libretro-thumbnails
	printf("Library Sync: Phase 2 - Downloading boxart\n");
	strncpy(sync_progress.current_phase, "Downloading Boxart", sizeof(sync_progress.current_phase));

	for (int i = 0; i < total_systems && !sync_stop_requested; i++) {
		const char *system_name = mgl_get_system_name(i);
		if (!system_name) continue;

		if (!mgl_system_has_roms(system_name)) continue;

		strncpy(sync_progress.current_system, system_name, sizeof(sync_progress.current_system));
		sync_progress.systems_processed = i;

		// Scan the MGL folder for games and check boxart
		// Try underscore-prefixed folder first (common convention)
		char mgl_dir[512];
		snprintf(mgl_dir, sizeof(mgl_dir), "/media/fat/_Games/_%s", system_name);

		DIR *dir = opendir(mgl_dir);
		if (!dir) {
			// Try without underscore
			snprintf(mgl_dir, sizeof(mgl_dir), "/media/fat/_Games/%s", system_name);
			dir = opendir(mgl_dir);
		}
		if (!dir) continue;

		struct dirent *entry;
		while ((entry = readdir(dir)) != NULL && !sync_stop_requested) {
			if (entry->d_name[0] == '.') continue;

			// Check if it's an MGL file
			const char *ext = strrchr(entry->d_name, '.');
			if (!ext || strcasecmp(ext, ".mgl") != 0) continue;

			// Get game name (without .mgl extension)
			char game_name[256];
			strncpy(game_name, entry->d_name, sizeof(game_name) - 1);
			game_name[sizeof(game_name) - 1] = '\0';
			char *dot = strrchr(game_name, '.');
			if (dot) *dot = '\0';

			strncpy(sync_progress.current_game, game_name, sizeof(sync_progress.current_game));

			// Check if boxart already exists
			if (scraper_has_artwork(game_name, system_name)) {
				continue;  // Already have it
			}

			// Try to download from libretro-thumbnails (and fallbacks)
			scraper_result_t result;
			if (scraper_scrape_game(game_name, NULL, system_name, &result)) {
				if (result.downloaded) {
					sync_progress.boxart_downloaded++;
				}
			}

			// Small delay to avoid hammering servers
			usleep(200000);  // 200ms between requests
		}

		closedir(dir);
	}

	// Complete
	sync_status = SYNC_COMPLETE;
	strncpy(sync_progress.current_phase, "Complete", sizeof(sync_progress.current_phase));
	sync_progress.current_system[0] = '\0';
	sync_progress.current_game[0] = '\0';
	sync_progress.systems_processed = total_systems;

	printf("Library Sync: Complete - Created %d MGLs, Downloaded %d boxart images\n",
	       sync_progress.mgls_created, sync_progress.boxart_downloaded);

	if (logf) {
		fprintf(logf, "Library Sync: Complete - Created %d MGLs, Downloaded %d boxart images\n",
		        sync_progress.mgls_created, sync_progress.boxart_downloaded);
		fclose(logf);
	}

	sync_thread_active = 0;
	return NULL;
}

//// Quick Sync (specific system) ////

int library_sync_system(const char *system_name)
{
	if (!system_name) return 0;

	printf("Library Sync: Quick sync for %s\n", system_name);

	// Generate MGLs for this system
	int mgls = mgl_generate_for_system(system_name);

	// Note: boxart download is handled by auto-scrape when games are browsed
	// This just ensures MGLs exist

	return mgls >= 0 ? 1 : 0;
}
