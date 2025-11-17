#pragma once

#include "message_types.hpp"
#include <string>
#include <memory>
#include <functional>
#include <stdexcept>

// Forward declare ZeroMQ types to avoid including zmq.hpp in header
typedef struct zmq_msg_t zmq_msg_t;

namespace dis {

class IPCError : public std::runtime_error {
public:
    explicit IPCError(const std::string& msg)
        : std::runtime_error(msg) {}
};

// ZeroMQ Publisher - sends messages
class Publisher {
public:
    explicit Publisher(const std::string& endpoint);
    ~Publisher();

    // Disable copy, allow move
    Publisher(const Publisher&) = delete;
    Publisher& operator=(const Publisher&) = delete;
    Publisher(Publisher&&) noexcept;
    Publisher& operator=(Publisher&&) noexcept;

    // Send raw binary data
    void send(const std::vector<uint8_t>& data);

    // Send ImageData
    void send(const ImageData& image);

    // Send ImageWithFeatures
    void send(const ImageWithFeatures& image_features);

    // Check if publisher is connected
    bool is_connected() const;

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

// ZeroMQ Subscriber - receives messages
class Subscriber {
public:
    explicit Subscriber(const std::string& endpoint, int timeout_ms = 1000);
    ~Subscriber();

    // Disable copy, allow move
    Subscriber(const Subscriber&) = delete;
    Subscriber& operator=(const Subscriber&) = delete;
    Subscriber(Subscriber&&) noexcept;
    Subscriber& operator=(Subscriber&&) noexcept;

    // Receive raw binary data (blocking with timeout)
    // Returns empty vector on timeout
    std::vector<uint8_t> receive();

    // Receive and deserialize ImageData
    // Returns empty ImageData on timeout
    ImageData receive_image();

    // Receive and deserialize ImageWithFeatures
    // Returns empty ImageWithFeatures on timeout
    ImageWithFeatures receive_image_with_features();

    // Set receive timeout in milliseconds
    void set_timeout(int timeout_ms);

    // Check if subscriber is connected
    bool is_connected() const;

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

}  // namespace dis
