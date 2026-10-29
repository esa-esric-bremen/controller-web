#include "robot_relay.hpp"
#include "recording_transport.hpp"
#include "sensor_decoder.hpp"

#include <cmath>
#include <iostream>
#include <thread>

#include <robot_remote_control/Transports/TransportHttp.hpp>
#include <robot_remote_control/Transports/TransportWebSocket.hpp>
#include <robot_remote_control/Transports/TransportZmq.hpp>

using namespace robot_remote_control;

namespace {

// commands from the browser should reach the robot quickly
constexpr auto LOOP_PERIOD = std::chrono::milliseconds(20);
// requesting the action states is a blocking round trip to the robot
constexpr auto SIMPLE_ACTIONS_PERIOD = std::chrono::milliseconds(500);
// processes incoming http commands of the web side
constexpr unsigned WEB_UPDATE_MS = 10;

double now() {
    return std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
}

TimeStamp toTimeStamp(double seconds) {
    TimeStamp ts;
    ts.set_secs(static_cast<uint32_t>(seconds));
    ts.set_nsecs(static_cast<uint32_t>((seconds - std::floor(seconds)) * 1e9));
    return ts;
}

std::string tcp(const std::string& ip, int port) {
    return "tcp://" + ip + ":" + std::to_string(port);
}

const char* levelName(LogLevel level) {
    if (level <= ERROR) return "error";
    if (level == WARN) return "warning";
    return "info";
}

}  // namespace

RobotRelay::RobotRelay(const SystemConfig& config, std::shared_ptr<TelemetryRecorder> recorder)
    : config(config), recorder(std::move(recorder)) {
    TransportSharedPtr telemetry =
        std::make_shared<TransportZmq>(tcp(config.robot_ip, config.robot_telemetry_port), TransportZmq::SUB);
    if (this->recorder) {
        telemetry = std::make_shared<RecordingTransport>(telemetry, this->recorder);
    }
    robot = std::make_unique<RobotController>(
        std::make_shared<TransportZmq>(tcp(config.robot_ip, config.robot_command_port), TransportZmq::REQ),
        telemetry);
    robot->setHeartBeatDuration(1);

    // SERVER_TEXT switches the websocket to JSON so browsers can parse it
    web = std::make_unique<ControlledRobot>(
        std::make_shared<TransportHttp>("http://0.0.0.0:" + std::to_string(config.web_command_port)),
        std::make_shared<TransportWebSocket>(TransportWebSocket::SERVER_TEXT, config.web_telemetry_port, "0.0.0.0"));
    web->initRobotName(config.system_name);

    if (this->recorder) {
        // recording problems (disk full, write errors) show up in the operators' log
        this->recorder->setNotify([this](const std::string& level, const std::string& message) {
            web->setLogMessage(level == "error" ? ERROR : level == "warning" ? WARN : INFO, message);
        });
        this->recorder->event("info", "relay started");
    }

    robot->setupConnectedCallback([this]() {
        last_reported_disconnect_second = -1;
        log(INFO, this->config.system_name + " connected.");
    });
    robot->setupDisconnectedCallback([this](const float& elapsed) {
        const int second = static_cast<int>(elapsed);
        // the UI flags missing telemetry itself, the log only needs the milestones
        const bool milestone = second == 1 || second == 2 || second == 5 || second == 10
            || second == 30 || (second > 0 && second % 60 == 0);
        if (!milestone || second == last_reported_disconnect_second) {
            return;
        }
        last_reported_disconnect_second = second;
        log(ERROR, "No connection to " + this->config.system_name + " ("
            + tcp(this->config.robot_ip, this->config.robot_telemetry_port) + ") for "
            + std::to_string(second) + " seconds");
    });

    std::cout << config.system_name
              << "\n  robot:     " << tcp(config.robot_ip, config.robot_command_port)
              << ", " << tcp(config.robot_ip, config.robot_telemetry_port)
              << "\n  commands:  http://0.0.0.0:" << config.web_command_port << "/api/command"
              << "\n  telemetry: ws://0.0.0.0:" << config.web_telemetry_port << std::endl;
}

RobotRelay::~RobotRelay() {
    if (recorder) {
        recorder->setNotify(nullptr);
    }
    web->stopUpdateThread();
}

void RobotRelay::run(const std::atomic<bool>& running) {
    web->startUpdateThread(WEB_UPDATE_MS);

    auto next_simple_actions = std::chrono::steady_clock::now();
    while (running) {
        // receives telemetry and sends the heartbeat
        robot->update();

        forwardCommands();
        forwardSensors();
        forwardLogMessages();

        if (std::chrono::steady_clock::now() >= next_simple_actions) {
            forwardSimpleActions();
            next_simple_actions = std::chrono::steady_clock::now() + SIMPLE_ACTIONS_PERIOD;
        }
        std::this_thread::sleep_for(LOOP_PERIOD);
    }
}

void RobotRelay::forwardCommands() {
    Twist twist;
    if (web->getTwistCommand(&twist)) {
        robot->setTwistCommand(twist);
    }
    SimpleAction action;
    while (web->getSimpleActionCommand(&action)) {
        robot->setSimpleActionCommand(action);
    }
}

void RobotRelay::forwardSensors() {
    SimpleSensor packed;
    DecodedSensors decoded;
    while (robot->getSimpleSensor(&packed)) {
        if (!decodeSensors(config.sensors, packed, &decoded)) {
            std::cerr << config.system_name << ": WARNING: empty sensor packet" << std::endl;
            continue;
        }
        const double local = now();
        if (first_robot_timestamp < 0.0) {
            first_robot_timestamp = decoded.robot_timestamp;
            first_local_timestamp = local;
        }
        const double sent = first_local_timestamp + (decoded.robot_timestamp - first_robot_timestamp);

        *decoded.sensor.mutable_header()->mutable_timestamp() = toTimeStamp(sent);
        // drift between the robot clock and ours, shown as "Com time" in the UI
        decoded.sensor.add_value_names("td");
        decoded.sensor.add_value(static_cast<float>(std::abs(local - sent)));
        web->setSimpleSensor(decoded.sensor);
        if (recorder) {
            recorder->record(TelemetryRecorder::Source::RELAY, SIMPLE_SENSOR, decoded.sensor);
        }
    }
}

void RobotRelay::forwardLogMessages() {
    LogMessage message;
    while (robot->getLogMessage(&message)) {
        web->setLogMessage(message);
    }
}

void RobotRelay::forwardSimpleActions() {
    SimpleActions actions;
    if (robot->requestSimpleActions(&actions)) {
        web->updateSimpleActions(actions);
        if (recorder) {
            recorder->record(TelemetryRecorder::Source::REQUEST, SIMPLE_ACTIONS, actions);
        }
    }
}

void RobotRelay::log(LogLevel level, const std::string& message) {
    (level <= ERROR ? std::cerr : std::cout) << message << std::endl;
    web->setLogMessage(level, message);
    if (recorder) {
        recorder->event(levelName(level), message);
    }
}
