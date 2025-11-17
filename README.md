# Distributed Imaging Services

A distributed image processing system written in modern C++ that generates, processes, and stores image data using SIFT feature extraction.

## Overview

This project implements a three-component pipeline for distributed image processing:

1. **Image Generator**: Reads images from disk and publishes them via IPC
2. **Feature Extractor**: Receives images, performs SIFT feature detection using OpenCV, and publishes results
3. **Data Logger**: Receives processed data and stores it in a SQLite database

The applications communicate via ZeroMQ publish-subscribe messaging and can run independently, start in any order, and gracefully handle each other's lifecycle.

## Architecture

```
┌─────────────────┐         ┌─────────────────┐         ┌─────────────────┐
│ Image Generator │ ──IPC──>│Feature Extractor│ ──IPC──>│  Data Logger    │
│  (Port 5555)    │         │  (Port 5556)    │         │   (SQLite)      │
└─────────────────┘         └─────────────────┘         └─────────────────┘
```

- **IPC Mechanism**: ZeroMQ (pub-sub pattern)
- **Image Processing**: OpenCV SIFT
- **Database**: SQLite3
- **Build System**: CMake
- **C++ Standard**: C++17

## Requirements

### System Dependencies

The following packages must be installed on your Linux system:

```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    libzmq3-dev \
    libopencv-dev \
    libsqlite3-dev \
    pkg-config

# Fedora/RHEL
sudo dnf install -y \
    gcc-c++ \
    cmake \
    zeromq-devel \
    opencv-devel \
    sqlite-devel \
    pkg-config

# Arch Linux
sudo pacman -S \
    base-devel \
    cmake \
    zeromq \
    opencv \
    sqlite \
    pkg-config
```

### Required Versions
- CMake >= 3.14
- GCC >= 7.0 or Clang >= 6.0 (C++17 support)
- ZeroMQ >= 4.0
- OpenCV >= 4.0 (with contrib modules for SIFT)
- SQLite >= 3.0

## Building the Project

### 1. Clone the Repository

```bash
git clone <repository-url>
cd Voyis
```

### 2. Create Build Directory

```bash
mkdir build
cd build
```

### 3. Configure with CMake

```bash
cmake ..
```

If you encounter any issues with OpenCV SIFT, ensure OpenCV was built with contrib modules:

```bash
cmake -DOPENCV_ENABLE_NONFREE=ON ..
```

### 4. Build All Applications

```bash
make -j$(nproc)
```

This will create three executables in `build/bin/`:
- `image_generator`
- `feature_extractor`
- `data_logger`

### 5. (Optional) Run Tests

```bash
make test
# Or for verbose output:
ctest --verbose
```

## Running the Applications

The applications should be started in separate terminal windows/sessions. They can start in any order and will automatically connect when all components are running.

### Preparing Test Data

First, create a directory with some test images:

```bash
mkdir -p ~/test_images
# Copy some JPG/PNG images to ~/test_images
```

You can download sample images from public datasets like:
- [COCO Dataset](https://cocodataset.org/)
- [ImageNet Sample](http://www.image-net.org/)
- [Unsplash](https://unsplash.com/)

### Terminal 1: Start Data Logger

```bash
cd build/bin
./data_logger [database_path]
```

Example:
```bash
./data_logger ./image_data.db
```

**Output**: You'll see connection status and waiting message.

### Terminal 2: Start Feature Extractor

```bash
cd build/bin
./feature_extractor
```

**Output**: The application will connect to both the Image Generator (input) and Data Logger (output).

### Terminal 3: Start Image Generator

```bash
cd build/bin
./image_generator <path_to_image_directory>
```

Example:
```bash
./image_generator ~/test_images
```

**Output**: The application will start publishing images from the specified directory in a continuous loop.

### Observing the Pipeline

Once all three applications are running, you should see:

- **Image Generator**: Logs each image being published
- **Feature Extractor**: Logs received images, SIFT keypoint count, and processing time
- **Data Logger**: Logs stored images and database statistics

### Stopping the Applications

Press `Ctrl+C` in each terminal to gracefully shut down each application. The applications handle shutdown signals properly and will exit cleanly.

## Application Details

### Image Generator

**Purpose**: Simulate a camera data source by reading images from disk.

**Command Line**:
```bash
./image_generator <image_directory>
```

**Behavior**:
- Scans directory for image files (jpg, jpeg, png, bmp, tiff)
- Continuously loops through images, publishing each via ZeroMQ
- Handles images from few KB to >30MB
- Publishes to `tcp://*:5555`

### Feature Extractor

**Purpose**: Process images using SIFT feature detection.

**Command Line**:
```bash
./feature_extractor
```

**Behavior**:
- Subscribes to images from `tcp://localhost:5555`
- Applies OpenCV SIFT algorithm to detect keypoints
- Computes 128-dimensional descriptors for each keypoint
- Publishes processed data to `tcp://*:5556`
- Logs processing time and keypoint count

### Data Logger

**Purpose**: Store processed images and features in a database.

**Command Line**:
```bash
./data_logger [database_path]
```

Default database path: `image_data.db`

**Behavior**:
- Subscribes to processed data from `tcp://localhost:5556`
- Stores images and SIFT features in SQLite database
- Creates two tables: `images` and `keypoints`
- Provides statistics on shutdown

**Database Schema**:

```sql
-- Images table
CREATE TABLE images (
    id INTEGER PRIMARY KEY,
    image_id TEXT,
    format TEXT,
    width INTEGER,
    height INTEGER,
    timestamp INTEGER,
    processed_timestamp INTEGER,
    num_keypoints INTEGER,
    image_data BLOB,
    created_at INTEGER
);

-- Keypoints table
CREATE TABLE keypoints (
    id INTEGER PRIMARY KEY,
    image_id INTEGER,
    x REAL,
    y REAL,
    size REAL,
    angle REAL,
    response REAL,
    octave INTEGER,
    descriptor BLOB,
    FOREIGN KEY (image_id) REFERENCES images(id)
);
```

## Querying the Database

After running the system, you can examine the stored data:

```bash
sqlite3 image_data.db
```

Example queries:

```sql
-- Count total images
SELECT COUNT(*) FROM images;

-- Count total keypoints
SELECT COUNT(*) FROM keypoints;

-- Average keypoints per image
SELECT AVG(num_keypoints) FROM images;

-- List all images with keypoint counts
SELECT image_id, width, height, num_keypoints FROM images;

-- Get keypoints for a specific image
SELECT x, y, size, angle FROM keypoints WHERE image_id = 1;
```

## Design Highlights

### Loose Coupling
- Applications communicate only via IPC (no shared memory or files)
- Each application can start/stop independently
- ZeroMQ handles automatic reconnection

### Resilience
- Applications don't crash if others exit or restart
- Graceful shutdown with `SIGINT`/`SIGTERM` handlers
- Error handling at all IPC boundaries

### Modularity
- Common library for shared IPC and serialization code
- Each application is a separate executable
- Clean separation of concerns

### Performance
- Binary serialization for efficient message passing
- ZeroMQ's high-performance messaging
- Batch database inserts with transactions
- Support for large images (>30MB)

### Testing
- Comprehensive unit tests for message serialization
- IPC communication tests
- Google Test framework

## Troubleshooting

### "Failed to bind to endpoint"
- Port might be in use. Wait a few seconds and try again.
- Or change the port in the source code (`main.cpp` files).

### "Failed to decode image"
- Ensure images are valid and in supported formats.
- Check that OpenCV was built with image codec support.

### SIFT Not Available
- OpenCV must be built with contrib modules and nonfree features.
- Rebuild OpenCV with: `-DOPENCV_ENABLE_NONFREE=ON -DOPENCV_EXTRA_MODULES_PATH=<path-to-opencv_contrib>`

### Slow Joiner Problem
- The Image Generator includes a small delay after binding to allow subscribers to connect.
- If subscribers miss initial messages, they'll catch up with subsequent messages.

### Build Errors
- Ensure all dependencies are installed (see Requirements section).
- Check CMake output for missing packages.
- Verify C++17 compiler support.

## Project Structure

```
Voyis/
├── CMakeLists.txt              # Root build configuration
├── README.md                   # This file
├── .gitignore                  # Git ignore rules
│
├── include/                    # Public headers
│   ├── message.h               # Message structures and serialization
│   └── ipc.h                   # ZeroMQ wrapper for pub-sub
│
├── src/
│   ├── common/                 # Shared library
│   │   ├── CMakeLists.txt
│   │   ├── message.cpp         # Message serialization implementation
│   │   └── ipc.cpp             # IPC implementation
│   │
│   ├── image_generator/        # App 1
│   │   ├── CMakeLists.txt
│   │   └── main.cpp
│   │
│   ├── feature_extractor/      # App 2
│   │   ├── CMakeLists.txt
│   │   └── main.cpp
│   │
│   └── data_logger/            # App 3
│       ├── CMakeLists.txt
│       └── main.cpp
│
├── tests/                      # Unit tests
│   ├── CMakeLists.txt
│   ├── test_message.cpp        # Message serialization tests
│   └── test_ipc.cpp            # IPC communication tests
│
└── docs/                       # Documentation
    └── DESIGN.md               # Design document
```

## Performance Considerations

- **Throughput**: The system can process ~10-30 images per second depending on image size and SIFT feature count.
- **Memory**: Each application has modest memory requirements (<100MB for typical images).
- **Scalability**: Could be extended to multiple feature extractors for parallel processing.

## Future Enhancements

- Multi-threaded feature extraction
- Additional feature detection algorithms (ORB, SURF)
- REST API for querying database
- Configuration files for endpoints and parameters
- Performance metrics and monitoring
- Docker containerization

## License

This project is provided as-is for evaluation purposes.

## Contact

For questions or clarifications, please contact via email during the project period.
