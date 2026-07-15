#pragma once
#include <robot_remote_control/RobotController.hpp>

#include <robot_remote_control/Transports/TransportZmq.hpp>
#include <robot_remote_control/Types/RobotRemoteControl.pb.h>
#include "shared_state.hpp"

#include <unistd.h>
#include <thread>
#include <boost/asio/strand.hpp>
#include <map>

#include <iostream>
#include <nlohmann/json.hpp>

using robot_remote_control::TransportSharedPtr;
using robot_remote_control::TransportZmq;

// forward declaration
class SharedState;

class RobotRemoteController 
{
    boost::shared_ptr<SharedState> state;
    // std::string command_address;
    // std::string telemetry_address;
    std::string system_name;
    YAML::Node config;

    std::thread thread_;
    bool sim;

    std::map<int, std::string> valueNames;
    std::unique_ptr<robot_remote_control::RobotController> controller;
public:
    std::string& getSystemName();
    RobotRemoteController(
        const YAML::Node& config,
        const boost::shared_ptr<SharedState>& state,
        const bool sim);
    void start();
    void join();
    void setSimpleActionCommand(robot_remote_control::SimpleAction action);
    void sendTwist(robot_remote_control::Twist twist);
    ~RobotRemoteController();
};