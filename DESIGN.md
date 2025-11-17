# Distributed Imaging System - Design Document

## 1. Executive Summary

This document describes the architecture and design decisions for the Distributed Imaging System, a three-application pipeline for processing images using Inter-Process Communication (IPC).

**System Goals:**
- Reliable distributed image processing
- Loose coupling between components
- Fault tolerance (apps can crash/restart independently)
- Support for large images (30MB+)
- Scalable architecture

## 2. System Architecture

### 2.1 High-Level Architecture

```
┌──────────────────────────────────────────────────────────────────┐
│                     Distributed Imaging System                    │
└──────────────────────────────────────────────────────────────────┘

┌─────────────────┐      ┌──────────────────┐      ┌─────────────────┐
│   Process 1     │      │    Process 2     │      │   Process 3     │
│                 │      │                  │      │                 │
│  Image          │──────│   Feature        │──────│   Data          │
│  Generator      │ ZMQ  │   Extractor      │ ZMQ  │   Logger        │
│                 │      │                  │      │                 │
│  • Reads images │      │  • Decodes imgs  │      │  • Stores data  │
│  • Publishes    │      │  • SIFT extract  │      │  • SQLite DB    │
│  • Loops ∞      │      │  • Re-publishes  │      │  • Persistence  │
└─────────────────┘      └──────────────────┘      └─────────────────┘
       ↓                        ↓                          ↓
  tcp://*:5555           tcp://*:5556                filesystem
                                                    (imaging_data.db)
```

### 2.2 Component Details

#### App 1: Image Generator
- **Purpose**: Simulate a camera/image source
- **Inputs**: Folder path containing images
- **Outputs**: Binary image data via ZeroMQ
- **Behavior**: Continuous loop, publishes indefinitely
- **Technology**: C++17, std::filesystem, ZeroMQ

#### App 2: Feature Extractor
- **Purpose**: Extract computer vision features from images
- **Inputs**: Binary image data from App 1
- **Processing**: OpenCV decoding + SIFT feature extraction
- **Outputs**: Image + keypoints + descriptors via ZeroMQ
- **Technology**: C++17, OpenCV 4.x, ZeroMQ

#### App 3: Data Logger
- **Purpose**: Persistent storage of processed data
- **Inputs**: Processed images with features from App 2
- **Processing**: Database insertion with transactions
- **Outputs**: SQLite database file
- **Technology**: C++17, SQLite3, ZeroMQ

## 3. Communication Protocol

### 3.1 IPC Mechanism: ZeroMQ

**Choice Rationale:**
- Automatic connection management
- Built-in pub-sub pattern
- Message queuing and buffering
- Language-agnostic (future extensibility)
- Battle-tested in production systems

**Topology:**
```
Publisher (App 1) → Subscriber (App 2)
Publisher (App 2) → Subscriber (App 3)
```

**Advantages:**
- Loose coupling: apps don't need to know about each other
- Resilient: automatic reconnection on failure
- Start-order independent: subscribers connect when available
- Multiple subscribers: can add monitoring/logging apps easily

### 3.2 Message Format

All messages use a binary protocol for efficiency:

#### Message Header (64 bytes, fixed)
```cpp
struct MessageHeader {
    uint32_t magic;           // 0xDEADBEEF (validation)
    MessageType type;         // IMAGE or IMAGE_WITH_FEATURES
    uint64_t payload_size;    // Total payload bytes
    uint64_t timestamp;       // Unix timestamp (ms)
    uint32_t width;           // Image width
    uint32_t height;          // Image height
    uint32_t channels;        // Number of channels (1=gray, 3=RGB)
    uint32_t filename_length; // Filename string length
    uint8_t reserved[24];     // Future use
};
```

#### IMAGE Message Format
```
[MessageHeader: 64 bytes]
[Filename: variable]
[Image data: variable]
```

#### IMAGE_WITH_FEATURES Message Format
```
[MessageHeader: 64 bytes]
[Filename: variable]
[Image data: variable]
[Keypoint count: 4 bytes]
[Keypoints: variable]
[Descriptor count: 4 bytes]
[Descriptors: variable]
```

**Design Decisions:**
- Fixed header size simplifies parsing
- Magic number prevents garbage data
- Binary format is efficient for large images
- Metadata in header enables quick filtering

## 4. Data Structures

### 4.1 Core Types

```cpp
namespace dis {
    struct ImageData {
        std::string filename;
        uint32_t width, height, channels;
        uint64_t timestamp;
        std::vector<uint8_t> data;
    };

    struct Keypoint {
        float x, y, size, angle, response;
        int32_t octave, class_id;
    };

    struct ImageWithFeatures {
        ImageData image;
        std::vector<Keypoint> keypoints;
        std::vector<float> descriptors;  // 128 floats per keypoint
    };
}
```

### 4.2 Database Schema

```sql
-- Images table: stores original image data
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

-- Keypoints table: stores SIFT keypoints
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
    FOREIGN KEY(image_id) REFERENCES images(id) ON DELETE CASCADE
);

-- Descriptors table: stores SIFT descriptors (BLOB)
CREATE TABLE descriptors (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    image_id INTEGER NOT NULL,
    descriptor_data BLOB NOT NULL,
    FOREIGN KEY(image_id) REFERENCES images(id) ON DELETE CASCADE
);
```

**Normalization:**
- 3NF (Third Normal Form)
- Keypoints normalized separately for queries
- Descriptors as BLOB (128 floats × N keypoints)

**Indices:**
```sql
CREATE INDEX idx_keypoints_image_id ON keypoints(image_id);
CREATE INDEX idx_descriptors_image_id ON descriptors(image_id);
CREATE INDEX idx_images_filename ON images(filename);
CREATE INDEX idx_images_timestamp ON images(timestamp);
```

## 5. Error Handling & Resilience

### 5.1 Fault Tolerance

**Loose Coupling Strategy:**
1. Each app is a separate process
2. No shared memory or direct dependencies
3. Apps can start in any order
4. Automatic reconnection via ZeroMQ

**Failure Scenarios:**

| Scenario | Behavior |
|----------|----------|
| App 1 crashes | Apps 2 & 3 wait for reconnection, continue when App 1 restarts |
| App 2 crashes | App 1 continues publishing (ZMQ buffers), App 3 waits |
| App 3 crashes | Apps 1 & 2 continue processing, data resumes when App 3 restarts |
| Late start | Apps connect when available, no data loss (within buffer limits) |

### 5.2 Error Handling Patterns

```cpp
// App 1: Skip corrupted images, log, continue
try {
    publish_image(path);
} catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    // Continue with next image
}

// App 2: Log errors, continue processing
try {
    process_image(image);
} catch (const cv::Exception& e) {
    std::cerr << "OpenCV error: " << e.what() << std::endl;
    // Continue waiting for next image
}

// App 3: Rollback transaction on error
try {
    begin_transaction();
    store_data(data);
    commit_transaction();
} catch (const std::exception& e) {
    rollback_transaction();
    std::cerr << "Database error: " << e.what() << std::endl;
}
```

## 6. Performance Considerations

### 6.1 Throughput

**Bottlenecks:**
1. **SIFT extraction**: ~50-200ms per image (depends on resolution)
2. **Database writes**: ~10-50ms per image (with transaction)
3. **Network**: Minimal (localhost, shared memory transport)

**Optimization Strategies:**
- Binary serialization (no JSON overhead)
- ZeroMQ zero-copy where possible
- SQLite WAL mode for concurrent reads
- Transaction batching in Data Logger

### 6.2 Memory Management

**Large Image Handling:**
- Stream processing (no accumulation)
- RAII for automatic cleanup
- Smart pointers for ownership
- std::vector for dynamic sizing

**Memory Profile:**
- App 1: ~1 image in memory at a time
- App 2: ~1 image + cv::Mat + features
- App 3: ~1 image + features during transaction

## 7. Testing Strategy

### 7.1 Unit Tests

**Coverage:**
- Serialization/deserialization (all message types)
- IPC send/receive (timeout, errors)
- Database operations (CRUD, transactions)
- Error handling (invalid data, corruption)

**Framework:** Google Test

### 7.2 Integration Tests

**Scenarios:**
1. Full pipeline with small images
2. Large image handling (30MB+)
3. App restart during processing
4. Multiple subscribers
5. High throughput stress test

### 7.3 Test Data

**Requirements:**
- Various sizes: 100KB, 1MB, 10MB, 30MB
- Various formats: JPG, PNG, BMP
- Edge cases: 1x1 pixel, 10000x10000 pixels
- Corrupted images

## 8. Design Trade-offs

### 8.1 Decisions Made

| Decision | Rationale | Trade-off |
|----------|-----------|-----------|
| ZeroMQ | Mature, reliable, feature-rich | Learning curve, dependency |
| SQLite | Serverless, simple, portable | Limited concurrency vs. PostgreSQL |
| SIFT | High-quality features, standard | Slower than ORB, patent issues |
| Binary protocol | Fast, efficient | Less human-readable than JSON |
| C++17 | Modern features, good support | Not cutting-edge (C++20/23) |

### 8.2 Alternative Approaches

**IPC Alternatives:**
- **Shared Memory**: Faster, but harder to manage, less flexible
- **Unix Domain Sockets**: Simpler, but manual protocol needed
- **gRPC**: More structured, but heavier weight

**Database Alternatives:**
- **PostgreSQL**: Better concurrency, but requires server
- **MongoDB**: Schema flexibility, but overkill for structured data
- **HDF5**: Good for binary data, but less query flexibility

**Feature Detector Alternatives:**
- **ORB**: Faster, patent-free, but less robust
- **AKAZE**: Good quality, faster than SIFT
- **SURF**: Similar to SIFT, but also patented

## 9. Future Enhancements

### 9.1 Scalability

**Horizontal Scaling:**
- Multiple Feature Extractors (ZeroMQ naturally supports this)
- Distributed Data Loggers (shard by image hash)
- Remote Image Generators (different machines)

**Vertical Scaling:**
- GPU acceleration for SIFT (OpenCV CUDA)
- Parallel processing within apps (thread pool)
- Memory-mapped files for large images

### 9.2 Features

**Monitoring:**
- Prometheus metrics (images/sec, latency)
- Health check endpoints
- Grafana dashboards

**Configuration:**
- YAML/JSON config files
- Environment variables
- Runtime configuration updates

**Additional Processing:**
- Object detection (YOLO)
- Image classification (CNN)
- Semantic segmentation
- Video support (frame extraction)

## 10. Deployment

### 10.1 Build System

**CMake:**
- Modern target-based approach
- Automatic dependency detection
- Cross-platform support
- Test integration (CTest)

### 10.2 Packaging

**Options:**
- **Docker**: Containerized deployment
- **Snap/AppImage**: Linux distribution
- **DEB/RPM**: Native package managers
- **Portable build**: Static linking

### 10.3 Production Considerations

**Security:**
- Input validation (image size limits)
- SQL injection prevention (prepared statements)
- ZeroMQ authentication (ZAP)

**Reliability:**
- Systemd services for auto-restart
- Log rotation
- Database backups
- Disk space monitoring

**Performance:**
- CPU pinning
- Priority scheduling
- NUMA awareness (multi-socket systems)

## 11. Conclusion

This design provides a robust, scalable foundation for distributed image processing. The architecture emphasizes:

1. **Reliability**: Fault-tolerant, automatic recovery
2. **Maintainability**: Clean interfaces, modular design
3. **Performance**: Efficient protocols, optimized processing
4. **Extensibility**: Easy to add new apps or features

The system meets all requirements while maintaining flexibility for future enhancements.

---

**Document Version:** 1.0
**Last Updated:** 2025-11-17
**Author:** Distributed Imaging System Team
