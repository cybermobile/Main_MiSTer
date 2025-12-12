#!/bin/bash
# MiSTer Artwork Folder Setup Script
# Creates the folder structure for game artwork
# Place downloaded images from SteamGridDB into these folders

MEDIA_PATH="/media/fat/media"

# Common MiSTer cores - add more as needed
CORES=(
    "Arcade"
    "NES"
    "SNES"
    "Genesis"
    "MegaCD"
    "SMS"
    "GameGear"
    "TurboGrafx16"
    "TGFX16-CD"
    "GBA"
    "GB"
    "GBC"
    "PSX"
    "N64"
    "Atari2600"
    "Atari5200"
    "Atari7800"
    "ColecoVision"
    "Intellivision"
    "NeoGeo"
    "Amiga"
    "C64"
    "AtariST"
    "MSX"
    "ao486"
)

# Artwork types
TYPES=("boxart" "snap" "title" "marquee" "wheel")

echo "Creating MiSTer artwork folder structure..."
echo "Base path: $MEDIA_PATH"
echo ""

# Create base media folder
mkdir -p "$MEDIA_PATH"

# Create folder structure for each core
for core in "${CORES[@]}"; do
    echo "Creating folders for: $core"
    for type in "${TYPES[@]}"; do
        mkdir -p "$MEDIA_PATH/$core/$type"
    done
done

echo ""
echo "✓ Folder structure created!"
echo ""
echo "=== How to add artwork ==="
echo ""
echo "1. Go to https://www.steamgriddb.com"
echo "2. Search for your game"
echo "3. Download the image you want"
echo "4. Rename it to match your ROM filename (without extension)"
echo "   Example: 'Super Mario World.png' for 'Super Mario World.sfc'"
echo ""
echo "5. Place images in the correct folder:"
echo "   - Boxart (cover):  $MEDIA_PATH/{Core}/boxart/"
echo "   - Screenshots:     $MEDIA_PATH/{Core}/snap/"
echo "   - Title screens:   $MEDIA_PATH/{Core}/title/"
echo "   - Marquees:        $MEDIA_PATH/{Core}/marquee/"
echo "   - Wheel logos:     $MEDIA_PATH/{Core}/wheel/"
echo ""
echo "=== Recommended image sizes ==="
echo "   Boxart:   600x900 px (vertical) or 460x215 px (horizontal)"
echo "   Snap:     320x240 or 640x480 px"
echo "   Title:    320x240 px"
echo "   Marquee:  400x150 px"
echo "   Wheel:    400x150 px (transparent PNG preferred)"
echo ""

