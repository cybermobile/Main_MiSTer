// core_settings.h
// Unified core settings system for MiSTer modern frontend
// Provides consistent settings menu across all cores (NES, SNES, Genesis, etc.)
// 2024

#ifndef __CORE_SETTINGS_H__
#define __CORE_SETTINGS_H__

#include <inttypes.h>

// Maximum settings per core
#define CORE_SETTINGS_MAX 64
#define CORE_SETTINGS_MAX_OPTIONS 32
#define CORE_SETTINGS_NAME_LEN 64
#define CORE_SETTINGS_DESC_LEN 256

// Setting types
typedef enum {
	SETTING_TYPE_TOGGLE,      // On/Off boolean
	SETTING_TYPE_CHOICE,      // Multiple choice (dropdown)
	SETTING_TYPE_RANGE,       // Numeric range with min/max
	SETTING_TYPE_SUBMENU,     // Opens a submenu
	SETTING_TYPE_ACTION,      // Triggers an action (reset, save, etc.)
	SETTING_TYPE_SEPARATOR,   // Visual separator
	SETTING_TYPE_INFO         // Display-only information
} setting_type_t;

// Setting categories for organization
typedef enum {
	SETTING_CAT_VIDEO,        // Video/display settings
	SETTING_CAT_AUDIO,        // Audio settings
	SETTING_CAT_INPUT,        // Controller/input settings
	SETTING_CAT_SYSTEM,       // System/hardware settings
	SETTING_CAT_CHEATS,       // Cheat codes
	SETTING_CAT_MEMORY,       // Memory card/save settings
	SETTING_CAT_ADVANCED,     // Advanced/misc settings
	SETTING_CAT_COUNT
} setting_category_t;

// Individual option for choice settings
typedef struct {
	char name[CORE_SETTINGS_NAME_LEN];
	int value;
	char description[CORE_SETTINGS_DESC_LEN];
} setting_option_t;

// Single setting definition
typedef struct {
	char id[32];                          // Unique identifier
	char name[CORE_SETTINGS_NAME_LEN];    // Display name
	char description[CORE_SETTINGS_DESC_LEN];
	setting_type_t type;
	setting_category_t category;

	// Current value
	int value;
	int default_value;

	// For SETTING_TYPE_RANGE
	int min_value;
	int max_value;
	int step;
	char format[16];                      // Printf format (e.g., "%d%%")

	// For SETTING_TYPE_CHOICE
	setting_option_t options[CORE_SETTINGS_MAX_OPTIONS];
	int option_count;

	// OSD mapping (for communication with core)
	int osd_id;                           // OSD menu item ID
	uint8_t osd_mask;                     // Bit mask for status word
	int osd_shift;                        // Bit shift amount

	// Flags
	uint8_t requires_restart;             // Need core restart to apply
	uint8_t hidden;                       // Not shown in menu
	uint8_t readonly;                     // Cannot be modified
} core_setting_t;

// Category info
typedef struct {
	char name[CORE_SETTINGS_NAME_LEN];
	char icon[32];                        // Icon name for UI
	int setting_count;
} category_info_t;

// Core profile (stores all settings for a core)
typedef struct {
	char core_name[64];                   // e.g., "NES", "SNES", "Genesis"
	char core_id[32];                     // Internal identifier
	char core_version[16];
	char rbf_name[64];                    // Core filename

	core_setting_t settings[CORE_SETTINGS_MAX];
	int setting_count;

	category_info_t categories[SETTING_CAT_COUNT];

	// Flags
	uint8_t loaded;
	uint8_t modified;
} core_profile_t;

// Settings menu state
typedef struct {
	core_profile_t *profile;
	setting_category_t current_category;
	int selected_index;
	int scroll_offset;
	int visible_count;

	// For editing
	uint8_t editing;
	int edit_value;

	// Animation
	float scroll_anim;
	float select_anim;
} settings_menu_state_t;

//// Core Functions ////

// Initialize core settings system
void core_settings_init(void);

// Shutdown core settings system
void core_settings_shutdown(void);

// Load settings for a specific core
int core_settings_load(const char *core_name);

// Save current core settings
int core_settings_save(void);

// Save settings to specific file
int core_settings_save_to(const char *filepath);

// Get current core profile
core_profile_t* core_settings_get_profile(void);

// Check if settings are loaded
int core_settings_is_loaded(void);

//// Setting Access ////

// Get setting by ID
core_setting_t* core_settings_get(const char *id);

// Get setting by index
core_setting_t* core_settings_get_by_index(int index);

// Get settings for category
int core_settings_get_by_category(setting_category_t category,
                                   core_setting_t **out_settings,
                                   int max_count);

// Get setting value
int core_settings_get_value(const char *id);

// Set setting value
void core_settings_set_value(const char *id, int value);

// Reset setting to default
void core_settings_reset(const char *id);

// Reset all settings to defaults
void core_settings_reset_all(void);

//// Menu State ////

// Get menu state
settings_menu_state_t* core_settings_get_menu_state(void);

// Open settings menu
void core_settings_menu_open(void);

// Close settings menu
void core_settings_menu_close(void);

// Check if menu is open
int core_settings_menu_is_open(void);

// Navigate menu
void core_settings_menu_up(void);
void core_settings_menu_down(void);
void core_settings_menu_left(void);
void core_settings_menu_right(void);
void core_settings_menu_select(void);
void core_settings_menu_back(void);

// Switch category
void core_settings_menu_next_category(void);
void core_settings_menu_prev_category(void);
void core_settings_menu_set_category(setting_category_t category);

//// OSD Integration ////

// Sync settings with OSD (read from core)
void core_settings_sync_from_osd(void);

// Apply settings to OSD (write to core)
void core_settings_sync_to_osd(void);

// Handle OSD status change
void core_settings_osd_changed(int osd_id, int value);

//// Profile Management ////

// Create default profile for known cores
void core_settings_create_nes_profile(core_profile_t *profile);
void core_settings_create_snes_profile(core_profile_t *profile);
void core_settings_create_genesis_profile(core_profile_t *profile);
void core_settings_create_gba_profile(core_profile_t *profile);
void core_settings_create_psx_profile(core_profile_t *profile);
void core_settings_create_n64_profile(core_profile_t *profile);
void core_settings_create_arcade_profile(core_profile_t *profile, const char *mra_name);

// Generic profile creation from OSD parsing
void core_settings_create_from_osd(core_profile_t *profile);

//// Common Settings (shared across cores) ////

// Add common video settings
void core_settings_add_video_common(core_profile_t *profile);

// Add common audio settings
void core_settings_add_audio_common(core_profile_t *profile);

// Add common input settings
void core_settings_add_input_common(core_profile_t *profile);

//// Utility Functions ////

// Get category name
const char* core_settings_category_name(setting_category_t category);

// Get category icon
const char* core_settings_category_icon(setting_category_t category);

// Get setting type name
const char* core_settings_type_name(setting_type_t type);

// Format setting value for display
void core_settings_format_value(core_setting_t *setting, char *out_str, int max_len);

// Get option name for current value
const char* core_settings_get_option_name(core_setting_t *setting);

#endif // __CORE_SETTINGS_H__
