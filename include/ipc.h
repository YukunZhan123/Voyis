#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <atomic>

// Forward declare ZeroMQ context and socket to avoid exposing zmq.h
typedef struct zmq_ctx_t zmq_ctx_t;
typedef struct zmq_msg_t zmq_msg_t;

namespace voyis {

/**
 * @brief Publisher for sending messages via ZeroMQ
 *
 * Publishes messages to a specified endpoint. Subscribers can connect
 * to this endpoint to receive messages.
 */
class Publisher {
public:
    /**
     * @brief Construct a publisher
     * @param endpoint ZeroMQ endpoint (e.g., "tcp://*:5555")
     */
    explicit Publisher(const std::string& endpoint);
    ~Publisher();

    // Disable copy
    Publisher(const Publisher&) = delete;
    Publisher& operator=(const Publisher&) = delete;

    /**
     * @brief Publish a message
     * @param data Message data to publish
     * @return true if successful, false otherwise
     */
    bool publish(const std::vector<uint8_t>& data);

    /**
     * @brief Check if publisher is connected
     */
    bool isConnected() const { return connected_; }

private:
    void* context_;
    void* socket_;
    std::string endpoint_;
    std::atomic<bool> connected_;
};

/**
 * @brief Subscriber for receiving messages via ZeroMQ
 *
 * Subscribes to messages from a specified endpoint.
 * Automatically reconnects if the publisher becomes unavailable.
 */
class Subscriber {
public:
    /**
     * @brief Construct a subscriber
     * @param endpoint ZeroMQ endpoint to connect to (e.g., "tcp://localhost:5555")
     * @param timeout_ms Receive timeout in milliseconds (-1 for blocking)
     */
    explicit Subscriber(const std::string& endpoint, int timeout_ms = 1000);
    ~Subscriber();

    // Disable copy
    Subscriber(const Subscriber&) = delete;
    Subscriber& operator=(const Subscriber&) = delete;

    /**
     * @brief Receive a message (blocking or with timeout)
     * @param data Output parameter for received message data
     * @return true if message received, false on timeout or error
     */
    bool receive(std::vector<uint8_t>& data);

    /**
     * @brief Set receive timeout
     * @param timeout_ms Timeout in milliseconds (-1 for blocking)
     */
    void setTimeout(int timeout_ms);

    /**
     * @brief Check if subscriber is connected
     */
    bool isConnected() const { return connected_; }

private:
    void* context_;
    void* socket_;
    std::string endpoint_;
    int timeout_ms_;
    std::atomic<bool> connected_;
};

} // namespace voyis
