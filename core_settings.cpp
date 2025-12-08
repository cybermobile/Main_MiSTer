// core_settings.cpp
// Unified core settings system for MiSTer modern frontend
// 2024

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include "core_settings.h"
#include "user_io.h"
#include "file_io.h"
#include "osd.h"

// Settings directory
#define SETTINGS_DIR "/media/fat/config/settings"

// Current state
static core_profile_t current_profile;
static settings_menu_state_t menu_state;
static int initialized = 0;

// Category names and icons
static const char *category_names[] = {
	"Video",
	"Audio",
	"Input",
	"System",
	"Cheats",
	"Memory",
	"Advanced"
};

static const char *category_icons[] = {
	"video",
	"audio",
	"gamepad",
	"chip",
	"star",
	"save",
	"gear"
};

//// Helper Functions ////

static void add_setting(core_profile_t *profile, core_setting_t *setting)
{
	if (profile->setting_count < CORE_SETTINGS_MAX) {
		memcpy(&profile->settings[profile->setting_count], setting, sizeof(core_setting_t));
		profile->setting_count++;
		profile->categories[setting->category].setting_count++;
	}
}

static void add_toggle(core_profile_t *profile, const char *id, const char *name,
                       const char *desc, setting_category_t cat, int default_val,
                       int osd_id, uint8_t osd_mask, int osd_shift)
{
	core_setting_t setting = {0};
	strncpy(setting.id, id, sizeof(setting.id) - 1);
	strncpy(setting.name, name, sizeof(setting.name) - 1);
	strncpy(setting.description, desc, sizeof(setting.description) - 1);
	setting.type = SETTING_TYPE_TOGGLE;
	setting.category = cat;
	setting.value = default_val;
	setting.default_value = default_val;
	setting.osd_id = osd_id;
	setting.osd_mask = osd_mask;
	setting.osd_shift = osd_shift;
	add_setting(profile, &setting);
}

static void add_choice(core_profile_t *profile, const char *id, const char *name,
                       const char *desc, setting_category_t cat, int default_val,
                       const char **options, int option_count,
                       int osd_id, uint8_t osd_mask, int osd_shift)
{
	core_setting_t setting = {0};
	strncpy(setting.id, id, sizeof(setting.id) - 1);
	strncpy(setting.name, name, sizeof(setting.name) - 1);
	strncpy(setting.description, desc, sizeof(setting.description) - 1);
	setting.type = SETTING_TYPE_CHOICE;
	setting.category = cat;
	setting.value = default_val;
	setting.default_value = default_val;
	setting.osd_id = osd_id;
	setting.osd_mask = osd_mask;
	setting.osd_shift = osd_shift;

	for (int i = 0; i < option_count && i < CORE_SETTINGS_MAX_OPTIONS; i++) {
		strncpy(setting.options[i].name, options[i], sizeof(setting.options[i].name) - 1);
		setting.options[i].value = i;
	}
	setting.option_count = option_count;

	add_setting(profile, &setting);
}

static void add_range(core_profile_t *profile, const char *id, const char *name,
                      const char *desc, setting_category_t cat, int default_val,
                      int min_val, int max_val, int step, const char *format,
                      int osd_id, uint8_t osd_mask, int osd_shift)
{
	core_setting_t setting = {0};
	strncpy(setting.id, id, sizeof(setting.id) - 1);
	strncpy(setting.name, name, sizeof(setting.name) - 1);
	strncpy(setting.description, desc, sizeof(setting.description) - 1);
	setting.type = SETTING_TYPE_RANGE;
	setting.category = cat;
	setting.value = default_val;
	setting.default_value = default_val;
	setting.min_value = min_val;
	setting.max_value = max_val;
	setting.step = step;
	strncpy(setting.format, format, sizeof(setting.format) - 1);
	setting.osd_id = osd_id;
	setting.osd_mask = osd_mask;
	setting.osd_shift = osd_shift;
	add_setting(profile, &setting);
}

static void add_separator(core_profile_t *profile, const char *name, setting_category_t cat)
{
	core_setting_t setting = {0};
	snprintf(setting.id, sizeof(setting.id), "sep_%d", profile->setting_count);
	strncpy(setting.name, name, sizeof(setting.name) - 1);
	setting.type = SETTING_TYPE_SEPARATOR;
	setting.category = cat;
	add_setting(profile, &setting);
}

static void add_action(core_profile_t *profile, const char *id, const char *name,
                       const char *desc, setting_category_t cat, int osd_id)
{
	core_setting_t setting = {0};
	strncpy(setting.id, id, sizeof(setting.id) - 1);
	strncpy(setting.name, name, sizeof(setting.name) - 1);
	strncpy(setting.description, desc, sizeof(setting.description) - 1);
	setting.type = SETTING_TYPE_ACTION;
	setting.category = cat;
	setting.osd_id = osd_id;
	add_setting(profile, &setting);
}

//// Common Settings ////

void core_settings_add_video_common(core_profile_t *profile)
{
	const char *aspect_options[] = {"Original", "Full Screen", "4:3", "16:9"};
	const char *scale_options[] = {"Auto", "1x", "2x", "3x", "4x", "5x"};
	const char *scanline_options[] = {"Off", "25%", "50%", "75%", "100%"};
	const char *filter_options[] = {"None", "Scanlines", "HQ2x", "Scale2x", "CRT"};

	add_separator(profile, "Display", SETTING_CAT_VIDEO);

	add_choice(profile, "aspect_ratio", "Aspect Ratio",
	           "Screen aspect ratio mode",
	           SETTING_CAT_VIDEO, 0, aspect_options, 4, 1, 0x03, 0);

	add_choice(profile, "scale", "Scale",
	           "Integer scaling factor",
	           SETTING_CAT_VIDEO, 0, scale_options, 6, 2, 0x07, 0);

	add_separator(profile, "Effects", SETTING_CAT_VIDEO);

	add_choice(profile, "video_filter", "Video Filter",
	           "Post-processing filter",
	           SETTING_CAT_VIDEO, 0, filter_options, 5, 3, 0x07, 0);

	add_choice(profile, "scanlines", "Scanlines",
	           "Scanline intensity",
	           SETTING_CAT_VIDEO, 0, scanline_options, 5, 4, 0x07, 0);

	add_toggle(profile, "vsync", "V-Sync",
	           "Synchronize to vertical refresh",
	           SETTING_CAT_VIDEO, 1, 5, 0x01, 0);
}

void core_settings_add_audio_common(core_profile_t *profile)
{
	const char *stereo_options[] = {"Stereo", "Mono", "Swap L/R"};

	add_range(profile, "audio_volume", "Volume",
	          "Master audio volume",
	          SETTING_CAT_AUDIO, 100, 0, 100, 5, "%d%%", 10, 0xFF, 0);

	add_choice(profile, "stereo_mode", "Stereo Mode",
	           "Audio channel configuration",
	           SETTING_CAT_AUDIO, 0, stereo_options, 3, 11, 0x03, 0);

	add_toggle(profile, "audio_filter", "Low-Pass Filter",
	           "Apply low-pass audio filter",
	           SETTING_CAT_AUDIO, 0, 12, 0x01, 0);
}

void core_settings_add_input_common(core_profile_t *profile)
{
	const char *dpad_options[] = {"D-Pad", "Left Analog", "Right Analog"};

	add_choice(profile, "dpad_mode", "D-Pad Mode",
	           "Controller D-Pad input source",
	           SETTING_CAT_INPUT, 0, dpad_options, 3, 20, 0x03, 0);

	add_toggle(profile, "swap_ab", "Swap A/B Buttons",
	           "Swap A and B button mapping",
	           SETTING_CAT_INPUT, 0, 21, 0x01, 0);

	add_toggle(profile, "turbo_enable", "Turbo Buttons",
	           "Enable turbo fire on buttons",
	           SETTING_CAT_INPUT, 0, 22, 0x01, 0);

	add_range(profile, "turbo_speed", "Turbo Speed",
	          "Turbo button repeat rate",
	          SETTING_CAT_INPUT, 10, 1, 30, 1, "%d Hz", 23, 0x1F, 0);
}

//// NES Profile ////

void core_settings_create_nes_profile(core_profile_t *profile)
{
	memset(profile, 0, sizeof(core_profile_t));
	strncpy(profile->core_name, "Nintendo Entertainment System", sizeof(profile->core_name) - 1);
	strncpy(profile->core_id, "NES", sizeof(profile->core_id) - 1);
	strncpy(profile->rbf_name, "NES", sizeof(profile->rbf_name) - 1);

	// Initialize category names
	for (int i = 0; i < SETTING_CAT_COUNT; i++) {
		strncpy(profile->categories[i].name, category_names[i], sizeof(profile->categories[i].name) - 1);
		strncpy(profile->categories[i].icon, category_icons[i], sizeof(profile->categories[i].icon) - 1);
	}

	// Video settings
	core_settings_add_video_common(profile);

	const char *palette_options[] = {"FCEUX", "Composite Direct", "PC-10", "PVM", "Wavebeam", "Smooth", "Grayscale"};
	add_choice(profile, "nes_palette", "Color Palette",
	           "NES color palette selection",
	           SETTING_CAT_VIDEO, 0, palette_options, 7, 30, 0x0F, 0);

	add_toggle(profile, "hide_overscan", "Hide Overscan",
	           "Hide overscan border area",
	           SETTING_CAT_VIDEO, 1, 31, 0x01, 0);

	// Audio settings
	core_settings_add_audio_common(profile);

	add_toggle(profile, "expansion_audio", "Expansion Audio",
	           "Enable expansion chip audio (FDS, VRC6, etc.)",
	           SETTING_CAT_AUDIO, 1, 40, 0x01, 0);

	// Input settings
	core_settings_add_input_common(profile);

	const char *controller_options[] = {"Standard", "Four Score", "Zapper", "Power Pad"};
	add_choice(profile, "controller_type", "Controller Type",
	           "NES controller peripheral type",
	           SETTING_CAT_INPUT, 0, controller_options, 4, 50, 0x03, 0);

	// System settings
	const char *region_options[] = {"Auto", "NTSC", "PAL", "Dendy"};
	add_choice(profile, "nes_region", "Region",
	           "Console region (affects timing)",
	           SETTING_CAT_SYSTEM, 0, region_options, 4, 60, 0x03, 0);

	add_toggle(profile, "fds_auto_insert", "FDS Auto-Insert",
	           "Automatically insert FDS disk",
	           SETTING_CAT_SYSTEM, 1, 61, 0x01, 0);

	add_action(profile, "fds_flip_disk", "Flip FDS Disk",
	           "Flip Famicom Disk System disk",
	           SETTING_CAT_SYSTEM, 62);

	// Memory settings
	add_toggle(profile, "autosave", "Auto-Save SRAM",
	           "Automatically save battery-backed RAM",
	           SETTING_CAT_MEMORY, 1, 70, 0x01, 0);

	add_action(profile, "save_state", "Save State",
	           "Save current game state",
	           SETTING_CAT_MEMORY, 71);

	add_action(profile, "load_state", "Load State",
	           "Load saved game state",
	           SETTING_CAT_MEMORY, 72);

	profile->loaded = 1;
}

//// SNES Profile ////

void core_settings_create_snes_profile(core_profile_t *profile)
{
	memset(profile, 0, sizeof(core_profile_t));
	strncpy(profile->core_name, "Super Nintendo", sizeof(profile->core_name) - 1);
	strncpy(profile->core_id, "SNES", sizeof(profile->core_id) - 1);
	strncpy(profile->rbf_name, "SNES", sizeof(profile->rbf_name) - 1);

	for (int i = 0; i < SETTING_CAT_COUNT; i++) {
		strncpy(profile->categories[i].name, category_names[i], sizeof(profile->categories[i].name) - 1);
		strncpy(profile->categories[i].icon, category_icons[i], sizeof(profile->categories[i].icon) - 1);
	}

	// Video settings
	core_settings_add_video_common(profile);

	add_toggle(profile, "pseudo_hires", "Pseudo Hi-Res",
	           "Enable pseudo hi-res blending",
	           SETTING_CAT_VIDEO, 1, 30, 0x01, 0);

	add_toggle(profile, "mode7_perspective", "Mode 7 Perspective",
	           "Enhanced Mode 7 perspective correction",
	           SETTING_CAT_VIDEO, 0, 31, 0x01, 0);

	const char *widescreen_options[] = {"Off", "64:49", "16:9"};
	add_choice(profile, "widescreen", "Widescreen",
	           "Widescreen mode (hacks required)",
	           SETTING_CAT_VIDEO, 0, widescreen_options, 3, 32, 0x03, 0);

	// Audio settings
	core_settings_add_audio_common(profile);

	add_toggle(profile, "dsp_audio", "DSP Audio",
	           "Enable DSP audio processing",
	           SETTING_CAT_AUDIO, 1, 40, 0x01, 0);

	add_toggle(profile, "msu1_audio", "MSU-1 Audio",
	           "Enable MSU-1 CD quality audio",
	           SETTING_CAT_AUDIO, 1, 41, 0x01, 0);

	// Input settings
	core_settings_add_input_common(profile);

	const char *snes_controller_options[] = {"Gamepad", "Mouse", "Super Scope", "Justifier", "Multitap"};
	add_choice(profile, "port1_device", "Port 1 Device",
	           "Controller port 1 device type",
	           SETTING_CAT_INPUT, 0, snes_controller_options, 5, 50, 0x07, 0);

	add_choice(profile, "port2_device", "Port 2 Device",
	           "Controller port 2 device type",
	           SETTING_CAT_INPUT, 0, snes_controller_options, 5, 51, 0x07, 0);

	// System settings
	const char *region_options[] = {"Auto", "NTSC", "PAL"};
	add_choice(profile, "snes_region", "Region",
	           "Console region (affects timing)",
	           SETTING_CAT_SYSTEM, 0, region_options, 3, 60, 0x03, 0);

	const char *chip_options[] = {"Auto", "CX4", "DSP", "SA-1", "SuperFX", "SDD-1"};
	add_choice(profile, "special_chip", "Special Chip",
	           "Enhancement chip override",
	           SETTING_CAT_SYSTEM, 0, chip_options, 6, 61, 0x07, 0);

	add_toggle(profile, "fast_loading", "Fast Loading",
	           "Skip BIOS and speed up loading",
	           SETTING_CAT_SYSTEM, 0, 62, 0x01, 0);

	// Memory settings
	add_toggle(profile, "autosave", "Auto-Save SRAM",
	           "Automatically save battery-backed RAM",
	           SETTING_CAT_MEMORY, 1, 70, 0x01, 0);

	add_action(profile, "save_state", "Save State",
	           "Save current game state",
	           SETTING_CAT_MEMORY, 71);

	add_action(profile, "load_state", "Load State",
	           "Load saved game state",
	           SETTING_CAT_MEMORY, 72);

	profile->loaded = 1;
}

//// Genesis/Mega Drive Profile ////

void core_settings_create_genesis_profile(core_profile_t *profile)
{
	memset(profile, 0, sizeof(core_profile_t));
	strncpy(profile->core_name, "Sega Genesis / Mega Drive", sizeof(profile->core_name) - 1);
	strncpy(profile->core_id, "Genesis", sizeof(profile->core_id) - 1);
	strncpy(profile->rbf_name, "Genesis", sizeof(profile->rbf_name) - 1);

	for (int i = 0; i < SETTING_CAT_COUNT; i++) {
		strncpy(profile->categories[i].name, category_names[i], sizeof(profile->categories[i].name) - 1);
		strncpy(profile->categories[i].icon, category_icons[i], sizeof(profile->categories[i].icon) - 1);
	}

	// Video settings
	core_settings_add_video_common(profile);

	add_toggle(profile, "border", "Display Border",
	           "Show overscan border area",
	           SETTING_CAT_VIDEO, 0, 30, 0x01, 0);

	add_toggle(profile, "shadow_highlight", "Shadow/Highlight",
	           "Enable shadow/highlight mode",
	           SETTING_CAT_VIDEO, 1, 31, 0x01, 0);

	const char *vdp_options[] = {"Original", "Enhanced", "No Sprite Limit"};
	add_choice(profile, "vdp_mode", "VDP Mode",
	           "Video display processor mode",
	           SETTING_CAT_VIDEO, 0, vdp_options, 3, 32, 0x03, 0);

	// Audio settings
	core_settings_add_audio_common(profile);

	const char *fm_options[] = {"YM2612 (Discrete)", "YM3438 (ASIC)", "YMF276 (OPN2C)"};
	add_choice(profile, "fm_chip", "FM Chip",
	           "FM synthesis chip emulation",
	           SETTING_CAT_AUDIO, 0, fm_options, 3, 40, 0x03, 0);

	add_toggle(profile, "psg_audio", "PSG Audio",
	           "Enable PSG sound chip",
	           SETTING_CAT_AUDIO, 1, 41, 0x01, 0);

	add_toggle(profile, "cd_audio", "CD Audio",
	           "Enable Sega CD audio",
	           SETTING_CAT_AUDIO, 1, 42, 0x01, 0);

	// Input settings
	core_settings_add_input_common(profile);

	const char *genesis_controller_options[] = {"3-Button", "6-Button", "Mouse", "Menacer", "Justifier", "Team Player"};
	add_choice(profile, "port1_device", "Port 1 Device",
	           "Controller port 1 device type",
	           SETTING_CAT_INPUT, 1, genesis_controller_options, 6, 50, 0x07, 0);

	add_choice(profile, "port2_device", "Port 2 Device",
	           "Controller port 2 device type",
	           SETTING_CAT_INPUT, 1, genesis_controller_options, 6, 51, 0x07, 0);

	// System settings
	const char *region_options[] = {"Auto", "USA", "Europe", "Japan"};
	add_choice(profile, "genesis_region", "Region",
	           "Console region",
	           SETTING_CAT_SYSTEM, 0, region_options, 4, 60, 0x03, 0);

	add_toggle(profile, "sega_cd", "Sega CD",
	           "Enable Sega CD / Mega CD",
	           SETTING_CAT_SYSTEM, 0, 61, 0x01, 0);

	add_toggle(profile, "32x", "32X",
	           "Enable 32X add-on",
	           SETTING_CAT_SYSTEM, 0, 62, 0x01, 0);

	add_toggle(profile, "bios", "Use BIOS",
	           "Boot with system BIOS",
	           SETTING_CAT_SYSTEM, 0, 63, 0x01, 0);

	// Memory settings
	add_toggle(profile, "autosave", "Auto-Save SRAM",
	           "Automatically save battery-backed RAM",
	           SETTING_CAT_MEMORY, 1, 70, 0x01, 0);

	add_action(profile, "save_state", "Save State",
	           "Save current game state",
	           SETTING_CAT_MEMORY, 71);

	add_action(profile, "load_state", "Load State",
	           "Load saved game state",
	           SETTING_CAT_MEMORY, 72);

	profile->loaded = 1;
}

//// GBA Profile ////

void core_settings_create_gba_profile(core_profile_t *profile)
{
	memset(profile, 0, sizeof(core_profile_t));
	strncpy(profile->core_name, "Game Boy Advance", sizeof(profile->core_name) - 1);
	strncpy(profile->core_id, "GBA", sizeof(profile->core_id) - 1);
	strncpy(profile->rbf_name, "GBA", sizeof(profile->rbf_name) - 1);

	for (int i = 0; i < SETTING_CAT_COUNT; i++) {
		strncpy(profile->categories[i].name, category_names[i], sizeof(profile->categories[i].name) - 1);
		strncpy(profile->categories[i].icon, category_icons[i], sizeof(profile->categories[i].icon) - 1);
	}

	// Video settings
	core_settings_add_video_common(profile);

	const char *color_options[] = {"GBA", "GBA SP", "DS", "Original"};
	add_choice(profile, "color_correction", "Color Correction",
	           "LCD color correction mode",
	           SETTING_CAT_VIDEO, 0, color_options, 4, 30, 0x03, 0);

	add_toggle(profile, "ghosting", "LCD Ghosting",
	           "Simulate LCD response time",
	           SETTING_CAT_VIDEO, 0, 31, 0x01, 0);

	// Audio settings
	core_settings_add_audio_common(profile);

	// Input settings
	core_settings_add_input_common(profile);

	add_toggle(profile, "tilt_enable", "Tilt Sensor",
	           "Enable tilt sensor emulation",
	           SETTING_CAT_INPUT, 0, 50, 0x01, 0);

	add_toggle(profile, "solar_sensor", "Solar Sensor",
	           "Enable solar sensor (Boktai)",
	           SETTING_CAT_INPUT, 0, 51, 0x01, 0);

	// System settings
	add_toggle(profile, "bios", "Use BIOS",
	           "Boot with GBA BIOS",
	           SETTING_CAT_SYSTEM, 1, 60, 0x01, 0);

	add_toggle(profile, "fast_forward", "Fast Forward",
	           "Enable fast forward (uncapped speed)",
	           SETTING_CAT_SYSTEM, 0, 61, 0x01, 0);

	// Memory settings
	add_toggle(profile, "autosave", "Auto-Save SRAM",
	           "Automatically save battery-backed RAM",
	           SETTING_CAT_MEMORY, 1, 70, 0x01, 0);

	const char *save_type_options[] = {"Auto", "EEPROM", "SRAM", "Flash 64K", "Flash 128K"};
	add_choice(profile, "save_type", "Save Type",
	           "Override save type detection",
	           SETTING_CAT_MEMORY, 0, save_type_options, 5, 71, 0x07, 0);

	add_action(profile, "save_state", "Save State",
	           "Save current game state",
	           SETTING_CAT_MEMORY, 72);

	add_action(profile, "load_state", "Load State",
	           "Load saved game state",
	           SETTING_CAT_MEMORY, 73);

	profile->loaded = 1;
}

//// PSX Profile ////

void core_settings_create_psx_profile(core_profile_t *profile)
{
	memset(profile, 0, sizeof(core_profile_t));
	strncpy(profile->core_name, "PlayStation", sizeof(profile->core_name) - 1);
	strncpy(profile->core_id, "PSX", sizeof(profile->core_id) - 1);
	strncpy(profile->rbf_name, "PSX", sizeof(profile->rbf_name) - 1);

	for (int i = 0; i < SETTING_CAT_COUNT; i++) {
		strncpy(profile->categories[i].name, category_names[i], sizeof(profile->categories[i].name) - 1);
		strncpy(profile->categories[i].icon, category_icons[i], sizeof(profile->categories[i].icon) - 1);
	}

	// Video settings
	core_settings_add_video_common(profile);

	const char *dither_options[] = {"Off", "Bayer", "On"};
	add_choice(profile, "dithering", "Dithering",
	           "GPU dithering mode",
	           SETTING_CAT_VIDEO, 2, dither_options, 3, 30, 0x03, 0);

	add_toggle(profile, "true_color", "True Color",
	           "Force 24-bit color (removes banding)",
	           SETTING_CAT_VIDEO, 0, 31, 0x01, 0);

	add_toggle(profile, "interlace", "Interlace",
	           "Enable interlaced video",
	           SETTING_CAT_VIDEO, 1, 32, 0x01, 0);

	// Audio settings
	core_settings_add_audio_common(profile);

	add_toggle(profile, "cd_audio", "CD Audio",
	           "Enable CD-DA audio playback",
	           SETTING_CAT_AUDIO, 1, 40, 0x01, 0);

	add_toggle(profile, "xa_audio", "XA Audio",
	           "Enable XA audio decoding",
	           SETTING_CAT_AUDIO, 1, 41, 0x01, 0);

	// Input settings
	core_settings_add_input_common(profile);

	const char *psx_controller_options[] = {"Digital", "Analog", "DualShock", "Mouse", "NeGcon", "GunCon"};
	add_choice(profile, "port1_device", "Port 1 Device",
	           "Controller port 1 device type",
	           SETTING_CAT_INPUT, 2, psx_controller_options, 6, 50, 0x07, 0);

	add_choice(profile, "port2_device", "Port 2 Device",
	           "Controller port 2 device type",
	           SETTING_CAT_INPUT, 2, psx_controller_options, 6, 51, 0x07, 0);

	add_toggle(profile, "rumble", "Rumble",
	           "Enable controller vibration",
	           SETTING_CAT_INPUT, 1, 52, 0x01, 0);

	// System settings
	const char *region_options[] = {"Auto", "NTSC-U", "NTSC-J", "PAL"};
	add_choice(profile, "psx_region", "Region",
	           "Console region",
	           SETTING_CAT_SYSTEM, 0, region_options, 4, 60, 0x03, 0);

	add_toggle(profile, "bios", "Use BIOS",
	           "Boot with PlayStation BIOS",
	           SETTING_CAT_SYSTEM, 1, 61, 0x01, 0);

	add_toggle(profile, "fast_boot", "Fast Boot",
	           "Skip BIOS boot animation",
	           SETTING_CAT_SYSTEM, 1, 62, 0x01, 0);

	add_action(profile, "swap_disc", "Swap Disc",
	           "Change disc for multi-disc games",
	           SETTING_CAT_SYSTEM, 63);

	// Memory settings
	const char *memcard_options[] = {"Shared", "Per-Game", "Off"};
	add_choice(profile, "memcard_mode", "Memory Card Mode",
	           "Memory card save behavior",
	           SETTING_CAT_MEMORY, 1, memcard_options, 3, 70, 0x03, 0);

	add_action(profile, "save_state", "Save State",
	           "Save current game state",
	           SETTING_CAT_MEMORY, 71);

	add_action(profile, "load_state", "Load State",
	           "Load saved game state",
	           SETTING_CAT_MEMORY, 72);

	profile->loaded = 1;
}

//// N64 Profile ////

void core_settings_create_n64_profile(core_profile_t *profile)
{
	memset(profile, 0, sizeof(core_profile_t));
	strncpy(profile->core_name, "Nintendo 64", sizeof(profile->core_name) - 1);
	strncpy(profile->core_id, "N64", sizeof(profile->core_id) - 1);
	strncpy(profile->rbf_name, "N64", sizeof(profile->rbf_name) - 1);

	for (int i = 0; i < SETTING_CAT_COUNT; i++) {
		strncpy(profile->categories[i].name, category_names[i], sizeof(profile->categories[i].name) - 1);
		strncpy(profile->categories[i].icon, category_icons[i], sizeof(profile->categories[i].icon) - 1);
	}

	// Video settings
	core_settings_add_video_common(profile);

	const char *res_options[] = {"Original", "320x240", "640x480"};
	add_choice(profile, "resolution", "Resolution",
	           "Internal resolution",
	           SETTING_CAT_VIDEO, 0, res_options, 3, 30, 0x03, 0);

	add_toggle(profile, "aa_filter", "Anti-Aliasing",
	           "Enable N64 anti-aliasing",
	           SETTING_CAT_VIDEO, 1, 31, 0x01, 0);

	add_toggle(profile, "vi_filter", "VI Filter",
	           "Enable VI de-blur filter",
	           SETTING_CAT_VIDEO, 1, 32, 0x01, 0);

	// Audio settings
	core_settings_add_audio_common(profile);

	// Input settings
	core_settings_add_input_common(profile);

	const char *n64_controller_options[] = {"Controller", "Mouse", "Transfer Pak", "Rumble Pak"};
	add_choice(profile, "port1_pak", "Port 1 Pak",
	           "Controller Pak slot item",
	           SETTING_CAT_INPUT, 0, n64_controller_options, 4, 50, 0x03, 0);

	add_toggle(profile, "rumble", "Rumble Pak",
	           "Enable Rumble Pak vibration",
	           SETTING_CAT_INPUT, 1, 51, 0x01, 0);

	// System settings
	const char *region_options[] = {"Auto", "NTSC", "PAL"};
	add_choice(profile, "n64_region", "Region",
	           "Console region",
	           SETTING_CAT_SYSTEM, 0, region_options, 3, 60, 0x03, 0);

	const char *cic_options[] = {"Auto", "6101", "6102", "6103", "6105", "6106"};
	add_choice(profile, "cic_chip", "CIC Chip",
	           "CIC lockout chip type",
	           SETTING_CAT_SYSTEM, 0, cic_options, 6, 61, 0x07, 0);

	add_toggle(profile, "fast_loading", "Fast Loading",
	           "Speed up loading times",
	           SETTING_CAT_SYSTEM, 0, 62, 0x01, 0);

	// Memory settings
	const char *cpak_options[] = {"Shared", "Per-Game", "Off"};
	add_choice(profile, "controller_pak", "Controller Pak",
	           "Controller Pak save mode",
	           SETTING_CAT_MEMORY, 1, cpak_options, 3, 70, 0x03, 0);

	add_action(profile, "save_state", "Save State",
	           "Save current game state",
	           SETTING_CAT_MEMORY, 71);

	add_action(profile, "load_state", "Load State",
	           "Load saved game state",
	           SETTING_CAT_MEMORY, 72);

	profile->loaded = 1;
}

//// Arcade Profile ////

void core_settings_create_arcade_profile(core_profile_t *profile, const char *mra_name)
{
	memset(profile, 0, sizeof(core_profile_t));
	strncpy(profile->core_name, "Arcade", sizeof(profile->core_name) - 1);
	strncpy(profile->core_id, "Arcade", sizeof(profile->core_id) - 1);
	if (mra_name) {
		strncpy(profile->rbf_name, mra_name, sizeof(profile->rbf_name) - 1);
	}

	for (int i = 0; i < SETTING_CAT_COUNT; i++) {
		strncpy(profile->categories[i].name, category_names[i], sizeof(profile->categories[i].name) - 1);
		strncpy(profile->categories[i].icon, category_icons[i], sizeof(profile->categories[i].icon) - 1);
	}

	// Video settings
	core_settings_add_video_common(profile);

	const char *rotate_options[] = {"Off", "90\xB0 CW", "180\xB0", "90\xB0 CCW"};
	add_choice(profile, "rotate", "Screen Rotation",
	           "Rotate display for vertical games",
	           SETTING_CAT_VIDEO, 0, rotate_options, 4, 30, 0x03, 0);

	add_toggle(profile, "flip", "Flip Screen",
	           "Flip screen for cocktail cabinets",
	           SETTING_CAT_VIDEO, 0, 31, 0x01, 0);

	// Audio settings
	core_settings_add_audio_common(profile);

	// Input settings (will be populated from MRA)
	add_separator(profile, "Buttons", SETTING_CAT_INPUT);

	// System settings
	add_separator(profile, "DIP Switches", SETTING_CAT_SYSTEM);

	add_toggle(profile, "service_mode", "Service Mode",
	           "Enter arcade service/test menu",
	           SETTING_CAT_SYSTEM, 0, 60, 0x01, 0);

	add_toggle(profile, "free_play", "Free Play",
	           "Enable free play (no coins needed)",
	           SETTING_CAT_SYSTEM, 0, 61, 0x01, 0);

	// Memory settings
	add_toggle(profile, "hiscore_save", "Save High Scores",
	           "Automatically save high score data",
	           SETTING_CAT_MEMORY, 1, 70, 0x01, 0);

	profile->loaded = 1;
}

//// Core Functions ////

void core_settings_init(void)
{
	if (initialized) return;

	memset(&current_profile, 0, sizeof(current_profile));
	memset(&menu_state, 0, sizeof(menu_state));

	// Create settings directory if it doesn't exist
	mkdir(SETTINGS_DIR, 0755);

	initialized = 1;
	printf("Core settings system initialized\n");
}

void core_settings_shutdown(void)
{
	if (!initialized) return;

	// Save any pending changes
	if (current_profile.modified) {
		core_settings_save();
	}

	initialized = 0;
}

int core_settings_load(const char *core_name)
{
	if (!core_name || !core_name[0]) return 0;

	// Check for known cores and create appropriate profile
	if (strcasecmp(core_name, "NES") == 0) {
		core_settings_create_nes_profile(&current_profile);
	} else if (strcasecmp(core_name, "SNES") == 0) {
		core_settings_create_snes_profile(&current_profile);
	} else if (strcasecmp(core_name, "Genesis") == 0 ||
	           strcasecmp(core_name, "MegaDrive") == 0) {
		core_settings_create_genesis_profile(&current_profile);
	} else if (strcasecmp(core_name, "GBA") == 0) {
		core_settings_create_gba_profile(&current_profile);
	} else if (strcasecmp(core_name, "PSX") == 0 ||
	           strcasecmp(core_name, "PlayStation") == 0) {
		core_settings_create_psx_profile(&current_profile);
	} else if (strcasecmp(core_name, "N64") == 0) {
		core_settings_create_n64_profile(&current_profile);
	} else {
		// Unknown core - create from OSD
		core_settings_create_from_osd(&current_profile);
		strncpy(current_profile.core_name, core_name, sizeof(current_profile.core_name) - 1);
		strncpy(current_profile.core_id, core_name, sizeof(current_profile.core_id) - 1);
	}

	// Try to load saved settings
	char filepath[512];
	snprintf(filepath, sizeof(filepath), "%s/%s.cfg", SETTINGS_DIR, current_profile.core_id);

	FILE *f = fopen(filepath, "r");
	if (f) {
		char line[256];
		while (fgets(line, sizeof(line), f)) {
			char id[64], value_str[64];
			if (sscanf(line, "%63[^=]=%63s", id, value_str) == 2) {
				int value = atoi(value_str);
				core_settings_set_value(id, value);
			}
		}
		fclose(f);
	}

	// Sync with OSD
	core_settings_sync_from_osd();

	menu_state.profile = &current_profile;
	printf("Loaded settings for core: %s (%d settings)\n",
	       current_profile.core_name, current_profile.setting_count);

	return 1;
}

int core_settings_save(void)
{
	if (!current_profile.loaded) return 0;

	char filepath[512];
	snprintf(filepath, sizeof(filepath), "%s/%s.cfg", SETTINGS_DIR, current_profile.core_id);

	return core_settings_save_to(filepath);
}

int core_settings_save_to(const char *filepath)
{
	if (!current_profile.loaded || !filepath) return 0;

	FILE *f = fopen(filepath, "w");
	if (!f) return 0;

	fprintf(f, "# MiSTer Core Settings: %s\n", current_profile.core_name);
	fprintf(f, "# Version: %s\n\n", current_profile.core_version);

	for (int i = 0; i < current_profile.setting_count; i++) {
		core_setting_t *s = &current_profile.settings[i];
		if (s->type != SETTING_TYPE_SEPARATOR &&
		    s->type != SETTING_TYPE_ACTION &&
		    s->type != SETTING_TYPE_INFO) {
			fprintf(f, "%s=%d\n", s->id, s->value);
		}
	}

	fclose(f);
	current_profile.modified = 0;
	return 1;
}

core_profile_t* core_settings_get_profile(void)
{
	return &current_profile;
}

int core_settings_is_loaded(void)
{
	return current_profile.loaded;
}

//// Setting Access ////

core_setting_t* core_settings_get(const char *id)
{
	if (!id) return NULL;

	for (int i = 0; i < current_profile.setting_count; i++) {
		if (strcmp(current_profile.settings[i].id, id) == 0) {
			return &current_profile.settings[i];
		}
	}
	return NULL;
}

core_setting_t* core_settings_get_by_index(int index)
{
	if (index < 0 || index >= current_profile.setting_count) return NULL;
	return &current_profile.settings[index];
}

int core_settings_get_by_category(setting_category_t category,
                                   core_setting_t **out_settings,
                                   int max_count)
{
	int count = 0;
	for (int i = 0; i < current_profile.setting_count && count < max_count; i++) {
		if (current_profile.settings[i].category == category) {
			out_settings[count++] = &current_profile.settings[i];
		}
	}
	return count;
}

int core_settings_get_value(const char *id)
{
	core_setting_t *s = core_settings_get(id);
	return s ? s->value : 0;
}

void core_settings_set_value(const char *id, int value)
{
	core_setting_t *s = core_settings_get(id);
	if (s && !s->readonly) {
		// Clamp value for range type
		if (s->type == SETTING_TYPE_RANGE) {
			if (value < s->min_value) value = s->min_value;
			if (value > s->max_value) value = s->max_value;
		}
		// Clamp value for choice type
		else if (s->type == SETTING_TYPE_CHOICE) {
			if (value < 0) value = 0;
			if (value >= s->option_count) value = s->option_count - 1;
		}
		// Clamp value for toggle type
		else if (s->type == SETTING_TYPE_TOGGLE) {
			value = value ? 1 : 0;
		}

		if (s->value != value) {
			s->value = value;
			current_profile.modified = 1;

			// Sync to OSD
			core_settings_sync_to_osd();
		}
	}
}

void core_settings_reset(const char *id)
{
	core_setting_t *s = core_settings_get(id);
	if (s) {
		core_settings_set_value(id, s->default_value);
	}
}

void core_settings_reset_all(void)
{
	for (int i = 0; i < current_profile.setting_count; i++) {
		current_profile.settings[i].value = current_profile.settings[i].default_value;
	}
	current_profile.modified = 1;
	core_settings_sync_to_osd();
}

//// Menu State ////

settings_menu_state_t* core_settings_get_menu_state(void)
{
	return &menu_state;
}

void core_settings_menu_open(void)
{
	menu_state.profile = &current_profile;
	menu_state.current_category = SETTING_CAT_VIDEO;
	menu_state.selected_index = 0;
	menu_state.scroll_offset = 0;
	menu_state.editing = 0;
}

void core_settings_menu_close(void)
{
	menu_state.editing = 0;

	// Save settings on close
	if (current_profile.modified) {
		core_settings_save();
	}
}

int core_settings_menu_is_open(void)
{
	return menu_state.profile != NULL;
}

void core_settings_menu_up(void)
{
	if (menu_state.editing) return;

	if (menu_state.selected_index > 0) {
		menu_state.selected_index--;

		// Skip separators
		core_setting_t *s = core_settings_get_by_index(menu_state.selected_index);
		if (s && s->type == SETTING_TYPE_SEPARATOR && menu_state.selected_index > 0) {
			menu_state.selected_index--;
		}

		// Adjust scroll
		if (menu_state.selected_index < menu_state.scroll_offset) {
			menu_state.scroll_offset = menu_state.selected_index;
		}
	}
}

void core_settings_menu_down(void)
{
	if (menu_state.editing) return;

	if (menu_state.selected_index < current_profile.setting_count - 1) {
		menu_state.selected_index++;

		// Skip separators
		core_setting_t *s = core_settings_get_by_index(menu_state.selected_index);
		if (s && s->type == SETTING_TYPE_SEPARATOR &&
		    menu_state.selected_index < current_profile.setting_count - 1) {
			menu_state.selected_index++;
		}

		// Adjust scroll
		if (menu_state.selected_index >= menu_state.scroll_offset + menu_state.visible_count) {
			menu_state.scroll_offset = menu_state.selected_index - menu_state.visible_count + 1;
		}
	}
}

void core_settings_menu_left(void)
{
	core_setting_t *s = core_settings_get_by_index(menu_state.selected_index);
	if (!s || s->readonly) return;

	if (s->type == SETTING_TYPE_TOGGLE) {
		core_settings_set_value(s->id, 0);
	} else if (s->type == SETTING_TYPE_CHOICE) {
		int new_val = s->value - 1;
		if (new_val < 0) new_val = s->option_count - 1;
		core_settings_set_value(s->id, new_val);
	} else if (s->type == SETTING_TYPE_RANGE) {
		int new_val = s->value - s->step;
		if (new_val < s->min_value) new_val = s->min_value;
		core_settings_set_value(s->id, new_val);
	}
}

void core_settings_menu_right(void)
{
	core_setting_t *s = core_settings_get_by_index(menu_state.selected_index);
	if (!s || s->readonly) return;

	if (s->type == SETTING_TYPE_TOGGLE) {
		core_settings_set_value(s->id, 1);
	} else if (s->type == SETTING_TYPE_CHOICE) {
		int new_val = (s->value + 1) % s->option_count;
		core_settings_set_value(s->id, new_val);
	} else if (s->type == SETTING_TYPE_RANGE) {
		int new_val = s->value + s->step;
		if (new_val > s->max_value) new_val = s->max_value;
		core_settings_set_value(s->id, new_val);
	}
}

void core_settings_menu_select(void)
{
	core_setting_t *s = core_settings_get_by_index(menu_state.selected_index);
	if (!s) return;

	if (s->type == SETTING_TYPE_TOGGLE) {
		core_settings_set_value(s->id, !s->value);
	} else if (s->type == SETTING_TYPE_ACTION) {
		// TODO: Trigger action via OSD
	} else if (s->type == SETTING_TYPE_SUBMENU) {
		// TODO: Open submenu
	}
}

void core_settings_menu_back(void)
{
	if (menu_state.editing) {
		menu_state.editing = 0;
	} else {
		core_settings_menu_close();
	}
}

void core_settings_menu_next_category(void)
{
	int cat = (int)menu_state.current_category + 1;
	if (cat >= SETTING_CAT_COUNT) cat = 0;
	core_settings_menu_set_category((setting_category_t)cat);
}

void core_settings_menu_prev_category(void)
{
	int cat = (int)menu_state.current_category - 1;
	if (cat < 0) cat = SETTING_CAT_COUNT - 1;
	core_settings_menu_set_category((setting_category_t)cat);
}

void core_settings_menu_set_category(setting_category_t category)
{
	menu_state.current_category = category;
	menu_state.selected_index = 0;
	menu_state.scroll_offset = 0;

	// Find first setting in this category
	for (int i = 0; i < current_profile.setting_count; i++) {
		if (current_profile.settings[i].category == category) {
			menu_state.selected_index = i;
			break;
		}
	}
}

//// OSD Integration ////

void core_settings_sync_from_osd(void)
{
	// TODO: Read current values from OSD status word
	// This would interface with user_io_status() etc.
}

void core_settings_sync_to_osd(void)
{
	// TODO: Write current values to OSD
	// This would interface with user_io_status() etc.
}

void core_settings_osd_changed(int osd_id, int value)
{
	// Find setting by OSD ID and update
	for (int i = 0; i < current_profile.setting_count; i++) {
		core_setting_t *s = &current_profile.settings[i];
		if (s->osd_id == osd_id) {
			s->value = (value >> s->osd_shift) & s->osd_mask;
			break;
		}
	}
}

//// Generic Profile Creation ////

void core_settings_create_from_osd(core_profile_t *profile)
{
	memset(profile, 0, sizeof(core_profile_t));

	for (int i = 0; i < SETTING_CAT_COUNT; i++) {
		strncpy(profile->categories[i].name, category_names[i], sizeof(profile->categories[i].name) - 1);
		strncpy(profile->categories[i].icon, category_icons[i], sizeof(profile->categories[i].icon) - 1);
	}

	// Add common settings as baseline
	core_settings_add_video_common(profile);
	core_settings_add_audio_common(profile);
	core_settings_add_input_common(profile);

	// Add generic memory settings
	add_toggle(profile, "autosave", "Auto-Save",
	           "Automatically save data",
	           SETTING_CAT_MEMORY, 1, 70, 0x01, 0);

	add_action(profile, "save_state", "Save State",
	           "Save current state",
	           SETTING_CAT_MEMORY, 71);

	add_action(profile, "load_state", "Load State",
	           "Load saved state",
	           SETTING_CAT_MEMORY, 72);

	profile->loaded = 1;
}

//// Utility Functions ////

const char* core_settings_category_name(setting_category_t category)
{
	if (category >= 0 && category < SETTING_CAT_COUNT) {
		return category_names[category];
	}
	return "Unknown";
}

const char* core_settings_category_icon(setting_category_t category)
{
	if (category >= 0 && category < SETTING_CAT_COUNT) {
		return category_icons[category];
	}
	return "gear";
}

const char* core_settings_type_name(setting_type_t type)
{
	switch (type) {
		case SETTING_TYPE_TOGGLE: return "Toggle";
		case SETTING_TYPE_CHOICE: return "Choice";
		case SETTING_TYPE_RANGE: return "Range";
		case SETTING_TYPE_SUBMENU: return "Submenu";
		case SETTING_TYPE_ACTION: return "Action";
		case SETTING_TYPE_SEPARATOR: return "Separator";
		case SETTING_TYPE_INFO: return "Info";
		default: return "Unknown";
	}
}

void core_settings_format_value(core_setting_t *setting, char *out_str, int max_len)
{
	if (!setting || !out_str) return;

	switch (setting->type) {
		case SETTING_TYPE_TOGGLE:
			snprintf(out_str, max_len, "%s", setting->value ? "On" : "Off");
			break;

		case SETTING_TYPE_CHOICE:
			if (setting->value >= 0 && setting->value < setting->option_count) {
				snprintf(out_str, max_len, "%s", setting->options[setting->value].name);
			} else {
				snprintf(out_str, max_len, "%d", setting->value);
			}
			break;

		case SETTING_TYPE_RANGE:
			if (setting->format[0]) {
				snprintf(out_str, max_len, setting->format, setting->value);
			} else {
				snprintf(out_str, max_len, "%d", setting->value);
			}
			break;

		case SETTING_TYPE_ACTION:
			snprintf(out_str, max_len, "[Press]");
			break;

		default:
			out_str[0] = '\0';
			break;
	}
}

const char* core_settings_get_option_name(core_setting_t *setting)
{
	if (!setting || setting->type != SETTING_TYPE_CHOICE) return NULL;

	if (setting->value >= 0 && setting->value < setting->option_count) {
		return setting->options[setting->value].name;
	}
	return NULL;
}
