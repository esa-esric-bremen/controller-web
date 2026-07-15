cmake_minimum_required(VERSION 3.10)
project(controller-web)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(yaml-cpp REQUIRED)

find_package(Boost 1.74.0 REQUIRED COMPONENTS system thread)
find_package(nlohmann_json 3.2.0 REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

add_executable(controller_web 
    src/main.cpp
    src/remote_robot_controller.cpp
     src/remote_robot_manager.cpp
    src/shared_state.cpp
    src/websocket_session.cpp
    src/websocket_server.cpp)

target_link_libraries(controller_web
    PRIVATE
    robot_remote_control-robot_controller
    robot_remote_control-types
    robot_remote_control-update_thread
    robot_remote_control-transport_zmq
    protobuf
    yaml-cpp
    Boost::system
    Boost::thread
    nlohmann_json::nlohmann_json
)
