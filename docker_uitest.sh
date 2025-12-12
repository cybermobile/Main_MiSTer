#!/bin/bash

# MiSTer UI Test - Docker Script
# Builds and runs the UI preview generator to test graphical menu rendering

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
IMAGE_NAME="mister-uitest"
OUTPUT_DIR="$SCRIPT_DIR/ui_preview"

echo "=== MiSTer UI Preview Test ==="
echo ""

# Check if Docker is running
if ! docker info >/dev/null 2>&1; then
    echo "Error: Docker is not running. Please start Docker."
    exit 1
fi

# Build Docker image if needed
if [[ "$1" == "--rebuild" ]] || ! docker image inspect "$IMAGE_NAME" >/dev/null 2>&1; then
    echo "Building Docker image for UI testing..."
    docker build --platform linux/amd64 -t "$IMAGE_NAME" -f "$SCRIPT_DIR/Dockerfile.uitest" "$SCRIPT_DIR"
    echo ""
fi

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Run the UI test in Docker
echo "Building and running UI preview generator..."
docker run --rm --platform linux/amd64 \
    -v "$SCRIPT_DIR:/src" \
    -w /src \
    "$IMAGE_NAME" \
    bash -c '
        echo "Compiling UI test utility..."

        # Compile required source files
        g++ -c -std=c++11 -I. -I./lib -DVDATE="\"test\"" \
            gfx_menu.cpp -o /tmp/gfx_menu.o 2>&1 || { echo "Failed to compile gfx_menu.cpp"; exit 1; }

        g++ -c -std=c++11 -I. -I./lib -DVDATE="\"test\"" \
            theme.cpp -o /tmp/theme.o 2>&1 || { echo "Failed to compile theme.cpp"; exit 1; }

        g++ -c -std=c++11 -I. -I./lib -DVDATE="\"test\"" \
            test_ui_preview.cpp -o /tmp/test_ui_preview.o 2>&1 || { echo "Failed to compile test_ui_preview.cpp"; exit 1; }

        # Link
        echo "Linking..."
        g++ -o /tmp/test_ui_preview \
            /tmp/test_ui_preview.o \
            /tmp/gfx_menu.o \
            /tmp/theme.o \
            $(pkg-config --libs imlib2) -lpng -ljpeg 2>&1 || { echo "Linking failed"; exit 1; }

        # Run the test
        echo ""
        echo "Running UI preview generator..."
        /tmp/test_ui_preview ui_preview/preview.png

        echo ""
        echo "Generated files:"
        ls -la ui_preview/*.png 2>/dev/null || echo "No PNG files generated"
    '

# Check results
if [[ -f "$OUTPUT_DIR/preview.png" ]]; then
    echo ""
    echo "=== Success! ==="
    echo "Preview images generated in: $OUTPUT_DIR/"
    ls -la "$OUTPUT_DIR"/*.png
    echo ""
    echo "Open the PNG files to see the UI preview."
else
    echo ""
    echo "=== Test completed ==="
    echo "Check $OUTPUT_DIR/ for output files."
fi
