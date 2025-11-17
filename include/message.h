#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <memory>

namespace voyis {

/**
 * @brief Structure to represent a 2D point (SIFT keypoint coordinate)
 */
struct Point2f {
    float x;
    float y;

    Point2f() : x(0.0f), y(0.0f) {}
    Point2f(float x_, float y_) : x(x_), y(y_) {}
};

/**
 * @brief Structure to represent a SIFT keypoint
 */
struct KeyPoint {
    Point2f pt;          // Keypoint coordinates
    float size;          // Diameter of the meaningful keypoint neighborhood
    float angle;         // Computed orientation of the keypoint (-1 if not applicable)
    float response;      // The response by which the keypoints have been selected
    int octave;          // Octave (pyramid layer) from which the keypoint was extracted

    KeyPoint() : size(0), angle(-1), response(0), octave(0) {}
};

/**
 * @brief Message containing raw image data
 * Used for communication between Image Generator and Feature Extractor
 */
struct ImageMessage {
    std::string image_id;           // Unique identifier for the image
    std::vector<uint8_t> image_data; // Raw image bytes
    std::string format;             // Image format (e.g., "png", "jpg")
    int width;                      // Image width
    int height;                     // Image height
    int64_t timestamp;              // Timestamp when image was read

    ImageMessage() : width(0), height(0), timestamp(0) {}

    // Serialize to bytes for IPC transmission
    std::vector<uint8_t> serialize() const;

    // Deserialize from bytes received via IPC
    static ImageMessage deserialize(const std::vector<uint8_t>& data);
};

/**
 * @brief Message containing image data plus extracted SIFT features
 * Used for communication between Feature Extractor and Data Logger
 */
struct ProcessedImageMessage {
    std::string image_id;           // Unique identifier for the image
    std::vector<uint8_t> image_data; // Raw image bytes
    std::string format;             // Image format
    int width;                      // Image width
    int height;                     // Image height
    int64_t timestamp;              // Original timestamp
    int64_t processed_timestamp;    // When SIFT processing completed
    std::vector<KeyPoint> keypoints; // Extracted SIFT keypoints
    std::vector<std::vector<float>> descriptors; // SIFT descriptors (128-dim per keypoint)

    ProcessedImageMessage() : width(0), height(0), timestamp(0), processed_timestamp(0) {}

    // Serialize to bytes for IPC transmission
    std::vector<uint8_t> serialize() const;

    // Deserialize from bytes received via IPC
    static ProcessedImageMessage deserialize(const std::vector<uint8_t>& data);
};

} // namespace voyis
