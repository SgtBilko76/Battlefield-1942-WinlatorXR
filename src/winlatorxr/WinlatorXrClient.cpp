#include "winlatorxr/WinlatorXrClient.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <iterator>
#include <mutex>
#include <string>
#include <thread>

namespace bfvr::winlatorxr
{
namespace
{
bool HasEnvironmentVariable(const wchar_t* name)
{
    wchar_t value[2] = {};
    SetLastError(ERROR_SUCCESS);
    const DWORD length = GetEnvironmentVariableW(name, value, 2);
    return length != 0 || GetLastError() != ERROR_ENVVAR_NOT_FOUND;
}

void WriteTextFile(const std::wstring& path, const char* text)
{
    const HANDLE file = CreateFileW(
        path.c_str(),
        GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return;
    }
    DWORD written = 0;
    WriteFile(file, text, static_cast<DWORD>(std::strlen(text)), &written, nullptr);
    CloseHandle(file);
}
} // namespace

bool DetectWinlatorXrEnvironment() noexcept
{
    wchar_t overrideValue[4] = {};
    const DWORD overrideLength = GetEnvironmentVariableW(
        L"BFVR_WINLATORXR",
        overrideValue,
        static_cast<DWORD>(std::size(overrideValue)));
    const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    EnvironmentProbe probe;
    probe.overrideValue = overrideLength > 0 && overrideLength < std::size(overrideValue)
        ? std::wstring_view(overrideValue, overrideLength)
        : std::wstring_view();
    probe.runningUnderWine =
        ntdll != nullptr && GetProcAddress(ntdll, "wine_get_version") != nullptr;
    probe.hasAndroidSysvShmServer = HasEnvironmentVariable(L"ANDROID_SYSVSHM_SERVER");
    probe.hasEvshimSharedMemory = HasEnvironmentVariable(L"EVSHIM_SHM_NAME");
    return IsWinlatorXrEnvironment(probe);
}

class Client::Impl
{
public:
    ~Impl()
    {
        Stop();
    }

    bool Start(LogCallback log)
    {
        Stop();
        log_ = log;
        WSADATA data = {};
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
        {
            Log(L"WinlatorXR: WSAStartup failed.");
            return false;
        }
        wsaStarted_ = true;
        WriteMarkerFiles();

        receiveSocket_ = BindReceiveSocket();
        if (receiveSocket_ == INVALID_SOCKET)
        {
            Stop();
            return false;
        }
        sendSocket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        running_ = true;
        thread_ = std::thread([this] { ReceiveLoop(); });
        // Announce the game immediately so WinlatorXR starts streaming poses.
        SendState(0.0F, 0.0F, DisplayMode::VirtualScreen, StereoLayout::Mono, 0.0F, 0.0F);
        return true;
    }

    void Stop()
    {
        running_ = false;
        if (receiveSocket_ != INVALID_SOCKET)
        {
            // Unblocks recvfrom.
            closesocket(receiveSocket_);
            receiveSocket_ = INVALID_SOCKET;
        }
        if (thread_.joinable())
        {
            thread_.join();
        }
        if (sendSocket_ != INVALID_SOCKET)
        {
            closesocket(sendSocket_);
            sendSocket_ = INVALID_SOCKET;
        }
        if (wsaStarted_)
        {
            WSACleanup();
            wsaStarted_ = false;
        }
    }

    bool IsRunning() const noexcept
    {
        return running_;
    }

    ReceivedState Latest() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return latest_;
    }

    void SendState(
        float leftHaptics,
        float rightHaptics,
        DisplayMode displayMode,
        StereoLayout layout,
        float fovHorizontalDegrees,
        float fovVerticalDegrees)
    {
        if (sendSocket_ == INVALID_SOCKET)
        {
            return;
        }
        const std::string packet = FormatStatePacket(
            leftHaptics,
            rightHaptics,
            displayMode,
            layout,
            fovHorizontalDegrees,
            fovVerticalDegrees);
        sockaddr_in target = {};
        target.sin_family = AF_INET;
        target.sin_port = htons(kSendPort);
        target.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        sendto(
            sendSocket_,
            packet.data(),
            static_cast<int>(packet.size()),
            0,
            reinterpret_cast<const sockaddr*>(&target),
            sizeof(target));
    }

private:
    void Log(const wchar_t* message) const
    {
        if (log_ != nullptr)
        {
            log_(message);
        }
    }

    void WriteMarkerFiles()
    {
        // WinlatorXR looks for these in the Linux /tmp/xr directory, which Wine
        // exposes through Z:. The version selects the XrAPI packet format.
        const bool haveZDrive = GetFileAttributesW(L"Z:\\") != INVALID_FILE_ATTRIBUTES;
        const std::wstring directory = haveZDrive ? L"Z:\\tmp\\xr" : L"D:\\xrtemp";
        if (haveZDrive)
        {
            CreateDirectoryW(L"Z:\\tmp", nullptr);
        }
        CreateDirectoryW(directory.c_str(), nullptr);
        const std::string version(kRequestedProtocolVersion);
        WriteTextFile(directory + L"\\version", version.c_str());
        WriteTextFile(directory + L"\\vr", "VR");
    }

    SOCKET BindReceiveSocket()
    {
        for (const std::uint16_t port : {kReceivePort, kReceiveFallbackPort})
        {
            SOCKET candidate = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (candidate == INVALID_SOCKET)
            {
                continue;
            }
            sockaddr_in address = {};
            address.sin_family = AF_INET;
            address.sin_port = htons(port);
            address.sin_addr.s_addr = htonl(INADDR_ANY);
            if (bind(candidate, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0)
            {
                wchar_t message[96] = {};
                swprintf_s(message, L"WinlatorXR: receiving XrAPI packets on UDP port %u.", port);
                Log(message);
                return candidate;
            }
            closesocket(candidate);
        }
        Log(L"WinlatorXR: no XrAPI UDP port could be bound.");
        return INVALID_SOCKET;
    }

    void ReceiveLoop()
    {
        char buffer[2048] = {};
        while (running_)
        {
            const int received = recvfrom(
                receiveSocket_,
                buffer,
                sizeof(buffer) - 1,
                0,
                nullptr,
                nullptr);
            if (!running_)
            {
                break;
            }
            if (received <= 0)
            {
                continue;
            }
            InputState parsed;
            if (!ParsePacket(std::string_view(buffer, static_cast<std::size_t>(received)), parsed))
            {
                continue;
            }
            std::lock_guard<std::mutex> lock(mutex_);
            const bool first = latest_.packetCount == 0;
            latest_.input = parsed;
            latest_.receivedAtMs = GetTickCount64();
            ++latest_.packetCount;
            if (first)
            {
                Log(L"WinlatorXR: first head pose received.");
            }
        }
    }

    LogCallback log_ = nullptr;
    bool wsaStarted_ = false;
    SOCKET receiveSocket_ = INVALID_SOCKET;
    SOCKET sendSocket_ = INVALID_SOCKET;
    std::atomic<bool> running_{false};
    std::thread thread_;
    mutable std::mutex mutex_;
    ReceivedState latest_;
};

Client::Client()
    : impl_(std::make_unique<Impl>())
{
}

Client::~Client() = default;

bool Client::Start(LogCallback log)
{
    return impl_->Start(log);
}

void Client::Stop()
{
    impl_->Stop();
}

bool Client::IsRunning() const noexcept
{
    return impl_->IsRunning();
}

ReceivedState Client::Latest() const
{
    return impl_->Latest();
}

void Client::SendState(
    float leftHaptics,
    float rightHaptics,
    DisplayMode displayMode,
    StereoLayout layout,
    float fovHorizontalDegrees,
    float fovVerticalDegrees)
{
    impl_->SendState(
        leftHaptics,
        rightHaptics,
        displayMode,
        layout,
        fovHorizontalDegrees,
        fovVerticalDegrees);
}

} // namespace bfvr::winlatorxr
