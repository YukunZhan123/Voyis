#include <gtest/gtest.h>
#include "serialization.hpp"
#include "message_types.hpp"
#include <chrono>

using namespace dis;

class SerializationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test image data
        test_image_.filename = "test_image.jpg";
        test_image_.width = 640;
        test_image_.height = 480;
        test_image_.channels = 3;
        test_image_.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        // Create dummy image data (640x480x3 = 921600 bytes)
        test_image_.data.resize(640 * 480 * 3);
        for (size_t i = 0; i < test_image_.data.size(); ++i) {
            test_image_.data[i] = static_cast<uint8_t>(i % 256);
        }
    }

    ImageData test_image_;
};

TEST_F(SerializationTest, SerializeDeserializeImageData) {
    // Serialize
    auto serialized = Serializer::serialize(test_image_);

    // Check that serialized data is not empty
    ASSERT_FALSE(serialized.empty());

    // Check size is reasonable (header + filename + data)
    size_t expected_min_size = sizeof(MessageHeader) +
                               test_image_.filename.size() +
                               test_image_.data.size();
    EXPECT_EQ(serialized.size(), expected_min_size);

    // Deserialize
    auto deserialized = Serializer::deserialize_image(serialized);

    // Verify all fields match
    EXPECT_EQ(deserialized.filename, test_image_.filename);
    EXPECT_EQ(deserialized.width, test_image_.width);
    EXPECT_EQ(deserialized.height, test_image_.height);
    EXPECT_EQ(deserialized.channels, test_image_.channels);
    EXPECT_EQ(deserialized.timestamp, test_image_.timestamp);
    EXPECT_EQ(deserialized.data.size(), test_image_.data.size());
    EXPECT_EQ(deserialized.data, test_image_.data);
}

TEST_F(SerializationTest, SerializeEmptyImage) {
    ImageData empty_image;
    empty_image.filename = "empty.jpg";
    empty_image.width = 0;
    empty_image.height = 0;
    empty_image.channels = 0;
    empty_image.timestamp = 12345;

    // Should be able to serialize empty image
    auto serialized = Serializer::serialize(empty_image);
    ASSERT_FALSE(serialized.empty());

    // Deserialize and verify
    auto deserialized = Serializer::deserialize_image(serialized);
    EXPECT_EQ(deserialized.filename, empty_image.filename);
    EXPECT_EQ(deserialized.width, 0u);
    EXPECT_EQ(deserialized.height, 0u);
    EXPECT_TRUE(deserialized.data.empty());
}

TEST_F(SerializationTest, SerializeLargeImage) {
    // Create a large image (30MB as per requirements)
    ImageData large_image;
    large_image.filename = "large_image.jpg";
    large_image.width = 4096;
    large_image.height = 2560;
    large_image.channels = 3;
    large_image.timestamp = 12345;

    size_t size = large_image.width * large_image.height * large_image.channels;
    large_image.data.resize(size);

    // Fill with test pattern
    for (size_t i = 0; i < size; ++i) {
        large_image.data[i] = static_cast<uint8_t>((i * 7) % 256);
    }

    // Serialize
    auto serialized = Serializer::serialize(large_image);
    ASSERT_FALSE(serialized.empty());

    // Deserialize
    auto deserialized = Serializer::deserialize_image(serialized);

    // Verify
    EXPECT_EQ(deserialized.width, large_image.width);
    EXPECT_EQ(deserialized.height, large_image.height);
    EXPECT_EQ(deserialized.data.size(), large_image.data.size());
    EXPECT_EQ(deserialized.data, large_image.data);
}

TEST_F(SerializationTest, SerializeDeserializeImageWithFeatures) {
    // Create image with features
    ImageWithFeatures image_features;
    image_features.image = test_image_;

    // Add some test keypoints
    for (int i = 0; i < 10; ++i) {
        Keypoint kp;
        kp.x = 10.0f * i;
        kp.y = 20.0f * i;
        kp.size = 5.0f + i;
        kp.angle = 45.0f * i;
        kp.response = 0.5f + 0.1f * i;
        kp.octave = i % 4;
        kp.class_id = -1;
        image_features.keypoints.push_back(kp);
    }

    // Add test descriptors (128 floats per keypoint)
    image_features.descriptors.resize(10 * 128);
    for (size_t i = 0; i < image_features.descriptors.size(); ++i) {
        image_features.descriptors[i] = static_cast<float>(i) / 100.0f;
    }

    // Serialize
    auto serialized = Serializer::serialize(image_features);
    ASSERT_FALSE(serialized.empty());

    // Deserialize
    auto deserialized = Serializer::deserialize_image_with_features(serialized);

    // Verify image data
    EXPECT_EQ(deserialized.image.filename, test_image_.filename);
    EXPECT_EQ(deserialized.image.width, test_image_.width);
    EXPECT_EQ(deserialized.image.height, test_image_.height);
    EXPECT_EQ(deserialized.image.data.size(), test_image_.data.size());

    // Verify keypoints
    ASSERT_EQ(deserialized.keypoints.size(), 10u);
    for (size_t i = 0; i < 10; ++i) {
        EXPECT_FLOAT_EQ(deserialized.keypoints[i].x, 10.0f * i);
        EXPECT_FLOAT_EQ(deserialized.keypoints[i].y, 20.0f * i);
        EXPECT_FLOAT_EQ(deserialized.keypoints[i].size, 5.0f + i);
        EXPECT_FLOAT_EQ(deserialized.keypoints[i].angle, 45.0f * i);
    }

    // Verify descriptors
    ASSERT_EQ(deserialized.descriptors.size(), 10u * 128);
    for (size_t i = 0; i < deserialized.descriptors.size(); ++i) {
        EXPECT_FLOAT_EQ(deserialized.descriptors[i], static_cast<float>(i) / 100.0f);
    }
}

TEST_F(SerializationTest, DeserializeInvalidData) {
    // Test with empty data
    std::vector<uint8_t> empty_data;
    EXPECT_THROW(Serializer::deserialize_image(empty_data), SerializationError);

    // Test with too small data
    std::vector<uint8_t> small_data(10);
    EXPECT_THROW(Serializer::deserialize_image(small_data), SerializationError);

    // Test with invalid magic number
    std::vector<uint8_t> invalid_magic(sizeof(MessageHeader) + 100);
    MessageHeader* header = reinterpret_cast<MessageHeader*>(invalid_magic.data());
    header->magic = 0x12345678;  // Wrong magic
    header->type = MessageType::IMAGE;
    header->payload_size = 100;
    EXPECT_THROW(Serializer::deserialize_image(invalid_magic), SerializationError);
}

TEST_F(SerializationTest, MessageHeaderSize) {
    // Verify header is exactly 64 bytes as specified
    EXPECT_EQ(sizeof(MessageHeader), 64u);
}

TEST_F(SerializationTest, SerializeImageWithLongFilename) {
    ImageData img;
    img.filename = std::string(1000, 'x');  // 1000 character filename
    img.width = 100;
    img.height = 100;
    img.channels = 3;
    img.timestamp = 12345;
    img.data.resize(100 * 100 * 3);

    auto serialized = Serializer::serialize(img);
    auto deserialized = Serializer::deserialize_image(serialized);

    EXPECT_EQ(deserialized.filename, img.filename);
    EXPECT_EQ(deserialized.filename.size(), 1000u);
}

TEST_F(SerializationTest, SerializeImageWithNoKeypoints) {
    ImageWithFeatures image_features;
    image_features.image = test_image_;
    // No keypoints or descriptors

    auto serialized = Serializer::serialize(image_features);
    auto deserialized = Serializer::deserialize_image_with_features(serialized);

    EXPECT_EQ(deserialized.image.filename, test_image_.filename);
    EXPECT_TRUE(deserialized.keypoints.empty());
    EXPECT_TRUE(deserialized.descriptors.empty());
}
