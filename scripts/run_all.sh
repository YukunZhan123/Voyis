#!/bin/bash

# Script to run all three applications
# This starts each app in a separate terminal or as background processes

set -e

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m'

# Get script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"

# Check if build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${RED}Build directory not found. Please run build.sh first.${NC}"
    exit 1
fi

# Default parameters
IMAGE_FOLDER="$PROJECT_ROOT/test_images"
DATABASE="$PROJECT_ROOT/imaging_data.db"
DELAY_MS=500

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --images)
            IMAGE_FOLDER="$2"
            shift 2
            ;;
        --database)
            DATABASE="$2"
            shift 2
            ;;
        --delay)
            DELAY_MS="$2"
            shift 2
            ;;
        --help)
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  --images <folder>    Path to image folder (default: ./test_images)"
            echo "  --database <path>    Database file path (default: ./imaging_data.db)"
            echo "  --delay <ms>         Delay between images in ms (default: 500)"
            echo "  --help               Show this help message"
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            exit 1
            ;;
    esac
done

# Check if image folder exists
if [ ! -d "$IMAGE_FOLDER" ]; then
    echo -e "${RED}Image folder not found: $IMAGE_FOLDER${NC}"
    echo "Please create the folder and add some test images, or specify a different folder with --images"
    exit 1
fi

# Check if there are images in the folder
IMAGE_COUNT=$(find "$IMAGE_FOLDER" -type f \( -iname "*.jpg" -o -iname "*.jpeg" -o -iname "*.png" -o -iname "*.bmp" \) | wc -l)
if [ "$IMAGE_COUNT" -eq 0 ]; then
    echo -e "${RED}No images found in $IMAGE_FOLDER${NC}"
    exit 1
fi

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Starting Distributed Imaging System${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo -e "${BLUE}Configuration:${NC}"
echo "  Image folder: $IMAGE_FOLDER ($IMAGE_COUNT images)"
echo "  Database: $DATABASE"
echo "  Delay: ${DELAY_MS}ms"
echo ""

# PIDs for cleanup
PIDS=()

# Cleanup function
cleanup() {
    echo ""
    echo -e "${BLUE}Shutting down applications...${NC}"
    for pid in "${PIDS[@]}"; do
        if kill -0 "$pid" 2>/dev/null; then
            kill -SIGTERM "$pid" 2>/dev/null || true
        fi
    done
    wait
    echo -e "${GREEN}All applications stopped${NC}"
}

trap cleanup EXIT INT TERM

# Start Data Logger (App 3) first
echo -e "${BLUE}Starting Data Logger...${NC}"
"$BUILD_DIR/apps/data_logger/data_logger" \
    --endpoint "tcp://localhost:5556" \
    --database "$DATABASE" &
PIDS+=($!)
sleep 1

# Start Feature Extractor (App 2)
echo -e "${BLUE}Starting Feature Extractor...${NC}"
"$BUILD_DIR/apps/feature_extractor/feature_extractor" \
    --sub-endpoint "tcp://localhost:5555" \
    --pub-endpoint "tcp://*:5556" &
PIDS+=($!)
sleep 1

# Start Image Generator (App 1)
echo -e "${BLUE}Starting Image Generator...${NC}"
"$BUILD_DIR/apps/image_generator/image_generator" \
    "$IMAGE_FOLDER" \
    --endpoint "tcp://*:5555" \
    --delay "$DELAY_MS" &
PIDS+=($!)

echo ""
echo -e "${GREEN}All applications started!${NC}"
echo "Press Ctrl+C to stop all applications"
echo ""

# Wait for all processes
wait
