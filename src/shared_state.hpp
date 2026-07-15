// Store all active sessions and provide them to the
// remote robot controller so it can broadcast data.
// based on https://github.com/boostorg/beast/blob/boost-1.88.0/example/websocket/server/chat-multi/shared_state.cpp
#pragma once
#include <robot_remote_control/RobotController.hpp>

#include <boost/smart_ptr.hpp>
#include <mutex>
#include <string>
#include <unordered_set>
#include "remote_robot_manager.hpp"

// Forward declaration
class WebsocketSession;

class RemoteRobotManager;

class SharedState
{
private:
    // This mutex synchronizes all access to sessions_
    std::mutex ws_mutex_;
    std::mutex rrc_mutex_;

    // Keep a list of all the connected clients
    std::unordered_set<WebsocketSession*> sessions_;

    RemoteRobotManager* manager;
public:
    explicit
    SharedState();

    void setManager(RemoteRobotManager* manager);

    // TODO: use std::shared_ptr instead of classic pointer?
    void join  (WebsocketSession* session);
    void leave (WebsocketSession* session);
    // send message via WebSocket
    void send  (std::string message);
    // Send message via RobotController
    // controller->setSimpleActionCommand(...)
    void sendSimpleAction(std::string sys, std::string name, float value);
    void sendTwist(std::string sys, robot_remote_control::Twist twist);
};
