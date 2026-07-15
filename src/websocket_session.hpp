// based on 
// https://www.boost.org/doc/libs/latest/libs/beast/example/websocket/server/async/websocket_server_async.cpp
// and 
// https://github.com/boostorg/beast/blob/boost-1.88.0/example/websocket/server/chat-multi/websocket_session.cpp
#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <boost/enable_shared_from_this.hpp> 
#include <nlohmann/json.hpp>
#include "shared_state.hpp"
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
using json = nlohmann::json;


class WebsocketSession : public std::enable_shared_from_this<WebsocketSession>
{
    websocket::stream<beast::tcp_stream> ws_;
    // received buffer
    beast::flat_buffer buffer_;
    // sending queue
    std::vector<boost::shared_ptr<const std::string>> queue_;
    // shared state to register our session to send data
    // from the rrc-telemetry loop
    boost::shared_ptr<SharedState> state_;

public:
    // Take ownership of the socket
    WebsocketSession(
        tcp::socket&& socket,
        boost::shared_ptr<SharedState> const& state);
    ~WebsocketSession();
    void run();
    void send(boost::shared_ptr<std::string const> const& ss);
protected:
    void on_send(boost::shared_ptr<std::string const> const& ss);
    void on_write(
        beast::error_code ec,
        std::size_t bytes_transferred);
    void on_run();
    void on_accept(beast::error_code ec);
    void do_read();
    void on_read(beast::error_code ec, std::size_t bytes_transferred);
};