// Accepts incoming connections and launches the sessions
// similar to the "listener" in the example at
// https://www.boost.org/doc/libs/latest/libs/beast/example/websocket/server/async/websocket_server_async.cpp
#pragma once
#include "shared_state.hpp"
#include "websocket_session.hpp"
#include <boost/enable_shared_from_this.hpp>
#include <boost/smart_ptr.hpp>
#include <boost/beast/core.hpp>
#include <boost/asio/strand.hpp>

namespace beast = boost::beast;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

// Forward declaration
class SharedState;

class WebsocketServer : public std::enable_shared_from_this<WebsocketServer>
{
    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    boost::shared_ptr<SharedState> state_;

public:
    WebsocketServer(
        net::io_context& ioc,
        tcp::endpoint endpoint,
        boost::shared_ptr<SharedState> const& state);
    void run();
private:
    void do_accept();
    void on_accept(beast::error_code ec, tcp::socket socket);
};