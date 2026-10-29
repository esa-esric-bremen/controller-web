#pragma once

#include <memory>
#include <stdexcept>
#include <string>

#include <robot_remote_control/Transports/TransportWrapper.hpp>

#include "telemetry_recorder.hpp"

class RecordingTransport : public robot_remote_control::TransportWrapper {
 public:
    RecordingTransport(robot_remote_control::TransportSharedPtr transport,
                       std::shared_ptr<TelemetryRecorder> recorder)
        : TransportWrapper(transport), recorder(std::move(recorder)) {
        // the recorder parses binary TelemetryMessages
        if (transport->getSerializationMode() != robot_remote_control::Serialization::BINARY) {
            throw std::runtime_error("RecordingTransport requires a binary transport");
        }
        setTransportSupport(transport->getTransportSupportBitmask());
    }

    int send(const std::string& buf, Transport::Flags flags = Transport::NONE) override {
        return transport->send(buf, flags);
    }

    int receive(std::string* buf, Transport::Flags flags = Transport::NONE) override {
        const int size = transport->receive(buf, flags);
        if (size > 0) {
            recorder->recordTelemetryMessage(*buf);
        }
        return size;
    }

 private:
    std::shared_ptr<TelemetryRecorder> recorder;
};
