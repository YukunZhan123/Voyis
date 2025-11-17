#include "data_logger.hpp"
#include <iostream>
#include <sstream>
#include <chrono>
#include <iomanip>

namespace dis {

DataLogger::DataLogger(const std::string& subscribe_endpoint,
                       const std::string& database_path,
                       int receive_timeout_ms)
    : subscribe_endpoint_(subscribe_endpoint)
    , database_path_(database_path)
    , receive_timeout_ms_(receive_timeout_ms)
    , db_(nullptr)
    , running_(false)
    , images_logged_(0)
    , keypoints_logged_(0) {

    // Create subscriber
    subscriber_ = std::make_unique<Subscriber>(subscribe_endpoint_, receive_timeout_ms_);
    std::cout << "Subscriber connected to " << subscribe_endpoint_ << std::endl;

    // Open database
    int rc = sqlite3_open(database_path_.c_str(), &db_);
    if (rc != SQLITE_OK) {
        throw DatabaseError("Failed to open database: " + std::string(sqlite3_errmsg(db_)));
    }

    std::cout << "Database opened: " << database_path_ << std::endl;

    // Enable WAL mode for better concurrency
    execute_sql("PRAGMA journal_mode=WAL;");

    // Initialize database schema
    init_database();
}

DataLogger::~DataLogger() {
    stop();

    if (db_) {
        sqlite3_close(db_);
    }
}

void DataLogger::execute_sql(const std::string& sql) {
    char* error_msg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &error_msg);

    if (rc != SQLITE_OK) {
        std::string error = error_msg ? error_msg : "Unknown error";
        sqlite3_free(error_msg);
        throw DatabaseError("SQL execution failed: " + error);
    }
}

void DataLogger::init_database() {
    std::cout << "Initializing database schema..." << std::endl;

    // Create images table
    const char* create_images_table = R"(
        CREATE TABLE IF NOT EXISTS images (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            filename TEXT NOT NULL,
            timestamp INTEGER NOT NULL,
            width INTEGER NOT NULL,
            height INTEGER NOT NULL,
            channels INTEGER NOT NULL,
            image_data BLOB NOT NULL,
            data_size INTEGER NOT NULL,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        );
    )";

    execute_sql(create_images_table);

    // Create keypoints table
    const char* create_keypoints_table = R"(
        CREATE TABLE IF NOT EXISTS keypoints (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            image_id INTEGER NOT NULL,
            x REAL NOT NULL,
            y REAL NOT NULL,
            size REAL NOT NULL,
            angle REAL NOT NULL,
            response REAL NOT NULL,
            octave INTEGER NOT NULL,
            class_id INTEGER NOT NULL,
            FOREIGN KEY(image_id) REFERENCES images(id) ON DELETE CASCADE
        );
    )";

    execute_sql(create_keypoints_table);

    // Create descriptors table
    const char* create_descriptors_table = R"(
        CREATE TABLE IF NOT EXISTS descriptors (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            image_id INTEGER NOT NULL,
            descriptor_data BLOB NOT NULL,
            FOREIGN KEY(image_id) REFERENCES images(id) ON DELETE CASCADE
        );
    )";

    execute_sql(create_descriptors_table);

    // Create indices for better query performance
    execute_sql("CREATE INDEX IF NOT EXISTS idx_keypoints_image_id ON keypoints(image_id);");
    execute_sql("CREATE INDEX IF NOT EXISTS idx_descriptors_image_id ON descriptors(image_id);");
    execute_sql("CREATE INDEX IF NOT EXISTS idx_images_filename ON images(filename);");
    execute_sql("CREATE INDEX IF NOT EXISTS idx_images_timestamp ON images(timestamp);");

    std::cout << "Database schema initialized" << std::endl;
}

void DataLogger::begin_transaction() {
    execute_sql("BEGIN TRANSACTION;");
}

void DataLogger::commit_transaction() {
    execute_sql("COMMIT;");
}

void DataLogger::rollback_transaction() {
    execute_sql("ROLLBACK;");
}

void DataLogger::store_image_with_features(const ImageWithFeatures& data) {
    begin_transaction();

    try {
        // Insert image
        const char* insert_image_sql = R"(
            INSERT INTO images (filename, timestamp, width, height, channels, image_data, data_size)
            VALUES (?, ?, ?, ?, ?, ?, ?);
        )";

        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, insert_image_sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            throw DatabaseError("Failed to prepare image insert statement");
        }

        // Bind parameters
        sqlite3_bind_text(stmt, 1, data.image.filename.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 2, data.image.timestamp);
        sqlite3_bind_int(stmt, 3, data.image.width);
        sqlite3_bind_int(stmt, 4, data.image.height);
        sqlite3_bind_int(stmt, 5, data.image.channels);
        sqlite3_bind_blob(stmt, 6, data.image.data.data(), data.image.data.size(), SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 7, data.image.data.size());

        // Execute
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            throw DatabaseError("Failed to insert image");
        }

        // Get inserted image ID
        sqlite3_int64 image_id = sqlite3_last_insert_rowid(db_);
        sqlite3_finalize(stmt);

        // Insert keypoints
        if (!data.keypoints.empty()) {
            const char* insert_keypoint_sql = R"(
                INSERT INTO keypoints (image_id, x, y, size, angle, response, octave, class_id)
                VALUES (?, ?, ?, ?, ?, ?, ?, ?);
            )";

            rc = sqlite3_prepare_v2(db_, insert_keypoint_sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                throw DatabaseError("Failed to prepare keypoint insert statement");
            }

            for (const auto& kp : data.keypoints) {
                sqlite3_bind_int64(stmt, 1, image_id);
                sqlite3_bind_double(stmt, 2, kp.x);
                sqlite3_bind_double(stmt, 3, kp.y);
                sqlite3_bind_double(stmt, 4, kp.size);
                sqlite3_bind_double(stmt, 5, kp.angle);
                sqlite3_bind_double(stmt, 6, kp.response);
                sqlite3_bind_int(stmt, 7, kp.octave);
                sqlite3_bind_int(stmt, 8, kp.class_id);

                rc = sqlite3_step(stmt);
                if (rc != SQLITE_DONE) {
                    sqlite3_finalize(stmt);
                    throw DatabaseError("Failed to insert keypoint");
                }

                sqlite3_reset(stmt);
            }

            sqlite3_finalize(stmt);
        }

        // Insert descriptors
        if (!data.descriptors.empty()) {
            const char* insert_descriptor_sql = R"(
                INSERT INTO descriptors (image_id, descriptor_data)
                VALUES (?, ?);
            )";

            rc = sqlite3_prepare_v2(db_, insert_descriptor_sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                throw DatabaseError("Failed to prepare descriptor insert statement");
            }

            sqlite3_bind_int64(stmt, 1, image_id);
            sqlite3_bind_blob(stmt, 2, data.descriptors.data(),
                            data.descriptors.size() * sizeof(float), SQLITE_TRANSIENT);

            rc = sqlite3_step(stmt);
            if (rc != SQLITE_DONE) {
                sqlite3_finalize(stmt);
                throw DatabaseError("Failed to insert descriptors");
            }

            sqlite3_finalize(stmt);
        }

        commit_transaction();

        images_logged_++;
        keypoints_logged_ += data.keypoints.size();

    } catch (const std::exception& e) {
        rollback_transaction();
        throw;
    }
}

void DataLogger::run() {
    running_ = true;

    std::cout << "\n=== Starting Data Logger ===" << std::endl;
    std::cout << "Subscribe endpoint: " << subscribe_endpoint_ << std::endl;
    std::cout << "Database: " << database_path_ << std::endl;
    std::cout << "Receive timeout: " << receive_timeout_ms_ << "ms" << std::endl;
    std::cout << "============================\n" << std::endl;

    std::cout << "Waiting for data..." << std::endl;

    while (running_) {
        try {
            // Receive image with features from App 2
            ImageWithFeatures data = subscriber_->receive_image_with_features();

            if (data.image.empty()) {
                // Timeout or no data - continue waiting
                continue;
            }

            std::cout << "\nReceived: " << data.image.filename
                     << " with " << data.keypoints.size() << " keypoints" << std::endl;

            // Store in database
            auto start = std::chrono::steady_clock::now();
            store_image_with_features(data);
            auto end = std::chrono::steady_clock::now();

            auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                end - start).count();

            std::cout << "  Stored in database (" << duration_ms << "ms)" << std::endl;
            std::cout << "  Total images logged: " << images_logged_ << std::endl;
            std::cout << "  Total keypoints logged: " << keypoints_logged_ << std::endl;

        } catch (const std::exception& e) {
            std::cerr << "Error logging data: " << e.what() << std::endl;
            // Continue processing next data
        }
    }

    std::cout << "\n=== Data Logger Stopped ===" << std::endl;
    std::cout << "Total images logged: " << images_logged_ << std::endl;
    std::cout << "Total keypoints logged: " << keypoints_logged_ << std::endl;
}

void DataLogger::stop() {
    running_ = false;
}

bool DataLogger::is_running() const {
    return running_;
}

size_t DataLogger::get_images_logged() const {
    return images_logged_;
}

size_t DataLogger::get_keypoints_logged() const {
    return keypoints_logged_;
}

}  // namespace dis
