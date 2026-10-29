#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct SensorConfig {
    std::string id;
    uint32_t mask;
};

struct SystemConfig {
    std::string system_name;

    std::string robot_ip;
    int robot_command_port;
    int robot_telemetry_port;

    int web_command_port;
    int web_telemetry_port;

    // order matters, it has to match the order the robot packs its values!
    std::vector<SensorConfig> sensors;
};

// Throws std::runtime_error naming the file and the offending key.
SystemConfig loadSystemConfig(const std::string& filename, bool sim);

// Throws std::runtime_error if two systems share a name or a port.
void checkUnique(const std::vector<SystemConfig>& configs);
