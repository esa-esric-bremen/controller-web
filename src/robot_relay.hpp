// Relays one robot to browsers using robot_remote_control on both ends:
//
//   robot  <--ZeroMQ--  RobotController | ControlledRobot  --http (commands)-->      browser
//                                                          --websocket (telemetry)-->
//
// The robot keeps its ZeroMQ interface (single connection, works over the field link),
// browsers get RRC's JSON protocol and need no custom message format.
#pragma once

#include <atomic>
#include <chrono>
#include <memory>

#include <robot_remote_control/ControlledRobot.hpp>
#include <robot_remote_control/RobotController.hpp>

#include "system_config.hpp"
#include "telemetry_recorder.hpp"

class RobotRelay {
 public:
    // recorder may be null (recording disabled)
    RobotRelay(const SystemConfig& config, std::shared_ptr<TelemetryRecorder> recorder);
    ~RobotRelay();

    // Blocks until `running` becomes false.
    void run(const std::atomic<bool>& running);

 private:
    void forwardCommands();
    void forwardSensors();
    void forwardLogMessages();
    void forwardSimpleActions();
    void log(robot_remote_control::LogLevel level, const std::string& message);

    const SystemConfig config;
    const std::shared_ptr<TelemetryRecorder> recorder;
    std::unique_ptr<robot_remote_control::RobotController> robot;
    std::unique_ptr<robot_remote_control::ControlledRobot> web;

    // Robot timestamps are relative; anchor the first one to local time.
    double first_robot_timestamp = -1.0;
    double first_local_timestamp = 0.0;

    // throttles the disconnect log, the callback fires on every failed request
    int last_reported_disconnect_second = -1;
};
