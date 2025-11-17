#include "ipc.h"
#include "message.h"
#include <opencv2/opencv.hpp>
#include <opencv2/features2d.hpp>
#include <iostream>
#include <chrono>
#include <csignal>
#include <atomic>
#include <thread>

// Global flag for graceful shutdown
std::atomic<bool> g_running(true);

void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\nReceived shutdown signal. Exiting gracefully..." << std::endl;
        g_running = false;
    }
}

/**
 * @brief Convert our KeyPoint structure to OpenCV format
 */
std::vector<voyis::KeyPoint> convertKeyPoints(const std::vector<cv::KeyPoint>& cv_keypoints) {
    std::vector<voyis::KeyPoint> keypoints;
    keypoints.reserve(cv_keypoints.size());

    for (const auto& kp : cv_keypoints) {
        voyis::KeyPoint vkp;
        vkp.pt.x = kp.pt.x;
        vkp.pt.y = kp.pt.y;
        vkp.size = kp.size;
        vkp.angle = kp.angle;
        vkp.response = kp.response;
        vkp.octave = kp.octave;
        keypoints.push_back(vkp);
    }

    return keypoints;
}

/**
 * @brief Convert OpenCV descriptor matrix to vector of vectors
 */
std::vector<std::vector<float>> convertDescriptors(const cv::Mat& cv_descriptors) {
    std::vector<std::vector<float>> descriptors;

    if (cv_descriptors.empty()) {
        return descriptors;
    }

    descriptors.reserve(cv_descriptors.rows);
    for (int i = 0; i < cv_descriptors.rows; ++i) {
        std::vector<float> desc;
        desc.reserve(cv_descriptors.cols);
        for (int j = 0; j < cv_descriptors.cols; ++j) {
            desc.push_back(cv_descriptors.at<float>(i, j));
        }
        descriptors.push_back(desc);
    }

    return descriptors;
}

/**
 * @brief Process an image with SIFT feature detection
 */
voyis::ProcessedImageMessage processImage(const voyis::ImageMessage& input_msg) {
    // Decode image from bytes
    cv::Mat image = cv::imdecode(input_msg.image_data, cv::IMREAD_GRAYSCALE);
    if (image.empty()) {
        throw std::runtime_error("Failed to decode image: " + input_msg.image_id);
    }

    // Create SIFT detector
    cv::Ptr<cv::SIFT> sift = cv::SIFT::create();

    // Detect keypoints and compute descriptors
    std::vector<cv::KeyPoint> cv_keypoints;
    cv::Mat cv_descriptors;
    sift->detectAndCompute(image, cv::noArray(), cv_keypoints, cv_descriptors);

    // Create processed message
    voyis::ProcessedImageMessage processed_msg;
    processed_msg.image_id = input_msg.image_id;
    processed_msg.image_data = input_msg.image_data;
    processed_msg.format = input_msg.format;
    processed_msg.width = image.cols;
    processed_msg.height = image.rows;
    processed_msg.timestamp = input_msg.timestamp;
    processed_msg.processed_timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    processed_msg.keypoints = convertKeyPoints(cv_keypoints);
    processed_msg.descriptors = convertDescriptors(cv_descriptors);

    return processed_msg;
}

int main(int argc, char* argv[]) {
    // Set up signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    try {
        std::cout << "Feature Extractor starting..." << std::endl;

        // Create subscriber for receiving images from Image Generator
        const std::string input_endpoint = "tcp://localhost:5555";
        voyis::Subscriber subscriber(input_endpoint, 1000); // 1 second timeout
        std::cout << "Subscriber connected to: " << input_endpoint << std::endl;

        // Create publisher for sending processed images to Data Logger
        const std::string output_endpoint = "tcp://*:5556";
        voyis::Publisher publisher(output_endpoint);
        std::cout << "Publisher bound to: " << output_endpoint << std::endl;

        std::cout << "Waiting for images to process..." << std::endl;
        std::cout << "Press Ctrl+C to stop." << std::endl;

        size_t processed_count = 0;
        size_t total_keypoints = 0;

        // Main processing loop
        while (g_running) {
            std::vector<uint8_t> raw_data;

            // Receive image message
            if (!subscriber.receive(raw_data)) {
                // Timeout or no data, continue waiting
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            try {
                // Deserialize image message
                voyis::ImageMessage img_msg = voyis::ImageMessage::deserialize(raw_data);

                std::cout << "\nReceived image: " << img_msg.image_id
                          << " (" << img_msg.image_data.size() / 1024.0 << " KB)" << std::endl;

                // Process image with SIFT
                auto start_time = std::chrono::high_resolution_clock::now();
                voyis::ProcessedImageMessage processed_msg = processImage(img_msg);
                auto end_time = std::chrono::high_resolution_clock::now();

                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                    end_time - start_time
                ).count();

                ++processed_count;
                total_keypoints += processed_msg.keypoints.size();

                std::cout << "  Dimensions: " << processed_msg.width << "x"
                          << processed_msg.height << std::endl;
                std::cout << "  SIFT keypoints detected: " << processed_msg.keypoints.size()
                          << std::endl;
                std::cout << "  Processing time: " << duration << " ms" << std::endl;

                // Serialize and publish processed message
                std::vector<uint8_t> serialized = processed_msg.serialize();
                if (publisher.publish(serialized)) {
                    std::cout << "  Published processed image ("
                              << serialized.size() / 1024.0 << " KB)" << std::endl;
                } else {
                    std::cerr << "  Failed to publish processed image" << std::endl;
                }

            } catch (const std::exception& e) {
                std::cerr << "Error processing image: " << e.what() << std::endl;
            }
        }

        std::cout << "\nShutdown complete." << std::endl;
        std::cout << "Total images processed: " << processed_count << std::endl;
        if (processed_count > 0) {
            std::cout << "Average keypoints per image: "
                      << total_keypoints / processed_count << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
