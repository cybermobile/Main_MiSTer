// zaparoo.h
// Zaparoo NFC integration for MiSTer graphical frontend
// Provides NFC reader status and card scan overlay support
// 2024

#ifndef __ZAPAROO_H__
#define __ZAPAROO_H__

#include <inttypes.h>

// NFC reader connection status
typedef enum {
	ZAPAROO_DISCONNECTED = 0,  // No NFC reader detected
	ZAPAROO_IDLE,              // Reader connected, waiting for card
	ZAPAROO_SCANNING,          // Actively scanning for card
	ZAPAROO_CARD_DETECTED      // Card detected and being processed
} zaparoo_status_t;

// Card information structure
typedef struct {
	char game_name[256];       // Display name of the game
	char game_path[1024];      // Full path to game file
	char core_name[64];        // Core to use (e.g., "SNES", "Genesis")
	char card_uid[32];         // NFC card UID
	uint32_t detect_time;      // When card was detected
} zaparoo_card_t;

// Overlay state for card scan display
typedef struct {
	int active;                // Overlay is visible
	zaparoo_card_t card;       // Card being displayed
	float opacity;             // Current opacity (for animation)
	float scale;               // Current scale (for animation)
	uint32_t show_time;        // When overlay was shown
	uint32_t anim_id;          // Animation ID for fade
	uint32_t scale_anim_id;    // Animation ID for scale
} zaparoo_overlay_t;

//// Core Functions ////

// Initialize Zaparoo system
void zaparoo_init(void);

// Shutdown Zaparoo system
void zaparoo_shutdown(void);

// Poll for NFC reader status updates (call periodically)
void zaparoo_poll(void);

//// Status Functions ////

// Get current NFC reader status
zaparoo_status_t zaparoo_get_status(void);

// Check if NFC reader is connected
int zaparoo_is_reader_connected(void);

// Get reader name (e.g., "ACR122U", "PN532")
const char* zaparoo_get_reader_name(void);

//// Card Functions ////

// Get last detected card info (NULL if none)
const zaparoo_card_t* zaparoo_get_last_card(void);

// Clear last card info
void zaparoo_clear_card(void);

// Simulate card scan (for testing)
void zaparoo_simulate_scan(const char *game_name, const char *game_path, const char *core_name);

//// Overlay Functions ////

// Get overlay state
zaparoo_overlay_t* zaparoo_get_overlay(void);

// Show card scan overlay
void zaparoo_show_overlay(const char *game_name, const char *game_path);

// Hide card scan overlay
void zaparoo_hide_overlay(void);

// Check if overlay is active
int zaparoo_overlay_active(void);

// Update overlay animations (call each frame)
void zaparoo_overlay_update(void);

//// Callback Registration ////

// Callback type for card detection
typedef void (*zaparoo_card_callback_t)(const zaparoo_card_t *card);

// Register callback for card detection
void zaparoo_set_card_callback(zaparoo_card_callback_t callback);

#endif // __ZAPAROO_H__
