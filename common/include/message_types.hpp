#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <cstring>

namespace dis {  // Distributed Imaging System

// Magic number to identify our messages
constexpr uint32_t MESSAGE_MAGIC = 0xDEADBEEF;

// Message types
enum class MessageType : uint32_t {
    IMAGE = 1,
    IMAGE_WITH_FEATURES = 2
};

// Message header structure (64 bytes, fixed size)
struct MessageHeader {
    uint32_t magic;           // Magic number for validation
    MessageType type;         // Message type
    uint64_t payload_size;    // Total payload size in bytes
    uint64_t timestamp;       // Unix timestamp in milliseconds
    uint32_t width;           // Image width
    uint32_t height;          // Image height
    uint32_t channels;        // Number of channels (1=gray, 3=RGB)
    uint32_t filename_length; // Length of filename string
    uint8_t reserved[24];     // Reserved for future use

    MessageHeader()
        : magic(MESSAGE_MAGIC)
        , type(MessageType::IMAGE)
        , payload_size(0)
        , timestamp(0)
        , width(0)
        , height(0)
        , channels(0)
        , filename_length(0)
        , reserved{0} {}
};

static_assert(sizeof(MessageHeader) == 64, "MessageHeader must be exactly 64 bytes");

// Structure to hold image data
struct ImageData {
    std::string filename;
    uint32_t width;
    uint32_t height;
    uint32_t channels;
    uint64_t timestamp;
    std::vector<uint8_t> data;  // Raw image data

    ImageData()
        : width(0), height(0), channels(0), timestamp(0) {}

    size_t size() const {
        return data.size();
    }

    bool empty() const {
        return data.empty();
    }
};

// Structure to hold a single keypoint
struct Keypoint {
    float x;          // X coordinate
    float y;          // Y coordinate
    float size;       // Keypoint size
    float angle;      // Orientation angle
    float response;   // Detection response strength
    int32_t octave;   // Octave (scale) where keypoint was detected
    int32_t class_id; // Object class (reserved for future use)

    Keypoint()
        : x(0.0f), y(0.0f), size(0.0f), angle(0.0f)
        , response(0.0f), octave(0), class_id(-1) {}

    Keypoint(float x_, float y_, float size_, float angle_, float response_,
             int32_t octave_ = 0, int32_t class_id_ = -1)
        : x(x_), y(y_), size(size_), angle(angle_)
        , response(response_), octave(octave_), class_id(class_id_) {}
};

// Structure to hold image with extracted features
struct ImageWithFeatures {
    ImageData image;
    std::vector<Keypoint> keypoints;
    std::vector<float> descriptors;  // SIFT descriptors (128 floats per keypoint)

    ImageWithFeatures() = default;

    size_t num_keypoints() const {
        return keypoints.size();
    }

    bool has_descriptors() const {
        return !descriptors.empty();
    }
};

}  // namespace dis
