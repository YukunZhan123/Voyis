#!/bin/bash

# Script to generate sample test images using ImageMagick

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
OUTPUT_DIR="$PROJECT_ROOT/test_images"

# Check if ImageMagick is installed
if ! command -v convert &> /dev/null; then
    echo "ImageMagick is not installed. Installing..."
    if command -v apt-get &> /dev/null; then
        sudo apt-get install -y imagemagick
    elif command -v yum &> /dev/null; then
        sudo yum install -y ImageMagick
    elif command -v pacman &> /dev/null; then
        sudo pacman -S imagemagick
    elif command -v brew &> /dev/null; then
        brew install imagemagick
    else
        echo "Please install ImageMagick manually"
        exit 1
    fi
fi

# Create output directory
mkdir -p "$OUTPUT_DIR"

echo "Generating test images in $OUTPUT_DIR"

# Generate various sized test images
echo "Creating small image (640x480)..."
convert -size 640x480 plasma:fractal "$OUTPUT_DIR/test_small_640x480.jpg"

echo "Creating medium image (1920x1080)..."
convert -size 1920x1080 plasma:fractal "$OUTPUT_DIR/test_medium_1920x1080.jpg"

echo "Creating large image (3840x2160)..."
convert -size 3840x2160 plasma:fractal "$OUTPUT_DIR/test_large_3840x2160.jpg"

echo "Creating gradient image..."
convert -size 1024x768 gradient:blue-yellow "$OUTPUT_DIR/test_gradient.png"

echo "Creating checkerboard pattern..."
convert -size 800x600 pattern:checkerboard "$OUTPUT_DIR/test_checkerboard.png"

echo "Creating text image with features..."
convert -size 1280x720 xc:white \
    -pointsize 72 -fill black \
    -draw "text 100,200 'VOYIS Imaging'" \
    -draw "text 100,300 'Test Image'" \
    -draw "circle 640,360 640,460" \
    -draw "rectangle 200,400 600,500" \
    "$OUTPUT_DIR/test_features.jpg"

echo "Creating noise image..."
convert -size 1024x768 xc: +noise Random "$OUTPUT_DIR/test_noise.png"

echo "Creating very large image (30MB+)..."
convert -size 7680x4320 plasma:fractal -quality 100 "$OUTPUT_DIR/test_xlarge_7680x4320.jpg"

echo ""
echo "Generated $(ls -1 "$OUTPUT_DIR" | wc -l) test images"
echo ""
echo "Image sizes:"
du -h "$OUTPUT_DIR"/*.{jpg,png} 2>/dev/null | sort -h

echo ""
echo "Total size: $(du -sh "$OUTPUT_DIR" | cut -f1)"
