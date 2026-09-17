#include "client/WinlatorXrPresentationCompanion.h"

#include "client/WinlatorXrDesktopMouseFilter.h"
#include "client/WinlatorXrWatchdog.h"
#include "presenter/SharedControlChannel.h"
#include "settings/UserSettings.h"
#include "stereo/MainMenuOverlayLayout.h"

#include <mmsystem.h>

#include <algorithm>
#include <cstdio>
#include <cwchar>
#include <fstream>
#include <iterator>

namespace bfvr
{
namespace
{
constexpr DWORD kDxgiFormatB8G8R8A8Unorm = 87;
constexpr DWORD kD3DFeatureLevel11_0 = 0xB000;
constexpr ULONGLONG kStalePacketMs = 500;
constexpr ULONGLONG kStaleFrameMs = 1000;
constexpr float kDefaultHudScale = 0.70F;
// Native menu UI drawn head-locked while a BFVR panel is open.
constexpr float kMenuUnderOverlayUiScale = 0.80F;
constexpr UINT kMinimumEyeDimension = 256;
constexpr ULONGLONG kSettingsPollMs = 250;
constexpr ULONGLONG kComfortMotionFreshMs = 150;
constexpr float kVisibleVignette = 0.001F;

bool ReadEyeSizeOverride(UINT& width, UINT& height)
{
    wchar_t value[32] = {};
    const DWORD length = GetEnvironmentVariableW(
        L"BFVR_WINLATORXR_EYE_SIZE",
        value,
        static_cast<DWORD>(std::size(value)));
    if (length == 0 || length >= std::size(value))
    {
        return false;
    }
    unsigned long parsedWidth = 0;
    unsigned long parsedHeight = 0;
    if (swscanf_s(value, L"%lux%lu", &parsedWidth, &parsedHeight) != 2 ||
        parsedWidth < kMinimumEyeDimension || parsedHeight < kMinimumEyeDimension ||
        parsedWidth > 4096 || parsedHeight > 4096)
    {
        return false;
    }
    width = static_cast<UINT>(parsedWidth);
    height = static_cast<UINT>(parsedHeight);
    return true;
}

// Optional field of view override in degrees (BFVR_WINLATORXR_FOV, and
// BFVR_WINLATORXR_FOV_V for a different vertical value). 0 keeps the
// headset's native FOV reported by WinlatorXR.
float ReadFovDegrees(const wchar_t* name)
{
    wchar_t value[16] = {};
    const DWORD length = GetEnvironmentVariableW(name, value, static_cast<DWORD>(std::size(value)));
    if (length == 0 || length >= std::size(value))
    {
        return 0.0F;
    }
    const float parsed = std::wcstof(value, nullptr);
    return parsed >= 40.0F && parsed <= 150.0F ? parsed : 0.0F;
}

bool ReadAlternateEyeEnabled()
{
    wchar_t value[4] = {};
    const DWORD length = GetEnvironmentVariableW(
        L"BFVR_WINLATORXR_AER",
        value,
        static_cast<DWORD>(std::size(value)));
    return !(length == 1 && value[0] == L'0');
}

bool ReadMenusEnabled()
{
    wchar_t value[4] = {};
    const DWORD length = GetEnvironmentVariableW(
        L"BFVR_WINLATORXR_MENUS",
        value,
        static_cast<DWORD>(std::size(value)));
    return !(length == 1 && value[0] == L'0');
}

float ReadHudScale()
{
    wchar_t value[16] = {};
    const DWORD length = GetEnvironmentVariableW(
        L"BFVR_WINLATORXR_HUD_SCALE",
        value,
        static_cast<DWORD>(std::size(value)));
    if (length == 0 || length >= std::size(value))
    {
        return kDefaultHudScale;
    }
    const float parsed = std::wcstof(value, nullptr);
    return parsed >= 0.3F && parsed <= 1.0F ? parsed : kDefaultHudScale;
}

std::int64_t NowNanoseconds()
{
    static const LARGE_INTEGER frequency = [] {
        LARGE_INTEGER value = {};
        QueryPerformanceFrequency(&value);
        return value;
    }();
    LARGE_INTEGER counter = {};
    QueryPerformanceCounter(&counter);
    const std::int64_t seconds = counter.QuadPart / frequency.QuadPart;
    const std::int64_t remainder = counter.QuadPart % frequency.QuadPart;
    return seconds * 1'000'000'000LL +
        remainder * 1'000'000'000LL / frequency.QuadPart;
}

LONG ReadCounter(const volatile LONG& counter)
{
    return InterlockedCompareExchange(
        const_cast<volatile LONG*>(&counter),
        0,
        0);
}
} // namespace

bool WinlatorXrPresentationCompanion::Start(
    LogCallback log,
    const std::wstring& payloadDirectory)
{
    Stop();
    log_ = log;
    StartWinlatorXrWatchdog(log);
    alternateEye_ = ReadAlternateEyeEnabled();
    fovHorizontalOverride_ = ReadFovDegrees(L"BFVR_WINLATORXR_FOV");
    fovVerticalOverride_ = ReadFovDegrees(L"BFVR_WINLATORXR_FOV_V");
    if (fovVerticalOverride_ == 0.0F)
    {
        fovVerticalOverride_ = fovHorizontalOverride_;
    }
    if (fovHorizontalOverride_ > 0.0F)
    {
        wchar_t message[120] = {};
        swprintf_s(
            message,
            L"WinlatorXR companion: rendering and displaying a %.0f x %.0f degree field of view.",
            fovHorizontalOverride_,
            fovVerticalOverride_);
        Log(message);
    }
    const HMODULE translator = GetModuleHandleW(L"BFVRD3D8To9.dll");
    compose_ = translator == nullptr
        ? nullptr
        : reinterpret_cast<BFVRD3D8To9ComposeSideBySideFn>(
            GetProcAddress(translator, "BFVRD3D8To9ComposeSideBySide"));
    if (compose_ == nullptr)
    {
        Log(L"WinlatorXR companion requires the BFVR d3d8to9 side-by-side export; it is unavailable.");
        return false;
    }
    if (!client_.Start(log))
    {
        return false;
    }
    Log(alternateEye_
        ? L"WinlatorXR companion started: in-process XrAPI presentation, alternate-eye composition (one full-window eye per frame) before the native Present."
        : L"WinlatorXR companion started: in-process XrAPI presentation, side-by-side composition before the native Present.");
    if (ReadMenusEnabled())
    {
        menus_.Start(payloadDirectory, log);
    }
    else
    {
        Log(L"WinlatorXR companion: Quick Menu and VR Settings are disabled (BFVR_WINLATORXR_MENUS=0).");
    }
    if (!killSoundLoaded_)
    {
        killSoundLoaded_ = true;
        std::ifstream file(payloadDirectory + L"\\assets\\Sounds\\killsound.wav", std::ios::binary);
        killSound_.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        if (killSound_.empty())
        {
            Log(L"WinlatorXR companion: assets\\Sounds\\killsound.wav is unavailable; kill sounds are off.");
        }
    }
    nextSettingsPollMs_ = 0;
    return true;
}

void WinlatorXrPresentationCompanion::Stop()
{
    if (client_.IsRunning())
    {
        client_.SendState(
            0.0F,
            0.0F,
            winlatorxr::DisplayMode::VirtualScreen,
            winlatorxr::StereoLayout::Mono,
            fovHorizontalOverride_,
            fovVerticalOverride_);
    }
    client_.Stop();
    menus_.Stop();
    SetWinlatorXrDesktopMouseBlocked(false);
    compose_ = nullptr;
    targets_ = {};
    shortcuts_ = {};
    requestSequence_ = 0;
    requestFrameId_ = 0;
    framePublished_ = false;
    showingVirtualScreen_ = true;
    haptics_ = {};
    hapticCounters_ = {};
    hapticCountersInitialized_ = false;
    composeFailureLogged_ = false;
    firstComposeLogged_ = false;
}

void WinlatorXrPresentationCompanion::PublishRequirements(
    shared::ControlBlock& block,
    UINT logicalUiWidth,
    UINT logicalUiHeight) const
{
    // Side by side, each half of the game window is stretched over one eye;
    // alternate-eye presentation gives every eye the whole window.
    UINT eyeWidth = std::max(
        alternateEye_ ? logicalUiWidth : logicalUiWidth / 2,
        kMinimumEyeDimension);
    UINT eyeHeight = std::max(logicalUiHeight, kMinimumEyeDimension);
    const bool overridden = ReadEyeSizeOverride(eyeWidth, eyeHeight);
    block.requirements.leftWorldWidth = eyeWidth;
    block.requirements.leftWorldHeight = eyeHeight;
    block.requirements.rightWorldWidth = eyeWidth;
    block.requirements.rightWorldHeight = eyeHeight;
    block.requirements.uiWidth = logicalUiWidth;
    block.requirements.uiHeight = logicalUiHeight;
    block.requirements.format = kDxgiFormatB8G8R8A8Unorm;
    block.requirements.adapterLuidHigh = 0;
    block.requirements.adapterLuidLow = 0;
    block.requirements.minimumFeatureLevel = kD3DFeatureLevel11_0;
    block.requirements.deviceFeatureLevel = kD3DFeatureLevel11_0;
    block.presenterProcessId = GetCurrentProcessId();
    wchar_t message[160] = {};
    swprintf_s(
        message,
        L"WinlatorXR companion requirements: eye destination %ux%u%s, UI %ux%u.",
        eyeWidth,
        eyeHeight,
        overridden ? L" (BFVR_WINLATORXR_EYE_SIZE)" : L"",
        logicalUiWidth,
        logicalUiHeight);
    Log(message);
    shared::PublishState(&block.presenterState, shared::ProcessState::RequirementsReady);
}

bool WinlatorXrPresentationCompanion::PublishRenderRequest(
    shared::ControlBlock& block,
    LONG sequence)
{
    const winlatorxr::ReceivedState received = client_.Latest();
    winlatorxr::InputState viewInput = received.input;
    if (fovHorizontalOverride_ > 0.0F)
    {
        // The same FOV is sent back in every state packet, so WinlatorXR
        // displays the frame with the projection it was rendered with.
        viewInput.fovHorizontalDegrees = fovHorizontalOverride_;
        viewInput.fovVerticalDegrees = fovVerticalOverride_;
    }
    winlatorxr::RenderViews views;
    if (received.packetCount == 0 || !winlatorxr::BuildRenderViews(viewInput, views))
    {
        return false;
    }

    const std::int64_t predictedTime =
        std::max(NowNanoseconds(), lastPredictedTime_ + 1);
    lastPredictedTime_ = predictedTime;

    shared::SharedControllerSample sample;
    winlatorxr::BuildControllerSample(received.input, predictedTime, sample);
    if (GetTickCount64() - received.receivedAtMs > kStalePacketMs)
    {
        // WinlatorXR stopped streaming (for example the headset was taken
        // off); native input must not keep acting on the last held buttons.
        sample.flags = 0;
    }
    const bool focused =
        (sample.flags & shared::kControllerSampleFlagSessionFocused) != 0;
    const OpenXRControllerShortcutOutput shortcuts = UpdateOpenXRControllerShortcuts(
        shortcuts_,
        {predictedTime, focused, false, received.input.buttons.buttonB});
    if (shortcuts.recenterRequested)
    {
        ++recenterSequence_;
        Log(L"WinlatorXR companion: recenter requested (B held).");
    }

    UpdateMenus(block, views, sample, predictedTime);
    block.controllerSample = sample;
    block.controllerSample.mountedCameraToggleSequence = mountedCameraToggleSequence_;
    block.controllerSample.hudToggleSequence = hudToggleSequence_;
    MemoryBarrier();
    InterlockedExchange(&block.controllerSampleSequence, sequence);

    shared::SharedRenderRequest& request = block.renderRequest;
    request.predictedDisplayTime = predictedTime;
    request.shouldRender = 1;
    request.viewsValid = 1;
    request.headPoseValid = 1;
    request.headPoseTracked = 1;
    request.standingHeightValid = views.standingHeightValid ? 1 : 0;
    request.standingHeightMeters = views.standingHeightMeters;
    request.recenterForwardSequence = recenterSequence_;
    request.headPose = views.head;
    request.views[0] = views.eyes[0];
    request.views[1] = views.eyes[1];
    MemoryBarrier();
    InterlockedExchange(&block.renderRequestSequence, sequence);

    requestSequence_ = sequence;
    requestFrameId_ = received.input.frameId;
    requestViews_ = views.eyes;
    requestPredictedTime_ = predictedTime;
    LogHandSample(received.input);
    return true;
}

void WinlatorXrPresentationCompanion::OnFramePublished(
    shared::ControlBlock& block,
    LONG sequence,
    bool headLocked,
    bool eyeFillingScope)
{
    if (sequence == requestSequence_)
    {
        publishedFrameId_ = requestFrameId_;
        publishedHeadLocked_ = headLocked;
        publishedScope_ = eyeFillingScope;
        publishedHasWorld_ = nextFrameHasWorld_;
        publishedEye_ = ActiveEyeForRequest(sequence);
        publishedViews_ = requestViews_;
        publishedOverlayFlags_ = ReadCounter(block.frameOverlayFlags);
        UpdateComfortMotion(block);
        publishedAtMs_ = GetTickCount64();
        framePublished_ = true;
    }
    InterlockedIncrement(&block.transportedFrameCount);
    InterlockedIncrement(&block.presentedFrameCount);
    MemoryBarrier();
    InterlockedExchange(&block.consumedFrameSequence, sequence);
    InterlockedExchange(&block.renderedFrameSequence, sequence);
}

void WinlatorXrPresentationCompanion::SetNextFrameHasWorld(bool hasWorld) noexcept
{
    nextFrameHasWorld_ = hasWorld;
}

int WinlatorXrPresentationCompanion::ActiveEyeForRequest(LONG sequence) const noexcept
{
    return alternateEye_ ? static_cast<int>(sequence & 1) : -1;
}

void WinlatorXrPresentationCompanion::SetTargets(
    const std::array<void*, shared::kTextureCount>& targets) noexcept
{
    targets_ = targets;
}

void WinlatorXrPresentationCompanion::ClearTargets() noexcept
{
    menus_.ReleaseTextures();
    targets_ = {};
    framePublished_ = false;
}

void WinlatorXrPresentationCompanion::Compose(
    void* d3d8Device,
    const shared::ControlBlock& block)
{
    if (compose_ == nullptr || !client_.IsRunning())
    {
        return;
    }
    NoteWinlatorXrFrameComposed();
    const std::array<float, 2> haptics = ConsumeHaptics(block);
    PollSettings();
    PlayKillSounds(block);
    UpdateWorldEffects(block);
    const bool targetsReady = std::all_of(
        targets_.begin(),
        targets_.end(),
        [](const void* target) { return target != nullptr; });
    if (!targetsReady || !framePublished_ ||
        GetTickCount64() - publishedAtMs_ > kStaleFrameMs)
    {
        // Nothing current to show: let WinlatorXR display the flat game
        // window on its virtual screen.
        client_.SendState(
            haptics[0],
            haptics[1],
            winlatorxr::DisplayMode::VirtualScreen,
            winlatorxr::StereoLayout::Mono,
            fovHorizontalOverride_,
            fovVerticalOverride_);
        SetWinlatorXrDesktopMouseBlocked(false);
        if (!showingVirtualScreen_)
        {
            Log(L"WinlatorXR companion: no current stereo frame; showing the flat window.");
            showingVirtualScreen_ = true;
        }
        return;
    }

    BFVRD3D8To9SideBySideParamsV2 overlayParams = {};
    BFVRD3D8To9SideBySideParams& params = overlayParams.base;
    params.size = sizeof(overlayParams);
    params.version = BFVR_D3D8TO9_SIDE_BY_SIDE_VERSION_OVERLAYS;
    const bool menusVisible = menus_.IsVisible();
    // Native menus are world-anchored panels on PC. Without a compositor they
    // are drawn once, flat, and WinlatorXR shows them on its world-fixed
    // virtual screen. Gameplay shows the world with the VIEW-space HUD
    // blended over each eye.
    // Frames without world geometry are menus, loading and spawn screens.
    const bool nativeMenuFrame = !publishedScope_ &&
        (!publishedHasWorld_ || !publishedHeadLocked_);
    // BFVR's own panels live in tracked space, so while one is open a native
    // menu is shown head-locked instead of on the virtual screen.
    const bool menuFrame = nativeMenuFrame && !menusVisible;
    const bool singleEye = !menuFrame && publishedEye_ >= 0;
    if (menuFrame)
    {
        params.flags = BFVR_D3D8TO9_SIDE_BY_SIDE_UI |
            BFVR_D3D8TO9_SIDE_BY_SIDE_MONO_UI;
        params.uiScale = 1.0F;
        // The frame-sync block only applies to head-tracked frames.
        params.syncColor = 0;
        AppendBackToGame(d3d8Device, 0.0F, 1.0F, 1.0F, overlayParams);
    }
    else
    {
        params.flags = BFVR_D3D8TO9_SIDE_BY_SIDE_UI |
            (nativeMenuFrame ? 0 : BFVR_D3D8TO9_SIDE_BY_SIDE_WORLD) |
            (singleEye ? BFVR_D3D8TO9_SIDE_BY_SIDE_SINGLE_EYE : 0);
        params.uiScale = publishedScope_ ? 1.0F
            : nativeMenuFrame           ? kMenuUnderOverlayUiScale
                                        : ReadHudScale();
        overlayParams.vignetteStrength = vignetteStrength_;
        overlayParams.vignetteDeathBlend = vignetteDeathBlend_;
        overlayParams.colorProfile = colorProfile_;
        overlayParams.colorExposureEv = colorExposureEv_;
        overlayParams.colorContrast = colorContrast_;
        overlayParams.colorSaturation = colorSaturation_;
        if (nativeMenuFrame)
        {
            if (singleEye)
            {
                AppendBackToGame(d3d8Device, 0.0F, 1.0F, params.uiScale, overlayParams);
            }
            else
            {
                AppendBackToGame(d3d8Device, 0.0F, 0.5F, params.uiScale, overlayParams);
                AppendBackToGame(d3d8Device, 0.5F, 0.5F, params.uiScale, overlayParams);
            }
        }
        if (menusVisible)
        {
            constexpr UINT kCapacity = BFVR_D3D8TO9_MAX_OVERLAY_QUADS;
            UINT& count = overlayParams.overlayCount;
            if (singleEye)
            {
                count += menus_.AppendQuads(
                    d3d8Device,
                    publishedViews_[static_cast<std::size_t>(publishedEye_)],
                    0.0F,
                    1.0F,
                    overlayParams.overlays + count,
                    kCapacity - count);
            }
            else
            {
                const UINT perEye = (kCapacity - count) / 2;
                count += menus_.AppendQuads(
                    d3d8Device, publishedViews_[0], 0.0F, 0.5F, overlayParams.overlays + count, perEye);
                count += menus_.AppendQuads(
                    d3d8Device, publishedViews_[1], 0.5F, 0.5F, overlayParams.overlays + count, perEye);
            }
        }
        // WinlatorXR reads pixel (0,0): G == 0 and A > 0 marks a head-tracked
        // frame whose R value is the index of the pose it was rendered with;
        // with alternate eyes, B > 0 marks the right eye.
        params.syncColor =
            0xFF000000UL |
            (static_cast<DWORD>(publishedFrameId_ & 0xFF) << 16) |
            (singleEye && publishedEye_ == 1 ? 0xFFUL : 0UL);
    }
    void* const firstEyeTarget =
        singleEye ? targets_[static_cast<std::size_t>(publishedEye_)] : targets_[0];

    const HRESULT result = compose_(
        d3d8Device,
        firstEyeTarget,
        targets_[1],
        targets_[2],
        &params);
    if (FAILED(result))
    {
        SetWinlatorXrDesktopMouseBlocked(false);
        if (!composeFailureLogged_)
        {
            wchar_t message[120] = {};
            swprintf_s(
                message,
                L"WinlatorXR companion: side-by-side composition failed (HRESULT=0x%08lX).",
                static_cast<unsigned long>(result));
            Log(message);
            composeFailureLogged_ = true;
        }
        return;
    }
    if (menuFrame)
    {
        client_.SendState(
            haptics[0],
            haptics[1],
            winlatorxr::DisplayMode::VirtualScreen,
            winlatorxr::StereoLayout::Mono,
            fovHorizontalOverride_,
            fovVerticalOverride_);
    }
    else
    {
        client_.SendState(
            haptics[0],
            haptics[1],
            winlatorxr::DisplayMode::HeadTracked,
            singleEye
                ? winlatorxr::StereoLayout::AlternateEye
                : winlatorxr::StereoLayout::SideBySide,
            fovHorizontalOverride_,
            fovVerticalOverride_);
    }
    showingVirtualScreen_ = menuFrame;
    // Menus use WinlatorXR's pointer as the mouse; gameplay turns only
    // through BFVR's right-stick handling.
    SetWinlatorXrDesktopMouseBlocked(!menuFrame);
    if (result == S_FALSE && !effectsFailureLogged_)
    {
        Log(L"WinlatorXR companion: comfort vignette and color grading are unavailable (the effect shader could not be compiled).");
        effectsFailureLogged_ = true;
    }
    else if (result == S_OK && !effectsLogged_ &&
        (overlayParams.vignetteStrength > 0.0F || overlayParams.vignetteDeathBlend > 0.0F ||
         overlayParams.colorProfile > 0.5F || overlayParams.colorExposureEv != 0.0F ||
         overlayParams.colorContrast != 0.0F || overlayParams.colorSaturation != 0.0F))
    {
        Log(L"WinlatorXR companion: first frame composed with comfort vignette or color grading.");
        effectsLogged_ = true;
    }
    if (!firstComposeLogged_)
    {
        Log(L"WinlatorXR companion: first side-by-side frame composed.");
        firstComposeLogged_ = true;
    }
}

void WinlatorXrPresentationCompanion::PollSettings()
{
    const ULONGLONG now = GetTickCount64();
    if (now < nextSettingsPollMs_)
    {
        return;
    }
    nextSettingsPollMs_ = now + kSettingsPollMs;
    auto& runtime = settings::ProcessUserSettingsRuntime();
    (void)runtime.ReloadIfChanged();
    const settings::UserSettingsValues values = runtime.IsReady()
        ? settings::DecodeUserSettings(runtime.Current())
        : settings::UserSettingsValues{};
    comfortVignetteEnabled_ = values.comfortVignetteEnabled;
    deathComfortEnabled_ = values.deathCameraComfortEnabled;
    killSoundEnabled_ = values.killSoundEnabled;
    colorProfile_ = std::clamp(static_cast<float>(values.colorProfile), 0.0F, 2.0F);
    colorExposureEv_ = std::clamp(static_cast<float>(values.colorExposureTenthsEv) / 10.0F, -1.0F, 1.0F);
    colorContrast_ = std::clamp(static_cast<float>(values.colorContrastPercent) / 100.0F, -0.5F, 0.5F);
    colorSaturation_ = std::clamp(static_cast<float>(values.colorSaturationPercent) / 100.0F, -1.0F, 1.0F);
}

void WinlatorXrPresentationCompanion::UpdateComfortMotion(const shared::ControlBlock& block)
{
    // The producer publishes the movement origin before frameSequence.
    const std::uint64_t token =
        static_cast<std::uint64_t>(block.frameMovementContextTokenLow) |
        (static_cast<std::uint64_t>(block.frameMovementContextTokenHigh) << 32U);
    comfortMotionTarget_ = stereo::UpdateComfortVignetteMotionTarget(
        comfortMotion_,
        {
            ReadCounter(block.frameMovementOriginValid) != 0,
            token,
            requestPredictedTime_,
            {block.frameMovementOriginX, block.frameMovementOriginY, block.frameMovementOriginZ}});
    lastComfortSampleMs_ = GetTickCount64();
}

void WinlatorXrPresentationCompanion::UpdateWorldEffects(const shared::ControlBlock& block)
{
    const ULONGLONG nowMs = GetTickCount64();
    const LONG deathSequence = ReadCounter(block.hapticDeathSequence);
    // Same read order as the PC presenter: death sequence, then life state.
    MemoryBarrier();
    const LONG lifeState = ReadCounter(block.localPlayerLifeState);
    const bool deathActive = stereo::UpdateDeathComfortActive(
        deathComfort_,
        deathComfortEnabled_,
        static_cast<std::int32_t>(deathSequence),
        lifeState != static_cast<LONG>(shared::LocalPlayerLifeState::Unknown),
        lifeState == static_cast<LONG>(shared::LocalPlayerLifeState::Alive),
        static_cast<std::uint64_t>(nowMs));
    const bool motionFresh = lastComfortSampleMs_ != 0 &&
        nowMs - lastComfortSampleMs_ <= kComfortMotionFreshMs;
    const float motionTarget = comfortVignetteEnabled_ && motionFresh
        ? std::clamp(comfortMotionTarget_, 0.0F, 1.0F)
        : 0.0F;

    const std::int64_t now = NowNanoseconds();
    const float deltaSeconds = lastEffectsTime_ == 0
        ? 0.0F
        : std::min(static_cast<float>(now - lastEffectsTime_) / 1.0e9F, 0.1F);
    lastEffectsTime_ = now;
    vignetteStrength_ = stereo::AdvanceComfortVignetteStrength(vignetteStrength_, motionTarget, deltaSeconds);
    vignetteDeathBlend_ = stereo::AdvanceComfortVignetteStrength(
        vignetteDeathBlend_,
        deathActive ? 1.0F : 0.0F,
        deltaSeconds);
    if (vignetteStrength_ <= kVisibleVignette && vignetteDeathBlend_ <= kVisibleVignette)
    {
        vignetteStrength_ = 0.0F;
        vignetteDeathBlend_ = 0.0F;
    }
}

void WinlatorXrPresentationCompanion::PlayKillSounds(const shared::ControlBlock& block)
{
    const LONG available = ReadCounter(block.killSoundSequence);
    if (!killSoundCounterInitialized_)
    {
        consumedKillSounds_ = available;
        killSoundCounterInitialized_ = true;
    }
    if (available == consumedKillSounds_)
    {
        return;
    }
    consumedKillSounds_ = available;
    // One asynchronous voice: a newer kill restarts the sound.
    if (killSoundEnabled_ && !killSound_.empty())
    {
        (void)PlaySoundW(
            reinterpret_cast<LPCWSTR>(killSound_.data()),
            nullptr,
            SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
    }
}

void WinlatorXrPresentationCompanion::AppendBackToGame(
    void* d3d8Device,
    float regionLeft,
    float regionWidth,
    float uiScale,
    BFVRD3D8To9SideBySideParamsV2& params)
{
    if ((publishedOverlayFlags_ & shared::kFrameOverlayBackToGameVisible) == 0 ||
        params.overlayCount >= BFVR_D3D8TO9_MAX_OVERLAY_QUADS)
    {
        return;
    }
    // The game's 800x600 logical menu canvas spans the whole UI texture.
    const stereo::UiCanvasRect rect = stereo::BackToGameButtonRect();
    const auto x = [&](float logical) {
        return regionLeft +
            (0.5F + (logical / stereo::kMainMenuCanvasWidth - 0.5F) * uiScale) * regionWidth;
    };
    const auto y = [&](float logical) {
        return 0.5F + (logical / stereo::kMainMenuCanvasHeight - 0.5F) * uiScale;
    };
    if (menus_.BuildBackToGameQuad(
            d3d8Device,
            (publishedOverlayFlags_ & shared::kFrameOverlayBackToGameHovered) != 0,
            x(rect.left),
            y(rect.top),
            x(rect.right),
            y(rect.bottom),
            params.overlays[params.overlayCount]))
    {
        ++params.overlayCount;
    }
}

void WinlatorXrPresentationCompanion::UpdateMenus(
    shared::ControlBlock& block,
    const winlatorxr::RenderViews& views,
    const shared::SharedControllerSample& sample,
    std::int64_t predictedTime)
{
    const shared::SharedControllerHandSample& right = sample.hands[1];
    const auto has = [&](DWORD flags) { return (right.flags & flags) == flags; };
    const auto toPose = [](const shared::SharedPresentationPose& pose) {
        stereo::Pose result;
        result.position = {pose.positionX, pose.positionY, pose.positionZ};
        result.orientation = {pose.orientationX, pose.orientationY, pose.orientationZ, pose.orientationW};
        return result;
    };
    stereo::QuickMenuFrameInput input;
    input.predictedDisplayTime = predictedTime;
    input.sessionFocused = (sample.flags & shared::kControllerSampleFlagSessionFocused) != 0;
    input.shouldRender = true;
    input.headTracked = true;
    input.headPose = toPose(views.head);
    input.rightGripTracked = has(
        shared::kControllerHandFlagGripActive |
        shared::kControllerHandFlagGripPositionValid |
        shared::kControllerHandFlagGripPositionTracked);
    input.rightAimTracked = has(
        shared::kControllerHandFlagAimActive |
        shared::kControllerHandFlagAimPositionValid |
        shared::kControllerHandFlagAimOrientationValid |
        shared::kControllerHandFlagAimPositionTracked |
        shared::kControllerHandFlagAimOrientationTracked);
    input.rightPrimaryHeld = (right.buttons & shared::kControllerHandButtonPrimary) != 0;
    input.rightGripPose = toPose(right.gripPose);
    input.rightAimPose = toPose(right.aimPose);
    input.standingHeightValid = views.standingHeightValid;
    input.standingHeightMeters = views.standingHeightMeters;

    WinlatorXrMenuActions actions;
    menus_.Update(
        input,
        ReadCounter(block.mountedCameraDecoupled) != 0,
        actions);
    mountedCameraToggleSequence_ += static_cast<LONG>(actions.mountedCameraToggles);
    hudToggleSequence_ += static_cast<LONG>(actions.hudToggles);
    if (actions.recenterForward)
    {
        ++recenterSequence_;
        menus_.SetForwardRecenterResult(true);
    }
    for (UINT index = 0; index < actions.soundHighlight; ++index)
    {
        InterlockedIncrement(&block.nativeMenuSoundHighlightSequence);
    }
    for (UINT index = 0; index < actions.soundOk; ++index)
    {
        InterlockedIncrement(&block.nativeMenuSoundOkSequence);
    }
    for (UINT index = 0; index < actions.soundCancel; ++index)
    {
        InterlockedIncrement(&block.nativeMenuSoundCancelSequence);
    }
    if (actions.hoverHaptic)
    {
        ++pendingMenuHoverHaptics_;
    }
}

void WinlatorXrPresentationCompanion::LogHandSample(const winlatorxr::InputState& input)
{
    // Bounded raw-pose trace for aligning XrAPI hands with BFVR's grip/aim use.
    constexpr int kMaximumHandLogs = 24;
    constexpr ULONGLONG kHandLogIntervalMs = 5000;
    const ULONGLONG now = GetTickCount64();
    if (handLogCount_ >= kMaximumHandLogs || now - lastHandLogMs_ < kHandLogIntervalMs)
    {
        return;
    }
    lastHandLogMs_ = now;
    ++handLogCount_;
    wchar_t message[640] = {};
    swprintf_s(
        message,
        L"WinlatorXR hand sample %d: fov=%.1fx%.1f ipd=%.4f head q=(%.3f %.3f %.3f %.3f) p=(%.3f %.3f %.3f) altitude=%d/%.2f grip=%d | "
        L"left aim q=(%.3f %.3f %.3f %.3f) p=(%.3f %.3f %.3f) grip q=(%.3f %.3f %.3f %.3f) | "
        L"right aim q=(%.3f %.3f %.3f %.3f) p=(%.3f %.3f %.3f) grip q=(%.3f %.3f %.3f %.3f)",
        handLogCount_,
        input.fovHorizontalDegrees, input.fovVerticalDegrees, input.ipd,
        input.headQx, input.headQy, input.headQz, input.headQw,
        input.headX, input.headY, input.headZ,
        input.hasHeadAltitude ? 1 : 0, input.headAltitude,
        input.hasGripOrientation ? 1 : 0,
        input.left.qx, input.left.qy, input.left.qz, input.left.qw,
        input.left.posX, input.left.posY, input.left.posZ,
        input.left.gripQx, input.left.gripQy, input.left.gripQz, input.left.gripQw,
        input.right.qx, input.right.qy, input.right.qz, input.right.qw,
        input.right.posX, input.right.posY, input.right.posZ,
        input.right.gripQx, input.right.gripQy, input.right.gripQz, input.right.gripQw);
    Log(message);
}

void WinlatorXrPresentationCompanion::Log(const wchar_t* message) const
{
    if (log_ != nullptr)
    {
        log_(message);
    }
}

std::array<float, 2> WinlatorXrPresentationCompanion::ConsumeHaptics(
    const shared::ControlBlock& block)
{
    const std::array<LONG, 4> current = {
        ReadCounter(block.hapticShotRightSequence),
        ReadCounter(block.hapticShotBothSequence),
        ReadCounter(block.hapticDeathSequence),
        ReadCounter(block.hapticNativeMenuHoverSequence)};
    winlatorxr::HapticEvents events;
    if (hapticCountersInitialized_)
    {
        events.shotRight = static_cast<std::uint32_t>(current[0] - hapticCounters_[0]);
        events.shotBoth = static_cast<std::uint32_t>(current[1] - hapticCounters_[1]);
        events.death = static_cast<std::uint32_t>(current[2] - hapticCounters_[2]);
        events.menuHover = static_cast<std::uint32_t>(current[3] - hapticCounters_[3]);
    }
    events.menuHover += pendingMenuHoverHaptics_;
    pendingMenuHoverHaptics_ = 0;
    hapticCounters_ = current;
    hapticCountersInitialized_ = true;
    return winlatorxr::UpdateHapticPulses(haptics_, events, NowNanoseconds());
}

} // namespace bfvr
