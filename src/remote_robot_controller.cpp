#include "remote_robot_controller.hpp"
#include "remote_robot_manager.hpp"
#include <iostream>
#include <string>
#include <chrono>
#include <map>

// wait time in microseconds.
const int LOOP_SLEEP = 500000;

double getCurrentTimestamp() {
    return std::chrono::duration<double>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

RobotRemoteController::RobotRemoteController(
    const YAML::Node& config,
    const boost::shared_ptr<SharedState>& state,
    const bool sim)
  : config(config), state(state), sim(sim)
{
}

RobotRemoteController::~RobotRemoteController() = default;

void RobotRemoteController::start()
{
    thread_ = std::thread{[this]() {
        std::string ip;
        if (this->sim) {
            ip = config["sim_ip_address"].as<std::string>();
        } else {
            ip = config["ip_address"].as<std::string>();
        }
        
        const int cmd_port = config["command_address_port"].as<int>();
        const int tel_port = config["telemetry_address_port"].as<int>();
        this->system_name = config["system_name"].as<std::string>();

        std::string cmd = "tcp://" + ip + ":" + std::to_string(cmd_port);
        std::string tel = "tcp://" + ip + ":" + std::to_string(tel_port);

        std::cout << "  " << this->getSystemName() << std::endl;
        std::cout << "    cmd: " << cmd << std::endl;
        std::cout << "    tel: " << tel << std::endl;
        TransportSharedPtr commands = TransportSharedPtr(
            new TransportZmq(cmd, TransportZmq::REQ));
        TransportSharedPtr telemetry = TransportSharedPtr(
            new TransportZmq(tel, TransportZmq::SUB));
        this->controller = std::make_unique<robot_remote_control::RobotController>(commands, telemetry);
        robot_remote_control::SimpleSensor sensor;

        controller->setHeartBeatDuration(1);

        controller->setupDisconnectedCallback([this, tel](const float &elapsed){
            if (elapsed > 0) {
                std::stringstream ss;
                ss << "No connection to " << tel << " for " << elapsed << " seconds";
                std::cout << ss.str() << std::endl;
                nlohmann::json log;
                log["type"] = LOG_MESSAGE;
                log["level"] = "error";
                log["message"] = ss.str();
                std::string json_string = log.dump();
                state->send(json_string);
            }
        });

        controller->setupConnectedCallback([this](){
            std::string s = "Raspberry Pi connected.";
            nlohmann::json log;
            log["type"] = LOG_MESSAGE;
            log["level"] = "info";
            log["message"] = s;
            std::string json_string = log.dump();
            std::cout << s << std::endl;
            state->send(json_string);
        });

        // to current time and add that
        std::cout << "Waiting for connection..." << std::endl;
        // time stamp value we received from the first frame
        double firstFrameRelativeTime = -1.0;
        // local timestamp when we received the first frame
        double firstFrameReceiveTime = 0.0;
        float lastTimestamp = 0;
        while (true) {
            double now = getCurrentTimestamp();
            controller->update();

            // TODO: only once every second
            robot_remote_control::SimpleActions simpleActions;
            controller->requestSimpleActions(&simpleActions);
            // simpleActions.PrintDebugString();
            nlohmann::json jaction;
            jaction["type"] = ACTION_MESSAGE;
            jaction["sys"] = this->system_name;
            for (auto &saction : simpleActions.actions()) {
                jaction[saction.name()] = saction.state();
            }
            std::string jaction_string = jaction.dump();
            state->send(jaction_string);

            controller->getSimpleSensor(&sensor);
            google::protobuf::RepeatedField<float>* values = sensor.mutable_value();

            // Forward values to the websocket
            // check mask
            if (values->size() <= 0) {
                std::cout << this->system_name << " - WARNING: nothing received." << std::endl;
                usleep(LOOP_SLEEP);
                continue;
            }
            int mask = values->Get(0);
            if (mask <= 0) {
                std::cout << this->system_name << " - WARNING: empty mask received" << std::endl;
                usleep(LOOP_SLEEP);
                continue;
            }
            // check timestamp
            float timestamp = values->Get(1);
            if (lastTimestamp == timestamp) {
                usleep(LOOP_SLEEP);
                continue;
            }
            lastTimestamp = timestamp;
            nlohmann::json telemetry;
            if (firstFrameRelativeTime < 0.0) {
                firstFrameRelativeTime = timestamp;
                firstFrameReceiveTime = now;
            }
            double sendTime = firstFrameReceiveTime + (timestamp - firstFrameRelativeTime);
            telemetry["type"] = TELEMETRY_MESSAGE;
            telemetry["ts"] = sendTime;
            telemetry["td"] = abs(now - sendTime);
            telemetry["sys"] = this->system_name;
            int sensor_pos = 0;
            std::cout << this->system_name << " - "  << timestamp << " - ";
            for (int i = 0; i < values->size()-2; i++) {
                double value = values->Get(i+2);
                bool found = false;

                while (sensor_pos < this->config["sensors"].size()) {
                    int sensor_mask = this->config["sensors"][sensor_pos]["mask"].as<int>();
                    // std::cout << sensor_mask << " & " << mask << std::endl;
                    if ((sensor_mask & mask) == sensor_mask) {
                        found = true;
                        break;
                    }
                    sensor_pos++;
                }
                if (found) {
                    std::string name = this->config["sensors"][sensor_pos]["id"].as<std::string>();
                    telemetry[name] = value;
                    std::cout << name << ": " << value << "\t";
                    sensor_pos++;
                }
            }
            std::cout << std::endl;
            std::string telemetry_string = telemetry.dump();
            state->send(telemetry_string);

            // std::cout << "latency: " << controller.getHeartBeatRoundTripTime()/2.0) << " seconds" << std::endl;
            usleep(LOOP_SLEEP);
        }
    }};
    std::cout << "THREAD STARTED" << std::endl;
};

void RobotRemoteController::setSimpleActionCommand(
    robot_remote_control::SimpleAction action)
{
    this->controller->setSimpleActionCommand(action);
}

void RobotRemoteController::sendTwist(
    robot_remote_control::Twist twist)
{
    this->controller->setTwistCommand(twist);
}

std::string& RobotRemoteController::getSystemName()
{
    return this->system_name;
}

void RobotRemoteController::join() {
    if (thread_.joinable()) {
        thread_.join();
    }
};
