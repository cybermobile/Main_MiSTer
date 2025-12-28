// scraper.cpp
// Artwork scraper for MiSTer graphical frontend
// Uses libretro-thumbnails (no API key required)
// 2024

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <pthread.h>
#include <ctype.h>

#include "scraper.h"
#include "file_io.h"

// Libretro-thumbnails repository mappings
// See: https://github.com/orgs/libretro-thumbnails/repositories
// Comprehensive list for all MiSTer cores
typedef struct {
	const char *core_name;
	const char *libretro_repo;
} libretro_platform_t;

static const libretro_platform_t libretro_platforms[] = {
	// === Nintendo Consoles ===
	{"NES",             "Nintendo_-_Nintendo_Entertainment_System"},
	{"Famicom",         "Nintendo_-_Nintendo_Entertainment_System"},
	{"FDS",             "Nintendo_-_Famicom_Disk_System"},
	{"SNES",            "Nintendo_-_Super_Nintendo_Entertainment_System"},
	{"SuperFamicom",    "Nintendo_-_Super_Nintendo_Entertainment_System"},
	{"SFC",             "Nintendo_-_Super_Nintendo_Entertainment_System"},
	{"N64",             "Nintendo_-_Nintendo_64"},
	{"GB",              "Nintendo_-_Game_Boy"},
	{"Gameboy",         "Nintendo_-_Game_Boy"},
	{"GBC",             "Nintendo_-_Game_Boy_Color"},
	{"GameboyColor",    "Nintendo_-_Game_Boy_Color"},
	{"GBA",             "Nintendo_-_Game_Boy_Advance"},
	{"GameboyAdvance",  "Nintendo_-_Game_Boy_Advance"},
	{"VirtualBoy",      "Nintendo_-_Virtual_Boy"},
	{"VB",              "Nintendo_-_Virtual_Boy"},
	{"PokemonMini",     "Nintendo_-_Pokemon_Mini"},
	{"PKM",             "Nintendo_-_Pokemon_Mini"},
	{"SGB",             "Nintendo_-_Super_Game_Boy"},
	{"SuperGameboy",    "Nintendo_-_Super_Game_Boy"},
	
	// === Sega Consoles ===
	{"Genesis",         "Sega_-_Mega_Drive_-_Genesis"},
	{"MegaDrive",       "Sega_-_Mega_Drive_-_Genesis"},
	{"MD",              "Sega_-_Mega_Drive_-_Genesis"},
	{"MegaCD",          "Sega_-_Mega-CD_-_Sega_CD"},
	{"SegaCD",          "Sega_-_Mega-CD_-_Sega_CD"},
	{"SCD",             "Sega_-_Mega-CD_-_Sega_CD"},
	{"MCD",             "Sega_-_Mega-CD_-_Sega_CD"},
	{"32X",             "Sega_-_32X"},
	{"S32X",            "Sega_-_32X"},
	{"SMS",             "Sega_-_Master_System_-_Mark_III"},
	{"MasterSystem",    "Sega_-_Master_System_-_Mark_III"},
	{"MarkIII",         "Sega_-_Master_System_-_Mark_III"},
	{"GameGear",        "Sega_-_Game_Gear"},
	{"GG",              "Sega_-_Game_Gear"},
	{"SG1000",          "Sega_-_SG-1000"},
	{"SC3000",          "Sega_-_SG-1000"},
	{"Saturn",          "Sega_-_Saturn"},
	{"Dreamcast",       "Sega_-_Dreamcast"},
	{"DC",              "Sega_-_Dreamcast"},
	{"Pico",            "Sega_-_PICO"},
	{"SegaPico",        "Sega_-_PICO"},
	
	// === NEC Consoles ===
	{"TurboGrafx16",    "NEC_-_PC_Engine_-_TurboGrafx_16"},
	{"TGFX16",          "NEC_-_PC_Engine_-_TurboGrafx_16"},
	{"PCEngine",        "NEC_-_PC_Engine_-_TurboGrafx_16"},
	{"PCE",             "NEC_-_PC_Engine_-_TurboGrafx_16"},
	{"TG16",            "NEC_-_PC_Engine_-_TurboGrafx_16"},
	{"TGFX16CD",        "NEC_-_PC_Engine_CD_-_TurboGrafx-CD"},
	{"PCECD",           "NEC_-_PC_Engine_CD_-_TurboGrafx-CD"},
	{"TurboCD",         "NEC_-_PC_Engine_CD_-_TurboGrafx-CD"},
	{"SuperGrafx",      "NEC_-_PC_Engine_SuperGrafx"},
	{"SGX",             "NEC_-_PC_Engine_SuperGrafx"},
	{"PCFX",            "NEC_-_PC-FX"},
	{"PC-FX",           "NEC_-_PC-FX"},
	
	// === Sony Consoles ===
	{"PSX",             "Sony_-_PlayStation"},
	{"PlayStation",     "Sony_-_PlayStation"},
	{"PS1",             "Sony_-_PlayStation"},
	{"PS",              "Sony_-_PlayStation"},
	{"PSP",             "Sony_-_PlayStation_Portable"},
	
	// === SNK Consoles ===
	{"NeoGeo",          "SNK_-_Neo_Geo"},
	{"NEOGEO",          "SNK_-_Neo_Geo"},
	{"MVS",             "SNK_-_Neo_Geo"},
	{"AES",             "SNK_-_Neo_Geo"},
	{"NeoGeoCD",        "SNK_-_Neo_Geo_CD"},
	{"NGCD",            "SNK_-_Neo_Geo_CD"},
	{"NGP",             "SNK_-_Neo_Geo_Pocket"},
	{"NeoGeoPocket",    "SNK_-_Neo_Geo_Pocket"},
	{"NGPC",            "SNK_-_Neo_Geo_Pocket_Color"},
	{"NeoGeoPocketColor", "SNK_-_Neo_Geo_Pocket_Color"},
	
	// === Atari Consoles ===
	{"Atari2600",       "Atari_-_2600"},
	{"ATARI2600",       "Atari_-_2600"},
	{"A2600",           "Atari_-_2600"},
	{"VCS",             "Atari_-_2600"},
	{"Atari5200",       "Atari_-_5200"},
	{"ATARI5200",       "Atari_-_5200"},
	{"A5200",           "Atari_-_5200"},
	{"Atari7800",       "Atari_-_7800"},
	{"ATARI7800",       "Atari_-_7800"},
	{"A7800",           "Atari_-_7800"},
	{"AtariLynx",       "Atari_-_Lynx"},
	{"Lynx",            "Atari_-_Lynx"},
	{"Jaguar",          "Atari_-_Jaguar"},
	{"AtariJaguar",     "Atari_-_Jaguar"},
	{"JaguarCD",        "Atari_-_Jaguar"},
	
	// === Bandai Consoles ===
	{"WonderSwan",      "Bandai_-_WonderSwan"},
	{"WS",              "Bandai_-_WonderSwan"},
	{"WonderSwanColor", "Bandai_-_WonderSwan_Color"},
	{"WSC",             "Bandai_-_WonderSwan_Color"},
	{"SwanCrystal",     "Bandai_-_WonderSwan_Color"},
	
	// === Other Consoles ===
	{"ColecoVision",    "Coleco_-_ColecoVision"},
	{"Coleco",          "Coleco_-_ColecoVision"},
	{"CV",              "Coleco_-_ColecoVision"},
	{"Intellivision",   "Mattel_-_Intellivision"},
	{"INTV",            "Mattel_-_Intellivision"},
	{"Vectrex",         "GCE_-_Vectrex"},
	{"VECTREX",         "GCE_-_Vectrex"},
	{"Odyssey2",        "Magnavox_-_Odyssey2"},
	{"O2",              "Magnavox_-_Odyssey2"},
	{"Videopac",        "Philips_-_Videopac"},
	{"G7000",           "Philips_-_Videopac"},
	{"ChannelF",        "Fairchild_-_Channel_F"},
	{"FairchildF",      "Fairchild_-_Channel_F"},
	{"Supervision",     "Watara_-_Supervision"},
	{"SV",              "Watara_-_Supervision"},
	{"MegaDuck",        "Mega_Duck"},
	{"GameKing",        "GameKing"},
	{"Gamate",          "Bit_Corporation_-_Gamate"},
	{"GameMaster",      "Hartung_-_Game_Master"},
	{"AdventureVision", "Entex_-_Adventure_Vision"},
	{"CreatiVision",    "VTech_-_CreatiVision"},
	{"Arcadia2001",     "Emerson_-_Arcadia_2001"},
	{"VC4000",          "Interton_-_VC_4000"},
	{"StudioII",        "RCA_-_Studio_II"},
	
	// === Computers - Commodore ===
	{"C64",             "Commodore_-_64"},
	{"Commodore64",     "Commodore_-_64"},
	{"C128",            "Commodore_-_64"},
	{"VIC20",           "Commodore_-_VIC-20"},
	{"VIC-20",          "Commodore_-_VIC-20"},
	{"C16",             "Commodore_-_Plus-4"},
	{"Plus4",           "Commodore_-_Plus-4"},
	{"C264",            "Commodore_-_Plus-4"},
	{"PET",             "Commodore_-_PET"},
	{"Amiga",           "Commodore_-_Amiga"},
	{"A500",            "Commodore_-_Amiga"},
	{"A1200",           "Commodore_-_Amiga"},
	{"Minimig",         "Commodore_-_Amiga"},
	{"CDTV",            "Commodore_-_Amiga_CD32"},
	{"CD32",            "Commodore_-_Amiga_CD32"},
	{"AmigaCD32",       "Commodore_-_Amiga_CD32"},
	
	// === Computers - Atari ===
	{"Atari800",        "Atari_-_8-bit"},
	{"ATARI800",        "Atari_-_8-bit"},
	{"Atari400",        "Atari_-_8-bit"},
	{"AtariXL",         "Atari_-_8-bit"},
	{"AtariXE",         "Atari_-_8-bit"},
	{"A800",            "Atari_-_8-bit"},
	{"AtariST",         "Atari_-_ST"},
	{"ATARIST",         "Atari_-_ST"},
	{"ST",              "Atari_-_ST"},
	{"STE",             "Atari_-_ST"},
	{"MegaST",          "Atari_-_ST"},
	{"Falcon",          "Atari_-_ST"},
	
	// === Computers - Sinclair ===
	{"ZXSpectrum",      "Sinclair_-_ZX_Spectrum"},
	{"Spectrum",        "Sinclair_-_ZX_Spectrum"},
	{"ZX-Spectrum",     "Sinclair_-_ZX_Spectrum"},
	{"ZX",              "Sinclair_-_ZX_Spectrum"},
	{"Spectrum128",     "Sinclair_-_ZX_Spectrum"},
	{"Spectrum48",      "Sinclair_-_ZX_Spectrum"},
	{"ZX81",            "Sinclair_-_ZX_81"},
	{"ZX80",            "Sinclair_-_ZX_81"},
	{"TS1000",          "Sinclair_-_ZX_81"},
	{"SpectrumNext",    "Sinclair_-_ZX_Spectrum"},
	{"ZXNext",          "Sinclair_-_ZX_Spectrum"},
	{"QL",              "Sinclair_-_ZX_Spectrum"},
	
	// === Computers - MSX ===
	{"MSX",             "Microsoft_-_MSX"},
	{"MSX1",            "Microsoft_-_MSX"},
	{"MSX2",            "Microsoft_-_MSX2"},
	{"MSX2+",           "Microsoft_-_MSX2"},
	{"MSXTurboR",       "Microsoft_-_MSX2"},
	{"SVI",             "Spectravideo_-_SV-318"},
	{"SVI318",          "Spectravideo_-_SV-318"},
	{"SVI328",          "Spectravideo_-_SV-318"},
	
	// === Computers - PC ===
	{"AO486",           "DOS"},
	{"ao486",           "DOS"},
	{"DOS",             "DOS"},
	{"486",             "DOS"},
	{"PCXT",            "DOS"},
	{"PC",              "DOS"},
	{"IBMPC",           "DOS"},
	
	// === Computers - Sharp ===
	{"X68000",          "Sharp_-_X68000"},
	{"SharpX68000",     "Sharp_-_X68000"},
	{"X68K",            "Sharp_-_X68000"},
	{"SharpMZ",         "Sharp_-_MZ-700"},
	{"MZ700",           "Sharp_-_MZ-700"},
	{"MZ800",           "Sharp_-_MZ-700"},
	{"MZ1500",          "Sharp_-_MZ-700"},
	{"MZ2500",          "Sharp_-_MZ-2500"},
	{"X1",              "Sharp_-_X1"},
	{"SharpX1",         "Sharp_-_X1"},
	
	// === Computers - NEC ===
	{"PC88",            "NEC_-_PC-8801"},
	{"PC8801",          "NEC_-_PC-8801"},
	{"PC8001",          "NEC_-_PC-8001"},
	{"PC6001",          "NEC_-_PC-6001"},
	{"PC98",            "NEC_-_PC-98"},
	{"PC9801",          "NEC_-_PC-98"},
	
	// === Computers - Apple ===
	{"Apple1",          "Apple_-_Apple_I"},
	{"AppleI",          "Apple_-_Apple_I"},
	{"Apple2",          "Apple_-_Apple_II"},
	{"Apple-II",        "Apple_-_Apple_II"},
	{"AppleII",         "Apple_-_Apple_II"},
	{"Apple2e",         "Apple_-_Apple_II"},
	{"Apple2c",         "Apple_-_Apple_II"},
	{"AppleIIGS",       "Apple_-_Apple_IIGS"},
	{"Apple2GS",        "Apple_-_Apple_IIGS"},
	{"Macintosh",       "Apple_-_Macintosh"},
	{"MacPlus",         "Apple_-_Macintosh"},
	{"Mac",             "Apple_-_Macintosh"},
	{"MacSE",           "Apple_-_Macintosh"},
	
	// === Computers - Amstrad ===
	{"Amstrad",         "Amstrad_-_CPC"},
	{"AmstradCPC",      "Amstrad_-_CPC"},
	{"CPC",             "Amstrad_-_CPC"},
	{"CPC464",          "Amstrad_-_CPC"},
	{"CPC6128",         "Amstrad_-_CPC"},
	{"AmstradPCW",      "Amstrad_-_CPC"},
	{"PCW",             "Amstrad_-_CPC"},
	{"GX4000",          "Amstrad_-_GX4000"},
	
	// === Computers - Acorn ===
	{"BBCMicro",        "Acorn_-_BBC_Micro"},
	{"BBC",             "Acorn_-_BBC_Micro"},
	{"BBCMaster",       "Acorn_-_BBC_Micro"},
	{"Archimedes",      "Acorn_-_Archimedes"},
	{"ARCHIE",          "Acorn_-_Archimedes"},
	{"Acorn",           "Acorn_-_Archimedes"},
	{"RiscPC",          "Acorn_-_Archimedes"},
	{"Electron",        "Acorn_-_Electron"},
	{"AcornAtom",       "Acorn_-_Atom"},
	{"Atom",            "Acorn_-_Atom"},
	
	// === Computers - Japanese ===
	{"FMTowns",         "Fujitsu_-_FM_Towns"},
	{"FM-Towns",        "Fujitsu_-_FM_Towns"},
	{"FM7",             "Fujitsu_-_FM-7"},
	{"FM77",            "Fujitsu_-_FM-7"},
	
	// === Computers - Eastern European ===
	{"BK0011M",         "Elektronika_-_BK"},
	{"BK0010",          "Elektronika_-_BK"},
	{"Apogee",          "Apogee_-_BK-01"},
	{"Vector06C",       "Vector-06C"},
	{"Specialist",      "Specialist"},
	{"Radio86RK",       "Radio-86RK"},
	{"Orao",            "Orao"},
	{"Galaksija",       "Galaksija"},
	{"PMD85",           "PMD_85"},
	
	// === Computers - Other ===
	{"TI994A",          "Texas_Instruments_-_TI-99"},
	{"TI-99_4A",        "Texas_Instruments_-_TI-99"},
	{"TI99",            "Texas_Instruments_-_TI-99"},
	{"SAMCoupe",        "MGT_-_SAM_Coupe"},
	{"SAM-Coupe",       "MGT_-_SAM_Coupe"},
	{"Aquarius",        "Mattel_-_Aquarius"},
	{"Oric",            "Tangerine_-_Oric"},
	{"OricAtmos",       "Tangerine_-_Oric"},
	{"Dragon",          "Dragon_Data_-_Dragon"},
	{"Dragon32",        "Dragon_Data_-_Dragon"},
	{"Dragon64",        "Dragon_Data_-_Dragon"},
	{"CoCo",            "Tandy_-_TRS-80_Color_Computer"},
	{"CoCo2",           "Tandy_-_TRS-80_Color_Computer"},
	{"CoCo3",           "Tandy_-_TRS-80_Color_Computer"},
	{"ColorComputer",   "Tandy_-_TRS-80_Color_Computer"},
	{"TRS80",           "Tandy_-_TRS-80"},
	{"TRS-80",          "Tandy_-_TRS-80"},
	{"Model1",          "Tandy_-_TRS-80"},
	{"Model3",          "Tandy_-_TRS-80"},
	{"Model4",          "Tandy_-_TRS-80"},
	{"AliceMC10",       "Tandy_-_TRS-80_Color_Computer"},
	{"MC10",            "Tandy_-_TRS-80_Color_Computer"},
	{"Jupiter",         "Cantab_-_Jupiter_Ace"},
	{"JupiterAce",      "Cantab_-_Jupiter_Ace"},
	{"Laser310",        "VTech_-_Laser_310"},
	{"Laser500",        "VTech_-_Laser_310"},
	{"RX78",            "Bandai_-_RX-78"},
	{"Gundam",          "Bandai_-_RX-78"},
	{"SordM5",          "Sord_-_M5"},
	{"M5",              "Sord_-_M5"},
	{"EpochSCV",        "Epoch_-_Super_Cassette_Vision"},
	{"SuperCassetteVision", "Epoch_-_Super_Cassette_Vision"},
	{"Interact",        "Interact"},
	{"CamputersLynx",   "Camputers_-_Lynx"},
	{"ColourGenie",     "EACA_-_Colour_Genie"},
	{"EG2000",          "EACA_-_Colour_Genie"},
	{"Einstein",        "Tatung_-_Einstein"},
	{"TatungEinstein",  "Tatung_-_Einstein"},
	{"Enterprise",      "Enterprise"},
	{"Enterprise64",    "Enterprise"},
	{"Enterprise128",   "Enterprise"},
	{"MultiComp",       "MultiComp"},
	{"ZXUno",           "Sinclair_-_ZX_Spectrum"},
	{"TSConf",          "Sinclair_-_ZX_Spectrum"},
	{"PDP1",            "PDP-1"},
	{"PDP11",           "PDP-11"},
	{"Chip8",           "Chip-8"},
	{"CHIP8",           "Chip-8"},
	{"ABC80",           "Luxor_-_ABC_80"},
	{"ABC800",          "Luxor_-_ABC_80"},
	{"Kaypro",          "Kaypro"},
	{"Osborne1",        "Osborne_-_1"},
	{"Ondra",           "Ondra"},
	{"Primo",           "Microkey_-_Primo"},
	{"HT1080Z",         "HT1080Z"},
	{"SPC1000",         "Samsung_-_SPC-1000"},
	{"SORD",            "Sord_-_M5"},
	
	// === Arcade - General ===
	{"Arcade",          "MAME"},
	{"FBNeo",           "FBNeo_-_Arcade_Games"},
	{"MAME",            "MAME"},
	
	// === Arcade - Capcom ===
	{"CPS1",            "Capcom_-_CPS-1"},
	{"CPS2",            "Capcom_-_CPS-2"},
	{"CPS3",            "Capcom_-_CPS-3"},
	{"CPS",             "MAME"},
	
	// === Arcade - Sega ===
	{"System1",         "MAME"},
	{"System16",        "MAME"},
	{"System18",        "MAME"},
	{"System24",        "MAME"},
	{"SystemE",         "MAME"},
	{"MegaTech",        "MAME"},
	{"MegaPlay",        "MAME"},
	{"Model1",          "MAME"},
	{"Naomi",           "MAME"},
	{"Naomi2",          "MAME"},
	{"Hikaru",          "MAME"},
	{"Triforce",        "MAME"},
	{"Chihiro",         "MAME"},
	{"Lindbergh",       "MAME"},
	
	// === Arcade - Other ===
	{"Konami",          "MAME"},
	{"Irem",            "MAME"},
	{"Namco",           "MAME"},
	{"Taito",           "MAME"},
	{"Toaplan",         "MAME"},
	{"Cave",            "MAME"},
	{"Jaleco",          "MAME"},
	{"DataEast",        "MAME"},
	{"Technos",         "MAME"},
	{"Tecmo",           "MAME"},
	{"SNKArcade",       "MAME"},
	{"PGM",             "MAME"},
	{"JAMMA",           "MAME"},
	
	{NULL, NULL}
};

// Global scraper state
static scraper_status_t scraper_status = SCRAPER_IDLE;
static scraper_progress_t scraper_progress;
static pthread_t scraper_thread;
static int scraper_thread_active = 0;
static int scraper_stop_requested = 0;
static pthread_mutex_t scraper_mutex = PTHREAD_MUTEX_INITIALIZER;

// Forward declarations
static int download_file(const char *url, const char *output_path);
static void clean_game_name(const char *input, char *output, size_t output_size);
static void url_encode(const char *input, char *output, size_t output_size);
static int file_exists(const char *path);

//// Core Functions ////

void scraper_init(void)
{
	memset(&scraper_progress, 0, sizeof(scraper_progress));
	scraper_status = SCRAPER_IDLE;
	printf("Scraper: Initialized (libretro-thumbnails)\n");
}

void scraper_shutdown(void)
{
	scraper_stop();
	printf("Scraper: Shutdown\n");
}

scraper_status_t scraper_get_status(void)
{
	return scraper_status;
}

scraper_progress_t* scraper_get_progress(void)
{
	return &scraper_progress;
}

//// Helper Functions ////

static int file_exists(const char *path)
{
	struct stat st;
	return stat(path, &st) == 0;
}

static void clean_game_name(const char *input, char *output, size_t output_size)
{
	if (!input || !output || output_size == 0) return;

	// Copy and clean the name
	strncpy(output, input, output_size - 1);
	output[output_size - 1] = '\0';

	// Remove file extension if present
	char *dot = strrchr(output, '.');
	if (dot && (strcasecmp(dot, ".mgl") == 0 || 
	            strcasecmp(dot, ".zip") == 0 ||
	            strcasecmp(dot, ".7z") == 0)) {
		*dot = '\0';
	}
}

static void url_encode(const char *input, char *output, size_t output_size)
{
	if (!input || !output || output_size == 0) return;

	size_t j = 0;
	for (size_t i = 0; input[i] && j < output_size - 4; i++) {
		unsigned char c = (unsigned char)input[i];
		if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
		    (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
			output[j++] = c;
		} else if (c == ' ') {
			output[j++] = '%';
			output[j++] = '2';
			output[j++] = '0';
		} else {
			// Percent encode
			snprintf(&output[j], 4, "%%%02X", c);
			j += 3;
		}
	}
	output[j] = '\0';
}

static int download_file(const char *url, const char *output_path)
{
	if (!url || !output_path) return 0;

	// Use wget for downloading (available on MiSTer)
	char cmd[2048];
	snprintf(cmd, sizeof(cmd),
		"wget -q --timeout=15 --tries=2 -O '%s' '%s' 2>/dev/null",
		output_path, url);

	int ret = system(cmd);
	
	// Check if file was actually downloaded
	if (ret == 0 && file_exists(output_path)) {
		struct stat st;
		if (stat(output_path, &st) == 0 && st.st_size > 100) {
			return 1;  // Success
		}
		// File too small, probably an error page
		unlink(output_path);
	}

	return 0;
}

//// Libretro Scraper ////

// Get libretro repository name for a core
const char* scraper_get_libretro_repo(const char *core_name)
{
	if (!core_name) return NULL;

	for (int i = 0; libretro_platforms[i].core_name; i++) {
		if (strcasecmp(core_name, libretro_platforms[i].core_name) == 0) {
			return libretro_platforms[i].libretro_repo;
		}
	}
	return NULL;
}

// Scrape from libretro-thumbnails
// URL format: https://raw.githubusercontent.com/libretro-thumbnails/{repo}/master/Named_Boxarts/{game}.png
static int scrape_from_libretro(const char *game_name, const char *core_name,
                                 const char *output_dir, scraper_result_t *result)
{
	// Find the libretro repository name for this core
	const char *libretro_repo = scraper_get_libretro_repo(core_name);
	if (!libretro_repo) {
		snprintf(result->error_msg, sizeof(result->error_msg),
			"Unknown system: %s", core_name);
		return 0;
	}

	// Clean game name
	char clean_name[256];
	clean_game_name(game_name, clean_name, sizeof(clean_name));

	// Sanitize for libretro URL (replace & / : * ? " < > | with _)
	char safe_name[256];
	strncpy(safe_name, clean_name, sizeof(safe_name) - 1);
	safe_name[sizeof(safe_name) - 1] = '\0';

	for (char *p = safe_name; *p; p++) {
		if (*p == '&' || *p == '/' || *p == ':' || *p == '*' ||
		    *p == '?' || *p == '"' || *p == '<' || *p == '>' || *p == '|') {
			*p = '_';
		}
	}

	// URL encode the game name
	char encoded_name[512];
	url_encode(safe_name, encoded_name, sizeof(encoded_name));

	// Build the libretro-thumbnails URL
	char url[1024];
	snprintf(url, sizeof(url),
		"https://raw.githubusercontent.com/libretro-thumbnails/%s/master/Named_Boxarts/%s.png",
		libretro_repo, encoded_name);

	// Build output path
	char output_path[1024];
	snprintf(output_path, sizeof(output_path), "%s/%s.png", output_dir, clean_name);

	// Create output directory
	char mkdir_cmd[1024];
	snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p '%s'", output_dir);
	system(mkdir_cmd);

	// Try to download
	if (download_file(url, output_path)) {
		result->found = 1;
		result->downloaded = 1;
		strncpy(result->artwork_path, output_path, sizeof(result->artwork_path));
		return 1;
	}

	// Try alternate naming: with region tags removed
	char alt_name[256];
	strncpy(alt_name, clean_name, sizeof(alt_name) - 1);
	alt_name[sizeof(alt_name) - 1] = '\0';

	// Remove common region/revision tags: (USA), (Europe), (Japan), etc.
	char *tag = strchr(alt_name, '(');
	if (tag) {
		*tag = '\0';
		// Trim trailing whitespace
		size_t len = strlen(alt_name);
		while (len > 0 && (alt_name[len-1] == ' ' || alt_name[len-1] == '\t')) {
			alt_name[--len] = '\0';
		}
	}

	if (strlen(alt_name) > 0 && strcmp(alt_name, clean_name) != 0) {
		// Sanitize alternate name
		for (char *p = alt_name; *p; p++) {
			if (*p == '&' || *p == '/' || *p == ':' || *p == '*' ||
			    *p == '?' || *p == '"' || *p == '<' || *p == '>' || *p == '|') {
				*p = '_';
			}
		}

		url_encode(alt_name, encoded_name, sizeof(encoded_name));
		snprintf(url, sizeof(url),
			"https://raw.githubusercontent.com/libretro-thumbnails/%s/master/Named_Boxarts/%s.png",
			libretro_repo, encoded_name);

		if (download_file(url, output_path)) {
			result->found = 1;
			result->downloaded = 1;
			strncpy(result->artwork_path, output_path, sizeof(result->artwork_path));
			return 1;
		}
	}

	snprintf(result->error_msg, sizeof(result->error_msg),
		"Not found on libretro-thumbnails");
	return 0;
}

//// Scraping Operations ////

int scraper_scrape_game(const char *game_name, const char *game_path,
                        const char *core_name, scraper_result_t *result)
{
	if (!result) return 0;
	memset(result, 0, sizeof(scraper_result_t));

	if (!game_name || !core_name) return 0;

	strncpy(result->game_name, game_name, sizeof(result->game_name) - 1);
	strncpy(result->game_path, game_path ? game_path : "", sizeof(result->game_path) - 1);

	// Build output directory
	char output_dir[1024];
	snprintf(output_dir, sizeof(output_dir), "%s/media/%s/boxart",
		getRootDir(), core_name);

	// Try to scrape
	if (scrape_from_libretro(game_name, core_name, output_dir, result)) {
		printf("Scraper: Found '%s' on libretro-thumbnails\n", game_name);
		return 1;
	}

	return 0;
}

int scraper_has_artwork(const char *game_path, const char *core_name)
{
	if (!game_path || !core_name) return 0;

	// Extract game name from path
	const char *filename = strrchr(game_path, '/');
	filename = filename ? filename + 1 : game_path;

	char game_name[256];
	clean_game_name(filename, game_name, sizeof(game_name));

	// Check if artwork file exists
	char artwork_path[1024];
	snprintf(artwork_path, sizeof(artwork_path), "%s/media/%s/boxart/%s.png",
		getRootDir(), core_name, game_name);

	return file_exists(artwork_path);
}

void scraper_stop(void)
{
	if (!scraper_thread_active) return;
	scraper_stop_requested = 1;

	// Wait for thread to finish
	for (int i = 0; i < 50 && scraper_thread_active; i++) {
		usleep(100000);
	}
}

//// Bulk Scraping ////

static void* scraper_system_thread(void *arg)
{
	char *system_name = (char*)arg;
	if (!system_name) {
		scraper_thread_active = 0;
		return NULL;
	}

	printf("Scraper: Starting bulk scrape for %s\n", system_name);
	scraper_status = SCRAPER_RUNNING;

	// Scan MGL folder for games
	char mgl_dir[512];
	snprintf(mgl_dir, sizeof(mgl_dir), "/media/fat/_Games/%s", system_name);

	DIR *dir = opendir(mgl_dir);
	if (!dir) {
		printf("Scraper: MGL folder not found: %s\n", mgl_dir);
		scraper_status = SCRAPER_ERROR;
		scraper_thread_active = 0;
		free(system_name);
		return NULL;
	}

	// Count files first
	int total = 0;
	struct dirent *entry;
	while ((entry = readdir(dir)) != NULL) {
		if (entry->d_name[0] == '.') continue;
		const char *ext = strrchr(entry->d_name, '.');
		if (ext && strcasecmp(ext, ".mgl") == 0) total++;
	}
	rewinddir(dir);

	scraper_progress.total_games = total;
	scraper_progress.processed = 0;
	scraper_progress.downloaded = 0;
	scraper_progress.failed = 0;

	// Process each MGL file
	while ((entry = readdir(dir)) != NULL && !scraper_stop_requested) {
		if (entry->d_name[0] == '.') continue;

		const char *ext = strrchr(entry->d_name, '.');
		if (!ext || strcasecmp(ext, ".mgl") != 0) continue;

		// Get game name
		char game_name[256];
		strncpy(game_name, entry->d_name, sizeof(game_name) - 1);
		game_name[sizeof(game_name) - 1] = '\0';
		char *dot = strrchr(game_name, '.');
		if (dot) *dot = '\0';

		strncpy(scraper_progress.current_game, game_name, sizeof(scraper_progress.current_game));

		// Check if we already have artwork
		if (scraper_has_artwork(game_name, system_name)) {
			scraper_progress.already_had++;
		} else {
			// Try to download
			scraper_result_t result;
			if (scraper_scrape_game(game_name, NULL, system_name, &result)) {
				scraper_progress.downloaded++;
			} else {
				scraper_progress.failed++;
			}
			// Rate limit
			usleep(200000);  // 200ms between requests
		}

		scraper_progress.processed++;
	}

	closedir(dir);
	free(system_name);

	printf("Scraper: Complete - Downloaded %d, Already had %d, Failed %d\n",
	       scraper_progress.downloaded, scraper_progress.already_had, scraper_progress.failed);

	scraper_status = SCRAPER_COMPLETE;
	scraper_thread_active = 0;
	return NULL;
}

int scraper_scrape_system(const char *system_name)
{
	if (!system_name || scraper_thread_active) return 0;

	char *name_copy = strdup(system_name);
	if (!name_copy) return 0;

	scraper_stop_requested = 0;
	memset(&scraper_progress, 0, sizeof(scraper_progress));

	if (pthread_create(&scraper_thread, NULL, scraper_system_thread, name_copy) != 0) {
		free(name_copy);
		return 0;
	}

	scraper_thread_active = 1;
	pthread_detach(scraper_thread);

	return 1;
}