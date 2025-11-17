#include "image_generator.hpp"
#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include <algorithm>

namespace dis {

namespace fs = std::filesystem;

ImageGenerator::ImageGenerator(const std::string& image_folder,
                               const std::string& publish_endpoint,
                               int delay_ms)
    : image_folder_(image_folder)
    , publish_endpoint_(publish_endpoint)
    , delay_ms_(delay_ms)
    , running_(false)
    , images_sent_(0) {

    // Validate folder exists
    if (!fs::exists(image_folder_)) {
        throw std::runtime_error("Image folder does not exist: " + image_folder_);
    }

    if (!fs::is_directory(image_folder_)) {
        throw std::runtime_error("Path is not a directory: " + image_folder_);
    }

    // Load image paths
    load_image_paths();

    if (image_paths_.empty()) {
        throw std::runtime_error("No images found in folder: " + image_folder_);
    }

    std::cout << "Found " << image_paths_.size() << " images in " << image_folder_ << std::endl;

    // Create publisher
    publisher_ = std::make_unique<Publisher>(publish_endpoint_);
    std::cout << "Publisher bound to " << publish_endpoint_ << std::endl;
}

ImageGenerator::~ImageGenerator() {
    stop();
}

void ImageGenerator::load_image_paths() {
    image_paths_.clear();

    // Supported image extensions
    std::vector<std::string> extensions = {
        ".jpg", ".jpeg", ".png", ".bmp", ".tiff", ".tif"
    };

    try {
        for (const auto& entry : fs::directory_iterator(image_folder_)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                // Convert to lowercase for comparison
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

                if (std::find(extensions.begin(), extensions.end(), ext) != extensions.end()) {
                    image_paths_.push_back(entry.path());
                }
            }
        }
    } catch (const fs::filesystem_error& e) {
        throw std::runtime_error("Error reading directory: " + std::string(e.what()));
    }

    // Sort paths for consistent ordering
    std::sort(image_paths_.begin(), image_paths_.end());
}

ImageData ImageGenerator::load_image_file(const fs::path& image_path) {
    // Open file in binary mode
    std::ifstream file(image_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open image file: " + image_path.string());
    }

    // Get file size
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    // Read entire file into buffer
    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        throw std::runtime_error("Failed to read image file: " + image_path.string());
    }

    // Create ImageData
    ImageData image;
    image.filename = image_path.filename().string();
    image.data = std::move(buffer);
    image.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    // For raw file data, we don't know exact dimensions without decoding
    // Set placeholder values - the Feature Extractor will decode with OpenCV
    image.width = 0;
    image.height = 0;
    image.channels = 0;

    return image;
}

void ImageGenerator::publish_image(const fs::path& image_path) {
    try {
        // Load image data
        ImageData image = load_image_file(image_path);

        // Publish via IPC
        publisher_->send(image);

        images_sent_++;

        std::cout << "[" << images_sent_ << "] Published: " << image.filename
                  << " (" << image.data.size() << " bytes)" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error publishing image " << image_path << ": " << e.what() << std::endl;
    }
}

void ImageGenerator::run() {
    running_ = true;

    std::cout << "\n=== Starting Image Generator ===" << std::endl;
    std::cout << "Publishing endpoint: " << publish_endpoint_ << std::endl;
    std::cout << "Total images: " << image_paths_.size() << std::endl;
    std::cout << "Delay between images: " << delay_ms_ << "ms" << std::endl;
    std::cout << "================================\n" << std::endl;

    size_t image_index = 0;

    while (running_) {
        // Publish current image
        publish_image(image_paths_[image_index]);

        // Move to next image (loop back to start if at end)
        image_index = (image_index + 1) % image_paths_.size();

        // Delay before next image
        std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms_));
    }

    std::cout << "\n=== Image Generator Stopped ===" << std::endl;
    std::cout << "Total images published: " << images_sent_ << std::endl;
}

void ImageGenerator::stop() {
    running_ = false;
}

bool ImageGenerator::is_running() const {
    return running_;
}

size_t ImageGenerator::get_images_sent() const {
    return images_sent_;
}

size_t ImageGenerator::get_total_images() const {
    return image_paths_.size();
}

}  // namespace dis
