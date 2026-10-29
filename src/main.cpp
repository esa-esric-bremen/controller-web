#include "robot_relay.hpp"
#include "system_config.hpp"
#include "telemetry_recorder.hpp"

#include <atomic>
#include <csignal>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

std::atomic<bool> running{true};

void stop(int) {
    running = false;
}

void printUsage(const char* name) {
    std::cerr << "usage: " << name << " [--sim] [--record-dir <dir> | --no-record] <config.yaml>...\n"
              << "  --record-dir  where to record all robot data (default: recordings)\n"
              << "  --no-record   disable recording" << std::endl;
}

}  // namespace

int main(int argc, char** argv) {
    bool sim = false;
    bool record = true;
    RecorderConfig recorder_config;
    recorder_config.directory = "recordings";
    std::vector<std::string> files;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--sim") {
            sim = true;
        } else if (arg == "--no-record") {
            record = false;
        } else if (arg == "--record-dir") {
            if (++i == argc) {
                printUsage(argv[0]);
                return 1;
            }
            recorder_config.directory = argv[i];
        } else if (arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg.rfind("--", 0) == 0) {
            std::cerr << "unknown option " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
        } else {
            files.push_back(arg);
        }
    }
    if (files.empty()) {
        printUsage(argv[0]);
        return 1;
    }
    if (sim) {
        std::cout << "------------------------------------------------------\n"
                  << "RUN FOR DEVELOPMENT - CONNECTING TO SIMULATED MACHINES!\n"
                  << "------------------------------------------------------" << std::endl;
    }

    // Load and validate everything before connecting anything.
    std::vector<SystemConfig> configs;
    try {
        for (const auto& file : files) {
            configs.push_back(loadSystemConfig(file, sim));
        }
        checkUnique(configs);
    } catch (const std::exception& e) {
        std::cerr << "Configuration error: " << e.what() << std::endl;
        return 1;
    }

    // Recorders are created before any connection: an unusable directory stops here.
    std::vector<std::shared_ptr<TelemetryRecorder>> recorders;
    try {
        for (const auto& config : configs) {
            recorders.push_back(record ? std::make_shared<TelemetryRecorder>(recorder_config, config) : nullptr);
        }
    } catch (const std::exception& e) {
        std::cerr << "Configuration error: " << e.what() << std::endl;
        return 1;
    }
    if (!record) {
        std::cout << "WARNING: recording disabled" << std::endl;
    }

    std::vector<std::unique_ptr<RobotRelay>> relays;
    for (size_t i = 0; i < configs.size(); ++i) {
        relays.push_back(std::make_unique<RobotRelay>(configs[i], recorders[i]));
    }

    std::signal(SIGINT, stop);
    std::signal(SIGTERM, stop);

    std::vector<std::thread> threads;
    for (auto& relay : relays) {
        threads.emplace_back([&relay]() { relay->run(running); });
    }
    for (auto& thread : threads) {
        thread.join();
    }
    return 0;
}
