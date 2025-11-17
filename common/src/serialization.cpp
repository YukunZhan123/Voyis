#include "serialization.hpp"
#include <chrono>

namespace dis {

std::vector<uint8_t> Serializer::serialize(const ImageData& image) {
    std::vector<uint8_t> buffer;
    buffer.reserve(sizeof(MessageHeader) + image.filename.size() + image.data.size());

    // Create and populate header
    MessageHeader header;
    header.type = MessageType::IMAGE;
    header.timestamp = image.timestamp;
    header.width = image.width;
    header.height = image.height;
    header.channels = image.channels;
    header.filename_length = static_cast<uint32_t>(image.filename.size());
    header.payload_size = header.filename_length + image.data.size();

    // Write header
    const uint8_t* header_ptr = reinterpret_cast<const uint8_t*>(&header);
    buffer.insert(buffer.end(), header_ptr, header_ptr + sizeof(MessageHeader));

    // Write filename
    buffer.insert(buffer.end(), image.filename.begin(), image.filename.end());

    // Write image data
    buffer.insert(buffer.end(), image.data.begin(), image.data.end());

    return buffer;
}

std::vector<uint8_t> Serializer::serialize(const ImageWithFeatures& image_features) {
    std::vector<uint8_t> buffer;

    // Calculate total size
    size_t keypoint_data_size = image_features.keypoints.size() * sizeof(Keypoint);
    size_t descriptor_data_size = image_features.descriptors.size() * sizeof(float);
    size_t total_size = sizeof(MessageHeader) +
                       image_features.image.filename.size() +
                       image_features.image.data.size() +
                       sizeof(uint32_t) +  // keypoint count
                       keypoint_data_size +
                       sizeof(uint32_t) +  // descriptor count
                       descriptor_data_size;

    buffer.reserve(total_size);

    // Create and populate header
    MessageHeader header;
    header.type = MessageType::IMAGE_WITH_FEATURES;
    header.timestamp = image_features.image.timestamp;
    header.width = image_features.image.width;
    header.height = image_features.image.height;
    header.channels = image_features.image.channels;
    header.filename_length = static_cast<uint32_t>(image_features.image.filename.size());
    header.payload_size = image_features.image.filename.size() +
                         image_features.image.data.size() +
                         sizeof(uint32_t) + keypoint_data_size +
                         sizeof(uint32_t) + descriptor_data_size;

    // Write header
    const uint8_t* header_ptr = reinterpret_cast<const uint8_t*>(&header);
    buffer.insert(buffer.end(), header_ptr, header_ptr + sizeof(MessageHeader));

    // Write filename
    buffer.insert(buffer.end(), image_features.image.filename.begin(),
                 image_features.image.filename.end());

    // Write image data
    buffer.insert(buffer.end(), image_features.image.data.begin(),
                 image_features.image.data.end());

    // Write keypoint count
    uint32_t keypoint_count = static_cast<uint32_t>(image_features.keypoints.size());
    write_to_buffer(buffer, keypoint_count);

    // Write keypoints
    if (keypoint_count > 0) {
        write_vector_to_buffer(buffer, image_features.keypoints);
    }

    // Write descriptor count
    uint32_t descriptor_count = static_cast<uint32_t>(image_features.descriptors.size());
    write_to_buffer(buffer, descriptor_count);

    // Write descriptors
    if (descriptor_count > 0) {
        write_vector_to_buffer(buffer, image_features.descriptors);
    }

    return buffer;
}

ImageData Serializer::deserialize_image(const std::vector<uint8_t>& data) {
    if (data.size() < sizeof(MessageHeader)) {
        throw SerializationError("Data too small to contain header");
    }

    // Read header
    MessageHeader header;
    std::memcpy(&header, data.data(), sizeof(MessageHeader));

    // Validate magic number
    if (header.magic != MESSAGE_MAGIC) {
        throw SerializationError("Invalid magic number in header");
    }

    // Validate message type
    if (header.type != MessageType::IMAGE) {
        throw SerializationError("Expected IMAGE message type");
    }

    // Validate data size
    size_t expected_size = sizeof(MessageHeader) + header.payload_size;
    if (data.size() != expected_size) {
        throw SerializationError("Data size mismatch");
    }

    size_t offset = sizeof(MessageHeader);

    // Read filename
    if (offset + header.filename_length > data.size()) {
        throw SerializationError("Invalid filename length");
    }
    std::string filename(data.begin() + offset, data.begin() + offset + header.filename_length);
    offset += header.filename_length;

    // Read image data
    size_t image_data_size = header.payload_size - header.filename_length;
    if (offset + image_data_size != data.size()) {
        throw SerializationError("Invalid image data size");
    }
    std::vector<uint8_t> image_data(data.begin() + offset, data.end());

    // Construct ImageData
    ImageData image;
    image.filename = std::move(filename);
    image.width = header.width;
    image.height = header.height;
    image.channels = header.channels;
    image.timestamp = header.timestamp;
    image.data = std::move(image_data);

    return image;
}

ImageWithFeatures Serializer::deserialize_image_with_features(const std::vector<uint8_t>& data) {
    if (data.size() < sizeof(MessageHeader)) {
        throw SerializationError("Data too small to contain header");
    }

    // Read header
    MessageHeader header;
    std::memcpy(&header, data.data(), sizeof(MessageHeader));

    // Validate magic number
    if (header.magic != MESSAGE_MAGIC) {
        throw SerializationError("Invalid magic number in header");
    }

    // Validate message type
    if (header.type != MessageType::IMAGE_WITH_FEATURES) {
        throw SerializationError("Expected IMAGE_WITH_FEATURES message type");
    }

    size_t offset = sizeof(MessageHeader);

    // Read filename
    if (offset + header.filename_length > data.size()) {
        throw SerializationError("Invalid filename length");
    }
    std::string filename(data.begin() + offset, data.begin() + offset + header.filename_length);
    offset += header.filename_length;

    // Calculate and read image data
    size_t remaining_after_filename = header.payload_size - header.filename_length;
    size_t image_data_size = header.width * header.height * header.channels;

    if (offset + image_data_size > data.size()) {
        throw SerializationError("Invalid image data size");
    }
    std::vector<uint8_t> image_data(data.begin() + offset, data.begin() + offset + image_data_size);
    offset += image_data_size;

    // Read keypoint count
    uint32_t keypoint_count = read_from_buffer<uint32_t>(data, offset);

    // Read keypoints
    std::vector<Keypoint> keypoints;
    if (keypoint_count > 0) {
        keypoints = read_vector_from_buffer<Keypoint>(data, offset, keypoint_count);
    }

    // Read descriptor count
    uint32_t descriptor_count = read_from_buffer<uint32_t>(data, offset);

    // Read descriptors
    std::vector<float> descriptors;
    if (descriptor_count > 0) {
        descriptors = read_vector_from_buffer<float>(data, offset, descriptor_count);
    }

    // Construct ImageWithFeatures
    ImageWithFeatures result;
    result.image.filename = std::move(filename);
    result.image.width = header.width;
    result.image.height = header.height;
    result.image.channels = header.channels;
    result.image.timestamp = header.timestamp;
    result.image.data = std::move(image_data);
    result.keypoints = std::move(keypoints);
    result.descriptors = std::move(descriptors);

    return result;
}

}  // namespace dis
