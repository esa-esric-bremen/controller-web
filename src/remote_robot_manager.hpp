#pragma once

#include <iostream>
#include <stdexcept>
#include <vector>
#include <memory>
#include <string>

#include <yaml-cpp/yaml.h>
#include "remote_robot_controller.hpp"

#define LOG_MESSAGE 0
#define TELEMETRY_MESSAGE 1
#define ACTION_MESSAGE 2

// forward declaration
class SharedState;

class RobotRemoteController;

class RemoteRobotManager {
private:
    boost::shared_ptr<SharedState> state;
    bool sim;
public:
    RemoteRobotManager(
        const boost::shared_ptr<SharedState>& state,
        bool sim);
    std::vector<std::unique_ptr<RobotRemoteController>> instances;
    void loadFromFile(
        const std::string& filename);
    void startAll();
    void joinAll();
    void sendSimpleAction(std::string sys, std::string name, float value);
    void sendTwist(std::string sys, robot_remote_control::Twist twist);
    ~RemoteRobotManager();
};