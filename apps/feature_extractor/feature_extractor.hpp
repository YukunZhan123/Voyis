#pragma once

#include "ipc_manager.hpp"
#include "message_types.hpp"
#include <opencv2/opencv.hpp>
#include <opencv2/features2d.hpp>
#include <string>
#include <memory>
#include <atomic>

namespace dis {

class FeatureExtractor {
public:
    explicit FeatureExtractor(const std::string& subscribe_endpoint,
                             const std::string& publish_endpoint,
                             int receive_timeout_ms = 5000);
    ~FeatureExtractor();

    // Start processing images (blocking call)
    void run();

    // Stop the extractor
    void stop();

    // Check if extractor is running
    bool is_running() const;

    // Get statistics
    size_t get_images_processed() const;
    size_t get_features_extracted() const;

private:
    // Process a single image
    ImageWithFeatures process_image(const ImageData& image);

    // Decode image data to cv::Mat
    cv::Mat decode_image(const ImageData& image);

    // Extract SIFT features from image
    void extract_sift_features(const cv::Mat& image,
                              std::vector<cv::KeyPoint>& keypoints,
                              cv::Mat& descriptors);

    // Convert OpenCV keypoints to our format
    std::vector<Keypoint> convert_keypoints(const std::vector<cv::KeyPoint>& cv_keypoints);

    // Convert OpenCV descriptors to our format
    std::vector<float> convert_descriptors(const cv::Mat& cv_descriptors);

    std::string subscribe_endpoint_;
    std::string publish_endpoint_;
    int receive_timeout_ms_;

    std::unique_ptr<Subscriber> subscriber_;
    std::unique_ptr<Publisher> publisher_;
    cv::Ptr<cv::SIFT> sift_detector_;

    std::atomic<bool> running_;
    std::atomic<size_t> images_processed_;
    std::atomic<size_t> total_features_extracted_;
};

}  // namespace dis
