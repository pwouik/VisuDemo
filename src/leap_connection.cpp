#include "leap_connection.h"

#include <iostream>
#include <cstring>

leap_connection_class::leap_connection_class(const uint64_t set, const uint64_t clear)
{
    auto res = LeapCreateConnection(nullptr, &leap_connection);
    if (res != eLeapRS_Success) std::cout << "Error: " << result_string(res) << ".\n";
    res = LeapOpenConnection(leap_connection);
    if (res != eLeapRS_Success) std::cout << "Error: " << result_string(res) << ".\n";
    res = LeapSetPolicyFlags(leap_connection, set, clear);
    if (res != eLeapRS_Success) std::cout << "Error: " << result_string(res) << ".\n";
}

leap_connection_class::leap_connection_class(const LEAP_ALLOCATOR& allocator, const uint64_t set,
                                             const uint64_t clear): leap_connection_class(set, clear)
{
    const auto res = LeapSetAllocator(leap_connection, &allocator);
    if (res != eLeapRS_Success) std::cout << "Error: " << result_string(res) << ".\n";
}

leap_connection_class::~leap_connection_class()
{
    LeapCloseConnection(leap_connection);
    terminate_service();
    LeapDestroyConnection(leap_connection);
}

void leap_connection_class::start_service()
{
    if (is_service_running) return;
    is_service_running = true;
    polling_thread = std::thread{[this] { this->service_message_loop(); }};
}

void leap_connection_class::start_playback(const std::string& filename)
{
    is_service_running = true;
    polling_thread = std::thread{
        [this, filename]
        {
            LEAP_RECORDING recording;
            LEAP_RECORDING_PARAMETERS parameters;

            parameters.mode = eLeapRecordingFlags_Reading;
            eLeapRS result = LeapRecordingOpen(&recording, filename.c_str(), parameters);
            if (result == eLeapRS_Success)
            {
                LEAP_TRACKING_EVENT* tracking_event = nullptr;
                while (is_service_running && result == eLeapRS_Success)
                {
                    uint64_t next_frame_size = 0;
                    result = LeapRecordingReadSize(recording, &next_frame_size);
                    if (result != eLeapRS_Success)
                    {
                        next_frame_size = 0;
                        std::cerr << "Could not read next frame size: " << result_string(result) << ".\n";
                    }
                    else if (next_frame_size > 0)
                    {
                        tracking_event = new LEAP_TRACKING_EVENT[next_frame_size];
                        result = LeapRecordingRead(recording, tracking_event, next_frame_size);
                        if (result == eLeapRS_Success)
                        {
                            on_frame(*tracking_event);
                        }
                        else
                        {
                            std::cerr << "Could not read frame: " << result_string(result) << ".\n";
                        }
                        delete[] tracking_event;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
                result = LeapRecordingClose(&recording);
                if (result != eLeapRS_Success)
                {
                    std::cerr << "Error: " << result_string(result) << ".\n";
                }
            }
            else
            {
                std::cerr << "Error: " << result_string(result) << ".\n";
            }
            is_service_running = false;
            std::cout << "Playback finished.\n";
        }
    };
}

void leap_connection_class::terminate_service()
{
    if (!is_service_running) return;
    is_service_running = false;
    polling_thread.join();
}

void leap_connection_class::service_message_loop()
{
    LEAP_CONNECTION_MESSAGE connection_message;
    while (is_service_running)
    {
        constexpr unsigned int timeout = 1000;
        const eLeapRS result = LeapPollConnection(leap_connection, timeout, &connection_message);
        if (result != eLeapRS_Success)
        {
            std::cout << "LeapC PollConnection call was " << result_string(result) << ".\n";
            continue;
        }
        switch (connection_message.type)
        {
        case eLeapEventType_Connection:
            handle_connection_event(connection_message.connection_event);
            break;
        case eLeapEventType_ConnectionLost:
            handle_connection_lost_event(connection_message.connection_lost_event);
            break;
        case eLeapEventType_Device:
            handle_device_event(connection_message.device_event);
            break;
        case eLeapEventType_DeviceLost:
            handle_device_lost_event(connection_message.device_event);
            break;
        case eLeapEventType_DeviceFailure:
            handle_device_failure_event(connection_message.device_failure_event);
            break;
        case eLeapEventType_Tracking:
            handle_tracking_event(connection_message.tracking_event);
            break;
        case eLeapEventType_ImageComplete:
        case eLeapEventType_ImageRequestError:
            // Ignore
            break;
        case eLeapEventType_Policy:
            handle_policy_event(connection_message.policy_event);
            break;
        case eLeapEventType_Image:
            handle_image_event(connection_message.image_event);
            break;
        case eLeapEventType_TrackingMode:
            handle_tracking_mode_event(connection_message.tracking_mode_event);
            break;
        case eLeapEventType_IMU:
            handle_imu_event(connection_message.imu_event);
            break;
        default:
            //discard unknown message types
            std::cout << "Unhandled message type " << connection_message.type << ".\n";
        }
    }
}

std::string leap_connection_class::result_string(const eLeapRS result)
{
    switch (result)
    {
    case eLeapRS_Success: return "eLeapRS_Success";
    case eLeapRS_UnknownError: return "eLeapRS_UnknownError";
    case eLeapRS_InvalidArgument: return "eLeapRS_InvalidArgument";
    case eLeapRS_InsufficientResources: return "eLeapRS_InsufficientResources";
    case eLeapRS_InsufficientBuffer: return "eLeapRS_InsufficientBuffer";
    case eLeapRS_Timeout: return "eLeapRS_Timeout";
    case eLeapRS_NotConnected: return "eLeapRS_NotConnected";
    case eLeapRS_HandshakeIncomplete: return "eLeapRS_HandshakeIncomplete";
    case eLeapRS_BufferSizeOverflow: return "eLeapRS_BufferSizeOverflow";
    case eLeapRS_ProtocolError: return "eLeapRS_ProtocolError";
    case eLeapRS_InvalidClientID: return "eLeapRS_InvalidClientID";
    case eLeapRS_UnexpectedClosed: return "eLeapRS_UnexpectedClosed";
    case eLeapRS_UnknownImageFrameRequest: return "eLeapRS_UnknownImageFrameRequest";
    case eLeapRS_UnknownTrackingFrameID: return "eLeapRS_UnknownTrackingFrameID";
    case eLeapRS_RoutineIsNotSeer: return "eLeapRS_RoutineIsNotSeer";
    case eLeapRS_TimestampTooEarly: return "eLeapRS_TimestampTooEarly";
    case eLeapRS_ConcurrentPoll: return "eLeapRS_ConcurrentPoll";
    case eLeapRS_NotAvailable: return "eLeapRS_NotAvailable";
    case eLeapRS_NotStreaming: return "eLeapRS_NotStreaming";
    case eLeapRS_CannotOpenDevice: return "eLeapRS_CannotOpenDevice";
    default: return "unknown result type.";
    }
}

void leap_connection_class::handle_connection_event(const LEAP_CONNECTION_EVENT*)
{
    is_connected = true;
    on_connection();
}

void leap_connection_class::handle_connection_lost_event(const LEAP_CONNECTION_LOST_EVENT*)
{
    is_connected = false;
    on_connection_lost();
}

void leap_connection_class::handle_device_event(const LEAP_DEVICE_EVENT* device_event)
{
    if (device_event == nullptr) return;
    LEAP_DEVICE device;
    eLeapRS result = LeapOpenDevice(device_event->device, &device);
    if (result != eLeapRS_Success) return;
    LEAP_DEVICE_INFO device_info{sizeof(device_info)};
    device_info.serial_length = 1;
    device_info.serial = new char[device_info.serial_length];
    result = LeapGetDeviceInfo(device, &device_info);
    if (result == eLeapRS_InsufficientBuffer)
    {
        //try again with correct buffer size
        device_info.serial = static_cast<char*>(realloc(device_info.serial, device_info.serial_length));
        result = LeapGetDeviceInfo(device, &device_info);
        if (result != eLeapRS_Success)
        {
            std::cout << "Failed to get device info " << result_string(result) << ".\n";
            free(device_info.serial);
        }
    }
    set_device(device_info);
    on_device_found(device_info);

    free(device_info.serial);
    LeapCloseDevice(device);
}

void leap_connection_class::handle_device_lost_event(const LEAP_DEVICE_EVENT*) const
{
    on_device_lost();
}

void leap_connection_class::handle_device_failure_event(const LEAP_DEVICE_FAILURE_EVENT* device_failure_event) const
{
    if (device_failure_event == nullptr) return;
    on_device_failure(device_failure_event->status, device_failure_event->hDevice);
}

void leap_connection_class::handle_tracking_event(const LEAP_TRACKING_EVENT* tracking_event)
{
    if (tracking_event == nullptr) return;
    set_frame(*tracking_event);
    on_frame(*tracking_event);
}

void leap_connection_class::handle_policy_event(const LEAP_POLICY_EVENT* policy_event) const
{
    if (policy_event == nullptr) return;

    on_policy(policy_event->current_policy);
}

void leap_connection_class::handle_image_event(const LEAP_IMAGE_EVENT* image_event) const
{
    if (image_event == nullptr) return;

    on_image(*image_event);
}

void leap_connection_class::handle_tracking_mode_event(const LEAP_TRACKING_MODE_EVENT* tracking_mode_event) const
{
    if (tracking_mode_event == nullptr) return;

    on_tracking_mode(*tracking_mode_event);
}

void leap_connection_class::handle_imu_event(const LEAP_IMU_EVENT* imu_event) const
{
    if (imu_event == nullptr) return;
    on_imu(*imu_event);
}

void leap_connection_class::set_device(LEAP_DEVICE_INFO& device_info)
{
    std::lock_guard<std::mutex> lock_guard(data_lock);
    if (last_device)
    {
        free(last_device->serial);
    }
    last_device.reset(&device_info);
    last_device->serial = new char[device_info.serial_length];
    std::memcpy(last_device->serial, device_info.serial, device_info.serial_length);
}

void deep_copy_tracking_event(LEAP_TRACKING_EVENT& dst, const LEAP_TRACKING_EVENT& src)
{
    std::memcpy(&dst.info, &src.info, sizeof(LEAP_FRAME_HEADER));
    dst.tracking_frame_id = src.tracking_frame_id;
    dst.nHands = src.nHands;
    dst.framerate = src.framerate;
    std::memcpy(dst.pHands, src.pHands, src.nHands * sizeof(LEAP_HAND));
}

void leap_connection_class::set_frame(const LEAP_TRACKING_EVENT& frame)
{
    std::lock_guard<std::mutex> lock_guard(data_lock);
    if (!last_frame)
    {
        last_frame.reset(new LEAP_TRACKING_EVENT);
        last_frame->pHands = new LEAP_HAND[2];
    }
    deep_copy_tracking_event(*last_frame, frame);
}
