// library_sync.h
// Background library synchronization for MiSTer graphical frontend
// 2024

#ifndef __LIBRARY_SYNC_H__
#define __LIBRARY_SYNC_H__

#include <dirent.h>

// Sync status
typedef enum {
	SYNC_IDLE = 0,
	SYNC_RUNNING,
	SYNC_COMPLETE,
	SYNC_ERROR
} library_sync_status_t;

// Sync progress
typedef struct {
	int total_systems;
	int systems_processed;
	int mgls_created;
	int boxart_downloaded;
	char current_phase[64];
	char current_system[64];
	char current_game[256];
} library_sync_progress_t;

// Initialize sync system
void library_sync_init(void);

// Shutdown sync system
void library_sync_shutdown(void);

// Start background sync (non-blocking)
// Returns: 1 on success, 0 if already running or error
int library_sync_start(void);

// Stop background sync
void library_sync_stop(void);

// Get current sync status
library_sync_status_t library_sync_get_status(void);

// Get sync progress
library_sync_progress_t* library_sync_get_progress(void);

// Check if sync is currently running
int library_sync_is_running(void);

// Quick sync for a specific system (blocking)
int library_sync_system(const char *system_name);

#endif // __LIBRARY_SYNC_H__
