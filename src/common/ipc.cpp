#include "ipc.h"
#include <zmq.h>
#include <stdexcept>
#include <cstring>
#include <iostream>
#include <thread>
#include <chrono>

namespace voyis {

// Publisher implementation
Publisher::Publisher(const std::string& endpoint)
    : context_(nullptr), socket_(nullptr), endpoint_(endpoint), connected_(false) {

    // Create ZeroMQ context
    context_ = zmq_ctx_new();
    if (!context_) {
        throw std::runtime_error("Failed to create ZeroMQ context");
    }

    // Create publisher socket
    socket_ = zmq_socket(context_, ZMQ_PUB);
    if (!socket_) {
        zmq_ctx_destroy(context_);
        throw std::runtime_error("Failed to create ZeroMQ socket");
    }

    // Set socket options for better reliability
    int linger = 0; // Don't wait for unsent messages on close
    zmq_setsockopt(socket_, ZMQ_LINGER, &linger, sizeof(linger));

    int sndhwm = 1000; // High water mark for outbound messages
    zmq_setsockopt(socket_, ZMQ_SNDHWM, &sndhwm, sizeof(sndhwm));

    // Bind to endpoint
    if (zmq_bind(socket_, endpoint_.c_str()) != 0) {
        zmq_close(socket_);
        zmq_ctx_destroy(context_);
        throw std::runtime_error("Failed to bind to endpoint: " + endpoint_);
    }

    connected_ = true;

    // Give subscribers time to connect (slow joiner problem mitigation)
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

Publisher::~Publisher() {
    connected_ = false;
    if (socket_) {
        zmq_close(socket_);
    }
    if (context_) {
        zmq_ctx_destroy(context_);
    }
}

bool Publisher::publish(const std::vector<uint8_t>& data) {
    if (!connected_) {
        return false;
    }

    // Send message
    int rc = zmq_send(socket_, data.data(), data.size(), ZMQ_DONTWAIT);
    if (rc == -1) {
        if (errno == EAGAIN) {
            // Would block, queue is full
            return false;
        }
        std::cerr << "Error publishing message: " << zmq_strerror(errno) << std::endl;
        return false;
    }

    return true;
}

// Subscriber implementation
Subscriber::Subscriber(const std::string& endpoint, int timeout_ms)
    : context_(nullptr), socket_(nullptr), endpoint_(endpoint),
      timeout_ms_(timeout_ms), connected_(false) {

    // Create ZeroMQ context
    context_ = zmq_ctx_new();
    if (!context_) {
        throw std::runtime_error("Failed to create ZeroMQ context");
    }

    // Create subscriber socket
    socket_ = zmq_socket(context_, ZMQ_SUB);
    if (!socket_) {
        zmq_ctx_destroy(context_);
        throw std::runtime_error("Failed to create ZeroMQ socket");
    }

    // Set socket options
    int rcvhwm = 1000; // High water mark for inbound messages
    zmq_setsockopt(socket_, ZMQ_RCVHWM, &rcvhwm, sizeof(rcvhwm));

    // Subscribe to all messages (empty filter)
    zmq_setsockopt(socket_, ZMQ_SUBSCRIBE, "", 0);

    // Set receive timeout
    zmq_setsockopt(socket_, ZMQ_RCVTIMEO, &timeout_ms_, sizeof(timeout_ms_));

    // Set reconnect interval
    int reconnect_ivl = 100; // 100ms reconnect interval
    zmq_setsockopt(socket_, ZMQ_RECONNECT_IVL, &reconnect_ivl, sizeof(reconnect_ivl));

    int reconnect_ivl_max = 5000; // Max 5 seconds
    zmq_setsockopt(socket_, ZMQ_RECONNECT_IVL_MAX, &reconnect_ivl_max, sizeof(reconnect_ivl_max));

    // Connect to endpoint
    if (zmq_connect(socket_, endpoint_.c_str()) != 0) {
        zmq_close(socket_);
        zmq_ctx_destroy(context_);
        throw std::runtime_error("Failed to connect to endpoint: " + endpoint_);
    }

    connected_ = true;
}

Subscriber::~Subscriber() {
    connected_ = false;
    if (socket_) {
        zmq_close(socket_);
    }
    if (context_) {
        zmq_ctx_destroy(context_);
    }
}

bool Subscriber::receive(std::vector<uint8_t>& data) {
    if (!connected_) {
        return false;
    }

    zmq_msg_t msg;
    if (zmq_msg_init(&msg) != 0) {
        return false;
    }

    // Receive message
    int rc = zmq_msg_recv(&msg, socket_, 0);
    if (rc == -1) {
        zmq_msg_close(&msg);
        if (errno == EAGAIN || errno == EINTR) {
            // Timeout or interrupted
            return false;
        }
        std::cerr << "Error receiving message: " << zmq_strerror(errno) << std::endl;
        return false;
    }

    // Copy message data
    size_t size = zmq_msg_size(&msg);
    data.resize(size);
    std::memcpy(data.data(), zmq_msg_data(&msg), size);

    zmq_msg_close(&msg);
    return true;
}

void Subscriber::setTimeout(int timeout_ms) {
    timeout_ms_ = timeout_ms;
    if (socket_) {
        zmq_setsockopt(socket_, ZMQ_RCVTIMEO, &timeout_ms_, sizeof(timeout_ms_));
    }
}

} // namespace voyis
