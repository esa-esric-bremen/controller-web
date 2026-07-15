#include "websocket_session.hpp"
#include <iostream>

WebsocketSession::WebsocketSession(
    boost::asio::ip::tcp::socket&& socket,
    boost::shared_ptr<SharedState> const& state)
    : ws_(std::move(socket))
    , state_(state)
{
}

WebsocketSession::~WebsocketSession()
{
    // Remove this session from the list of active sessions
    state_->leave(this);
}

void WebsocketSession::run()
{
    // We need to be executing within a strand to perform async operations
    // on the I/O objects in this session. Although not strictly necessary
    // for single-threaded contexts, this example code is written to be
    // thread-safe by default.
    net::dispatch(ws_.get_executor(),
        beast::bind_front_handler(
            &WebsocketSession::on_run,
            shared_from_this()));
}

void WebsocketSession::on_run()
{
    beast::get_lowest_layer(ws_).expires_never();

    ws_.set_option(
        websocket::stream_base::timeout::suggested(
            beast::role_type::server));

    // Set a decorator to change the Server of the handshake
    ws_.set_option(websocket::stream_base::decorator(
        [](websocket::response_type& res)
        {
            res.set(http::field::server,
                std::string(BOOST_BEAST_VERSION_STRING) +
                    " websocket-server-async");
        }));

    // Accept the websocket handshake
    ws_.async_accept(
        beast::bind_front_handler(
            &WebsocketSession::on_accept,
            shared_from_this()));
}

void WebsocketSession::on_accept(beast::error_code ec)
{
    if (ec) {
        std::cerr << "Error on accept: " << ec.message() << std::endl;
        return;
    }
    // Read a message
    state_->join(this);
    do_read();
}

void WebsocketSession::do_read()
{
    // Read a message into our buffer
    ws_.async_read(
        buffer_,
        beast::bind_front_handler(
            &WebsocketSession::on_read,
            shared_from_this()));
}

void WebsocketSession::on_read(
    beast::error_code ec,
    std::size_t bytes_transferred)
{
    boost::ignore_unused(bytes_transferred);

    // This indicates that the WebsocketSession was closed
    if(ec == websocket::error::closed || ec == net::error::eof) {
        std::cerr << "Connection closed." << std::endl;
        return;
    }

    if (ec) {
        std::cerr << "Error on read: " << ec.message() << std::endl;
        return;
    }

    std::string data(boost::beast::buffers_to_string(buffer_.data()));
    std::istringstream input_stream(data);

    try {
        json j;
        input_stream >> j;
    
        if (j.contains("sys") && j.contains("cmd") && j.contains("value")) {
            std::string sys = j["sys"].template get<std::string>();
            std::string name = j["cmd"].template get<std::string>();
            float value = j["value"].template get<float>();
            std::cout << "Set " << sys << ":" << name << " to " << value << std::endl;

            // Forward to robot via state
            state_->sendSimpleAction(sys, name, value);
        } else if (j.contains("sys") && j.contains("twist")) {
            std::string sys = j["sys"].template get<std::string>();
            float linear_x = j["twist"]["linear_x"].template get<float>();
            float linear_y = j["twist"]["linear_y"].template get<float>();
            float linear_z = j["twist"]["linear_z"].template get<float>();
            float angular_x = j["twist"]["angular_x"].template get<float>();
            float angular_y = j["twist"]["angular_y"].template get<float>();
            float angular_z = j["twist"]["angular_z"].template get<float>();
            std::cout << "Set " << sys << "twist: lin.x: " << linear_x << std::endl;
            robot_remote_control::Twist twist;
            twist.mutable_linear()->set_x(linear_x);
            twist.mutable_linear()->set_y(linear_y);
            twist.mutable_linear()->set_z(linear_z);
            twist.mutable_angular()->set_x(angular_x);
            twist.mutable_angular()->set_y(angular_y);
            twist.mutable_angular()->set_z(angular_z);
            // Forward to robot via state
            state_->sendTwist(sys, twist);
        } else {
            std::cerr << "Received invalid command: " << data << std::endl;
        }
    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
    }

    // Clear the buffer and reveive next message
    buffer_.consume(buffer_.size());
    do_read();
}

void WebsocketSession::send(boost::shared_ptr<std::string const> const& ss)
{
    // Post our work to the strand, this ensures
    // that the members of `this` will not be
    // accessed concurrently.
    net::post(
        ws_.get_executor(),
        beast::bind_front_handler(
            &WebsocketSession::on_send,
            shared_from_this(),
            ss));
}

void WebsocketSession::on_send(boost::shared_ptr<std::string const> const& ss)
{
    // Always add to queue
    queue_.push_back(ss);

    // Are we already writing?
    if(queue_.size() > 1)
        return;
    
    // We are not currently writing, so send this immediately
    ws_.async_write(
        net::buffer(*queue_.front()),
        beast::bind_front_handler(
            &WebsocketSession::on_write,
            shared_from_this()));
}

void WebsocketSession::on_write(
    beast::error_code ec,
    std::size_t bytes_transferred)
{
    boost::ignore_unused(bytes_transferred);

    if (ec) {
        std::cerr << "Error on write: " << ec.message() << std::endl;
        return;
    }

    if(queue_.empty())
        return;

    queue_.erase(queue_.begin());

    if (!queue_.empty()) {
        // If the queue is not empty, start writing the NEXT message.
        // This is what creates the continuous stream of data.
        ws_.async_write(
            net::buffer(*queue_.front()),
            beast::bind_front_handler(&WebsocketSession::on_write, shared_from_this()));
    } 
}