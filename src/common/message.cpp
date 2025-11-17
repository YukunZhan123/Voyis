#include "message.h"
#include <cstring>
#include <stdexcept>

namespace voyis {

// Helper functions for serialization
namespace {

// Write a value to a byte vector
template<typename T>
void writeValue(std::vector<uint8_t>& buffer, const T& value) {
    const uint8_t* data = reinterpret_cast<const uint8_t*>(&value);
    buffer.insert(buffer.end(), data, data + sizeof(T));
}

// Write a string to a byte vector (length-prefixed)
void writeString(std::vector<uint8_t>& buffer, const std::string& str) {
    uint32_t length = static_cast<uint32_t>(str.size());
    writeValue(buffer, length);
    buffer.insert(buffer.end(), str.begin(), str.end());
}

// Write a byte vector to another byte vector (length-prefixed)
void writeBytes(std::vector<uint8_t>& buffer, const std::vector<uint8_t>& bytes) {
    uint32_t length = static_cast<uint32_t>(bytes.size());
    writeValue(buffer, length);
    buffer.insert(buffer.end(), bytes.begin(), bytes.end());
}

// Read a value from a byte vector
template<typename T>
T readValue(const uint8_t*& data, size_t& remaining) {
    if (remaining < sizeof(T)) {
        throw std::runtime_error("Insufficient data to read value");
    }
    T value;
    std::memcpy(&value, data, sizeof(T));
    data += sizeof(T);
    remaining -= sizeof(T);
    return value;
}

// Read a string from a byte vector (length-prefixed)
std::string readString(const uint8_t*& data, size_t& remaining) {
    uint32_t length = readValue<uint32_t>(data, remaining);
    if (remaining < length) {
        throw std::runtime_error("Insufficient data to read string");
    }
    std::string str(reinterpret_cast<const char*>(data), length);
    data += length;
    remaining -= length;
    return str;
}

// Read a byte vector from another byte vector (length-prefixed)
std::vector<uint8_t> readBytes(const uint8_t*& data, size_t& remaining) {
    uint32_t length = readValue<uint32_t>(data, remaining);
    if (remaining < length) {
        throw std::runtime_error("Insufficient data to read bytes");
    }
    std::vector<uint8_t> bytes(data, data + length);
    data += length;
    remaining -= length;
    return bytes;
}

} // anonymous namespace

// ImageMessage serialization
std::vector<uint8_t> ImageMessage::serialize() const {
    std::vector<uint8_t> buffer;
    buffer.reserve(image_data.size() + 1024); // Estimate size

    writeString(buffer, image_id);
    writeBytes(buffer, image_data);
    writeString(buffer, format);
    writeValue(buffer, width);
    writeValue(buffer, height);
    writeValue(buffer, timestamp);

    return buffer;
}

ImageMessage ImageMessage::deserialize(const std::vector<uint8_t>& data) {
    const uint8_t* ptr = data.data();
    size_t remaining = data.size();

    ImageMessage msg;
    msg.image_id = readString(ptr, remaining);
    msg.image_data = readBytes(ptr, remaining);
    msg.format = readString(ptr, remaining);
    msg.width = readValue<int>(ptr, remaining);
    msg.height = readValue<int>(ptr, remaining);
    msg.timestamp = readValue<int64_t>(ptr, remaining);

    return msg;
}

// ProcessedImageMessage serialization
std::vector<uint8_t> ProcessedImageMessage::serialize() const {
    std::vector<uint8_t> buffer;
    buffer.reserve(image_data.size() + keypoints.size() * 64 + 1024);

    writeString(buffer, image_id);
    writeBytes(buffer, image_data);
    writeString(buffer, format);
    writeValue(buffer, width);
    writeValue(buffer, height);
    writeValue(buffer, timestamp);
    writeValue(buffer, processed_timestamp);

    // Write keypoints
    uint32_t num_keypoints = static_cast<uint32_t>(keypoints.size());
    writeValue(buffer, num_keypoints);
    for (const auto& kp : keypoints) {
        writeValue(buffer, kp.pt.x);
        writeValue(buffer, kp.pt.y);
        writeValue(buffer, kp.size);
        writeValue(buffer, kp.angle);
        writeValue(buffer, kp.response);
        writeValue(buffer, kp.octave);
    }

    // Write descriptors
    uint32_t num_descriptors = static_cast<uint32_t>(descriptors.size());
    writeValue(buffer, num_descriptors);
    for (const auto& desc : descriptors) {
        uint32_t desc_size = static_cast<uint32_t>(desc.size());
        writeValue(buffer, desc_size);
        for (float val : desc) {
            writeValue(buffer, val);
        }
    }

    return buffer;
}

ProcessedImageMessage ProcessedImageMessage::deserialize(const std::vector<uint8_t>& data) {
    const uint8_t* ptr = data.data();
    size_t remaining = data.size();

    ProcessedImageMessage msg;
    msg.image_id = readString(ptr, remaining);
    msg.image_data = readBytes(ptr, remaining);
    msg.format = readString(ptr, remaining);
    msg.width = readValue<int>(ptr, remaining);
    msg.height = readValue<int>(ptr, remaining);
    msg.timestamp = readValue<int64_t>(ptr, remaining);
    msg.processed_timestamp = readValue<int64_t>(ptr, remaining);

    // Read keypoints
    uint32_t num_keypoints = readValue<uint32_t>(ptr, remaining);
    msg.keypoints.reserve(num_keypoints);
    for (uint32_t i = 0; i < num_keypoints; ++i) {
        KeyPoint kp;
        kp.pt.x = readValue<float>(ptr, remaining);
        kp.pt.y = readValue<float>(ptr, remaining);
        kp.size = readValue<float>(ptr, remaining);
        kp.angle = readValue<float>(ptr, remaining);
        kp.response = readValue<float>(ptr, remaining);
        kp.octave = readValue<int>(ptr, remaining);
        msg.keypoints.push_back(kp);
    }

    // Read descriptors
    uint32_t num_descriptors = readValue<uint32_t>(ptr, remaining);
    msg.descriptors.reserve(num_descriptors);
    for (uint32_t i = 0; i < num_descriptors; ++i) {
        uint32_t desc_size = readValue<uint32_t>(ptr, remaining);
        std::vector<float> desc;
        desc.reserve(desc_size);
        for (uint32_t j = 0; j < desc_size; ++j) {
            desc.push_back(readValue<float>(ptr, remaining));
        }
        msg.descriptors.push_back(desc);
    }

    return msg;
}

} // namespace voyis
