#include "system_config.hpp"

#include <set>
#include <stdexcept>

#include <yaml-cpp/yaml.h>

namespace {

template <typename T>
T require(const YAML::Node& node, const std::string& key, const std::string& context) {
    if (!node[key]) {
        throw std::runtime_error(context + ": missing required key '" + key + "'");
    }
    try {
        return node[key].as<T>();
    } catch (const YAML::Exception& e) {
        throw std::runtime_error(context + ": invalid value for '" + key + "': " + e.what());
    }
}

int requirePort(const YAML::Node& node, const std::string& key, const std::string& context) {
    const int port = require<int>(node, key, context);
    if (port <= 0 || port > 65535) {
        throw std::runtime_error(context + ": '" + key + "' out of range: " + std::to_string(port));
    }
    return port;
}

}  // namespace

SystemConfig loadSystemConfig(const std::string& filename, bool sim) {
    YAML::Node root;
    try {
        root = YAML::LoadFile(filename);
    } catch (const YAML::Exception& e) {
        throw std::runtime_error(filename + ": " + e.what());
    }

    SystemConfig config;
    config.system_name = require<std::string>(root, "system_name", filename);
    config.robot_ip = require<std::string>(root, sim ? "sim_ip_address" : "ip_address", filename);
    config.robot_command_port = requirePort(root, "command_address_port", filename);
    config.robot_telemetry_port = requirePort(root, "telemetry_address_port", filename);
    config.web_command_port = requirePort(root, "web_command_port", filename);
    config.web_telemetry_port = requirePort(root, "web_telemetry_port", filename);

    std::set<std::string> ids;
    for (const auto& node : root["sensors"]) {
        const auto id = require<std::string>(node, "id", filename + ": sensors");
        const auto context = filename + ": sensor '" + id + "'";
        const int mask = require<int>(node, "mask", context);
        if (mask <= 0) {
            throw std::runtime_error(context + ": mask must be > 0");
        }
        if (!ids.insert(id).second) {
            throw std::runtime_error(context + ": duplicate sensor id");
        }
        config.sensors.push_back({id, static_cast<uint32_t>(mask)});
    }
    return config;
}

void checkUnique(const std::vector<SystemConfig>& configs) {
    std::set<std::string> names;
    std::set<int> ports;
    for (const auto& c : configs) {
        if (!names.insert(c.system_name).second) {
            throw std::runtime_error("duplicate system_name '" + c.system_name + "'");
        }
        for (int port : {c.web_command_port, c.web_telemetry_port}) {
            if (!ports.insert(port).second) {
                throw std::runtime_error(
                    c.system_name + ": web port " + std::to_string(port) + " already in use by another system");
            }
        }
    }
}
