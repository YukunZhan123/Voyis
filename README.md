# Distributed Imaging System

A distributed image processing system written in modern C++ that generates, processes, and stores image data using Inter-Process Communication (IPC).

## Overview

This system consists of three separate applications that communicate via ZeroMQ:

1. **Image Generator** - Reads images from disk and publishes them continuously
2. **Feature Extractor** - Receives images, extracts SIFT features using OpenCV, and publishes results
3. **Data Logger** - Receives processed data and stores it in SQLite database

## Architecture

```
┌─────────────────┐      ┌──────────────────┐      ┌─────────────────┐
│     App 1       │      │      App 2       │      │     App 3       │
│ Image Generator │─────>│Feature Extractor │─────>│  Data Logger    │
│                 │ IPC  │    (OpenCV)      │ IPC  │   (SQLite)      │
└─────────────────┘      └──────────────────┘      └─────────────────┘
  tcp://*:5555           tcp://localhost:5555      tcp://localhost:5556
                         tcp://*:5556
```

### Key Features

- **Loose Coupling**: Apps can start in any order and survive if others crash/restart
- **ZeroMQ IPC**: Reliable pub-sub messaging with automatic reconnection
- **SIFT Feature Detection**: Scale-invariant feature extraction using OpenCV
- **SQLite Storage**: Persistent storage with transaction support
- **Large Image Support**: Handles images from KB to 30MB+
- **Graceful Shutdown**: Clean termination on SIGINT/SIGTERM

## Prerequisites

### Ubuntu/Debian

```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    libzmq3-dev \
    libopencv-dev \
    libsqlite3-dev \
    libgtest-dev \
    pkg-config
```

### Arch Linux

```bash
sudo pacman -S base-devel cmake zeromq opencv sqlite gtest
```

### macOS

```bash
brew install cmake zeromq opencv sqlite3 googletest
```

## Building

### Quick Build

```bash
./scripts/build.sh
```

### Build Options

```bash
# Debug build
./scripts/build.sh --debug

# Clean build (removes build directory)
./scripts/build.sh --clean

# Build without tests
./scripts/build.sh --no-tests
```

### Manual Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
ctest --output-on-failure  # Run tests
```

## Running

### Prerequisites

Before running, you need a folder with test images:

```bash
mkdir -p test_images
# Add some test images (jpg, png, bmp, tiff)
```

You can download sample images from:
- [ImageNet Sample](http://www.image-net.org/)
- [COCO Dataset](https://cocodataset.org/)
- Or use your own images

### Quick Start (All Apps)

```bash
./scripts/run_all.sh
```

This starts all three applications with default settings. Press `Ctrl+C` to stop all apps.

### Custom Configuration

```bash
# Specify custom image folder and database
./scripts/run_all.sh --images /path/to/images --database ./my_data.db

# Adjust delay between images (milliseconds)
./scripts/run_all.sh --delay 1000
```

### Running Apps Individually

#### App 1: Image Generator

```bash
./build/apps/image_generator/image_generator /path/to/images
```

Options:
- `--endpoint <endpoint>`: Publisher endpoint (default: `tcp://*:5555`)
- `--delay <ms>`: Delay between images in milliseconds (default: `100`)

Example:
```bash
./build/apps/image_generator/image_generator ./test_images --delay 500
```

#### App 2: Feature Extractor

```bash
./build/apps/feature_extractor/feature_extractor
```

Options:
- `--sub-endpoint <endpoint>`: Subscriber endpoint (default: `tcp://localhost:5555`)
- `--pub-endpoint <endpoint>`: Publisher endpoint (default: `tcp://*:5556`)
- `--timeout <ms>`: Receive timeout (default: `5000`)

Example:
```bash
./build/apps/feature_extractor/feature_extractor \
    --sub-endpoint tcp://localhost:5555 \
    --pub-endpoint tcp://*:5556
```

#### App 3: Data Logger

```bash
./build/apps/data_logger/data_logger
```

Options:
- `--endpoint <endpoint>`: Subscriber endpoint (default: `tcp://localhost:5556`)
- `--database <path>`: Database file path (default: `./imaging_data.db`)
- `--timeout <ms>`: Receive timeout (default: `5000`)

Example:
```bash
./build/apps/data_logger/data_logger \
    --endpoint tcp://localhost:5556 \
    --database ./my_data.db
```

## Testing

### Run All Tests

```bash
cd build
ctest --output-on-failure
```

### Run Specific Tests

```bash
# Serialization tests
./build/tests/test_serialization

# IPC tests
./build/tests/test_ipc

# Integration tests
./build/tests/test_integration
```

## Database Schema

The SQLite database contains three tables:

### `images` table
```sql
CREATE TABLE images (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    filename TEXT NOT NULL,
    timestamp INTEGER NOT NULL,
    width INTEGER NOT NULL,
    height INTEGER NOT NULL,
    channels INTEGER NOT NULL,
    image_data BLOB NOT NULL,
    data_size INTEGER NOT NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);
```

### `keypoints` table
```sql
CREATE TABLE keypoints (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    image_id INTEGER NOT NULL,
    x REAL NOT NULL,
    y REAL NOT NULL,
    size REAL NOT NULL,
    angle REAL NOT NULL,
    response REAL NOT NULL,
    octave INTEGER NOT NULL,
    class_id INTEGER NOT NULL,
    FOREIGN KEY(image_id) REFERENCES images(id)
);
```

### `descriptors` table
```sql
CREATE TABLE descriptors (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    image_id INTEGER NOT NULL,
    descriptor_data BLOB NOT NULL,
    FOREIGN KEY(image_id) REFERENCES images(id)
);
```

## Querying the Database

```bash
# Open database
sqlite3 imaging_data.db

# Show all tables
.tables

# Count images
SELECT COUNT(*) FROM images;

# Show recent images with keypoint counts
SELECT
    i.filename,
    i.width,
    i.height,
    COUNT(k.id) as keypoint_count
FROM images i
LEFT JOIN keypoints k ON i.id = k.image_id
GROUP BY i.id
ORDER BY i.created_at DESC
LIMIT 10;

# Show average keypoints per image
SELECT AVG(kp_count) as avg_keypoints
FROM (
    SELECT COUNT(*) as kp_count
    FROM keypoints
    GROUP BY image_id
);
```

## Troubleshooting

### "Address already in use" error

If you see ZeroMQ binding errors, another process may be using the port:

```bash
# Check what's using port 5555
sudo lsof -i :5555

# Kill the process or use different ports
./build/apps/image_generator/image_generator ./test_images --endpoint tcp://*:5557
./build/apps/feature_extractor/feature_extractor --sub-endpoint tcp://localhost:5557
```

### SIFT not available in OpenCV

Some OpenCV distributions don't include SIFT (patent-related). Solutions:

1. Build OpenCV with contrib modules:
```bash
git clone https://github.com/opencv/opencv.git
git clone https://github.com/opencv/opencv_contrib.git
cd opencv && mkdir build && cd build
cmake -DOPENCV_EXTRA_MODULES_PATH=../../opencv_contrib/modules ..
make -j$(nproc) && sudo make install
```

2. Or use ORB features instead (modify feature_extractor.cpp)

### Apps not receiving data

1. Check if all apps are running: `ps aux | grep -E "(image_generator|feature_extractor|data_logger)"`
2. Verify endpoints match between publisher/subscriber
3. Check firewall: `sudo ufw status`
4. Increase timeout values

### Build errors

```bash
# Clean build
rm -rf build
./scripts/build.sh --clean

# Check dependencies
pkg-config --modversion libzmq
pkg-config --modversion opencv4
```

## Performance Tips

- **Large images**: Increase ZeroMQ buffer sizes or add compression
- **High throughput**: Reduce delay in Image Generator
- **Database**: Use SSD storage, enable WAL mode (already enabled)
- **Multiple subscribers**: ZeroMQ pub-sub naturally supports this

## Project Structure

```
.
├── CMakeLists.txt              # Root build configuration
├── README.md                   # This file
├── apps/
│   ├── image_generator/        # App 1
│   ├── feature_extractor/      # App 2
│   └── data_logger/            # App 3
├── common/                     # Shared library
│   ├── include/
│   │   ├── ipc_manager.hpp
│   │   ├── message_types.hpp
│   │   └── serialization.hpp
│   └── src/
├── tests/                      # Unit and integration tests
│   ├── unit/
│   └── integration/
├── scripts/                    # Build and run scripts
│   ├── build.sh
│   └── run_all.sh
└── test_images/                # Sample images (not in repo)
```

## Design Decisions

### Why ZeroMQ?
- Automatic reconnection handling
- Built-in pub-sub pattern
- Language agnostic (could extend to Python, etc.)
- Handles large messages efficiently

### Why SQLite?
- No separate server process needed
- ACID compliance
- Good for read-heavy workloads
- Easy to backup (single file)

### Why SIFT?
- Scale and rotation invariant
- Well-tested in computer vision
- Good descriptor quality
- Standard in industry

## License

This project was created as part of a technical assessment for Voyis Imaging Inc.

## Contact

For questions or issues, please contact the development team.
