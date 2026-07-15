#include "websocket_server.hpp"
#include "shared_state.hpp"
#include <iostream>

WebsocketServer::WebsocketServer(
    net::io_context& ioc,
    tcp::endpoint endpoint,
    boost::shared_ptr<SharedState> const& state)
    : ioc_(ioc), acceptor_(ioc), state_(state) {
    beast::error_code ec;
    
    acceptor_.open(endpoint.protocol(), ec);

    if(ec)
    {
        std::cerr << "Error on open: " << ec.message() << std::endl;
        return;
    }

    // Allow address reuse
    acceptor_.set_option(net::socket_base::reuse_address(true), ec);
    if(ec)
    {
        std::cerr << "Error on set_option: " << ec.message() << std::endl;
        return;
    }

    // Bind to the server address
    acceptor_.bind(endpoint, ec);
    if(ec)
    {
        std::cerr << "Error on bind: " << ec.message() << std::endl;
        return;
    }

    // Start listening for connections
    acceptor_.listen(
        net::socket_base::max_listen_connections, ec);
    if(ec)
    {
        std::cerr << "Error on listen: " << ec.message() << std::endl;
        return;
    }
}

void WebsocketServer::run() 
{
    do_accept();
}

void WebsocketServer::do_accept() {
    acceptor_.async_accept(
        net::make_strand(ioc_),
        beast::bind_front_handler(
            &WebsocketServer::on_accept,
            shared_from_this()));
}

void WebsocketServer::on_accept(
    beast::error_code ec, tcp::socket socket)
{
    if(ec)
    {
        std::cerr << "Error on accept: " << ec.message() << std::endl;
    }
    else
    {
        // Create the session and run it
        auto session = std::make_shared<WebsocketSession>(std::move(socket), state_);
        session->run();
    }

    // Accept another connection
    do_accept();
}