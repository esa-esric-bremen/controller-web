// Decodes the robots' packed SimpleSensor into a named one.
//
// Packed layout sent by the robots: [mask, robot_timestamp, v0, v1, ...]
// The values belong, in config order, to the sensors whose mask bits are all set in `mask`.
#pragma once

#include <vector>

#include <robot_remote_control/Types/RobotRemoteControl.pb.h>

#include "system_config.hpp"

struct DecodedSensors {
    float robot_timestamp;
    // value_names holds the sensor ids, value the matching values
    robot_remote_control::SimpleSensor sensor;
};

// Returns false if the packet is empty or has no mask set.
bool decodeSensors(
    const std::vector<SensorConfig>& sensors,
    const robot_remote_control::SimpleSensor& packed,
    DecodedSensors* decoded);
