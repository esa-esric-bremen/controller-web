#include "sensor_decoder.hpp"

bool decodeSensors(
    const std::vector<SensorConfig>& sensors,
    const robot_remote_control::SimpleSensor& packed,
    DecodedSensors* decoded) {
    const auto& values = packed.value();
    if (values.size() < 2) {
        return false;
    }
    const auto mask = static_cast<uint32_t>(values.Get(0));
    if (mask == 0) {
        return false;
    }

    decoded->robot_timestamp = values.Get(1);
    decoded->sensor.Clear();

    size_t sensor_pos = 0;
    for (int i = 2; i < values.size(); ++i) {
        while (sensor_pos < sensors.size() && (sensors[sensor_pos].mask & mask) != sensors[sensor_pos].mask) {
            ++sensor_pos;
        }
        if (sensor_pos == sensors.size()) {
            break;  // more values than configured sensors
        }
        decoded->sensor.add_value_names(sensors[sensor_pos].id);
        decoded->sensor.add_value(values.Get(i));
        ++sensor_pos;
    }
    return true;
}
