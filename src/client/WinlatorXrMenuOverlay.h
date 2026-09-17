#pragma once

#include "presenter/SharedPresentationProtocol.h"
#include "settings/UserSettings.h"
#include "stereo/QuickMenuInteraction.h"
#include "stereo/SettingsMenuInteraction.h"

#include "bfvr_shared_bridge.hpp"

#include <windows.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace bfvr
{

class MainMenuOverlay;
class QuickMenuArt;
class SettingsMenuArt;

// What one menu update asks the WinlatorXR companion to publish.
struct WinlatorXrMenuActions
{
    UINT mountedCameraToggles = 0;
    UINT hudToggles = 0;
    bool recenterForward = false;
    UINT soundHighlight = 0;
    UINT soundOk = 0;
    UINT soundCancel = 0;
    bool hoverHaptic = false;
};

// In-process port of the presenter's Quick Menu (hold right A) and VR
// Settings panel for WinlatorXR. Interaction, settings sessions and menu
// commands follow OpenXRQuickMenu and BFVRPresenter; the panels are drawn by
// projecting their LOCAL quads into each eye of the composed game frame.
// All calls except the art loader come from the game's D3D8 device thread.
class WinlatorXrMenuOverlay
{
public:
    using LogCallback = void (*)(const wchar_t* message);

    WinlatorXrMenuOverlay();
    ~WinlatorXrMenuOverlay();
    WinlatorXrMenuOverlay(const WinlatorXrMenuOverlay&) = delete;
    WinlatorXrMenuOverlay& operator=(const WinlatorXrMenuOverlay&) = delete;

    // Loads the menu art on a worker thread; the menus stay inert until then.
    void Start(const std::wstring& payloadDirectory, LogCallback log);
    void Stop();

    void Update(
        const stereo::QuickMenuFrameInput& input,
        bool mountedCameraDecoupled,
        WinlatorXrMenuActions& actions);
    void SetForwardRecenterResult(bool succeeded) noexcept;

    [[nodiscard]] bool IsVisible() const noexcept;

    // Appends the visible panels projected into one eye whose image covers
    // the normalized back-buffer columns [regionLeft, regionLeft+regionWidth).
    UINT AppendQuads(
        void* d3d8Device,
        const shared::SharedPresentationView& eye,
        float regionLeft,
        float regionWidth,
        BFVRD3D8To9OverlayQuad* quads,
        UINT capacity);

    // Adds the main-menu "Back to game" button over the rectangle given in
    // normalized back-buffer coordinates. Returns false while unavailable.
    bool BuildBackToGameQuad(
        void* d3d8Device,
        bool hovered,
        float left,
        float top,
        float right,
        float bottom,
        BFVRD3D8To9OverlayQuad& quad);

    // Default-pool textures must be gone before the device is Reset.
    void ReleaseTextures() noexcept;

private:
    struct Texture
    {
        void* texture = nullptr;
        UINT width = 0;
        UINT height = 0;
        std::uint64_t key = ~0ULL;
    };

    struct Panel
    {
        std::size_t texture = 0;
        stereo::Pose pose = {};
        float widthMeters = 0.0F;
        float heightMeters = 0.0F;
    };

    void StartLoaderWhenSettled();
    void LoadArt(std::wstring payloadDirectory);
    void OpenSettingsMenu();
    void UpdateSettings(const stereo::QuickMenuFrameInput& input, WinlatorXrMenuActions& actions);
    void Dispatch(stereo::QuickMenuSelection selection, WinlatorXrMenuActions& actions);
    void DispatchPendingRadioKey();
    void RefreshSettingsPixels();
    [[nodiscard]] std::uint64_t HoverTarget() const noexcept;
    [[nodiscard]] bool HapticsEnabled() const noexcept;
    bool UseDevice(void* d3d8Device);
    bool EnsureTexture(void* d3d8Device, std::size_t index, std::uint64_t key);
    void Log(const wchar_t* format, ...) const;

    LogCallback log_ = nullptr;
    std::thread loader_;
    std::wstring payloadDirectory_;
    ULONGLONG loadAfterMs_ = 0;
    std::atomic<bool> artReady_ = false;
    std::atomic<bool> stopLoading_ = false;
    std::unique_ptr<QuickMenuArt> quickArt_;
    std::unique_ptr<SettingsMenuArt> settingsArt_;
    std::unique_ptr<MainMenuOverlay> backToGameArt_;
    bool settingsAvailable_ = false;

    stereo::QuickMenuInteraction interaction_ = {};
    stereo::SettingsMenuInteraction settingsInteraction_ = {};
    settings::UserSettingsSession session_ = {};
    settings::UserSettingsValues startupValues_ = {};
    bool pointerSmoothing_ = true;
    bool mountedCameraDecoupled_ = false;
    std::uint64_t lastHoverTarget_ = 0;

    UINT pendingRadioKey_ = 0;
    ULONGLONG pendingRadioDueAt_ = 0;

    std::vector<std::uint32_t> settingsPixels_;
    stereo::SettingsMenuSnapshot renderedSettings_ = {};
    bool settingsPixelsValid_ = false;
    std::uint64_t settingsPixelsVersion_ = 0;

    BFVRD3D8To9CreateOverlayTextureFn createTexture_ = nullptr;
    BFVRD3D8To9UpdateOverlayTextureFn updateTexture_ = nullptr;
    BFVRD3D8To9ReleaseOverlayTextureFn releaseTexture_ = nullptr;
    void* textureDevice_ = nullptr;
    std::array<Texture, 7> textures_ = {};
    bool textureFailureLogged_ = false;
};

} // namespace bfvr
