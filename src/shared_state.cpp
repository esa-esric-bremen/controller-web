#include "shared_state.hpp"
#include "websocket_session.hpp"
#include <iostream>

SharedState::SharedState()
{
}

void SharedState::join(WebsocketSession* session)
{
    std::lock_guard<std::mutex> lock(ws_mutex_);
    sessions_.insert(session);
}

void SharedState::leave(WebsocketSession* session)
{
    std::lock_guard<std::mutex> lock(ws_mutex_);
    sessions_.erase(session);
}

void SharedState::send(std::string message)
{
    // Put the message in a shared pointer so we can re-use it for each client
    auto const ss = boost::make_shared<std::string const>(std::move(message));

    // Make a local list of all the weak pointers representing
    // the sessions, so we can do the actual sending without
    // holding the mutex:
    std::vector<std::weak_ptr<WebsocketSession>> weak_session_ptr;
    {
        std::lock_guard<std::mutex> lock(ws_mutex_);
        weak_session_ptr.reserve(sessions_.size());
        for(auto p : sessions_)
            weak_session_ptr.emplace_back(p->weak_from_this());
    }

    // For each session in our local list, try to acquire a strong
    // pointer. If successful, then send the message on that session.
    for(auto const& wp : weak_session_ptr)
        if(auto shared_ptr = wp.lock())
            shared_ptr->send(ss);
}

void SharedState::setManager(
    RemoteRobotManager* manager)
{
    this->manager = manager;
};

void SharedState::sendTwist(std::string sys, robot_remote_control::Twist twist)
{
    std::lock_guard<std::mutex> lock(rrc_mutex_);
    if (!this->manager) {
        return;
    }
    this->manager->sendTwist(sys, twist);
}

void SharedState::sendSimpleAction(std::string sys, std::string name, float value)
{
    std::lock_guard<std::mutex> lock(rrc_mutex_);
    if (!this->manager) {
        return;
    }
    this->manager->sendSimpleAction(sys, name, value);
}