#include "ipc_manager.hpp"
#include "serialization.hpp"
#include <zmq.h>
#include <cstring>
#include <iostream>

namespace dis {

// Publisher Implementation
class Publisher::Impl {
public:
    explicit Impl(const std::string& endpoint)
        : context_(nullptr), socket_(nullptr), endpoint_(endpoint) {
        // Create ZeroMQ context
        context_ = zmq_ctx_new();
        if (!context_) {
            throw IPCError("Failed to create ZeroMQ context");
        }

        // Create publisher socket
        socket_ = zmq_socket(context_, ZMQ_PUB);
        if (!socket_) {
            zmq_ctx_destroy(context_);
            throw IPCError("Failed to create publisher socket");
        }

        // Bind to endpoint
        if (zmq_bind(socket_, endpoint.c_str()) != 0) {
            zmq_close(socket_);
            zmq_ctx_destroy(context_);
            throw IPCError("Failed to bind publisher to " + endpoint + ": " +
                          std::string(zmq_strerror(zmq_errno())));
        }

        // Give ZeroMQ time to establish connections
        // This helps prevent lost messages when subscribers connect later
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    ~Impl() {
        if (socket_) {
            zmq_close(socket_);
        }
        if (context_) {
            zmq_ctx_destroy(context_);
        }
    }

    void send(const std::vector<uint8_t>& data) {
        if (data.empty()) {
            throw IPCError("Cannot send empty data");
        }

        int rc = zmq_send(socket_, data.data(), data.size(), 0);
        if (rc == -1) {
            throw IPCError("Failed to send message: " +
                          std::string(zmq_strerror(zmq_errno())));
        }
    }

    bool is_connected() const {
        return socket_ != nullptr;
    }

private:
    void* context_;
    void* socket_;
    std::string endpoint_;
};

Publisher::Publisher(const std::string& endpoint)
    : pimpl_(std::make_unique<Impl>(endpoint)) {}

Publisher::~Publisher() = default;

Publisher::Publisher(Publisher&&) noexcept = default;
Publisher& Publisher::operator=(Publisher&&) noexcept = default;

void Publisher::send(const std::vector<uint8_t>& data) {
    pimpl_->send(data);
}

void Publisher::send(const ImageData& image) {
    auto data = Serializer::serialize(image);
    pimpl_->send(data);
}

void Publisher::send(const ImageWithFeatures& image_features) {
    auto data = Serializer::serialize(image_features);
    pimpl_->send(data);
}

bool Publisher::is_connected() const {
    return pimpl_->is_connected();
}

// Subscriber Implementation
class Subscriber::Impl {
public:
    explicit Impl(const std::string& endpoint, int timeout_ms)
        : context_(nullptr), socket_(nullptr), endpoint_(endpoint), timeout_ms_(timeout_ms) {
        // Create ZeroMQ context
        context_ = zmq_ctx_new();
        if (!context_) {
            throw IPCError("Failed to create ZeroMQ context");
        }

        // Create subscriber socket
        socket_ = zmq_socket(context_, ZMQ_SUB);
        if (!socket_) {
            zmq_ctx_destroy(context_);
            throw IPCError("Failed to create subscriber socket");
        }

        // Set receive timeout
        if (zmq_setsockopt(socket_, ZMQ_RCVTIMEO, &timeout_ms_, sizeof(timeout_ms_)) != 0) {
            zmq_close(socket_);
            zmq_ctx_destroy(context_);
            throw IPCError("Failed to set receive timeout");
        }

        // Subscribe to all messages (empty filter)
        if (zmq_setsockopt(socket_, ZMQ_SUBSCRIBE, "", 0) != 0) {
            zmq_close(socket_);
            zmq_ctx_destroy(context_);
            throw IPCError("Failed to set subscription filter");
        }

        // Connect to endpoint
        if (zmq_connect(socket_, endpoint.c_str()) != 0) {
            zmq_close(socket_);
            zmq_ctx_destroy(context_);
            throw IPCError("Failed to connect subscriber to " + endpoint + ": " +
                          std::string(zmq_strerror(zmq_errno())));
        }

        // Give ZeroMQ time to establish connection
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    ~Impl() {
        if (socket_) {
            zmq_close(socket_);
        }
        if (context_) {
            zmq_ctx_destroy(context_);
        }
    }

    std::vector<uint8_t> receive() {
        zmq_msg_t msg;
        if (zmq_msg_init(&msg) != 0) {
            throw IPCError("Failed to initialize message");
        }

        int rc = zmq_msg_recv(&msg, socket_, 0);
        if (rc == -1) {
            int err = zmq_errno();
            zmq_msg_close(&msg);

            // Timeout is not an error, return empty vector
            if (err == EAGAIN) {
                return std::vector<uint8_t>();
            }

            throw IPCError("Failed to receive message: " +
                          std::string(zmq_strerror(err)));
        }

        // Copy data from message
        size_t size = zmq_msg_size(&msg);
        std::vector<uint8_t> data(size);
        std::memcpy(data.data(), zmq_msg_data(&msg), size);

        zmq_msg_close(&msg);
        return data;
    }

    void set_timeout(int timeout_ms) {
        timeout_ms_ = timeout_ms;
        if (socket_) {
            zmq_setsockopt(socket_, ZMQ_RCVTIMEO, &timeout_ms_, sizeof(timeout_ms_));
        }
    }

    bool is_connected() const {
        return socket_ != nullptr;
    }

private:
    void* context_;
    void* socket_;
    std::string endpoint_;
    int timeout_ms_;
};

Subscriber::Subscriber(const std::string& endpoint, int timeout_ms)
    : pimpl_(std::make_unique<Impl>(endpoint, timeout_ms)) {}

Subscriber::~Subscriber() = default;

Subscriber::Subscriber(Subscriber&&) noexcept = default;
Subscriber& Subscriber::operator=(Subscriber&&) noexcept = default;

std::vector<uint8_t> Subscriber::receive() {
    return pimpl_->receive();
}

ImageData Subscriber::receive_image() {
    auto data = pimpl_->receive();
    if (data.empty()) {
        return ImageData();
    }
    return Serializer::deserialize_image(data);
}

ImageWithFeatures Subscriber::receive_image_with_features() {
    auto data = pimpl_->receive();
    if (data.empty()) {
        return ImageWithFeatures();
    }
    return Serializer::deserialize_image_with_features(data);
}

void Subscriber::set_timeout(int timeout_ms) {
    pimpl_->set_timeout(timeout_ms);
}

bool Subscriber::is_connected() const {
    return pimpl_->is_connected();
}

}  // namespace dis
