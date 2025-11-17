#pragma once

#include "ipc_manager.hpp"
#include "message_types.hpp"
#include <sqlite3.h>
#include <string>
#include <memory>
#include <atomic>

namespace dis {

class DatabaseError : public std::runtime_error {
public:
    explicit DatabaseError(const std::string& msg)
        : std::runtime_error(msg) {}
};

class DataLogger {
public:
    explicit DataLogger(const std::string& subscribe_endpoint,
                       const std::string& database_path,
                       int receive_timeout_ms = 5000);
    ~DataLogger();

    // Start logging data (blocking call)
    void run();

    // Stop the logger
    void stop();

    // Check if logger is running
    bool is_running() const;

    // Get statistics
    size_t get_images_logged() const;
    size_t get_keypoints_logged() const;

private:
    // Initialize database schema
    void init_database();

    // Store image with features in database
    void store_image_with_features(const ImageWithFeatures& data);

    // Execute SQL statement
    void execute_sql(const std::string& sql);

    // Begin transaction
    void begin_transaction();

    // Commit transaction
    void commit_transaction();

    // Rollback transaction
    void rollback_transaction();

    std::string subscribe_endpoint_;
    std::string database_path_;
    int receive_timeout_ms_;

    std::unique_ptr<Subscriber> subscriber_;
    sqlite3* db_;

    std::atomic<bool> running_;
    std::atomic<size_t> images_logged_;
    std::atomic<size_t> keypoints_logged_;
};

}  // namespace dis
