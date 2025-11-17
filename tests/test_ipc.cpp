#include "ipc.h"
#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>

using namespace voyis;

class IPCTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Use a unique endpoint for each test
        endpoint_ = "tcp://127.0.0.1:5999";
    }

    std::string endpoint_;
};

// Test basic publisher creation
TEST_F(IPCTest, PublisherCreation) {
    EXPECT_NO_THROW({
        Publisher pub("tcp://*:5990");
        EXPECT_TRUE(pub.isConnected());
    });
}

// Test basic subscriber creation
TEST_F(IPCTest, SubscriberCreation) {
    EXPECT_NO_THROW({
        Subscriber sub("tcp://localhost:5991", 100);
        EXPECT_TRUE(sub.isConnected());
    });
}

// Test publisher-subscriber communication
TEST_F(IPCTest, PublishSubscribe) {
    // Create publisher
    Publisher pub("tcp://*:5992");
    ASSERT_TRUE(pub.isConnected());

    // Give publisher time to bind
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Create subscriber
    Subscriber sub("tcp://localhost:5992", 1000);
    ASSERT_TRUE(sub.isConnected());

    // Give subscriber time to connect (slow joiner problem)
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Prepare test data
    std::vector<uint8_t> test_data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    // Publish message
    ASSERT_TRUE(pub.publish(test_data));

    // Give some time for message to arrive
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Receive message
    std::vector<uint8_t> received_data;
    ASSERT_TRUE(sub.receive(received_data));

    // Verify data
    EXPECT_EQ(test_data, received_data);
}

// Test multiple messages
TEST_F(IPCTest, MultipleMessages) {
    Publisher pub("tcp://*:5993");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    Subscriber sub("tcp://localhost:5993", 1000);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    const int num_messages = 10;

    // Send multiple messages
    for (int i = 0; i < num_messages; ++i) {
        std::vector<uint8_t> data = {static_cast<uint8_t>(i)};
        ASSERT_TRUE(pub.publish(data));
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Receive all messages
    int received_count = 0;
    for (int i = 0; i < num_messages; ++i) {
        std::vector<uint8_t> received;
        if (sub.receive(received)) {
            ++received_count;
            EXPECT_EQ(1u, received.size());
        }
    }

    // We should receive at least some messages (may not get all due to timing)
    EXPECT_GT(received_count, 0);
}

// Test large message
TEST_F(IPCTest, LargeMessage) {
    Publisher pub("tcp://*:5994");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    Subscriber sub("tcp://localhost:5994", 2000);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Create a large message (1 MB)
    std::vector<uint8_t> large_data(1024 * 1024);
    for (size_t i = 0; i < large_data.size(); ++i) {
        large_data[i] = static_cast<uint8_t>(i % 256);
    }

    // Publish large message
    ASSERT_TRUE(pub.publish(large_data));

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Receive large message
    std::vector<uint8_t> received_data;
    ASSERT_TRUE(sub.receive(received_data));

    // Verify size and some data points
    EXPECT_EQ(large_data.size(), received_data.size());
    EXPECT_EQ(large_data[0], received_data[0]);
    EXPECT_EQ(large_data[large_data.size() / 2], received_data[received_data.size() / 2]);
    EXPECT_EQ(large_data.back(), received_data.back());
}

// Test subscriber timeout
TEST_F(IPCTest, SubscriberTimeout) {
    Subscriber sub("tcp://localhost:5995", 100); // 100ms timeout
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto start = std::chrono::steady_clock::now();

    std::vector<uint8_t> data;
    bool received = sub.receive(data); // Should timeout

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    EXPECT_FALSE(received);
    EXPECT_TRUE(data.empty());
    // Check that timeout was approximately respected (with some tolerance)
    EXPECT_GE(duration, 80);  // At least 80ms
    EXPECT_LE(duration, 200); // At most 200ms
}

// Test empty message
TEST_F(IPCTest, EmptyMessage) {
    Publisher pub("tcp://*:5996");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    Subscriber sub("tcp://localhost:5996", 1000);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::vector<uint8_t> empty_data;
    ASSERT_TRUE(pub.publish(empty_data));

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    std::vector<uint8_t> received_data;
    ASSERT_TRUE(sub.receive(received_data));
    EXPECT_TRUE(received_data.empty());
}

// Test publisher continues after subscriber disconnects
TEST_F(IPCTest, SubscriberDisconnect) {
    Publisher pub("tcp://*:5997");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    {
        Subscriber sub("tcp://localhost:5997", 1000);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        std::vector<uint8_t> data = {1, 2, 3};
        ASSERT_TRUE(pub.publish(data));
        // Subscriber goes out of scope and disconnects
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Publisher should still be able to publish
    std::vector<uint8_t> data = {4, 5, 6};
    EXPECT_TRUE(pub.publish(data));
}

// Test late subscriber (slow joiner)
TEST_F(IPCTest, LateSubscriber) {
    Publisher pub("tcp://*:5998");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Publish some messages before subscriber connects
    for (int i = 0; i < 5; ++i) {
        std::vector<uint8_t> data = {static_cast<uint8_t>(i)};
        pub.publish(data);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Now create subscriber (late joiner)
    Subscriber sub("tcp://localhost:5998", 1000);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Publish more messages
    for (int i = 5; i < 10; ++i) {
        std::vector<uint8_t> data = {static_cast<uint8_t>(i)};
        ASSERT_TRUE(pub.publish(data));
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Subscriber should only receive messages sent after it connected
    std::vector<uint8_t> received;
    int count = 0;
    while (sub.receive(received) && count < 10) {
        ++count;
        // Verify we're getting messages from the second batch (5-9)
        EXPECT_GE(received[0], 5);
    }

    EXPECT_GT(count, 0);
}
