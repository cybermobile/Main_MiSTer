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
	char type;                  // MGL type: 'f' for files, 's' for CD/streaming
} system_config_t;

// Supported systems - comprehensive list of all MiSTer cores
// Based on https://github.com/wizzomafizzo/MiSTer_GamesMenu and MiSTer Wiki
static const system_config_t systems[] = {
	// === Nintendo Consoles ===
	// {name, core_path, rom_folder, extensions, libretro_repo, index, delay, type}
	{"NES",           "_Console/NES",           "NES",           ".nes|.fds|.nsf|.zip|.7z",  "Nintendo_-_Nintendo_Entertainment_System", 0, 1, 'f'},
	{"Famicom",       "_Console/NES",           "NES",           ".nes|.fds|.nsf|.zip|.7z",  "Nintendo_-_Nintendo_Entertainment_System", 0, 1, 'f'},
	{"FDS",           "_Console/NES",           "NES",           ".fds|.zip|.7z",            "Nintendo_-_Famicom_Disk_System", 0, 1, 'f'},
	{"SNES",          "_Console/SNES",          "SNES",          ".sfc|.smc|.zip|.7z",       "Nintendo_-_Super_Nintendo_Entertainment_System", 0, 2, 'f'},
	{"SuperFamicom",  "_Console/SNES",          "SNES",          ".sfc|.smc|.zip|.7z",       "Nintendo_-_Super_Nintendo_Entertainment_System", 0, 2, 'f'},
	{"N64",           "_Console/N64",           "N64",           ".n64|.z64|.v64|.zip|.7z",  "Nintendo_-_Nintendo_64", 1, 1, 'f'},
	{"GB",            "_Console/Gameboy",       "GAMEBOY",       ".gb|.gbc|.zip|.7z",        "Nintendo_-_Game_Boy", 0, 1, 'f'},
	{"Gameboy",       "_Console/Gameboy",       "GAMEBOY",       ".gb|.gbc|.zip|.7z",        "Nintendo_-_Game_Boy", 0, 1, 'f'},
	{"GBC",           "_Console/Gameboy",       "GBC",           ".gbc|.gb|.zip|.7z",        "Nintendo_-_Game_Boy_Color", 0, 1, 'f'},
	{"GBA",           "_Console/GBA",           "GBA",           ".gba|.zip|.7z",            "Nintendo_-_Game_Boy_Advance", 0, 1, 'f'},
	{"VirtualBoy",    "_Console/VirtualBoy",    "VirtualBoy",    ".vb|.vboy|.zip|.7z",       "Nintendo_-_Virtual_Boy", 1, 1, 'f'},
	{"SGB",           "_Console/SGB",           "SGB",           ".gb|.gbc|.zip|.7z",        "Nintendo_-_Super_Game_Boy", 0, 1, 'f'},
	{"PokemonMini",   "_Console/PokemonMini",   "PokemonMini",   ".min|.zip|.7z",            "Nintendo_-_Pokemon_Mini", 1, 1, 'f'},
	
	// === Sega Consoles ===
	{"Genesis",       "_Console/Genesis",       "Genesis",       ".md|.bin|.gen|.zip|.7z",   "Sega_-_Mega_Drive_-_Genesis", 0, 1, 'f'},
	{"MegaDrive",     "_Console/Genesis",       "MegaDrive",     ".md|.bin|.gen|.zip|.7z",   "Sega_-_Mega_Drive_-_Genesis", 0, 1, 'f'},
	{"MegaCD",        "_Console/MegaCD",        "MegaCD",        ".chd|.cue|.iso|.zip|.7z",  "Sega_-_Mega-CD_-_Sega_CD", 0, 1, 's'},
	{"SegaCD",        "_Console/MegaCD",        "SegaCD",        ".chd|.cue|.iso|.zip|.7z",  "Sega_-_Mega-CD_-_Sega_CD", 0, 1, 's'},
	{"32X",           "_Console/S32X",          "32X",           ".32x|.bin|.zip|.7z",       "Sega_-_32X", 0, 1, 'f'},
	{"SMS",           "_Console/SMS",           "SMS",           ".sms|.sg|.zip|.7z",        "Sega_-_Master_System_-_Mark_III", 0, 1, 'f'},
	{"MasterSystem",  "_Console/SMS",           "SMS",           ".sms|.sg|.zip|.7z",        "Sega_-_Master_System_-_Mark_III", 0, 1, 'f'},
	{"GameGear",      "_Console/SMS",           "GameGear",      ".gg|.zip|.7z",             "Sega_-_Game_Gear", 1, 1, 'f'},
	{"SG1000",        "_Console/ColecoVision",  "SG1000",        ".sg|.sc|.zip|.7z",         "Sega_-_SG-1000", 2, 1, 'f'},
	{"SC3000",        "_Console/ColecoVision",  "SG1000",        ".sg|.sc|.zip|.7z",         "Sega_-_SG-1000", 2, 1, 'f'},
	{"Saturn",        "_Console/Saturn",        "Saturn",        ".chd|.cue|.iso|.zip|.7z",  "Sega_-_Saturn", 0, 1, 's'},
	
	// === NEC Consoles ===
	{"TurboGrafx16",  "_Console/TurboGrafx16",  "TGFX16",        ".pce|.bin|.zip|.7z",       "NEC_-_PC_Engine_-_TurboGrafx_16", 0, 1, 'f'},
	{"PCEngine",      "_Console/TurboGrafx16",  "TGFX16",        ".pce|.bin|.zip|.7z",       "NEC_-_PC_Engine_-_TurboGrafx_16", 0, 1, 'f'},
	{"TGFX16CD",      "_Console/TurboGrafx16",  "TGFX16-CD",     ".chd|.cue|.iso|.zip|.7z",  "NEC_-_PC_Engine_CD_-_TurboGrafx-CD", 0, 1, 's'},
	{"PCECD",         "_Console/TurboGrafx16",  "TGFX16-CD",     ".chd|.cue|.iso|.zip|.7z",  "NEC_-_PC_Engine_CD_-_TurboGrafx-CD", 0, 1, 's'},
	{"SuperGrafx",    "_Console/TurboGrafx16",  "SuperGrafx",    ".sgx|.pce|.zip|.7z",       "NEC_-_PC_Engine_SuperGrafx", 1, 1, 'f'},
	
	// === Sony Consoles ===
	{"PSX",           "_Console/PSX",           "PSX",           ".chd|.cue|.iso|.bin|.zip|.7z", "Sony_-_PlayStation", 1, 1, 's'},
	{"PlayStation",   "_Console/PSX",           "PSX",           ".chd|.cue|.iso|.bin|.zip|.7z", "Sony_-_PlayStation", 1, 1, 's'},
	
	// === SNK Consoles ===
	{"NeoGeo",        "_Console/NeoGeo",        "NEOGEO",        ".neo|.zip|.7z",            "SNK_-_Neo_Geo", 1, 1, 'f'},
	{"MVS",           "_Console/NeoGeo",        "NEOGEO",        ".neo|.zip|.7z",            "SNK_-_Neo_Geo", 1, 1, 'f'},
	{"AES",           "_Console/NeoGeo",        "NEOGEO",        ".neo|.zip|.7z",            "SNK_-_Neo_Geo", 1, 1, 'f'},
	{"NeoGeoCD",      "_Console/NeoGeo",        "NeoGeo-CD",     ".chd|.cue|.iso|.zip|.7z",  "SNK_-_Neo_Geo_CD", 0, 1, 's'},
	{"NGP",           "_Console/NeoGeo",        "NGP",           ".ngp|.ngc|.zip|.7z",       "SNK_-_Neo_Geo_Pocket", 0, 1, 'f'},
	{"NeoGeoPocket",  "_Console/NeoGeo",        "NGP",           ".ngp|.ngc|.zip|.7z",       "SNK_-_Neo_Geo_Pocket", 0, 1, 'f'},
	{"NGPC",          "_Console/NeoGeo",        "NGPC",          ".ngc|.ngp|.zip|.7z",       "SNK_-_Neo_Geo_Pocket_Color", 0, 1, 'f'},
	
	// === Atari Consoles ===
	{"Atari2600",     "_Console/Atari7800",     "ATARI2600",     ".a26|.bin|.zip|.7z",       "Atari_-_2600", 1, 1, 'f'},
	{"VCS",           "_Console/Atari7800",     "ATARI2600",     ".a26|.bin|.zip|.7z",       "Atari_-_2600", 1, 1, 'f'},
	{"Atari5200",     "_Console/Atari5200",     "ATARI5200",     ".a52|.car|.bin|.rom|.zip|.7z", "Atari_-_5200", 0, 1, 'f'},
	{"Atari7800",     "_Console/Atari7800",     "ATARI7800",     ".a78|.a26|.bin|.zip|.7z",  "Atari_-_7800", 0, 1, 'f'},
	{"AtariLynx",     "_Console/AtariLynx",     "AtariLynx",     ".lnx|.lyx|.zip|.7z",       "Atari_-_Lynx", 0, 1, 'f'},
	{"Lynx",          "_Console/AtariLynx",     "AtariLynx",     ".lnx|.lyx|.zip|.7z",       "Atari_-_Lynx", 0, 1, 'f'},
	{"Jaguar",        "_Console/Jaguar",        "Jaguar",        ".j64|.jag|.rom|.zip|.7z",  "Atari_-_Jaguar", 1, 1, 'f'},
	
	// === Bandai Consoles ===
	{"WonderSwan",    "_Console/WonderSwan",    "WonderSwan",    ".ws|.wsc|.zip|.7z",        "Bandai_-_WonderSwan", 0, 1, 'f'},
	{"WonderSwanColor","_Console/WonderSwan",   "WonderSwanColor",".wsc|.ws|.zip|.7z",       "Bandai_-_WonderSwan_Color", 0, 1, 'f'},
	{"SwanCrystal",   "_Console/WonderSwan",    "WonderSwanColor",".wsc|.ws|.zip|.7z",       "Bandai_-_WonderSwan_Color", 0, 1, 'f'},
	
	// === Other Consoles ===
	{"ColecoVision",  "_Console/ColecoVision",  "Coleco",        ".col|.bin|.rom|.zip|.7z",  "Coleco_-_ColecoVision", 0, 1, 'f'},
	{"Coleco",        "_Console/ColecoVision",  "Coleco",        ".col|.bin|.rom|.zip|.7z",  "Coleco_-_ColecoVision", 0, 1, 'f'},
	{"Intellivision", "_Console/Intellivision", "Intellivision", ".int|.bin|.rom|.zip|.7z",  "Mattel_-_Intellivision", 0, 1, 'f'},
	{"INTV",          "_Console/Intellivision", "Intellivision", ".int|.bin|.rom|.zip|.7z",  "Mattel_-_Intellivision", 0, 1, 'f'},
	{"Vectrex",       "_Console/Vectrex",       "VECTREX",       ".vec|.ovr|.bin|.rom|.zip|.7z", "GCE_-_Vectrex", 0, 1, 'f'},
	{"Odyssey2",      "_Console/Odyssey2",      "Odyssey2",      ".bin|.o2|.zip|.7z",        "Magnavox_-_Odyssey2", 0, 1, 'f'},
	{"Videopac",      "_Console/Odyssey2",      "Odyssey2",      ".bin|.o2|.zip|.7z",        "Philips_-_Videopac", 0, 1, 'f'},
	{"ChannelF",      "_Console/ChannelF",      "ChannelF",      ".bin|.chf|.rom|.zip|.7z",  "Fairchild_-_Channel_F", 0, 1, 'f'},
	{"Supervision",   "_Console/Supervision",   "Supervision",   ".sv|.bin|.zip|.7z",        "Watara_-_Supervision", 0, 1, 'f'},
	{"MegaDuck",      "_Console/MegaDuck",      "MegaDuck",      ".bin|.zip|.7z",            "Mega_Duck", 0, 1, 'f'},
	{"Gamate",        "_Console/Gamate",        "Gamate",        ".bin|.zip|.7z",            "Bit_Corporation_-_Gamate", 0, 1, 'f'},
	{"GameMaster",    "_Console/GameMaster",    "GameMaster",    ".bin|.zip|.7z",            "Hartung_-_Game_Master", 0, 1, 'f'},
	{"CreatiVision",  "_Console/CreatiVision",  "CreatiVision",  ".bin|.rom|.zip|.7z",       "VTech_-_CreatiVision", 0, 1, 'f'},
	{"Arcadia2001",   "_Console/Arcadia",       "Arcadia",       ".bin|.zip|.7z",            "Emerson_-_Arcadia_2001", 0, 1, 'f'},
	{"VC4000",        "_Console/VC4000",        "VC4000",        ".bin|.zip|.7z",            "Interton_-_VC_4000", 0, 1, 'f'},
	{"AdventureVision","_Console/AVision",      "AVision",       ".bin|.zip|.7z",            "Entex_-_Adventure_Vision", 0, 1, 'f'},
	{"StudioII",      "_Console/StudioII",      "StudioII",      ".st2|.bin|.zip|.7z",       "RCA_-_Studio_II", 0, 1, 'f'},
	
	// === Computers - Commodore ===
	{"C64",           "_Computer/C64",          "C64",           ".prg|.crt|.d64|.t64|.tap|.reu|.zip|.7z", "Commodore_-_64", 1, 1, 'f'},
	{"Commodore64",   "_Computer/C64",          "C64",           ".prg|.crt|.d64|.t64|.tap|.reu|.zip|.7z", "Commodore_-_64", 1, 1, 'f'},
	{"C128",          "_Computer/C128",         "C128",          ".prg|.crt|.d64|.d81|.zip|.7z", "Commodore_-_64", 1, 1, 'f'},
	{"VIC20",         "_Computer/VIC20",        "VIC20",         ".prg|.crt|.20|.a0|.zip|.7z", "Commodore_-_VIC-20", 1, 1, 'f'},
	{"C16",           "_Computer/C16",          "C16",           ".prg|.crt|.zip|.7z",       "Commodore_-_Plus-4", 1, 1, 'f'},
	{"Plus4",         "_Computer/C16",          "C16",           ".prg|.crt|.zip|.7z",       "Commodore_-_Plus-4", 1, 1, 'f'},
	{"PET",           "_Computer/PET2001",      "PET",           ".prg|.tap|.zip|.7z",       "Commodore_-_PET", 1, 1, 'f'},
	{"Amiga",         "_Computer/Minimig",      "Amiga",         ".adf|.hdf|.lha|.zip|.7z",  "Commodore_-_Amiga", 0, 1, 'f'},
	{"Minimig",       "_Computer/Minimig",      "Amiga",         ".adf|.hdf|.lha|.zip|.7z",  "Commodore_-_Amiga", 0, 1, 'f'},
	{"CD32",          "_Computer/MegaAGS",      "Amiga",         ".adf|.hdf|.chd|.zip|.7z",  "Commodore_-_Amiga_CD32", 0, 1, 'f'},
	
	// === Computers - Atari ===
	{"Atari800",      "_Computer/Atari800",     "ATARI800",      ".atr|.xex|.xfd|.car|.atx|.zip|.7z", "Atari_-_8-bit", 0, 1, 'f'},
	{"Atari400",      "_Computer/Atari800",     "ATARI800",      ".atr|.xex|.xfd|.car|.atx|.zip|.7z", "Atari_-_8-bit", 0, 1, 'f'},
	{"AtariXL",       "_Computer/Atari800",     "ATARI800",      ".atr|.xex|.xfd|.car|.atx|.zip|.7z", "Atari_-_8-bit", 0, 1, 'f'},
	{"AtariXE",       "_Computer/Atari800",     "ATARI800",      ".atr|.xex|.xfd|.car|.atx|.zip|.7z", "Atari_-_8-bit", 0, 1, 'f'},
	{"AtariST",       "_Computer/AtariST",      "AtariST",       ".st|.stx|.msa|.img|.zip|.7z", "Atari_-_ST", 0, 1, 'f'},
	{"STE",           "_Computer/AtariST",      "AtariST",       ".st|.stx|.msa|.img|.zip|.7z", "Atari_-_ST", 0, 1, 'f'},
	{"MegaST",        "_Computer/AtariST",      "AtariST",       ".st|.stx|.msa|.img|.zip|.7z", "Atari_-_ST", 0, 1, 'f'},
	
	// === Computers - Sinclair ===
	{"ZXSpectrum",    "_Computer/ZX-Spectrum",  "Spectrum",      ".tap|.tzx|.z80|.sna|.trd|.scl|.zip|.7z", "Sinclair_-_ZX_Spectrum", 0, 1, 'f'},
	{"Spectrum",      "_Computer/ZX-Spectrum",  "Spectrum",      ".tap|.tzx|.z80|.sna|.trd|.scl|.zip|.7z", "Sinclair_-_ZX_Spectrum", 0, 1, 'f'},
	{"Spectrum128",   "_Computer/ZX-Spectrum",  "Spectrum",      ".tap|.tzx|.z80|.sna|.trd|.scl|.zip|.7z", "Sinclair_-_ZX_Spectrum", 0, 1, 'f'},
	{"ZX81",          "_Computer/ZX81",         "ZX81",          ".p|.o|.81|.zip|.7z",       "Sinclair_-_ZX_81", 0, 1, 'f'},
	{"QL",            "_Computer/QL",           "QL",            ".mdv|.zip|.7z",            "Sinclair_-_ZX_Spectrum", 0, 1, 'f'},
	{"ZXNext",        "_Computer/ZXNext",       "ZXNext",        ".nex|.tap|.tzx|.z80|.sna|.zip|.7z", "Sinclair_-_ZX_Spectrum", 0, 1, 'f'},
	
	// === Computers - MSX ===
	{"MSX",           "_Computer/MSX",          "MSX",           ".rom|.mx1|.dsk|.zip|.7z",  "Microsoft_-_MSX", 0, 1, 'f'},
	{"MSX1",          "_Computer/MSX",          "MSX",           ".rom|.mx1|.dsk|.zip|.7z",  "Microsoft_-_MSX", 0, 1, 'f'},
	{"MSX2",          "_Computer/MSX",          "MSX2",          ".rom|.mx2|.dsk|.zip|.7z",  "Microsoft_-_MSX2", 0, 1, 'f'},
	{"MSX2+",         "_Computer/MSX",          "MSX2",          ".rom|.mx2|.dsk|.zip|.7z",  "Microsoft_-_MSX2", 0, 1, 'f'},
	{"SVI328",        "_Computer/SVI",          "SVI",           ".rom|.dsk|.cas|.zip|.7z",  "Spectravideo_-_SV-318", 0, 1, 'f'},
	
	// === Computers - PC ===
	{"AO486",         "_Computer/ao486",        "AO486",         ".img|.ima|.vhd|.iso|.zip|.7z", "DOS", 0, 2, 'f'},
	{"DOS",           "_Computer/ao486",        "AO486",         ".img|.ima|.vhd|.iso|.zip|.7z", "DOS", 0, 2, 'f'},
	{"PCXT",          "_Computer/PCXT",         "PCXT",          ".img|.ima|.vhd|.zip|.7z",  "DOS", 0, 1, 'f'},
	
	// === Computers - Sharp ===
	{"X68000",        "_Computer/X68000",       "X68000",        ".dim|.xdf|.hdf|.2hd|.zip|.7z", "Sharp_-_X68000", 0, 1, 'f'},
	{"SharpMZ",       "_Computer/SharpMZ",      "SharpMZ",       ".mzf|.m12|.zip|.7z",       "Sharp_-_MZ-700", 0, 1, 'f'},
	{"MZ700",         "_Computer/SharpMZ",      "SharpMZ",       ".mzf|.m12|.zip|.7z",       "Sharp_-_MZ-700", 0, 1, 'f'},
	{"MZ800",         "_Computer/SharpMZ",      "SharpMZ",       ".mzf|.m12|.zip|.7z",       "Sharp_-_MZ-700", 0, 1, 'f'},
	{"X1",            "_Computer/X1",           "X1",            ".2d|.d88|.zip|.7z",        "Sharp_-_X1", 0, 1, 'f'},
	
	// === Computers - NEC ===
	{"PC88",          "_Computer/PC8801",       "PC8801",        ".d88|.cmt|.t88|.zip|.7z",  "NEC_-_PC-8801", 0, 1, 'f'},
	{"PC8801",        "_Computer/PC8801",       "PC8801",        ".d88|.cmt|.t88|.zip|.7z",  "NEC_-_PC-8801", 0, 1, 'f'},
	{"PC8001",        "_Computer/PC8001",       "PC8001",        ".d88|.cmt|.zip|.7z",       "NEC_-_PC-8001", 0, 1, 'f'},
	{"PC6001",        "_Computer/PC6001",       "PC6001",        ".cas|.p6|.zip|.7z",        "NEC_-_PC-6001", 0, 1, 'f'},
	{"PC98",          "_Computer/PC9801",       "PC9801",        ".fdi|.hdi|.fdd|.d88|.zip|.7z", "NEC_-_PC-98", 0, 1, 'f'},
	{"PC9801",        "_Computer/PC9801",       "PC9801",        ".fdi|.hdi|.fdd|.d88|.zip|.7z", "NEC_-_PC-98", 0, 1, 'f'},
	
	// === Computers - Apple ===
	{"Apple1",        "_Computer/Apple-I",      "Apple-I",       ".txt|.zip|.7z",            "Apple_-_Apple_I", 0, 1, 'f'},
	{"Apple2",        "_Computer/Apple-II",     "Apple-II",      ".dsk|.do|.po|.nib|.woz|.hdv|.zip|.7z", "Apple_-_Apple_II", 0, 1, 'f'},
	{"AppleII",       "_Computer/Apple-II",     "Apple-II",      ".dsk|.do|.po|.nib|.woz|.hdv|.zip|.7z", "Apple_-_Apple_II", 0, 1, 'f'},
	{"AppleIIGS",     "_Computer/Apple-II",     "AppleIIGS",     ".2mg|.po|.hdv|.zip|.7z",   "Apple_-_Apple_IIGS", 0, 1, 'f'},
	{"Macintosh",     "_Computer/MacPlus",      "MacPlus",       ".dsk|.img|.hda|.zip|.7z",  "Apple_-_Macintosh", 0, 1, 'f'},
	{"MacPlus",       "_Computer/MacPlus",      "MacPlus",       ".dsk|.img|.hda|.zip|.7z",  "Apple_-_Macintosh", 0, 1, 'f'},
	
	// === Computers - Amstrad ===
	{"Amstrad",       "_Computer/Amstrad",      "Amstrad",       ".dsk|.cdt|.cpr|.zip|.7z",  "Amstrad_-_CPC", 0, 1, 'f'},
	{"CPC",           "_Computer/Amstrad",      "Amstrad",       ".dsk|.cdt|.cpr|.zip|.7z",  "Amstrad_-_CPC", 0, 1, 'f'},
	{"CPC464",        "_Computer/Amstrad",      "Amstrad",       ".dsk|.cdt|.cpr|.zip|.7z",  "Amstrad_-_CPC", 0, 1, 'f'},
	{"CPC6128",       "_Computer/Amstrad",      "Amstrad",       ".dsk|.cdt|.cpr|.zip|.7z",  "Amstrad_-_CPC", 0, 1, 'f'},
	{"AmstradPCW",    "_Computer/Amstrad-PCW",  "AmstradPCW",    ".dsk|.zip|.7z",            "Amstrad_-_CPC", 0, 1, 'f'},
	{"GX4000",        "_Computer/Amstrad",      "GX4000",        ".cpr|.bin|.zip|.7z",       "Amstrad_-_GX4000", 0, 1, 'f'},
	
	// === Computers - Acorn ===
	{"BBCMicro",      "_Computer/BBCMicro",     "BBCMicro",      ".ssd|.dsd|.uef|.zip|.7z",  "Acorn_-_BBC_Micro", 0, 1, 'f'},
	{"BBC",           "_Computer/BBCMicro",     "BBCMicro",      ".ssd|.dsd|.uef|.zip|.7z",  "Acorn_-_BBC_Micro", 0, 1, 'f'},
	{"BBCMaster",     "_Computer/BBCMicro",     "BBCMicro",      ".ssd|.dsd|.uef|.zip|.7z",  "Acorn_-_BBC_Micro", 0, 1, 'f'},
	{"Archimedes",    "_Computer/Archimedes",   "ARCHIE",        ".adf|.hdf|.zip|.7z",       "Acorn_-_Archimedes", 0, 1, 'f'},
	{"Acorn",         "_Computer/Archimedes",   "ARCHIE",        ".adf|.hdf|.zip|.7z",       "Acorn_-_Archimedes", 0, 1, 'f'},
	{"Electron",      "_Computer/Electron",     "Electron",      ".uef|.rom|.zip|.7z",       "Acorn_-_Electron", 0, 1, 'f'},
	{"AcornAtom",     "_Computer/AcornAtom",    "AcornAtom",     ".atm|.zip|.7z",            "Acorn_-_Atom", 0, 1, 'f'},
	
	// === Computers - Japanese ===
	{"FMTowns",       "_Computer/FMTowns",      "FMTowns",       ".cue|.chd|.iso|.zip|.7z",  "Fujitsu_-_FM_Towns", 0, 1, 's'},
	{"FM7",           "_Computer/FM7",          "FM7",           ".t77|.d77|.zip|.7z",       "Fujitsu_-_FM-7", 0, 1, 'f'},
	
	// === Computers - Eastern European ===
	{"BK0011M",       "_Computer/BK0011M",      "BK0011M",       ".bin|.dsk|.zip|.7z",       "Elektronika_-_BK", 0, 1, 'f'},
	{"Vector06C",     "_Computer/Vector-06C",   "Vector06C",     ".rom|.fdd|.zip|.7z",       "Vector-06C", 0, 1, 'f'},
	{"Specialist",    "_Computer/Specialist",   "Specialist",    ".rks|.zip|.7z",            "Specialist", 0, 1, 'f'},
	{"Radio86RK",     "_Computer/Radio86RK",    "Radio86RK",     ".rk|.rkr|.zip|.7z",        "Radio-86RK", 0, 1, 'f'},
	{"Orao",          "_Computer/Orao",         "Orao",          ".tap|.zip|.7z",            "Orao", 0, 1, 'f'},
	{"Galaksija",     "_Computer/Galaksija",    "Galaksija",     ".gtp|.zip|.7z",            "Galaksija", 0, 1, 'f'},
	{"PMD85",         "_Computer/PMD85",        "PMD85",         ".pmd|.ptp|.zip|.7z",       "PMD_85", 0, 1, 'f'},
	{"Ondra",         "_Computer/Ondra",        "Ondra",         ".tap|.zip|.7z",            "Ondra", 0, 1, 'f'},
	{"Primo",         "_Computer/Primo",        "Primo",         ".ptp|.zip|.7z",            "Microkey_-_Primo", 0, 1, 'f'},
	
	// === Computers - Other ===
	{"TI994A",        "_Computer/TI-99_4A",     "TI-99_4A",      ".bin|.rpk|.ctg|.zip|.7z",  "Texas_Instruments_-_TI-99", 0, 1, 'f'},
	{"TI99",          "_Computer/TI-99_4A",     "TI-99_4A",      ".bin|.rpk|.ctg|.zip|.7z",  "Texas_Instruments_-_TI-99", 0, 1, 'f'},
	{"SAMCoupe",      "_Computer/SAM-Coupe",    "SAMCoupe",      ".dsk|.mgt|.sdf|.zip|.7z",  "MGT_-_SAM_Coupe", 0, 1, 'f'},
	{"Aquarius",      "_Computer/Aquarius",     "Aquarius",      ".bin|.caq|.zip|.7z",       "Mattel_-_Aquarius", 0, 1, 'f'},
	{"Oric",          "_Computer/Oric",         "Oric",          ".dsk|.tap|.zip|.7z",       "Tangerine_-_Oric", 0, 1, 'f'},
	{"OricAtmos",     "_Computer/Oric",         "Oric",          ".dsk|.tap|.zip|.7z",       "Tangerine_-_Oric", 0, 1, 'f'},
	{"Dragon32",      "_Computer/CoCo2",        "Dragon",        ".dsk|.cas|.ccc|.zip|.7z",  "Dragon_Data_-_Dragon", 0, 1, 'f'},
	{"Dragon64",      "_Computer/CoCo2",        "Dragon",        ".dsk|.cas|.ccc|.zip|.7z",  "Dragon_Data_-_Dragon", 0, 1, 'f'},
	{"CoCo",          "_Computer/CoCo2",        "CoCo",          ".dsk|.cas|.ccc|.vhd|.zip|.7z", "Tandy_-_TRS-80_Color_Computer", 0, 1, 'f'},
	{"CoCo2",         "_Computer/CoCo2",        "CoCo",          ".dsk|.cas|.ccc|.vhd|.zip|.7z", "Tandy_-_TRS-80_Color_Computer", 0, 1, 'f'},
	{"CoCo3",         "_Computer/CoCo3",        "CoCo3",         ".dsk|.cas|.ccc|.vhd|.zip|.7z", "Tandy_-_TRS-80_Color_Computer", 0, 1, 'f'},
	{"TRS80",         "_Computer/TRS-80",       "TRS-80",        ".dsk|.cas|.cmd|.zip|.7z",  "Tandy_-_TRS-80", 0, 1, 'f'},
	{"MC10",          "_Computer/MC-10",        "MC-10",         ".c10|.cas|.zip|.7z",       "Tandy_-_TRS-80_Color_Computer", 0, 1, 'f'},
	{"AliceMC10",     "_Computer/MC-10",        "MC-10",         ".c10|.cas|.zip|.7z",       "Tandy_-_TRS-80_Color_Computer", 0, 1, 'f'},
	{"Jupiter",       "_Computer/Jupiter",      "Jupiter",       ".ace|.tap|.zip|.7z",       "Cantab_-_Jupiter_Ace", 0, 1, 'f'},
	{"JupiterAce",    "_Computer/Jupiter",      "Jupiter",       ".ace|.tap|.zip|.7z",       "Cantab_-_Jupiter_Ace", 0, 1, 'f'},
	{"Laser310",      "_Computer/Laser",        "Laser",         ".vz|.zip|.7z",             "VTech_-_Laser_310", 0, 1, 'f'},
	{"RX78",          "_Computer/RX78",         "RX78",          ".bin|.zip|.7z",            "Bandai_-_RX-78", 0, 1, 'f'},
	{"SordM5",        "_Computer/SordM5",       "SordM5",        ".bin|.rom|.zip|.7z",       "Sord_-_M5", 0, 1, 'f'},
	{"EpochSCV",      "_Computer/EpochSCV",     "EpochSCV",      ".cart|.bin|.zip|.7z",      "Epoch_-_Super_Cassette_Vision", 0, 1, 'f'},
	{"Interact",      "_Computer/Interact",     "Interact",      ".cin|.k7|.zip|.7z",        "Interact", 0, 1, 'f'},
	{"CamputersLynx", "_Computer/CamputersLynx","CamputersLynx", ".tap|.zip|.7z",            "Camputers_-_Lynx", 0, 1, 'f'},
	{"ColourGenie",   "_Computer/ColourGenie",  "ColourGenie",   ".cmd|.cas|.zip|.7z",       "EACA_-_Colour_Genie", 0, 1, 'f'},
	{"Einstein",      "_Computer/Einstein",     "Einstein",      ".dsk|.com|.zip|.7z",       "Tatung_-_Einstein", 0, 1, 'f'},
	{"Enterprise",    "_Computer/Enterprise",   "Enterprise",    ".ep|.wav|.zip|.7z",        "Enterprise", 0, 1, 'f'},
	{"MultiComp",     "_Computer/MultiComp",    "MultiComp",     ".img|.zip|.7z",            "MultiComp", 0, 1, 'f'},
	{"CHIP8",         "_Computer/Chip8",        "Chip8",         ".ch8|.c8|.zip|.7z",        "Chip-8", 0, 1, 'f'},
	{"PDP1",          "_Computer/PDP1",         "PDP1",          ".pdp|.rim|.zip|.7z",       "PDP-1", 0, 1, 'f'},
	{"Apogee",        "_Computer/Apogee",       "Apogee",        ".rka|.zip|.7z",            "Apogee_-_BK-01", 0, 1, 'f'},
	{"SPC1000",       "_Computer/SPC1000",      "SPC1000",       ".cas|.tap|.zip|.7z",       "Samsung_-_SPC-1000", 0, 1, 'f'},
	{"HT1080Z",       "_Computer/HT1080Z",      "HT1080Z",       ".cas|.zip|.7z",            "HT1080Z", 0, 1, 'f'},
	{"ABC80",         "_Computer/ABC80",        "ABC80",         ".bac|.zip|.7z",            "Luxor_-_ABC_80", 0, 1, 'f'},
	{"Kaypro",        "_Computer/Kaypro",       "Kaypro",        ".dsk|.zip|.7z",            "Kaypro", 0, 1, 'f'},
	{"Osborne1",      "_Computer/Osborne",      "Osborne",       ".dsk|.zip|.7z",            "Osborne_-_1", 0, 1, 'f'},
	
	// === Arcade (FBNeo/MAME style) ===
	{"Arcade",        "_Arcade",                "mame",          ".zip|.7z",                 "MAME", 1, 1, 'f'},
	{"MAME",          "_Arcade",                "mame",          ".zip|.7z",                 "MAME", 1, 1, 'f'},
	{"FBNeo",         "_Arcade",                "fbneo",         ".zip|.7z",                 "FBNeo_-_Arcade_Games", 1, 1, 'f'},
	{"HBMAME",        "_Arcade",                "hbmame",        ".zip|.7z",                 "MAME", 1, 1, 'f'},
	
	// === Arcade - Capcom ===
	{"CPS1",          "_Arcade/CPS1",           "cps1",          ".zip|.7z",                 "Capcom_-_CPS-1", 1, 1, 'f'},
	{"CPS2",          "_Arcade/CPS2",           "cps2",          ".zip|.7z",                 "Capcom_-_CPS-2", 1, 1, 'f'},
	{"CPS15",         "_Arcade/CPS15",          "cps15",         ".zip|.7z",                 "Capcom_-_CPS-1", 1, 1, 'f'},
	
	// === Arcade - Sega ===
	{"System1",       "_Arcade/System1",        "sega_system1",  ".zip|.7z",                 "MAME", 1, 1, 'f'},
	{"System16",      "_Arcade",                "sega_system16", ".zip|.7z",                 "MAME", 1, 1, 'f'},
	{"MegaTech",      "_Arcade",                "megatech",      ".zip|.7z",                 "MAME", 1, 1, 'f'},
	{"MegaPlay",      "_Arcade",                "megaplay",      ".zip|.7z",                 "MAME", 1, 1, 'f'},
	
	// === Arcade - Konami ===
	{"Konami",        "_Arcade",                "konami",        ".zip|.7z",                 "MAME", 1, 1, 'f'},
	
	// === Arcade - Namco ===
	{"Namco",         "_Arcade",                "namco",         ".zip|.7z",                 "MAME", 1, 1, 'f'},
	
	// === Arcade - Other Manufacturers ===
	{"Irem",          "_Arcade",                "irem",          ".zip|.7z",                 "MAME", 1, 1, 'f'},
	{"Taito",         "_Arcade",                "taito",         ".zip|.7z",                 "MAME", 1, 1, 'f'},
	{"Toaplan",       "_Arcade",                "toaplan",       ".zip|.7z",                 "MAME", 1, 1, 'f'},
	{"Cave",          "_Arcade",                "cave",          ".zip|.7z",                 "MAME", 1, 1, 'f'},
	{"Jaleco",        "_Arcade",                "jaleco",        ".zip|.7z",                 "MAME", 1, 1, 'f'},
	{"DataEast",      "_Arcade",                "dataeast",      ".zip|.7z",                 "MAME", 1, 1, 'f'},
	{"Technos",       "_Arcade",                "technos",       ".zip|.7z",                 "MAME", 1, 1, 'f'},
	{"Tecmo",         "_Arcade",                "tecmo",         ".zip|.7z",                 "MAME", 1, 1, 'f'},
	{"Williams",      "_Arcade",                "williams",      ".zip|.7z",                 "MAME", 1, 1, 'f'},
	{"Midway",        "_Arcade",                "midway",        ".zip|.7z",                 "MAME", 1, 1, 'f'},
	{"Atari",         "_Arcade",                "atari",         ".zip|.7z",                 "MAME", 1, 1, 'f'},
	{"Nintendo",      "_Arcade",                "nintendo",      ".zip|.7z",                 "MAME", 1, 1, 'f'},
	{"Nichibutsu",    "_Arcade",                "nichibutsu",    ".zip|.7z",                 "MAME", 1, 1, 'f'},
	{"Universal",     "_Arcade",                "universal",     ".zip|.7z",                 "MAME", 1, 1, 'f'},
	{"Cinematronics", "_Arcade",                "cinematronics", ".zip|.7z",                 "MAME", 1, 1, 'f'},
	{"Gottlieb",      "_Arcade",                "gottlieb",      ".zip|.7z",                 "MAME", 1, 1, 'f'},
	{"SNKArcade",     "_Arcade",                "snk",           ".zip|.7z",                 "MAME", 1, 1, 'f'},
	
	{NULL, NULL, NULL, NULL, NULL, 0, 0, 0}
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
                                const char *core_path, int delay, char type,
                                int file_index, const char *rom_path)
{
	int written = snprintf(buffer, buffer_size,
		"<mistergamedescription>\n"
		"\t<rbf>%s</rbf>\n"
		"\t<file delay=\"%d\" type=\"%c\" index=\"%d\" path=\"%s\"/>\n"
		"</mistergamedescription>\n",
		core_path, delay, type, file_index, rom_path);

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
	                          sys->core_path, sys->delay, sys->type, sys->file_index, rel_rom_path)) {
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
