#pragma once

#include "message_types.hpp"
#include <vector>
#include <stdexcept>
#include <cstring>

namespace dis {

class SerializationError : public std::runtime_error {
public:
    explicit SerializationError(const std::string& msg)
        : std::runtime_error(msg) {}
};

// Serializer class for encoding messages
class Serializer {
public:
    // Serialize ImageData to binary format
    static std::vector<uint8_t> serialize(const ImageData& image);

    // Serialize ImageWithFeatures to binary format
    static std::vector<uint8_t> serialize(const ImageWithFeatures& image_features);

    // Deserialize ImageData from binary format
    static ImageData deserialize_image(const std::vector<uint8_t>& data);

    // Deserialize ImageWithFeatures from binary format
    static ImageWithFeatures deserialize_image_with_features(const std::vector<uint8_t>& data);

private:
    // Helper: write data to buffer
    template<typename T>
    static void write_to_buffer(std::vector<uint8_t>& buffer, const T& value) {
        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&value);
        buffer.insert(buffer.end(), ptr, ptr + sizeof(T));
    }

    // Helper: write vector to buffer
    template<typename T>
    static void write_vector_to_buffer(std::vector<uint8_t>& buffer, const std::vector<T>& vec) {
        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(vec.data());
        buffer.insert(buffer.end(), ptr, ptr + vec.size() * sizeof(T));
    }

    // Helper: read data from buffer
    template<typename T>
    static T read_from_buffer(const std::vector<uint8_t>& buffer, size_t& offset) {
        if (offset + sizeof(T) > buffer.size()) {
            throw SerializationError("Buffer underflow: not enough data to read");
        }
        T value;
        std::memcpy(&value, buffer.data() + offset, sizeof(T));
        offset += sizeof(T);
        return value;
    }

    // Helper: read vector from buffer
    template<typename T>
    static std::vector<T> read_vector_from_buffer(const std::vector<uint8_t>& buffer,
                                                   size_t& offset, size_t count) {
        size_t bytes_needed = count * sizeof(T);
        if (offset + bytes_needed > buffer.size()) {
            throw SerializationError("Buffer underflow: not enough data for vector");
        }
        std::vector<T> vec(count);
        std::memcpy(vec.data(), buffer.data() + offset, bytes_needed);
        offset += bytes_needed;
        return vec;
    }
};

}  // namespace dis
