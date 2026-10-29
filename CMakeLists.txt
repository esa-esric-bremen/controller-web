cmake_minimum_required(VERSION 3.10)
project(controller-web)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(yaml-cpp REQUIRED)
find_package(robot_remote_control REQUIRED)

# robot_remote_control builds these transports only if websocketpp / cpprestsdk were found
foreach(target
        robot_remote_control-transport_ws
        robot_remote_control-transport_http)
    if(NOT TARGET ${target})
        message(FATAL_ERROR "${target} not found: rebuild robot_remote_control with "
                            "libwebsocketpp-dev and libcpprest-dev installed")
    endif()
endforeach()

add_executable(controller_web
    src/main.cpp
    src/robot_relay.cpp
    src/sensor_decoder.cpp
    src/system_config.cpp
    src/telemetry_recorder.cpp)

target_link_libraries(controller_web
    PRIVATE
    robot_remote_control-robot_controller
    robot_remote_control-controlled_robot
    robot_remote_control-types
    robot_remote_control-update_thread
    robot_remote_control-transport_zmq
    robot_remote_control-transport_ws
    robot_remote_control-transport_http
    yaml-cpp
)
