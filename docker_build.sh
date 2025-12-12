#!/bin/bash

# MiSTer Docker Build Script
# Builds MiSTer binary using Docker for cross-compilation

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
IMAGE_NAME="mister-builder"
TODAY=$(date +"%Y%m%d")

echo "=== MiSTer Docker Build ==="
echo ""

# Check if Docker is running
if ! docker info >/dev/null 2>&1; then
    echo "Error: Docker is not running. Please start Docker Desktop."
    exit 1
fi

# Build Docker image if it doesn't exist or if --rebuild is passed
if [[ "$1" == "--rebuild" ]] || ! docker image inspect "$IMAGE_NAME" >/dev/null 2>&1; then
    echo "Building Docker image (this may take a few minutes on first run)..."
    docker build --platform linux/amd64 -t "$IMAGE_NAME" "$SCRIPT_DIR"
    echo ""
fi

# Clean previous build
echo "Cleaning previous build..."
rm -rf "$SCRIPT_DIR/bin"

# Run the build in Docker
echo "Building MiSTer..."
docker run --rm --platform linux/amd64 \
    -v "$SCRIPT_DIR:/src" \
    -w /src \
    "$IMAGE_NAME" \
    make -j$(sysctl -n hw.ncpu 2>/dev/null || echo 4)

# Check if build succeeded
if [[ -f "$SCRIPT_DIR/bin/MiSTer" ]]; then
    echo ""
    echo "Build successful!"
    echo ""
    
    # Show file info
    file "$SCRIPT_DIR/bin/MiSTer"
    ls -lh "$SCRIPT_DIR/bin/MiSTer"
    echo ""
    
    # Ask about creating release
    RELEASE_NAME="MiSTer_$TODAY"
    RELEASE_PATH="$SCRIPT_DIR/releases/$RELEASE_NAME"
    
    echo "To create a release, run:"
    echo "  cp bin/MiSTer releases/$RELEASE_NAME"
    echo ""
    
    # If --release flag is passed, auto-create release
    if [[ "$1" == "--release" ]] || [[ "$2" == "--release" ]]; then
        cp "$SCRIPT_DIR/bin/MiSTer" "$RELEASE_PATH"
        echo "Release created: releases/$RELEASE_NAME"
        ls -lh "$RELEASE_PATH"
    fi
else
    echo "Build failed!"
    exit 1
fi

