#include <gtest/gtest.h>
#include "ipc_manager.hpp"
#include "message_types.hpp"
#include <thread>
#include <chrono>

using namespace dis;

class IPCTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Use unique endpoints for each test to avoid conflicts
        static int port = 15555;
        endpoint_ = "tcp://127.0.0.1:" + std::to_string(port++);

        // Create test image
        test_image_.filename = "test.jpg";
        test_image_.width = 320;
        test_image_.height = 240;
        test_image_.channels = 3;
        test_image_.timestamp = 12345;
        test_image_.data.resize(320 * 240 * 3);
        for (size_t i = 0; i < test_image_.data.size(); ++i) {
            test_image_.data[i] = static_cast<uint8_t>(i % 256);
        }
    }

    std::string endpoint_;
    ImageData test_image_;
};

TEST_F(IPCTest, PublisherCreation) {
    EXPECT_NO_THROW({
        Publisher pub(endpoint_);
        EXPECT_TRUE(pub.is_connected());
    });
}

TEST_F(IPCTest, SubscriberCreation) {
    // Create publisher first
    Publisher pub(endpoint_);

    EXPECT_NO_THROW({
        Subscriber sub(endpoint_);
        EXPECT_TRUE(sub.is_connected());
    });
}

TEST_F(IPCTest, SendReceiveImageData) {
    // Start publisher in separate thread
    std::thread pub_thread([this]() {
        Publisher pub(endpoint_);
        // Give subscriber time to connect
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // Send test image
        pub.send(test_image_);
    });

    // Subscriber receives
    std::thread sub_thread([this]() {
        // Give publisher time to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        Subscriber sub(endpoint_, 2000);  // 2 second timeout

        ImageData received = sub.receive_image();

        EXPECT_EQ(received.filename, test_image_.filename);
        EXPECT_EQ(received.width, test_image_.width);
        EXPECT_EQ(received.height, test_image_.height);
        EXPECT_EQ(received.channels, test_image_.channels);
        EXPECT_EQ(received.data.size(), test_image_.data.size());
    });

    pub_thread.join();
    sub_thread.join();
}

TEST_F(IPCTest, SendReceiveMultipleImages) {
    const int num_images = 5;

    std::thread pub_thread([this, num_images]() {
        Publisher pub(endpoint_);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        for (int i = 0; i < num_images; ++i) {
            ImageData img = test_image_;
            img.filename = "image_" + std::to_string(i) + ".jpg";
            img.timestamp = 1000 + i;
            pub.send(img);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    std::thread sub_thread([this, num_images]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        Subscriber sub(endpoint_, 2000);

        int received_count = 0;
        for (int i = 0; i < num_images; ++i) {
            ImageData received = sub.receive_image();
            if (!received.empty()) {
                received_count++;
                EXPECT_EQ(received.width, test_image_.width);
                EXPECT_EQ(received.height, test_image_.height);
            }
        }

        EXPECT_GT(received_count, 0);
    });

    pub_thread.join();
    sub_thread.join();
}

TEST_F(IPCTest, SendReceiveImageWithFeatures) {
    ImageWithFeatures test_features;
    test_features.image = test_image_;

    // Add keypoints
    for (int i = 0; i < 5; ++i) {
        Keypoint kp;
        kp.x = 10.0f * i;
        kp.y = 20.0f * i;
        kp.size = 5.0f;
        kp.angle = 0.0f;
        kp.response = 1.0f;
        test_features.keypoints.push_back(kp);
    }

    // Add descriptors
    test_features.descriptors.resize(5 * 128);

    std::thread pub_thread([this, &test_features]() {
        Publisher pub(endpoint_);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        pub.send(test_features);
    });

    std::thread sub_thread([this, &test_features]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        Subscriber sub(endpoint_, 2000);

        ImageWithFeatures received = sub.receive_image_with_features();

        EXPECT_EQ(received.image.filename, test_features.image.filename);
        EXPECT_EQ(received.keypoints.size(), test_features.keypoints.size());
        EXPECT_EQ(received.descriptors.size(), test_features.descriptors.size());
    });

    pub_thread.join();
    sub_thread.join();
}

TEST_F(IPCTest, ReceiveTimeout) {
    Subscriber sub(endpoint_, 500);  // 500ms timeout

    // No publisher, should timeout
    auto start = std::chrono::steady_clock::now();
    ImageData received = sub.receive_image();
    auto end = std::chrono::steady_clock::now();

    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Should have timed out and returned empty image
    EXPECT_TRUE(received.empty());
    EXPECT_GE(elapsed_ms, 400);  // At least 400ms (some tolerance)
    EXPECT_LE(elapsed_ms, 700);  // At most 700ms
}

TEST_F(IPCTest, SendRawData) {
    std::vector<uint8_t> test_data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    std::thread pub_thread([this, &test_data]() {
        Publisher pub(endpoint_);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        pub.send(test_data);
    });

    std::thread sub_thread([this]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        Subscriber sub(endpoint_, 2000);

        auto received = sub.receive();
        EXPECT_FALSE(received.empty());
        EXPECT_GE(received.size(), 10u);
    });

    pub_thread.join();
    sub_thread.join();
}

TEST_F(IPCTest, MultipleSubscribers) {
    const int num_subscribers = 3;
    std::atomic<int> received_count{0};

    std::thread pub_thread([this]() {
        Publisher pub(endpoint_);
        std::this_thread::sleep_for(std::chrono::milliseconds(300));

        // Send multiple images
        for (int i = 0; i < 3; ++i) {
            pub.send(test_image_);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    std::vector<std::thread> sub_threads;
    for (int i = 0; i < num_subscribers; ++i) {
        sub_threads.emplace_back([this, &received_count]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            Subscriber sub(endpoint_, 2000);

            for (int j = 0; j < 3; ++j) {
                ImageData received = sub.receive_image();
                if (!received.empty()) {
                    received_count++;
                }
            }
        });
    }

    pub_thread.join();
    for (auto& thread : sub_threads) {
        thread.join();
    }

    // At least some messages should have been received
    EXPECT_GT(received_count.load(), 0);
}

TEST_F(IPCTest, LateSubscriber) {
    // Test that subscriber can connect after publisher is already running

    std::thread pub_thread([this]() {
        Publisher pub(endpoint_);

        // Wait longer for late subscriber
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // Send multiple images
        for (int i = 0; i < 5; ++i) {
            pub.send(test_image_);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    });

    // Subscriber starts later
    std::thread sub_thread([this]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(400));

        Subscriber sub(endpoint_, 2000);

        // Should receive at least some images
        int received = 0;
        for (int i = 0; i < 5; ++i) {
            ImageData img = sub.receive_image();
            if (!img.empty()) {
                received++;
            }
        }

        EXPECT_GT(received, 0);
    });

    pub_thread.join();
    sub_thread.join();
}
