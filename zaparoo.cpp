// zaparoo.cpp
// Zaparoo NFC integration for MiSTer graphical frontend
// 2024

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>

#include "zaparoo.h"

// Static state
static zaparoo_status_t current_status = ZAPAROO_DISCONNECTED;
static zaparoo_card_t last_card;
static zaparoo_overlay_t overlay_state;
static zaparoo_card_callback_t card_callback = NULL;
static char reader_name[64] = "";
static int reader_detected = 0;

// Overlay timing constants
#define OVERLAY_DURATION_MS 3000  // Auto-hide after 3 seconds
#define OVERLAY_FADE_IN_MS  300   // Fade in duration
#define OVERLAY_FADE_OUT_MS 200   // Fade out duration

//// Helper: Check for NFC reader in /dev ////
static int detect_nfc_reader(void)
{
	// Check for common NFC reader device paths
	// ACR122U typically appears as /dev/usb/hiddevX or via PC/SC
	// PN532 via serial appears as /dev/ttyUSBX or /dev/ttyACMX

	DIR *dir;
	struct dirent *entry;

	// Check /dev for USB serial devices (PN532)
	dir = opendir("/dev");
	if (dir)
	{
		while ((entry = readdir(dir)) != NULL)
		{
			if (strncmp(entry->d_name, "ttyUSB", 6) == 0 ||
			    strncmp(entry->d_name, "ttyACM", 6) == 0)
			{
				// Could be a PN532 - would need further probing
				// For now, just note presence
				closedir(dir);
				strncpy(reader_name, "PN532", sizeof(reader_name));
				return 1;
			}
		}
		closedir(dir);
	}

	// Check for PC/SC daemon (used by ACR122U)
	if (access("/var/run/pcscd/pcscd.comm", F_OK) == 0)
	{
		strncpy(reader_name, "ACR122U", sizeof(reader_name));
		return 1;
	}

	reader_name[0] = '\0';
	return 0;
}

//// Core Functions ////

void zaparoo_init(void)
{
	memset(&last_card, 0, sizeof(last_card));
	memset(&overlay_state, 0, sizeof(overlay_state));
	overlay_state.opacity = 0.0f;
	overlay_state.scale = 0.95f;

	// Initial reader detection
	reader_detected = detect_nfc_reader();
	current_status = reader_detected ? ZAPAROO_IDLE : ZAPAROO_DISCONNECTED;

	printf("Zaparoo: Initialized, reader %s\n",
	       reader_detected ? reader_name : "not detected");
}

void zaparoo_shutdown(void)
{
	zaparoo_hide_overlay();
	card_callback = NULL;
	current_status = ZAPAROO_DISCONNECTED;
	reader_detected = 0;
}

void zaparoo_poll(void)
{
	// Periodically re-check for reader connection
	static int poll_counter = 0;
	poll_counter++;

	// Check every ~60 frames (2 seconds at 30fps)
	if (poll_counter >= 60)
	{
		poll_counter = 0;
		int was_detected = reader_detected;
		reader_detected = detect_nfc_reader();

		if (reader_detected && !was_detected)
		{
			printf("Zaparoo: Reader connected (%s)\n", reader_name);
			current_status = ZAPAROO_IDLE;
		}
		else if (!reader_detected && was_detected)
		{
			printf("Zaparoo: Reader disconnected\n");
			current_status = ZAPAROO_DISCONNECTED;
		}
	}

	// Update overlay auto-hide
	zaparoo_overlay_update();
}

//// Status Functions ////

zaparoo_status_t zaparoo_get_status(void)
{
	return current_status;
}

int zaparoo_is_reader_connected(void)
{
	return reader_detected;
}

const char* zaparoo_get_reader_name(void)
{
	return reader_name[0] ? reader_name : "None";
}

//// Card Functions ////

const zaparoo_card_t* zaparoo_get_last_card(void)
{
	if (last_card.game_name[0])
		return &last_card;
	return NULL;
}

void zaparoo_clear_card(void)
{
	memset(&last_card, 0, sizeof(last_card));
	current_status = reader_detected ? ZAPAROO_IDLE : ZAPAROO_DISCONNECTED;
}

void zaparoo_simulate_scan(const char *game_name, const char *game_path, const char *core_name)
{
	// Simulate a card scan for testing purposes
	strncpy(last_card.game_name, game_name, sizeof(last_card.game_name) - 1);
	strncpy(last_card.game_path, game_path, sizeof(last_card.game_path) - 1);
	strncpy(last_card.core_name, core_name ? core_name : "", sizeof(last_card.core_name) - 1);
	strncpy(last_card.card_uid, "TEST-UID-1234", sizeof(last_card.card_uid) - 1);
	last_card.detect_time = 0; // Would use GetTimer(0) on real hardware

	current_status = ZAPAROO_CARD_DETECTED;

	// Show overlay
	zaparoo_show_overlay(game_name, game_path);

	// Invoke callback if registered
	if (card_callback)
	{
		card_callback(&last_card);
	}

	printf("Zaparoo: Simulated scan - %s\n", game_name);
}

//// Overlay Functions ////

zaparoo_overlay_t* zaparoo_get_overlay(void)
{
	return &overlay_state;
}

void zaparoo_show_overlay(const char *game_name, const char *game_path)
{
	strncpy(overlay_state.card.game_name, game_name, sizeof(overlay_state.card.game_name) - 1);
	strncpy(overlay_state.card.game_path, game_path, sizeof(overlay_state.card.game_path) - 1);
	overlay_state.active = 1;
	overlay_state.opacity = 0.0f;  // Start transparent for fade-in
	overlay_state.scale = 0.95f;   // Start slightly smaller for pop effect
	overlay_state.show_time = 0;   // Would use GetTimer(0) on real hardware

	// Animation would be started here using animator system
	// For now, snap to visible
	overlay_state.opacity = 1.0f;
	overlay_state.scale = 1.0f;
}

void zaparoo_hide_overlay(void)
{
	overlay_state.active = 0;
	overlay_state.opacity = 0.0f;
}

int zaparoo_overlay_active(void)
{
	return overlay_state.active;
}

void zaparoo_overlay_update(void)
{
	if (!overlay_state.active) return;

	// Auto-hide logic would check elapsed time here
	// For testing, overlay stays visible until explicitly hidden
}

//// Callback Registration ////

void zaparoo_set_card_callback(zaparoo_card_callback_t callback)
{
	card_callback = callback;
}
