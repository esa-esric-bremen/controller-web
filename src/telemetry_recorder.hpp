// Records everything received by one robot into JSON files.
#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

#include <google/protobuf/message.h>
#include <robot_remote_control/Types/RobotRemoteControl.pb.h>

#include "system_config.hpp"

struct RecorderConfig {
    std::string directory;
    uint64_t max_file_bytes = 100ull * 1024 * 1024; // 100 MB max.
    // recording pauses below this, nothing gets deleted automatically
    uint64_t min_free_bytes = 500ull * 1024 * 1024; // 500 MB max. 
    size_t max_queued_records = 20000;
    std::chrono::milliseconds flush_interval{1000};
    std::chrono::milliseconds retry_interval{5000};
};

class TelemetryRecorder {
 public:
    // Where a record came from.
    enum class Source {
        ROBOT,    // telemetry as pushed by the robot, unmodified
        REQUEST,  // reply to a request of the relay (e.g. simple action states)
        RELAY     // derived by the relay (e.g. decoded sensors)
    };

    // Throws std::runtime_error if the directory can't be created or written.
    TelemetryRecorder(const RecorderConfig& config, const SystemConfig& system);
    ~TelemetryRecorder();

    TelemetryRecorder(const TelemetryRecorder&) = delete;
    TelemetryRecorder& operator=(const TelemetryRecorder&) = delete;

    // A serialized (binary) TelemetryMessage as received from the robot.
    void recordTelemetryMessage(const std::string& serialized);
    void record(Source source, robot_remote_control::TelemetryMessageType type,
                const google::protobuf::Message& message, uint32_t channel = 0);
    void event(const std::string& level, const std::string& message);

    // Called from the writer thread when recording stops or resumes (disk full, write
    // errors, dropped records), so operators notice. Arguments: level, message.
    using Notify = std::function<void(const std::string&, const std::string&)>;
    void setNotify(Notify notify);

 private:
    struct Record {
        uint64_t seq;
        double time;
        enum class Kind { TELEMETRY_MESSAGE, MESSAGE, EVENT } kind;
        Source source;
        robot_remote_control::TelemetryMessageType type;
        uint32_t channel;
        // serialized protobuf, or the event message
        std::string payload;
        std::string level;
    };

    Record makeEvent(const std::string& level, const std::string& message);
    void enqueue(Record record);
    void problem(const std::string& level, const std::string& message);
    void writerLoop();
    void write(const Record& record);
    void writeLine(const std::string& line);
    bool openFile();
    void closeFile();
    void flush();
    bool enoughDiskSpace() const;
    std::string metaLine() const;

    const RecorderConfig config;
    const SystemConfig system;
    const std::string directory;
    const std::string session;

    std::mutex mutex;
    std::condition_variable wake;
    std::deque<Record> queue;
    uint64_t next_seq = 0;
    uint64_t dropped = 0;  // guarded by mutex, reported by the writer
    bool stopping = false;

    // held while notify runs, so setNotify(nullptr) waits for a running call
    std::mutex notify_mutex;
    Notify notify;

    // writer thread only
    FILE* file = nullptr;
    std::string file_path;
    int file_index = 0;
    uint64_t file_bytes = 0;
    bool paused = false;  // low disk space or write error
    uint64_t lost = 0;    // records dropped while paused or by queue overflow, not yet reported
    std::chrono::steady_clock::time_point retry_at;

    std::thread writer;
};
