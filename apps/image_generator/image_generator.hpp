#pragma once

#include "ipc_manager.hpp"
#include "message_types.hpp"
#include <string>
#include <vector>
#include <filesystem>
#include <memory>
#include <atomic>

namespace dis {

class ImageGenerator {
public:
    explicit ImageGenerator(const std::string& image_folder,
                           const std::string& publish_endpoint,
                           int delay_ms = 100);
    ~ImageGenerator();

    // Start publishing images (blocking call)
    void run();

    // Stop the generator
    void stop();

    // Check if generator is running
    bool is_running() const;

    // Get statistics
    size_t get_images_sent() const;
    size_t get_total_images() const;

private:
    // Load all image paths from the folder
    void load_image_paths();

    // Load and publish a single image
    void publish_image(const std::filesystem::path& image_path);

    // Read image file into ImageData
    ImageData load_image_file(const std::filesystem::path& image_path);

    std::string image_folder_;
    std::string publish_endpoint_;
    int delay_ms_;

    std::vector<std::filesystem::path> image_paths_;
    std::unique_ptr<Publisher> publisher_;

    std::atomic<bool> running_;
    std::atomic<size_t> images_sent_;
};

}  // namespace dis
