#include "data_logger.hpp"
#include <iostream>
#include <csignal>
#include <memory>

// Global pointer for signal handler
std::unique_ptr<dis::DataLogger> g_logger;

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\n\nReceived shutdown signal..." << std::endl;
        if (g_logger) {
            g_logger->stop();
        }
    }
}

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]" << std::endl;
    std::cout << "\nOptions:" << std::endl;
    std::cout << "  --endpoint <endpoint>   Subscriber endpoint (default: tcp://localhost:5556)" << std::endl;
    std::cout << "  --database <path>       Database file path (default: ./imaging_data.db)" << std::endl;
    std::cout << "  --timeout <ms>          Receive timeout in milliseconds (default: 5000)" << std::endl;
    std::cout << "  --help                  Show this help message" << std::endl;
    std::cout << "\nExample:" << std::endl;
    std::cout << "  " << program_name << " --endpoint tcp://localhost:5556 --database ./data.db" << std::endl;
}

int main(int argc, char* argv[]) {
    // Default parameters
    std::string endpoint = "tcp://localhost:5556";
    std::string database_path = "./imaging_data.db";
    int timeout_ms = 5000;

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
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
        } else if (arg == "--database") {
            if (i + 1 < argc) {
                database_path = argv[++i];
            } else {
                std::cerr << "Error: --database requires a value" << std::endl;
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
        // Create and run data logger
        g_logger = std::make_unique<dis::DataLogger>(
            endpoint, database_path, timeout_ms);

        g_logger->run();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
