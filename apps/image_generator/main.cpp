#include "image_generator.hpp"
#include <iostream>
#include <csignal>
#include <memory>

// Global pointer for signal handler
std::unique_ptr<dis::ImageGenerator> g_generator;

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\n\nReceived shutdown signal..." << std::endl;
        if (g_generator) {
            g_generator->stop();
        }
    }
}

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " <image_folder> [options]" << std::endl;
    std::cout << "\nOptions:" << std::endl;
    std::cout << "  --endpoint <endpoint>  Publisher endpoint (default: tcp://*:5555)" << std::endl;
    std::cout << "  --delay <ms>           Delay between images in milliseconds (default: 100)" << std::endl;
    std::cout << "  --help                 Show this help message" << std::endl;
    std::cout << "\nExample:" << std::endl;
    std::cout << "  " << program_name << " /path/to/images --endpoint tcp://*:5555 --delay 200" << std::endl;
}

int main(int argc, char* argv[]) {
    // Default parameters
    std::string image_folder;
    std::string endpoint = "tcp://*:5555";
    int delay_ms = 100;

    // Parse command line arguments
    if (argc < 2) {
        std::cerr << "Error: Image folder path is required" << std::endl;
        print_usage(argv[0]);
        return 1;
    }

    image_folder = argv[1];

    // Parse optional arguments
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--endpoint") {
            if (i + 1 < argc) {
                endpoint = argv[++i];
            } else {
                std::cerr << "Error: --endpoint requires a value" << std::endl;
                return 1;
            }
        } else if (arg == "--delay") {
            if (i + 1 < argc) {
                try {
                    delay_ms = std::stoi(argv[++i]);
                    if (delay_ms < 0) {
                        std::cerr << "Error: delay must be non-negative" << std::endl;
                        return 1;
                    }
                } catch (...) {
                    std::cerr << "Error: invalid delay value" << std::endl;
                    return 1;
                }
            } else {
                std::cerr << "Error: --delay requires a value" << std::endl;
                return 1;
            }
        } else {
            std::cerr << "Error: unknown argument: " << arg << std::endl;
            print_usage(argv[0]);
            return 1;
        }
    }

    // Set up signal handlers for graceful shutdown
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    try {
        // Create and run generator
        g_generator = std::make_unique<dis::ImageGenerator>(
            image_folder, endpoint, delay_ms);

        g_generator->run();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
