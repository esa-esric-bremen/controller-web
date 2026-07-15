#include "remote_robot_manager.hpp"
#include "websocket_server.hpp"

#include <iostream>
#include <unistd.h>
#include <memory>

#include <boost/asio.hpp>
#include <boost/make_shared.hpp> 

namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>


int main(int argc, char** argv)
{
    bool sim = false;
    if (argc != 1) {
        if (std::string(argv[1]) == "--sim") {
            std::cout << "IMPORTANT! IMPORTANT! IMPORTANT! IMPORTANT! IMPORTANT!" << std::endl;
            std::cout << "------------------------------------------------" << std::endl;
            std::cout << "RUN FOR DEVELOPMENT - CONNECTING TO SIMULATED MACHINES!" << std::endl;
            std::cout << "------------------------------------------------" << std::endl;
            sim = true;
        }
    }
    
    auto state = boost::make_shared<SharedState>();

    RemoteRobotManager manager(state, sim);

    manager.loadFromFile("data/config_rpi_robot.yaml");
    // manager.loadFromFile("data/config_beneficiation.yaml");
    manager.loadFromFile("data/config_coyote.yaml");

    std::cout << "Server starting..." << std::endl;
    
    auto const address = net::ip::make_address("0.0.0.0");
    auto const port = static_cast<unsigned short>(8000);
    

    auto const threads = std::max<int>(
        8, std::thread::hardware_concurrency());

    net::io_context ioc{threads};
    
    auto srv = std::make_shared<WebsocketServer>(
        ioc, tcp::endpoint{address, port}, state);
    
    std::cout << "Start RRC connections..." << std::endl;
    manager.startAll();

    srv->run();

    // Run the I/O service on the requested number of threads
    std::vector<std::thread> vec;
    vec.reserve(threads - 1);
    for(auto i = threads - 1; i > 0; --i)
        vec.emplace_back(
        [&ioc]
        {
            ioc.run();
        });
    ioc.run();

    // shutdown, join rrc and websocket threads
    manager.joinAll();
    for(auto& t : vec)
        t.join();
    return 0;
}