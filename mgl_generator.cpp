// mgl_generator.cpp
// MGL file generator for MiSTer graphical frontend
// Scans ROM folders and generates MGL launch files
// 2024

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>

#include "mgl_generator.h"
#include "file_io.h"

// System configurations for MGL generation
typedef struct {
	const char *name;           // System display name (e.g., "SNES")
	const char *core_path;      // Core path for MGL (e.g., "_Console/SNES")
	const char *rom_folder;     // ROM folder name (e.g., "SNES")
	const char *extensions;     // Supported extensions (pipe-separated)
	const char *libretro_repo;  // For boxart matching
	int file_index;             // MGL file index (usually 0 or 1)
	int delay;                  // MGL delay value
} system_config_t;

// Supported systems - comprehensive list of all MiSTer cores
static const system_config_t systems[] = {
	// === Nintendo Consoles ===
	{"NES",           "_Console/NES",           "NES",           ".nes|.fds|.zip|.7z",       "Nintendo_-_Nintendo_Entertainment_System", 0, 2},
	{"SNES",          "_Console/SNES",          "SNES",          ".sfc|.smc|.zip|.7z",       "Nintendo_-_Super_Nintendo_Entertainment_System", 0, 2},
	{"N64",           "_Console/N64",           "N64",           ".n64|.z64|.v64|.zip|.7z",  "Nintendo_-_Nintendo_64", 0, 2},
	{"GB",            "_Console/Gameboy",       "GAMEBOY",       ".gb|.zip|.7z",             "Nintendo_-_Game_Boy", 0, 2},
	{"GBC",           "_Console/Gameboy",       "GBC",           ".gbc|.zip|.7z",            "Nintendo_-_Game_Boy_Color", 0, 2},
	{"GBA",           "_Console/GBA",           "GBA",           ".gba|.zip|.7z",            "Nintendo_-_Game_Boy_Advance", 0, 2},
	{"VirtualBoy",    "_Console/VirtualBoy",    "VirtualBoy",    ".vb|.zip|.7z",             "Nintendo_-_Virtual_Boy", 0, 2},
	
	// === Sega Consoles ===
	{"Genesis",       "_Console/Genesis",       "Genesis",       ".md|.bin|.gen|.zip|.7z",   "Sega_-_Mega_Drive_-_Genesis", 0, 2},
	{"MegaDrive",     "_Console/Genesis",       "MegaDrive",     ".md|.bin|.gen|.zip|.7z",   "Sega_-_Mega_Drive_-_Genesis", 0, 2},
	{"MegaCD",        "_Console/MegaCD",        "MegaCD",        ".chd|.cue|.bin|.7z",       "Sega_-_Mega-CD_-_Sega_CD", 1, 2},
	{"SegaCD",        "_Console/MegaCD",        "SegaCD",        ".chd|.cue|.bin|.7z",       "Sega_-_Mega-CD_-_Sega_CD", 1, 2},
	{"32X",           "_Console/S32X",          "32X",           ".32x|.zip|.7z",            "Sega_-_32X", 0, 2},
	{"SMS",           "_Console/SMS",           "SMS",           ".sms|.zip|.7z",            "Sega_-_Master_System_-_Mark_III", 0, 2},
	{"GameGear",      "_Console/SMS",           "GameGear",      ".gg|.zip|.7z",             "Sega_-_Game_Gear", 0, 2},
	{"SG1000",        "_Console/ColecoVision",  "SG1000",        ".sg|.zip|.7z",             "Sega_-_SG-1000", 0, 2},
	{"Saturn",        "_Console/Saturn",        "Saturn",        ".chd|.cue|.bin|.7z",       "Sega_-_Saturn", 1, 2},
	
	// === NEC Consoles ===
	{"TurboGrafx16",  "_Console/TurboGrafx16",  "TGFX16",        ".pce|.zip|.7z",            "NEC_-_PC_Engine_-_TurboGrafx_16", 0, 2},
	{"PCEngine",      "_Console/TurboGrafx16",  "TGFX16",        ".pce|.zip|.7z",            "NEC_-_PC_Engine_-_TurboGrafx_16", 0, 2},
	{"TGFX16CD",      "_Console/TurboGrafx16",  "TGFX16-CD",     ".chd|.cue|.bin|.7z",       "NEC_-_PC_Engine_CD_-_TurboGrafx-CD", 1, 2},
	{"SuperGrafx",    "_Console/TurboGrafx16",  "SuperGrafx",    ".sgx|.zip|.7z",            "NEC_-_PC_Engine_SuperGrafx", 0, 2},
	
	// === Sony Consoles ===
	{"PSX",           "_Console/PSX",           "PSX",           ".chd|.cue|.bin|.7z",       "Sony_-_PlayStation", 1, 2},
	{"PlayStation",   "_Console/PSX",           "PSX",           ".chd|.cue|.bin|.7z",       "Sony_-_PlayStation", 1, 2},
	
	// === SNK Consoles ===
	{"NeoGeo",        "_Console/NeoGeo",        "NEOGEO",        ".zip|.7z",                 "SNK_-_Neo_Geo", 1, 2},
	{"NGP",           "_Console/NeoGeo",        "NGP",           ".ngp|.zip|.7z",            "SNK_-_Neo_Geo_Pocket", 0, 2},
	{"NGPC",          "_Console/NeoGeo",        "NGPC",          ".ngc|.zip|.7z",            "SNK_-_Neo_Geo_Pocket_Color", 0, 2},
	
	// === Atari Consoles ===
	{"Atari2600",     "_Console/Atari2600",     "ATARI2600",     ".a26|.bin|.zip|.7z",       "Atari_-_2600", 0, 2},
	{"Atari5200",     "_Console/Atari5200",     "ATARI5200",     ".a52|.bin|.zip|.7z",       "Atari_-_5200", 0, 2},
	{"Atari7800",     "_Console/Atari7800",     "ATARI7800",     ".a78|.bin|.zip|.7z",       "Atari_-_7800", 0, 2},
	{"AtariLynx",     "_Console/AtariLynx",     "AtariLynx",     ".lnx|.zip|.7z",            "Atari_-_Lynx", 0, 2},
	{"Jaguar",        "_Console/Jaguar",        "Jaguar",        ".j64|.jag|.zip|.7z",       "Atari_-_Jaguar", 0, 2},
	
	// === Bandai Consoles ===
	{"WonderSwan",    "_Console/WonderSwan",    "WonderSwan",    ".ws|.zip|.7z",             "Bandai_-_WonderSwan", 0, 2},
	{"WonderSwanColor","_Console/WonderSwan",   "WonderSwanColor",".wsc|.zip|.7z",           "Bandai_-_WonderSwan_Color", 0, 2},
	
	// === Other Consoles ===
	{"ColecoVision",  "_Console/ColecoVision",  "Coleco",        ".col|.bin|.zip|.7z",       "Coleco_-_ColecoVision", 0, 2},
	{"Intellivision", "_Console/Intellivision", "Intellivision", ".int|.bin|.zip|.7z",       "Mattel_-_Intellivision", 0, 2},
	{"Vectrex",       "_Console/Vectrex",       "VECTREX",       ".vec|.bin|.zip|.7z",       "GCE_-_Vectrex", 0, 2},
	{"Odyssey2",      "_Console/Odyssey2",      "Odyssey2",      ".bin|.zip|.7z",            "Magnavox_-_Odyssey2", 0, 2},
	{"ChannelF",      "_Console/ChannelF",      "ChannelF",      ".bin|.zip|.7z",            "Fairchild_-_Channel_F", 0, 2},
	
	// === Computers - Commodore ===
	{"C64",           "_Computer/C64",          "C64",           ".prg|.crt|.d64|.t64|.zip|.7z", "Commodore_-_64", 0, 2},
	{"C128",          "_Computer/C128",         "C128",          ".prg|.crt|.d64|.zip|.7z",  "Commodore_-_64", 0, 2},
	{"VIC20",         "_Computer/VIC20",        "VIC20",         ".prg|.crt|.zip|.7z",       "Commodore_-_VIC-20", 0, 2},
	{"PET",           "_Computer/PET2001",      "PET",           ".prg|.zip|.7z",            "Commodore_-_PET", 0, 2},
	{"Amiga",         "_Computer/Minimig",      "Amiga",         ".adf|.hdf|.zip|.7z",       "Commodore_-_Amiga", 0, 2},
	
	// === Computers - Atari ===
	{"Atari800",      "_Computer/Atari800",     "ATARI800",      ".atr|.xex|.car|.zip|.7z",  "Atari_-_8-bit", 0, 2},
	{"AtariST",       "_Computer/AtariST",      "AtariST",       ".st|.stx|.zip|.7z",        "Atari_-_ST", 0, 2},
	
	// === Computers - Sinclair ===
	{"ZXSpectrum",    "_Computer/ZX-Spectrum",  "Spectrum",      ".tap|.tzx|.z80|.sna|.zip|.7z", "Sinclair_-_ZX_Spectrum", 0, 2},
	{"ZX81",          "_Computer/ZX81",         "ZX81",          ".p|.o|.zip|.7z",           "Sinclair_-_ZX_81", 0, 2},
	
	// === Computers - MSX ===
	{"MSX",           "_Computer/MSX",          "MSX",           ".rom|.mx1|.zip|.7z",       "Microsoft_-_MSX", 0, 2},
	{"MSX2",          "_Computer/MSX",          "MSX2",          ".rom|.mx2|.zip|.7z",       "Microsoft_-_MSX2", 0, 2},
	
	// === Computers - PC ===
	{"AO486",         "_Computer/ao486",        "AO486",         ".img|.vhd|.zip|.7z",       "DOS", 0, 2},
	{"PCXT",          "_Computer/PCXT",         "PCXT",          ".img|.vhd|.zip|.7z",       "DOS", 0, 2},
	
	// === Computers - Sharp ===
	{"X68000",        "_Computer/X68000",       "X68000",        ".dim|.xdf|.zip|.7z",       "Sharp_-_X68000", 0, 2},
	{"MZ700",         "_Computer/SharpMZ",      "SharpMZ",       ".mzf|.zip|.7z",            "Sharp_-_MZ-700", 0, 2},
	
	// === Computers - NEC ===
	{"PC88",          "_Computer/PC8801",       "PC8801",        ".d88|.zip|.7z",            "NEC_-_PC-8801", 0, 2},
	{"PC98",          "_Computer/PC9801",       "PC9801",        ".fdi|.hdi|.zip|.7z",       "NEC_-_PC-98", 0, 2},
	
	// === Computers - Apple ===
	{"Apple2",        "_Computer/Apple-II",     "Apple-II",      ".dsk|.nib|.woz|.zip|.7z",  "Apple_-_Apple_II", 0, 2},
	{"Macintosh",     "_Computer/MacPlus",      "MacPlus",       ".dsk|.img|.zip|.7z",       "Apple_-_Macintosh", 0, 2},
	
	// === Computers - Other ===
	{"Amstrad",       "_Computer/Amstrad",      "Amstrad",       ".dsk|.cdt|.zip|.7z",       "Amstrad_-_CPC", 0, 2},
	{"AmstradPCW",    "_Computer/Amstrad-PCW",  "AmstradPCW",    ".dsk|.zip|.7z",            "Amstrad_-_CPC", 0, 2},
	{"BBCMicro",      "_Computer/BBCMicro",     "BBCMicro",      ".ssd|.dsd|.zip|.7z",       "Acorn_-_BBC_Micro", 0, 2},
	{"Acorn",         "_Computer/Archimedes",   "ARCHIE",        ".adf|.zip|.7z",            "Acorn_-_Archimedes", 0, 2},
	{"TI994A",        "_Computer/TI-99_4A",     "TI-99_4A",      ".bin|.zip|.7z",            "Texas_Instruments_-_TI-99", 0, 2},
	{"SAMCoupe",      "_Computer/SAM-Coupe",    "SAMCoupe",      ".dsk|.mgt|.zip|.7z",       "MGT_-_SAM_Coupe", 0, 2},
	{"Aquarius",      "_Computer/Aquarius",     "Aquarius",      ".bin|.caq|.zip|.7z",       "Mattel_-_Aquarius", 0, 2},
	{"Oric",          "_Computer/Oric",         "Oric",          ".dsk|.tap|.zip|.7z",       "Tangerine_-_Oric", 0, 2},
	{"Dragon",        "_Computer/CoCo2",        "CoCo",          ".dsk|.cas|.zip|.7z",       "Dragon_Data_-_Dragon", 0, 2},
	{"CoCo",          "_Computer/CoCo2",        "CoCo",          ".dsk|.cas|.zip|.7z",       "Tandy_-_TRS-80_Color_Computer", 0, 2},
	{"TRS80",         "_Computer/TRS-80",       "TRS-80",        ".dsk|.cas|.zip|.7z",       "Tandy_-_TRS-80", 0, 2},
	{"AliceMC10",     "_Computer/AliceMC10",    "AliceMC10",     ".c10|.zip|.7z",            "Tandy_-_TRS-80_Color_Computer", 0, 2},
	
	// === Arcade (FBNeo/MAME style) ===
	{"Arcade",        "_Arcade",                "mame",          ".zip|.7z",                 "MAME", 1, 2},
	{"FBNeo",         "_Arcade",                "fbneo",         ".zip|.7z",                 "FBNeo_-_Arcade_Games", 1, 2},
	{"CPS1",          "_Arcade/cores",          "cps1",          ".zip|.7z",                 "MAME", 1, 2},
	{"CPS2",          "_Arcade/cores",          "cps2",          ".zip|.7z",                 "MAME", 1, 2},
	{"CPS3",          "_Arcade/cores",          "cps3",          ".zip|.7z",                 "MAME", 1, 2},
	
	{NULL, NULL, NULL, NULL, NULL, 0, 0}
};

// MGL output directory
#define MGL_OUTPUT_DIR "/media/fat/_Games"

// Generate display name from ROM filename
static void get_display_name(const char *filename, char *display_name, size_t max_len)
{
	// Copy filename without extension
	strncpy(display_name, filename, max_len - 1);
	display_name[max_len - 1] = '\0';

	// Find and remove extension
	char *dot = strrchr(display_name, '.');
	if (dot) *dot = '\0';

	// Optionally could remove region tags like (USA), (Japan) etc.
	// For now, keep them for better matching with libretro-thumbnails
}

// Check if file has a valid ROM extension for the system
static int has_valid_extension(const char *filename, const char *extensions)
{
	const char *dot = strrchr(filename, '.');
	if (!dot) return 0;

	// Make a copy of extensions string to tokenize
	char ext_copy[256];
	strncpy(ext_copy, extensions, sizeof(ext_copy) - 1);
	ext_copy[sizeof(ext_copy) - 1] = '\0';

	// Check each extension (pipe-separated)
	char *ext = strtok(ext_copy, "|");
	while (ext) {
		if (strcasecmp(dot, ext) == 0) return 1;
		ext = strtok(NULL, "|");
	}

	return 0;
}

// Check if file exists
static int file_exists(const char *path)
{
	struct stat st;
	return stat(path, &st) == 0;
}

// Generate MGL content for a ROM
static int generate_mgl_content(char *buffer, size_t buffer_size,
                                const char *core_path, int delay, 
                                int file_index, const char *rom_path)
{
	int written = snprintf(buffer, buffer_size,
		"<mistergamedescription>\n"
		"    <rbf>%s</rbf>\n"
		"    <file delay=\"%d\" type=\"f\" index=\"%d\" path=\"%s\"/>\n"
		"</mistergamedescription>\n",
		core_path, delay, file_index, rom_path);

	return (written > 0 && (size_t)written < buffer_size) ? 1 : 0;
}

// Generate MGL file for a ROM
static int generate_mgl_for_rom(const system_config_t *sys, const char *rom_filename, 
                                 const char *rom_full_path)
{
	char display_name[256];
	get_display_name(rom_filename, display_name, sizeof(display_name));

	// Build MGL output path - use underscore prefix to match MiSTer convention
	char mgl_dir[512];
	char mgl_path[1024];
	
	// First check if underscore-prefixed folder exists (common MiSTer convention)
	snprintf(mgl_dir, sizeof(mgl_dir), "%s/_%s", MGL_OUTPUT_DIR, sys->name);
	snprintf(mgl_path, sizeof(mgl_path), "%s/%s.mgl", mgl_dir, display_name);
	
	// If not, try without underscore
	if (!file_exists(mgl_dir)) {
		snprintf(mgl_dir, sizeof(mgl_dir), "%s/%s", MGL_OUTPUT_DIR, sys->name);
		snprintf(mgl_path, sizeof(mgl_path), "%s/%s.mgl", mgl_dir, display_name);
	}

	// Skip if MGL already exists
	if (file_exists(mgl_path)) {
		return 0;  // Already exists
	}

	// Create output directory
	char mkdir_cmd[1024];
	snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p '%s'", mgl_dir);
	system(mkdir_cmd);

	// Build relative ROM path for MGL
	// MGL expects path relative to /media/fat/
	char rel_rom_path[1024];
	if (strncmp(rom_full_path, "/media/fat/", 11) == 0) {
		strncpy(rel_rom_path, rom_full_path + 11, sizeof(rel_rom_path) - 1);
	} else {
		strncpy(rel_rom_path, rom_full_path, sizeof(rel_rom_path) - 1);
	}
	rel_rom_path[sizeof(rel_rom_path) - 1] = '\0';

	// Generate MGL content
	char mgl_content[2048];
	if (!generate_mgl_content(mgl_content, sizeof(mgl_content),
	                          sys->core_path, sys->delay, sys->file_index, rel_rom_path)) {
		printf("MGL Generator: Failed to generate content for '%s'\n", display_name);
		return -1;
	}

	// Write MGL file
	FILE *f = fopen(mgl_path, "w");
	if (!f) {
		printf("MGL Generator: Failed to create '%s'\n", mgl_path);
		return -1;
	}

	fprintf(f, "%s", mgl_content);
	fclose(f);

	printf("MGL Generator: Created '%s'\n", mgl_path);
	return 1;
}

// Scan a system's ROM folder and generate MGLs
int mgl_generate_for_system(const char *system_name)
{
	// Find system config
	const system_config_t *sys = NULL;
	for (int i = 0; systems[i].name; i++) {
		if (strcasecmp(system_name, systems[i].name) == 0) {
			sys = &systems[i];
			break;
		}
	}

	if (!sys) {
		printf("MGL Generator: Unknown system '%s'\n", system_name);
		return -1;
	}

	// Build ROM folder path
	char rom_dir[512];
	snprintf(rom_dir, sizeof(rom_dir), "%s/games/%s", getRootDir(), sys->rom_folder);

	DIR *dir = opendir(rom_dir);
	if (!dir) {
		printf("MGL Generator: ROM folder not found: %s\n", rom_dir);
		return 0;
	}

	int created = 0;
	int skipped = 0;
	struct dirent *entry;

	while ((entry = readdir(dir)) != NULL) {
		if (entry->d_name[0] == '.') continue;  // Skip hidden files

		// Check if it's a file with valid extension
		if (!has_valid_extension(entry->d_name, sys->extensions)) continue;

		char rom_path[1024];
		snprintf(rom_path, sizeof(rom_path), "%s/%s", rom_dir, entry->d_name);

		int result = generate_mgl_for_rom(sys, entry->d_name, rom_path);
		if (result > 0) created++;
		else if (result == 0) skipped++;
	}

	closedir(dir);

	printf("MGL Generator: %s - Created %d, Skipped %d (already exist)\n",
	       sys->name, created, skipped);

	return created;
}

// Generate MGLs for all systems
int mgl_generate_all(void)
{
	int total_created = 0;

	for (int i = 0; systems[i].name; i++) {
		int created = mgl_generate_for_system(systems[i].name);
		if (created > 0) total_created += created;
	}

	printf("MGL Generator: Total created: %d\n", total_created);
	return total_created;
}

// Get list of available systems (for UI)
int mgl_get_system_count(void)
{
	int count = 0;
	for (int i = 0; systems[i].name; i++) count++;
	return count;
}

const char* mgl_get_system_name(int index)
{
	int count = 0;
	for (int i = 0; systems[i].name; i++) {
		if (count == index) return systems[i].name;
		count++;
	}
	return NULL;
}

const char* mgl_get_system_core(int index)
{
	int count = 0;
	for (int i = 0; systems[i].name; i++) {
		if (count == index) return systems[i].core_path;
		count++;
	}
	return NULL;
}

// Check if a system has any ROMs
int mgl_system_has_roms(const char *system_name)
{
	// Find system config
	const system_config_t *sys = NULL;
	for (int i = 0; systems[i].name; i++) {
		if (strcasecmp(system_name, systems[i].name) == 0) {
			sys = &systems[i];
			break;
		}
	}

	if (!sys) return 0;

	char rom_dir[512];
	snprintf(rom_dir, sizeof(rom_dir), "%s/games/%s", getRootDir(), sys->rom_folder);

	DIR *dir = opendir(rom_dir);
	if (!dir) return 0;

	int has_roms = 0;
	struct dirent *entry;

	while ((entry = readdir(dir)) != NULL) {
		if (entry->d_name[0] == '.') continue;
		if (has_valid_extension(entry->d_name, sys->extensions)) {
			has_roms = 1;
			break;
		}
	}

	closedir(dir);
	return has_roms;
}

// Get ROM count for a system
int mgl_get_rom_count(const char *system_name)
{
	// Find system config
	const system_config_t *sys = NULL;
	for (int i = 0; systems[i].name; i++) {
		if (strcasecmp(system_name, systems[i].name) == 0) {
			sys = &systems[i];
			break;
		}
	}

	if (!sys) return 0;

	char rom_dir[512];
	snprintf(rom_dir, sizeof(rom_dir), "%s/games/%s", getRootDir(), sys->rom_folder);

	DIR *dir = opendir(rom_dir);
	if (!dir) return 0;

	int count = 0;
	struct dirent *entry;

	while ((entry = readdir(dir)) != NULL) {
		if (entry->d_name[0] == '.') continue;
		if (has_valid_extension(entry->d_name, sys->extensions)) {
			count++;
		}
	}

	closedir(dir);
	return count;
}
