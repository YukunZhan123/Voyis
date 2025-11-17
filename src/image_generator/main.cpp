#include "ipc.h"
#include "message.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <csignal>
#include <atomic>

namespace fs = std::filesystem;

// Global flag for graceful shutdown
std::atomic<bool> g_running(true);

void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\nReceived shutdown signal. Exiting gracefully..." << std::endl;
        g_running = false;
    }
}

/**
 * @brief Read all bytes from a file
 */
std::vector<uint8_t> readFile(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("Failed to open file: " + filepath);
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        throw std::runtime_error("Failed to read file: " + filepath);
    }

    return buffer;
}

/**
 * @brief Get file extension (lowercase)
 */
std::string getFileExtension(const std::string& filepath) {
    fs::path p(filepath);
    std::string ext = p.extension().string();
    if (!ext.empty() && ext[0] == '.') {
        ext = ext.substr(1);
    }
    // Convert to lowercase
    for (char& c : ext) {
        c = std::tolower(c);
    }
    return ext;
}

/**
 * @brief Check if file is a supported image format
 */
bool isImageFile(const std::string& filepath) {
    std::string ext = getFileExtension(filepath);
    return ext == "jpg" || ext == "jpeg" || ext == "png" ||
           ext == "bmp" || ext == "tiff" || ext == "tif";
}

/**
 * @brief Collect all image files from a directory
 */
std::vector<std::string> collectImageFiles(const std::string& directory) {
    std::vector<std::string> image_files;

    if (!fs::exists(directory)) {
        throw std::runtime_error("Directory does not exist: " + directory);
    }

    if (!fs::is_directory(directory)) {
        throw std::runtime_error("Path is not a directory: " + directory);
    }

    for (const auto& entry : fs::directory_iterator(directory)) {
        if (entry.is_regular_file() && isImageFile(entry.path().string())) {
            image_files.push_back(entry.path().string());
        }
    }

    std::sort(image_files.begin(), image_files.end());
    return image_files;
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <image_directory>" << std::endl;
        return 1;
    }

    std::string image_dir = argv[1];

    // Set up signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    try {
        std::cout << "Image Generator starting..." << std::endl;
        std::cout << "Image directory: " << image_dir << std::endl;

        // Collect all image files
        std::vector<std::string> image_files = collectImageFiles(image_dir);
        if (image_files.empty()) {
            std::cerr << "No image files found in directory: " << image_dir << std::endl;
            return 1;
        }

        std::cout << "Found " << image_files.size() << " image file(s)" << std::endl;

        // Create publisher
        const std::string endpoint = "tcp://*:5555";
        voyis::Publisher publisher(endpoint);
        std::cout << "Publisher bound to: " << endpoint << std::endl;
        std::cout << "Publishing images in a continuous loop..." << std::endl;
        std::cout << "Press Ctrl+C to stop." << std::endl;

        size_t image_count = 0;
        size_t total_bytes = 0;

        // Continuously loop through images
        while (g_running) {
            for (size_t i = 0; i < image_files.size() && g_running; ++i) {
                const std::string& filepath = image_files[i];

                try {
                    // Read image file
                    std::vector<uint8_t> image_data = readFile(filepath);
                    total_bytes += image_data.size();

                    // Create message
                    voyis::ImageMessage msg;
                    msg.image_id = fs::path(filepath).filename().string() +
                                   "_" + std::to_string(image_count);
                    msg.image_data = std::move(image_data);
                    msg.format = getFileExtension(filepath);
                    msg.width = 0;  // Will be determined by receiver
                    msg.height = 0;
                    msg.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()
                    ).count();

                    // Serialize and publish
                    std::vector<uint8_t> serialized = msg.serialize();
                    if (publisher.publish(serialized)) {
                        ++image_count;
                        std::cout << "[" << image_count << "] Published: " << filepath
                                  << " (" << msg.image_data.size() / 1024.0 << " KB)"
                                  << std::endl;
                    } else {
                        std::cerr << "Failed to publish image: " << filepath << std::endl;
                    }

                    // Small delay to avoid overwhelming the system
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));

                } catch (const std::exception& e) {
                    std::cerr << "Error processing image " << filepath << ": "
                              << e.what() << std::endl;
                }
            }
        }

        std::cout << "\nShutdown complete." << std::endl;
        std::cout << "Total images published: " << image_count << std::endl;
        std::cout << "Total data sent: " << total_bytes / (1024.0 * 1024.0) << " MB" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
