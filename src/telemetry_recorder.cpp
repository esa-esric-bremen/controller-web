#include "telemetry_recorder.hpp"

#include <cerrno>
#include <cmath>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>

#include <sys/statvfs.h>
#include <unistd.h>

#include <google/protobuf/util/json_util.h>
#include <google/protobuf/wrappers.pb.h>

using namespace robot_remote_control;
namespace fs = std::filesystem;

namespace {

double now() {
    return std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
}

std::string utcStamp() {
    const std::time_t t = std::time(nullptr);
    std::tm tm{};
    gmtime_r(&t, &tm);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y%m%d-%H%M%S", &tm);
    return buf;
}

std::string jsonString(const std::string& s) {
    std::string out = "\"";
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out + "\"";
}

std::string jsonTime(double t) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.6f", t);
    return buf;
}

const char* sourceName(TelemetryRecorder::Source source) {
    switch (source) {
        case TelemetryRecorder::Source::ROBOT: return "robot";
        case TelemetryRecorder::Source::REQUEST: return "request";
        case TelemetryRecorder::Source::RELAY: return "relay";
    }
    return "unknown";
}

// Same mapping as RobotController's registerTelemetryType calls.
std::unique_ptr<google::protobuf::Message> messageFor(TelemetryMessageType type) {
    switch (type) {
        case CURRENT_POSE: return std::make_unique<Pose>();
        case JOINT_STATE: return std::make_unique<JointState>();
        case CONTROLLABLE_JOINTS: return std::make_unique<JointState>();
        case SIMPLE_ACTIONS: return std::make_unique<SimpleActions>();
        case COMPLEX_ACTIONS: return std::make_unique<ComplexActions>();
        case ROBOT_NAME: return std::make_unique<RobotName>();
        case ROBOT_STATE: return std::make_unique<RobotState>();
        case LOG_MESSAGE: return std::make_unique<LogMessage>();
        case VIDEO_STREAMS: return std::make_unique<VideoStreams>();
        case SIMPLE_SENSOR: return std::make_unique<SimpleSensor>();
        case WRENCH_STATE: return std::make_unique<WrenchState>();
        case MAP: return std::make_unique<Map>();
        case POSES: return std::make_unique<Poses>();
        case TRANSFORMS: return std::make_unique<Transforms>();
        case PERMISSION_REQUEST: return std::make_unique<PermissionRequest>();
        case POINTCLOUD: return std::make_unique<PointCloud>();
        case IMU_VALUES: return std::make_unique<IMU>();
        case CONTACT_POINTS: return std::make_unique<ContactPoints>();
        case CURRENT_TWIST: return std::make_unique<Twist>();
        case CURRENT_ACCELERATION: return std::make_unique<Acceleration>();
        case CAMERA_INFORMATION: return std::make_unique<CameraInformation>();
        case IMAGE: return std::make_unique<Image>();
        case IMAGE_LAYERS: return std::make_unique<ImageLayers>();
        case ODOMETRY: return std::make_unique<Odometry>();
        case CONTROLLABLE_FRAMES: return std::make_unique<ControllableFrames>();
        case FILE_DEFINITION: return std::make_unique<FileDefinition>();
        case ROBOT_MODEL_INFORMATION: return std::make_unique<RobotModelInformation>();
        case INTERFACE_OPTIONS: return std::make_unique<InterfaceOptions>();
        case CHANNELS_DEFINITION: return std::make_unique<ChannelsDefinition>();
        case LASER_SCAN: return std::make_unique<LaserScan>();
        default: return nullptr;
    }
}

bool toJson(const google::protobuf::Message& message, std::string* json) {
    google::protobuf::util::JsonPrintOptions options;
    // explicit zeros: protobuf omits default values otherwise
    options.always_print_primitive_fields = true;
    json->clear();
    return google::protobuf::util::MessageToJsonString(message, json, options).ok();
}

// "data":{...} for known types, otherwise the undecoded bytes, so nothing is lost.
std::string payloadField(TelemetryMessageType type, const std::string& bytes) {
    std::string json;
    auto message = messageFor(type);
    if (message && message->ParseFromString(bytes) && toJson(*message, &json)) {
        return "\"data\":" + json;
    }
    google::protobuf::BytesValue raw;
    raw.set_value(bytes);
    toJson(raw, &json);  // a base64 JSON string
    return "\"data_base64\":" + json;
}

}  // namespace

TelemetryRecorder::TelemetryRecorder(const RecorderConfig& config, const SystemConfig& system)
    : config(config),
      system(system),
      directory(config.directory + "/" + system.system_name),
      session(utcStamp()) {
    std::error_code ec;
    fs::create_directories(directory, ec);
    if (ec) {
        throw std::runtime_error("recorder: cannot create " + directory + ": " + ec.message());
    }
    if (!openFile()) {
        throw std::runtime_error("recorder: cannot write to " + directory + ": " + std::strerror(errno));
    }
    if (!enoughDiskSpace()) {
        // not a configuration error: controlling the robot must still work
        std::cerr << "recorder: WARNING: less than " << (config.min_free_bytes >> 20)
                  << " MiB free in " << directory << ", recording paused" << std::endl;
        closeFile();
        paused = true;
        retry_at = std::chrono::steady_clock::now() + config.retry_interval;
    }
    std::cout << "  recording: " << directory << std::endl;
    writer = std::thread(&TelemetryRecorder::writerLoop, this);
}

TelemetryRecorder::~TelemetryRecorder() {
    event("info", "recording stopped");
    {
        std::lock_guard<std::mutex> lock(mutex);
        stopping = true;
    }
    wake.notify_one();
    writer.join();
}

void TelemetryRecorder::setNotify(Notify callback) {
    std::lock_guard<std::mutex> lock(notify_mutex);
    notify = std::move(callback);
}

void TelemetryRecorder::recordTelemetryMessage(const std::string& serialized) {
    Record record;
    record.kind = Record::Kind::TELEMETRY_MESSAGE;
    record.source = Source::ROBOT;
    record.payload = serialized;
    enqueue(std::move(record));
}

void TelemetryRecorder::record(Source source, TelemetryMessageType type,
                               const google::protobuf::Message& message, uint32_t channel) {
    Record record;
    record.kind = Record::Kind::MESSAGE;
    record.source = source;
    record.type = type;
    record.channel = channel;
    message.SerializeToString(&record.payload);
    enqueue(std::move(record));
}

void TelemetryRecorder::event(const std::string& level, const std::string& message) {
    Record record;
    record.kind = Record::Kind::EVENT;
    record.level = level;
    record.payload = message;
    enqueue(std::move(record));
}

TelemetryRecorder::Record TelemetryRecorder::makeEvent(const std::string& level, const std::string& message) {
    Record record;
    record.kind = Record::Kind::EVENT;
    record.level = level;
    record.payload = message;
    record.time = now();
    std::lock_guard<std::mutex> lock(mutex);
    record.seq = next_seq++;
    return record;
}

void TelemetryRecorder::enqueue(Record record) {
    record.time = now();
    {
        std::lock_guard<std::mutex> lock(mutex);
        // the seq is taken even when dropped, so gaps show up in the recording
        record.seq = next_seq++;
        if (queue.size() >= config.max_queued_records) {
            ++dropped;
            return;
        }
        queue.push_back(std::move(record));
    }
    wake.notify_one();
}

void TelemetryRecorder::problem(const std::string& level, const std::string& message) {
    std::cerr << "recorder " << system.system_name << ": " << message << std::endl;
    std::lock_guard<std::mutex> lock(notify_mutex);
    if (notify) {
        notify(level, "recorder " + system.system_name + ": " + message);
    }
}

void TelemetryRecorder::writerLoop() {
    auto next_flush = std::chrono::steady_clock::now() + config.flush_interval;
    bool stop = false;
    while (!stop) {
        std::deque<Record> batch;
        {
            std::unique_lock<std::mutex> lock(mutex);
            wake.wait_until(lock, next_flush, [this]() { return stopping || !queue.empty(); });
            batch.swap(queue);
            lost += dropped;
            dropped = 0;
            stop = stopping;
        }
        const auto steady_now = std::chrono::steady_clock::now();

        if (paused && (stop || steady_now >= retry_at)) {
            if (enoughDiskSpace() && openFile()) {
                paused = false;
                problem("info", "recording resumed");
            } else {
                retry_at = steady_now + config.retry_interval;
            }
        }

        if (!paused && lost > 0) {
            const std::string message = std::to_string(lost) + " records lost";
            lost = 0;
            write(makeEvent("warning", message));
            problem("warning", message);
        }
        for (const auto& record : batch) {
            if (paused) {
                ++lost;
            } else {
                write(record);
            }
        }

        if (steady_now >= next_flush) {
            flush();
            if (!paused && !enoughDiskSpace()) {
                closeFile();
                paused = true;
                retry_at = steady_now + config.retry_interval;
                problem("error", "less than " + std::to_string(config.min_free_bytes >> 20)
                    + " MiB free disk space, recording paused");
            }
            next_flush = steady_now + config.flush_interval;
        }
    }
    if (!paused && lost > 0) {
        write(makeEvent("warning", std::to_string(lost) + " records lost"));
    }
    closeFile();
}

void TelemetryRecorder::write(const Record& record) {
    std::string line = "{\"seq\":" + std::to_string(record.seq) + ",\"t\":" + jsonTime(record.time);
    switch (record.kind) {
        case Record::Kind::EVENT:
            line += ",\"src\":\"event\",\"level\":" + jsonString(record.level)
                + ",\"message\":" + jsonString(record.payload);
            break;
        case Record::Kind::MESSAGE:
            line += std::string(",\"src\":\"") + sourceName(record.source) + "\""
                + ",\"type\":\"" + TelemetryMessageType_Name(record.type) + "\""
                + ",\"channel\":" + std::to_string(record.channel)
                + "," + payloadField(record.type, record.payload);
            break;
        case Record::Kind::TELEMETRY_MESSAGE: {
            line += std::string(",\"src\":\"") + sourceName(record.source) + "\"";
            TelemetryMessage message;
            if (message.ParseFromString(record.payload)) {
                const auto type = message.type();
                const auto name = TelemetryMessageType_IsValid(type)
                    ? TelemetryMessageType_Name(type) : std::to_string(type);
                line += ",\"type\":\"" + name + "\""
                    + ",\"channel\":" + std::to_string(message.channel())
                    + "," + payloadField(type, message.data());
            } else {
                // keep what arrived, even if we can't read it
                line += ",\"type\":null," + payloadField(NO_TELEMETRY_DATA, record.payload);
            }
            break;
        }
    }
    writeLine(line + "}\n");
}

void TelemetryRecorder::writeLine(const std::string& line) {
    if (std::fwrite(line.data(), 1, line.size(), file) != line.size() || std::ferror(file)) {
        const std::string reason = std::strerror(errno);
        closeFile();
        paused = true;
        ++lost;
        retry_at = std::chrono::steady_clock::now() + config.retry_interval;
        problem("error", "write to " + file_path + " failed (" + reason + "), recording paused");
        return;
    }
    file_bytes += line.size();
    if (file_bytes >= config.max_file_bytes) {
        closeFile();
        if (!openFile()) {
            paused = true;
            retry_at = std::chrono::steady_clock::now() + config.retry_interval;
            problem("error", std::string("cannot open next file (") + std::strerror(errno) + "), recording paused");
        }
    }
}

bool TelemetryRecorder::openFile() {
    // "x": never overwrite, a restart within the same second just takes the next index
    for (int attempts = 0; attempts < 1000; ++attempts) {
        const std::string path = directory + "/" + system.system_name + "_" + session + "_"
            + std::to_string(file_index++) + ".jsonl.part";
        file = std::fopen(path.c_str(), "wx");
        if (file) {
            file_path = path;
            file_bytes = 0;
            const std::string meta = metaLine();
            if (std::fwrite(meta.data(), 1, meta.size(), file) != meta.size()) {
                closeFile();
                return false;
            }
            file_bytes = meta.size();
            return true;
        }
        if (errno != EEXIST) {
            return false;
        }
    }
    return false;
}

void TelemetryRecorder::closeFile() {
    if (!file) {
        return;
    }
    flush();
    std::fclose(file);
    file = nullptr;
    // drop ".part": the file is complete
    std::error_code ec;
    fs::rename(file_path, file_path.substr(0, file_path.size() - 5), ec);
}

void TelemetryRecorder::flush() {
    if (file) {
        std::fflush(file);
        fsync(fileno(file));
    }
}

bool TelemetryRecorder::enoughDiskSpace() const {
    struct statvfs stats;
    if (statvfs(directory.c_str(), &stats) != 0) {
        return false;
    }
    return static_cast<uint64_t>(stats.f_bavail) * stats.f_frsize >= config.min_free_bytes;
}

// First line of every file: everything needed to interpret the records on its own.
std::string TelemetryRecorder::metaLine() const {
    std::string sensors = "[";
    for (size_t i = 0; i < system.sensors.size(); ++i) {
        sensors += (i ? "," : "") + std::string("{\"id\":") + jsonString(system.sensors[i].id)
            + ",\"mask\":" + std::to_string(system.sensors[i].mask) + "}";
    }
    sensors += "]";
    return "{\"src\":\"meta\",\"format\":1,\"t\":" + jsonTime(now())
        + ",\"system_name\":" + jsonString(system.system_name)
        + ",\"session\":" + jsonString(session)
        + ",\"file_index\":" + std::to_string(file_index - 1)
        + ",\"robot\":" + jsonString(system.robot_ip + ":" + std::to_string(system.robot_command_port)
                                     + "/" + std::to_string(system.robot_telemetry_port))
        + ",\"sensors\":" + sensors
        + ",\"sensor_layout\":\"SIMPLE_SENSOR from src robot is packed: [mask, robot_time, values of "
          "the sensors whose mask bits are all set in mask, in this order]\"}\n";
}
