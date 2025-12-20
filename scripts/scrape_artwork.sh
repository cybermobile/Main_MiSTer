#!/bin/bash
#
# MiSTer Artwork Scraper using SteamGridDB API
# 
# SETUP:
# 1. Go to https://www.steamgriddb.com/profile/preferences/api
# 2. Generate an API key (free account required)
# 3. Set your API key below or pass as argument
#
# USAGE:
#   ./scrape_artwork.sh [API_KEY] [SYSTEM]
#   ./scrape_artwork.sh abc123def456 SNES
#   ./scrape_artwork.sh abc123def456        # All systems
#

# === CONFIGURATION ===
API_KEY="${1:-YOUR_API_KEY_HERE}"
TARGET_SYSTEM="${2:-}"  # Empty = all systems

GAMES_PATH="/media/fat/games"
MEDIA_PATH="/media/fat/media"
LOG_FILE="/tmp/scrape_artwork.log"

# Map MiSTer folder names to SteamGridDB platform IDs
# See: https://www.steamgriddb.com/api/v2
declare -A PLATFORM_MAP=(
    ["SNES"]="snes"
    ["NES"]="nes"
    ["Genesis"]="genesis,megadrive"
    ["MegaCD"]="segacd"
    ["SMS"]="mastersystem"
    ["GameGear"]="gamegear"
    ["TurboGrafx16"]="turbografx16,pcengine"
    ["GBA"]="gba"
    ["GB"]="gameboy"
    ["GBC"]="gameboycolor"
    ["N64"]="n64"
    ["PSX"]="psx"
    ["NEOGEO"]="neogeo"
    ["Arcade"]="arcade"
    ["mame"]="arcade"
    ["ATARI2600"]="atari2600"
    ["ATARI5200"]="atari5200"
    ["ATARI7800"]="atari7800"
)

# === FUNCTIONS ===

log() {
    echo "[$(date '+%H:%M:%S')] $1" | tee -a "$LOG_FILE"
}

clean_game_name() {
    local name="$1"
    # Remove file extension
    name="${name%.*}"
    # Remove common tags like (USA), (Proto), [!], etc.
    name=$(echo "$name" | sed -E 's/\([^)]*\)//g' | sed -E 's/\[[^]]*\]//g')
    # Remove extra spaces
    name=$(echo "$name" | sed 's/  */ /g' | sed 's/^ *//;s/ *$//')
    echo "$name"
}

search_steamgriddb() {
    local game_name="$1"
    local encoded_name=$(echo "$game_name" | sed 's/ /%20/g')
    
    # Search for game ID (-k for SSL bypass on MiSTer)
    local response=$(curl -sk -H "Authorization: Bearer $API_KEY" \
        "https://www.steamgriddb.com/api/v2/search/autocomplete/$encoded_name")
    
    # Extract first game ID
    local game_id=$(echo "$response" | grep -o '"id":[0-9]*' | head -1 | cut -d: -f2)
    echo "$game_id"
}

download_grid() {
    local game_id="$1"
    local output_path="$2"
    local game_name="$3"
    
    if [ -z "$game_id" ]; then
        return 1
    fi
    
    # Get grid images (boxart)
    local response=$(curl -sk -H "Authorization: Bearer $API_KEY" \
        "https://www.steamgriddb.com/api/v2/grids/game/$game_id?dimensions=600x900,460x215")
    
    # Extract first image URL and unescape JSON slashes (\/ -> /)
    local image_url=$(echo "$response" | grep -o '"url":"[^"]*"' | head -1 | cut -d'"' -f4 | sed 's/\\\//\//g')
    
    if [ -n "$image_url" ] && [ "$image_url" != "null" ]; then
        local ext="${image_url##*.}"
        ext="${ext%%\?*}"  # Remove query params
        [ -z "$ext" ] && ext="png"
        
        local output_file="$output_path/${game_name}.${ext}"
        
        if curl -sk -o "$output_file" "$image_url"; then
            log "  ✓ Downloaded: $game_name"
            return 0
        fi
    fi
    
    return 1
}

process_system() {
    local system="$1"
    local games_dir="$GAMES_PATH/$system"
    local boxart_dir="$MEDIA_PATH/$system/boxart"
    
    if [ ! -d "$games_dir" ]; then
        return
    fi
    
    # Create artwork directory
    mkdir -p "$boxart_dir"
    
    log ""
    log "=== Processing: $system ==="
    
    local count=0
    local downloaded=0
    local skipped=0
    
    # Find all game files AND directories (for MAME/NeoGeo style romsets)
    local items=()
    
    # Add zip/rom files
    while IFS= read -r -d '' file; do
        items+=("$file")
    done < <(find "$games_dir" -maxdepth 1 -type f \( -name "*.zip" -o -name "*.7z" -o -name "*.sfc" -o -name "*.smc" -o -name "*.nes" -o -name "*.md" -o -name "*.bin" -o -name "*.gen" -o -name "*.sms" -o -name "*.gg" -o -name "*.pce" -o -name "*.gba" -o -name "*.gb" -o -name "*.gbc" -o -name "*.n64" -o -name "*.z64" \) -print0 2>/dev/null)
    
    # Add directories (for MAME/NeoGeo style)
    while IFS= read -r -d '' dir; do
        items+=("$dir")
    done < <(find "$games_dir" -maxdepth 1 -mindepth 1 -type d -print0 2>/dev/null)
    
    for item in "${items[@]}"; do
        local filename=$(basename "$item")
        local game_name=$(clean_game_name "$filename")
        
        # Skip special files/dirs
        [[ "$game_name" == "." ]] && continue
        [[ "$game_name" == ".." ]] && continue
        [[ "$filename" == *.lo ]] && continue
        [[ "$filename" == "mister-boot" ]] && continue
        
        # Skip if artwork already exists
        if [ -n "$(ls "$boxart_dir/$game_name".* 2>/dev/null)" ]; then
            ((skipped++))
            continue
        fi
        
        ((count++))
        
        log "  Searching: $game_name"
        
        local game_id=$(search_steamgriddb "$game_name")
        
        if [ -n "$game_id" ]; then
            if download_grid "$game_id" "$boxart_dir" "$game_name"; then
                ((downloaded++))
            fi
        else
            log "  ✗ Not found: $game_name"
        fi
        
        # Rate limit: max 5 requests per second
        sleep 0.2
        
    done
    
    log "  --- $system: $downloaded downloaded, $skipped already had artwork, $count processed ---"
}

# === MAIN ===

echo ""
echo "╔════════════════════════════════════════════╗"
echo "║   MiSTer Artwork Scraper (SteamGridDB)     ║"
echo "╚════════════════════════════════════════════╝"
echo ""

# Check API key
if [ "$API_KEY" = "YOUR_API_KEY_HERE" ] || [ -z "$API_KEY" ]; then
    echo "ERROR: Please provide your SteamGridDB API key!"
    echo ""
    echo "1. Go to: https://www.steamgriddb.com/profile/preferences/api"
    echo "2. Create a free account and generate an API key"
    echo "3. Run: ./scrape_artwork.sh YOUR_API_KEY"
    echo ""
    exit 1
fi

# Test API key
log "Testing API key..."
test_response=$(curl -sk -H "Authorization: Bearer $API_KEY" \
    "https://www.steamgriddb.com/api/v2/search/autocomplete/mario")

if echo "$test_response" | grep -q '"success":false'; then
    echo "ERROR: Invalid API key!"
    exit 1
fi

log "API key valid ✓"
log ""

# Process systems
if [ -n "$TARGET_SYSTEM" ]; then
    process_system "$TARGET_SYSTEM"
else
    # Process all systems with games
    for system_dir in "$GAMES_PATH"/*/; do
        system=$(basename "$system_dir")
        process_system "$system"
    done
fi

log ""
log "=== Scraping complete! ==="
log "Artwork saved to: $MEDIA_PATH/{system}/boxart/"
log ""

