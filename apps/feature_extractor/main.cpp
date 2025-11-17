#include "feature_extractor.hpp"
#include <iostream>
#include <csignal>
#include <memory>

// Global pointer for signal handler
std::unique_ptr<dis::FeatureExtractor> g_extractor;

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\n\nReceived shutdown signal..." << std::endl;
        if (g_extractor) {
            g_extractor->stop();
        }
    }
}

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]" << std::endl;
    std::cout << "\nOptions:" << std::endl;
    std::cout << "  --sub-endpoint <endpoint>   Subscriber endpoint (default: tcp://localhost:5555)" << std::endl;
    std::cout << "  --pub-endpoint <endpoint>   Publisher endpoint (default: tcp://*:5556)" << std::endl;
    std::cout << "  --timeout <ms>              Receive timeout in milliseconds (default: 5000)" << std::endl;
    std::cout << "  --help                      Show this help message" << std::endl;
    std::cout << "\nExample:" << std::endl;
    std::cout << "  " << program_name << " --sub-endpoint tcp://localhost:5555 --pub-endpoint tcp://*:5556" << std::endl;
}

int main(int argc, char* argv[]) {
    // Default parameters
    std::string sub_endpoint = "tcp://localhost:5555";
    std::string pub_endpoint = "tcp://*:5556";
    int timeout_ms = 5000;

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--sub-endpoint") {
            if (i + 1 < argc) {
                sub_endpoint = argv[++i];
            } else {
                std::cerr << "Error: --sub-endpoint requires a value" << std::endl;
                return 1;
            }
        } else if (arg == "--pub-endpoint") {
            if (i + 1 < argc) {
                pub_endpoint = argv[++i];
            } else {
                std::cerr << "Error: --pub-endpoint requires a value" << std::endl;
                return 1;
            }
        } else if (arg == "--timeout") {
            if (i + 1 < argc) {
                try {
                    timeout_ms = std::stoi(argv[++i]);
                    if (timeout_ms <= 0) {
                        std::cerr << "Error: timeout must be positive" << std::endl;
                        return 1;
                    }
                } catch (...) {
                    std::cerr << "Error: invalid timeout value" << std::endl;
                    return 1;
                }
            } else {
                std::cerr << "Error: --timeout requires a value" << std::endl;
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
        // Create and run feature extractor
        g_extractor = std::make_unique<dis::FeatureExtractor>(
            sub_endpoint, pub_endpoint, timeout_ms);

        g_extractor->run();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
