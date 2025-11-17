#include "ipc.h"
#include "message.h"
#include <sqlite3.h>
#include <iostream>
#include <string>
#include <chrono>
#include <csignal>
#include <atomic>
#include <thread>
#include <sstream>

// Global flag for graceful shutdown
std::atomic<bool> g_running(true);

void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\nReceived shutdown signal. Exiting gracefully..." << std::endl;
        g_running = false;
    }
}

/**
 * @brief Database manager class for storing processed images and keypoints
 */
class Database {
public:
    explicit Database(const std::string& db_path) : db_(nullptr) {
        // Open database
        int rc = sqlite3_open(db_path.c_str(), &db_);
        if (rc != SQLITE_OK) {
            throw std::runtime_error("Failed to open database: " +
                                     std::string(sqlite3_errmsg(db_)));
        }

        // Create tables
        createTables();
    }

    ~Database() {
        if (db_) {
            sqlite3_close(db_);
        }
    }

    // Disable copy
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    /**
     * @brief Store a processed image message in the database
     */
    bool storeProcessedImage(const voyis::ProcessedImageMessage& msg) {
        // Begin transaction for better performance
        executeSQL("BEGIN TRANSACTION");

        try {
            // Insert image record
            int64_t image_db_id = insertImage(msg);

            // Insert keypoints
            for (size_t i = 0; i < msg.keypoints.size(); ++i) {
                const auto& kp = msg.keypoints[i];

                // Get descriptor for this keypoint (if available)
                std::vector<float> descriptor;
                if (i < msg.descriptors.size()) {
                    descriptor = msg.descriptors[i];
                }

                insertKeypoint(image_db_id, kp, descriptor);
            }

            // Commit transaction
            executeSQL("COMMIT");
            return true;

        } catch (const std::exception& e) {
            executeSQL("ROLLBACK");
            std::cerr << "Error storing image: " << e.what() << std::endl;
            return false;
        }
    }

    /**
     * @brief Get statistics about stored data
     */
    void printStatistics() {
        auto stmt = prepareStatement(
            "SELECT COUNT(*), SUM(num_keypoints) FROM images"
        );

        if (sqlite3_step(stmt) == SQLITE_ROW) {
            int64_t image_count = sqlite3_column_int64(stmt, 0);
            int64_t total_keypoints = sqlite3_column_int64(stmt, 1);

            std::cout << "\n=== Database Statistics ===" << std::endl;
            std::cout << "Total images stored: " << image_count << std::endl;
            std::cout << "Total keypoints stored: " << total_keypoints << std::endl;
            if (image_count > 0) {
                std::cout << "Average keypoints per image: "
                          << total_keypoints / image_count << std::endl;
            }
            std::cout << "===========================" << std::endl;
        }

        sqlite3_finalize(stmt);
    }

private:
    sqlite3* db_;

    void createTables() {
        // Images table
        std::string create_images_sql = R"(
            CREATE TABLE IF NOT EXISTS images (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                image_id TEXT NOT NULL,
                format TEXT NOT NULL,
                width INTEGER NOT NULL,
                height INTEGER NOT NULL,
                timestamp INTEGER NOT NULL,
                processed_timestamp INTEGER NOT NULL,
                num_keypoints INTEGER NOT NULL,
                image_data BLOB NOT NULL,
                created_at INTEGER NOT NULL
            )
        )";
        executeSQL(create_images_sql);

        // Keypoints table
        std::string create_keypoints_sql = R"(
            CREATE TABLE IF NOT EXISTS keypoints (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                image_id INTEGER NOT NULL,
                x REAL NOT NULL,
                y REAL NOT NULL,
                size REAL NOT NULL,
                angle REAL NOT NULL,
                response REAL NOT NULL,
                octave INTEGER NOT NULL,
                descriptor BLOB,
                FOREIGN KEY (image_id) REFERENCES images(id) ON DELETE CASCADE
            )
        )";
        executeSQL(create_keypoints_sql);

        // Create indices for better query performance
        executeSQL("CREATE INDEX IF NOT EXISTS idx_images_image_id ON images(image_id)");
        executeSQL("CREATE INDEX IF NOT EXISTS idx_keypoints_image_id ON keypoints(image_id)");
    }

    void executeSQL(const std::string& sql) {
        char* err_msg = nullptr;
        int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err_msg);
        if (rc != SQLITE_OK) {
            std::string error = err_msg ? err_msg : "Unknown error";
            sqlite3_free(err_msg);
            throw std::runtime_error("SQL error: " + error);
        }
    }

    sqlite3_stmt* prepareStatement(const std::string& sql) {
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            throw std::runtime_error("Failed to prepare statement: " +
                                     std::string(sqlite3_errmsg(db_)));
        }
        return stmt;
    }

    int64_t insertImage(const voyis::ProcessedImageMessage& msg) {
        auto stmt = prepareStatement(R"(
            INSERT INTO images (
                image_id, format, width, height, timestamp, processed_timestamp,
                num_keypoints, image_data, created_at
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
        )");

        int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();

        sqlite3_bind_text(stmt, 1, msg.image_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, msg.format.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 3, msg.width);
        sqlite3_bind_int(stmt, 4, msg.height);
        sqlite3_bind_int64(stmt, 5, msg.timestamp);
        sqlite3_bind_int64(stmt, 6, msg.processed_timestamp);
        sqlite3_bind_int(stmt, 7, static_cast<int>(msg.keypoints.size()));
        sqlite3_bind_blob(stmt, 8, msg.image_data.data(),
                          static_cast<int>(msg.image_data.size()), SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 9, now);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            throw std::runtime_error("Failed to insert image: " +
                                     std::string(sqlite3_errmsg(db_)));
        }

        int64_t id = sqlite3_last_insert_rowid(db_);
        sqlite3_finalize(stmt);
        return id;
    }

    void insertKeypoint(int64_t image_id, const voyis::KeyPoint& kp,
                       const std::vector<float>& descriptor) {
        auto stmt = prepareStatement(R"(
            INSERT INTO keypoints (
                image_id, x, y, size, angle, response, octave, descriptor
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?)
        )");

        sqlite3_bind_int64(stmt, 1, image_id);
        sqlite3_bind_double(stmt, 2, kp.pt.x);
        sqlite3_bind_double(stmt, 3, kp.pt.y);
        sqlite3_bind_double(stmt, 4, kp.size);
        sqlite3_bind_double(stmt, 5, kp.angle);
        sqlite3_bind_double(stmt, 6, kp.response);
        sqlite3_bind_int(stmt, 7, kp.octave);

        // Store descriptor as binary blob
        if (!descriptor.empty()) {
            sqlite3_bind_blob(stmt, 8, descriptor.data(),
                              static_cast<int>(descriptor.size() * sizeof(float)),
                              SQLITE_TRANSIENT);
        } else {
            sqlite3_bind_null(stmt, 8);
        }

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            throw std::runtime_error("Failed to insert keypoint: " +
                                     std::string(sqlite3_errmsg(db_)));
        }

        sqlite3_finalize(stmt);
    }
};

int main(int argc, char* argv[]) {
    // Parse command line arguments
    std::string db_path = "image_data.db";
    if (argc > 1) {
        db_path = argv[1];
    }

    // Set up signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    try {
        std::cout << "Data Logger starting..." << std::endl;
        std::cout << "Database: " << db_path << std::endl;

        // Initialize database
        Database database(db_path);

        // Create subscriber for receiving processed images from Feature Extractor
        const std::string input_endpoint = "tcp://localhost:5556";
        voyis::Subscriber subscriber(input_endpoint, 1000); // 1 second timeout
        std::cout << "Subscriber connected to: " << input_endpoint << std::endl;

        std::cout << "Waiting for processed images to log..." << std::endl;
        std::cout << "Press Ctrl+C to stop." << std::endl;

        size_t stored_count = 0;
        size_t total_keypoints = 0;

        // Main logging loop
        while (g_running) {
            std::vector<uint8_t> raw_data;

            // Receive processed message
            if (!subscriber.receive(raw_data)) {
                // Timeout or no data, continue waiting
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            try {
                // Deserialize processed message
                voyis::ProcessedImageMessage msg =
                    voyis::ProcessedImageMessage::deserialize(raw_data);

                std::cout << "\nReceived processed image: " << msg.image_id << std::endl;
                std::cout << "  Dimensions: " << msg.width << "x" << msg.height << std::endl;
                std::cout << "  Keypoints: " << msg.keypoints.size() << std::endl;
                std::cout << "  Descriptors: " << msg.descriptors.size() << std::endl;

                // Store in database
                if (database.storeProcessedImage(msg)) {
                    ++stored_count;
                    total_keypoints += msg.keypoints.size();
                    std::cout << "  Successfully stored in database" << std::endl;
                } else {
                    std::cerr << "  Failed to store in database" << std::endl;
                }

            } catch (const std::exception& e) {
                std::cerr << "Error processing message: " << e.what() << std::endl;
            }
        }

        std::cout << "\nShutdown complete." << std::endl;
        std::cout << "Total images stored: " << stored_count << std::endl;
        std::cout << "Total keypoints stored: " << total_keypoints << std::endl;

        // Print final statistics
        database.printStatistics();

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
