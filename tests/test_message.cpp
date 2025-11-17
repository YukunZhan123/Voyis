#include "message.h"
#include <gtest/gtest.h>
#include <vector>
#include <string>

using namespace voyis;

class MessageTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create sample data
        sample_image_data_ = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    }

    std::vector<uint8_t> sample_image_data_;
};

// Test ImageMessage serialization and deserialization
TEST_F(MessageTest, ImageMessageSerializeDeserialize) {
    // Create message
    ImageMessage original;
    original.image_id = "test_image_001";
    original.image_data = sample_image_data_;
    original.format = "png";
    original.width = 640;
    original.height = 480;
    original.timestamp = 1234567890;

    // Serialize
    std::vector<uint8_t> serialized = original.serialize();
    ASSERT_FALSE(serialized.empty());

    // Deserialize
    ImageMessage deserialized = ImageMessage::deserialize(serialized);

    // Verify all fields match
    EXPECT_EQ(original.image_id, deserialized.image_id);
    EXPECT_EQ(original.image_data, deserialized.image_data);
    EXPECT_EQ(original.format, deserialized.format);
    EXPECT_EQ(original.width, deserialized.width);
    EXPECT_EQ(original.height, deserialized.height);
    EXPECT_EQ(original.timestamp, deserialized.timestamp);
}

TEST_F(MessageTest, ImageMessageEmptyData) {
    ImageMessage original;
    original.image_id = "empty_image";
    original.image_data.clear(); // Empty image data
    original.format = "jpg";
    original.width = 0;
    original.height = 0;
    original.timestamp = 0;

    std::vector<uint8_t> serialized = original.serialize();
    ImageMessage deserialized = ImageMessage::deserialize(serialized);

    EXPECT_EQ(original.image_id, deserialized.image_id);
    EXPECT_TRUE(deserialized.image_data.empty());
    EXPECT_EQ(original.format, deserialized.format);
}

TEST_F(MessageTest, ImageMessageLargeData) {
    ImageMessage original;
    original.image_id = "large_image";
    original.image_data.resize(10 * 1024 * 1024); // 10 MB
    std::fill(original.image_data.begin(), original.image_data.end(), 0xFF);
    original.format = "tiff";
    original.width = 4096;
    original.height = 4096;
    original.timestamp = 9876543210;

    std::vector<uint8_t> serialized = original.serialize();
    ImageMessage deserialized = ImageMessage::deserialize(serialized);

    EXPECT_EQ(original.image_data.size(), deserialized.image_data.size());
    EXPECT_EQ(original.image_data, deserialized.image_data);
}

// Test ProcessedImageMessage serialization and deserialization
TEST_F(MessageTest, ProcessedImageMessageSerializeDeserialize) {
    ProcessedImageMessage original;
    original.image_id = "processed_001";
    original.image_data = sample_image_data_;
    original.format = "png";
    original.width = 800;
    original.height = 600;
    original.timestamp = 1111111111;
    original.processed_timestamp = 2222222222;

    // Add keypoints
    for (int i = 0; i < 5; ++i) {
        KeyPoint kp;
        kp.pt.x = 100.0f + i * 10.0f;
        kp.pt.y = 200.0f + i * 20.0f;
        kp.size = 5.0f + i;
        kp.angle = 45.0f * i;
        kp.response = 0.8f + i * 0.01f;
        kp.octave = i;
        original.keypoints.push_back(kp);
    }

    // Add descriptors (SIFT descriptors are 128-dimensional)
    for (int i = 0; i < 5; ++i) {
        std::vector<float> desc(128);
        for (int j = 0; j < 128; ++j) {
            desc[j] = static_cast<float>(i * 128 + j) / 1000.0f;
        }
        original.descriptors.push_back(desc);
    }

    // Serialize
    std::vector<uint8_t> serialized = original.serialize();
    ASSERT_FALSE(serialized.empty());

    // Deserialize
    ProcessedImageMessage deserialized = ProcessedImageMessage::deserialize(serialized);

    // Verify basic fields
    EXPECT_EQ(original.image_id, deserialized.image_id);
    EXPECT_EQ(original.image_data, deserialized.image_data);
    EXPECT_EQ(original.format, deserialized.format);
    EXPECT_EQ(original.width, deserialized.width);
    EXPECT_EQ(original.height, deserialized.height);
    EXPECT_EQ(original.timestamp, deserialized.timestamp);
    EXPECT_EQ(original.processed_timestamp, deserialized.processed_timestamp);

    // Verify keypoints
    ASSERT_EQ(original.keypoints.size(), deserialized.keypoints.size());
    for (size_t i = 0; i < original.keypoints.size(); ++i) {
        EXPECT_FLOAT_EQ(original.keypoints[i].pt.x, deserialized.keypoints[i].pt.x);
        EXPECT_FLOAT_EQ(original.keypoints[i].pt.y, deserialized.keypoints[i].pt.y);
        EXPECT_FLOAT_EQ(original.keypoints[i].size, deserialized.keypoints[i].size);
        EXPECT_FLOAT_EQ(original.keypoints[i].angle, deserialized.keypoints[i].angle);
        EXPECT_FLOAT_EQ(original.keypoints[i].response, deserialized.keypoints[i].response);
        EXPECT_EQ(original.keypoints[i].octave, deserialized.keypoints[i].octave);
    }

    // Verify descriptors
    ASSERT_EQ(original.descriptors.size(), deserialized.descriptors.size());
    for (size_t i = 0; i < original.descriptors.size(); ++i) {
        ASSERT_EQ(original.descriptors[i].size(), deserialized.descriptors[i].size());
        for (size_t j = 0; j < original.descriptors[i].size(); ++j) {
            EXPECT_FLOAT_EQ(original.descriptors[i][j], deserialized.descriptors[i][j]);
        }
    }
}

TEST_F(MessageTest, ProcessedImageMessageNoKeypoints) {
    ProcessedImageMessage original;
    original.image_id = "no_keypoints";
    original.image_data = sample_image_data_;
    original.format = "jpg";
    original.width = 100;
    original.height = 100;
    original.timestamp = 3333333333;
    original.processed_timestamp = 4444444444;
    // No keypoints or descriptors

    std::vector<uint8_t> serialized = original.serialize();
    ProcessedImageMessage deserialized = ProcessedImageMessage::deserialize(serialized);

    EXPECT_EQ(original.image_id, deserialized.image_id);
    EXPECT_TRUE(deserialized.keypoints.empty());
    EXPECT_TRUE(deserialized.descriptors.empty());
}

// Test Point2f
TEST(Point2fTest, Construction) {
    Point2f p1;
    EXPECT_FLOAT_EQ(p1.x, 0.0f);
    EXPECT_FLOAT_EQ(p1.y, 0.0f);

    Point2f p2(10.5f, 20.5f);
    EXPECT_FLOAT_EQ(p2.x, 10.5f);
    EXPECT_FLOAT_EQ(p2.y, 20.5f);
}

// Test KeyPoint
TEST(KeyPointTest, Construction) {
    KeyPoint kp;
    EXPECT_FLOAT_EQ(kp.pt.x, 0.0f);
    EXPECT_FLOAT_EQ(kp.pt.y, 0.0f);
    EXPECT_FLOAT_EQ(kp.size, 0.0f);
    EXPECT_FLOAT_EQ(kp.angle, -1.0f);
    EXPECT_FLOAT_EQ(kp.response, 0.0f);
    EXPECT_EQ(kp.octave, 0);
}
