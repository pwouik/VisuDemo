#ifndef LEAP_CONNECTION_H
#define LEAP_CONNECTION_H
#include <functional>
#include <LeapC.h>
#include <mutex>
#include <thread>

using connection_callback = std::function<void()>;
using device_callback = std::function<void(const LEAP_DEVICE_INFO& device)>;
using device_lost_callback = std::function<void()>;
using device_failure_callback = std::function<void(eLeapDeviceStatus failure_code, LEAP_DEVICE failed_device)>;
using policy_callback = std::function<void(uint32_t current_policies)>;
using tracking_callback = std::function<void(const LEAP_TRACKING_EVENT& tracking_event)>;
using image_callback = std::function<void(const LEAP_IMAGE_EVENT& image_event)>;
using imu_callback = std::function<void(const LEAP_IMU_EVENT& imu_event)>;
using tracking_mode_callback = std::function<void(const LEAP_TRACKING_MODE_EVENT& mode_event)>;


class leap_connection_class
{
    std::thread polling_thread;
    std::mutex data_lock;

    LEAP_CONNECTION leap_connection{};
    bool is_service_running_ = false;
    bool is_connected = false;

    std::unique_ptr<LEAP_DEVICE_INFO> last_device;
    std::unique_ptr<LEAP_TRACKING_EVENT> last_frame;

    void playback_record(const std::string& filename);
    void service_message_loop();
    static std::string result_string(eLeapRS result);
    void handle_connection_event(const LEAP_CONNECTION_EVENT*);
    void handle_connection_lost_event(const LEAP_CONNECTION_LOST_EVENT*);
    void handle_device_event(const LEAP_DEVICE_EVENT*);
    void handle_device_lost_event(const LEAP_DEVICE_EVENT*) const;
    void handle_device_failure_event(const LEAP_DEVICE_FAILURE_EVENT*) const;
    void handle_tracking_event(const LEAP_TRACKING_EVENT*);
    void handle_policy_event(const LEAP_POLICY_EVENT* ) const;
    void handle_image_event(const LEAP_IMAGE_EVENT*) const;
    void handle_tracking_mode_event(const LEAP_TRACKING_MODE_EVENT*) const;
    void handle_imu_event(const LEAP_IMU_EVENT*) const;

    void set_frame(const LEAP_TRACKING_EVENT& frame);
    void set_device(LEAP_DEVICE_INFO& device_info);

public:
    connection_callback on_connection;
    connection_callback on_connection_lost;
    device_callback on_device_found;
    device_lost_callback on_device_lost;
    device_failure_callback on_device_failure;
    policy_callback on_policy;
    tracking_callback on_frame;
    image_callback on_image;
    imu_callback on_imu;
    tracking_mode_callback on_tracking_mode;
    
    explicit leap_connection_class(uint64_t set = 0, uint64_t clear = 0);
    explicit leap_connection_class(const LEAP_ALLOCATOR& allocator, uint64_t set = 0, uint64_t clear = 0);
    ~leap_connection_class();
    void start_service();
    void start_playback(const std::string& filename);
    void terminate_service();
    bool is_service_running() const;
};


#endif //LEAP_CONNECTION_H
