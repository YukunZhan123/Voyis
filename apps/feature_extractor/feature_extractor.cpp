#include "feature_extractor.hpp"
#include <iostream>
#include <chrono>

namespace dis {

FeatureExtractor::FeatureExtractor(const std::string& subscribe_endpoint,
                                   const std::string& publish_endpoint,
                                   int receive_timeout_ms)
    : subscribe_endpoint_(subscribe_endpoint)
    , publish_endpoint_(publish_endpoint)
    , receive_timeout_ms_(receive_timeout_ms)
    , running_(false)
    , images_processed_(0)
    , total_features_extracted_(0) {

    // Create subscriber for incoming images
    subscriber_ = std::make_unique<Subscriber>(subscribe_endpoint_, receive_timeout_ms_);
    std::cout << "Subscriber connected to " << subscribe_endpoint_ << std::endl;

    // Create publisher for processed images with features
    publisher_ = std::make_unique<Publisher>(publish_endpoint_);
    std::cout << "Publisher bound to " << publish_endpoint_ << std::endl;

    // Create SIFT detector
    try {
        sift_detector_ = cv::SIFT::create();
        std::cout << "SIFT detector initialized" << std::endl;
    } catch (const cv::Exception& e) {
        throw std::runtime_error("Failed to create SIFT detector. "
                               "Make sure OpenCV is built with contrib modules: " +
                               std::string(e.what()));
    }
}

FeatureExtractor::~FeatureExtractor() {
    stop();
}

cv::Mat FeatureExtractor::decode_image(const ImageData& image) {
    if (image.data.empty()) {
        throw std::runtime_error("Empty image data");
    }

    // Decode image from memory buffer
    cv::Mat decoded = cv::imdecode(image.data, cv::IMREAD_COLOR);

    if (decoded.empty()) {
        throw std::runtime_error("Failed to decode image: " + image.filename);
    }

    return decoded;
}

void FeatureExtractor::extract_sift_features(const cv::Mat& image,
                                            std::vector<cv::KeyPoint>& keypoints,
                                            cv::Mat& descriptors) {
    if (image.empty()) {
        throw std::runtime_error("Cannot extract features from empty image");
    }

    // Convert to grayscale if needed (SIFT works on grayscale)
    cv::Mat gray;
    if (image.channels() == 3) {
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = image;
    }

    // Detect keypoints and compute descriptors
    sift_detector_->detectAndCompute(gray, cv::noArray(), keypoints, descriptors);
}

std::vector<Keypoint> FeatureExtractor::convert_keypoints(
    const std::vector<cv::KeyPoint>& cv_keypoints) {

    std::vector<Keypoint> keypoints;
    keypoints.reserve(cv_keypoints.size());

    for (const auto& cv_kp : cv_keypoints) {
        Keypoint kp;
        kp.x = cv_kp.pt.x;
        kp.y = cv_kp.pt.y;
        kp.size = cv_kp.size;
        kp.angle = cv_kp.angle;
        kp.response = cv_kp.response;
        kp.octave = cv_kp.octave;
        kp.class_id = cv_kp.class_id;
        keypoints.push_back(kp);
    }

    return keypoints;
}

std::vector<float> FeatureExtractor::convert_descriptors(const cv::Mat& cv_descriptors) {
    if (cv_descriptors.empty()) {
        return std::vector<float>();
    }

    // SIFT descriptors are typically CV_32F (float)
    // Each descriptor is 128 dimensions
    std::vector<float> descriptors;
    descriptors.reserve(cv_descriptors.rows * cv_descriptors.cols);

    // Convert cv::Mat to flat vector
    if (cv_descriptors.isContinuous()) {
        descriptors.assign(cv_descriptors.ptr<float>(0),
                          cv_descriptors.ptr<float>(0) + cv_descriptors.total());
    } else {
        for (int i = 0; i < cv_descriptors.rows; ++i) {
            descriptors.insert(descriptors.end(),
                             cv_descriptors.ptr<float>(i),
                             cv_descriptors.ptr<float>(i) + cv_descriptors.cols);
        }
    }

    return descriptors;
}

ImageWithFeatures FeatureExtractor::process_image(const ImageData& image) {
    ImageWithFeatures result;

    // Decode image
    cv::Mat mat = decode_image(image);

    // Extract SIFT features
    std::vector<cv::KeyPoint> cv_keypoints;
    cv::Mat cv_descriptors;
    extract_sift_features(mat, cv_keypoints, cv_descriptors);

    // Convert to our format
    result.keypoints = convert_keypoints(cv_keypoints);
    result.descriptors = convert_descriptors(cv_descriptors);

    // Store the original image data along with dimensions
    result.image = image;
    result.image.width = mat.cols;
    result.image.height = mat.rows;
    result.image.channels = mat.channels();

    return result;
}

void FeatureExtractor::run() {
    running_ = true;

    std::cout << "\n=== Starting Feature Extractor ===" << std::endl;
    std::cout << "Subscribe endpoint: " << subscribe_endpoint_ << std::endl;
    std::cout << "Publish endpoint: " << publish_endpoint_ << std::endl;
    std::cout << "Receive timeout: " << receive_timeout_ms_ << "ms" << std::endl;
    std::cout << "==================================\n" << std::endl;

    std::cout << "Waiting for images..." << std::endl;

    while (running_) {
        try {
            // Receive image from App 1
            ImageData image = subscriber_->receive_image();

            if (image.empty()) {
                // Timeout or no data - continue waiting
                continue;
            }

            std::cout << "\nReceived: " << image.filename
                     << " (" << image.data.size() << " bytes)" << std::endl;

            // Process image and extract features
            auto start = std::chrono::steady_clock::now();
            ImageWithFeatures result = process_image(image);
            auto end = std::chrono::steady_clock::now();

            auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                end - start).count();

            images_processed_++;
            total_features_extracted_ += result.keypoints.size();

            std::cout << "  Extracted " << result.keypoints.size() << " keypoints"
                     << " (" << duration_ms << "ms)" << std::endl;
            std::cout << "  Image size: " << result.image.width << "x"
                     << result.image.height << "x" << result.image.channels << std::endl;

            // Publish to App 3
            publisher_->send(result);

            std::cout << "  Published to data logger" << std::endl;

        } catch (const std::exception& e) {
            std::cerr << "Error processing image: " << e.what() << std::endl;
            // Continue processing next image
        }
    }

    std::cout << "\n=== Feature Extractor Stopped ===" << std::endl;
    std::cout << "Total images processed: " << images_processed_ << std::endl;
    std::cout << "Total features extracted: " << total_features_extracted_ << std::endl;
}

void FeatureExtractor::stop() {
    running_ = false;
}

bool FeatureExtractor::is_running() const {
    return running_;
}

size_t FeatureExtractor::get_images_processed() const {
    return images_processed_;
}

size_t FeatureExtractor::get_features_extracted() const {
    return total_features_extracted_;
}

}  // namespace dis
