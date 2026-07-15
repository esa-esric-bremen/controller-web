#include "remote_robot_manager.hpp"
#include <robot_remote_control/Types/RobotRemoteControl.pb.h>


RemoteRobotManager::RemoteRobotManager(
    const boost::shared_ptr<SharedState>& state, bool sim) : state(state), sim(sim)
{
    state->setManager(this);
}

RemoteRobotManager::~RemoteRobotManager() = default;

void RemoteRobotManager::loadFromFile(
        const std::string& filename) {
    std::cout << "Loading robot configurations from: " << filename << std::endl;
    try {
        YAML::Node config = YAML::LoadFile(filename);
        instances.push_back(
            std::make_unique<RobotRemoteController>(config, state, sim));
    } catch (const YAML::Exception& e) {
        throw std::runtime_error(
            "Failed to parse YAML file '" + filename + "': " + e.what());
    } catch (const std::exception& e) {
        throw std::runtime_error(
            "Error processing file '" + filename + "': " + e.what());
    }
}

void RemoteRobotManager::startAll()
{
    for (const auto& controller : this->instances) {
        if (controller) {
            controller->start();
        }
    }
}

void RemoteRobotManager::joinAll()
{
    for (const auto& controller : this->instances) {
        if (controller) {
            controller->join();
        }
    }
}

void RemoteRobotManager::sendTwist(std::string sys, robot_remote_control::Twist twist)
{
    for (const auto& controller : this->instances) {
        if (controller->getSystemName() == sys) {
            controller->sendTwist(twist);
            return;
        }
    }
}

void RemoteRobotManager::sendSimpleAction(std::string sys, std::string name, float value)
{
    for (const auto& controller : this->instances) {
        if (controller->getSystemName() == sys) {
            robot_remote_control::SimpleAction action;
            action.set_name(name);
            action.set_state(value);
            controller->setSimpleActionCommand(action);
            return;
        }
    }
}