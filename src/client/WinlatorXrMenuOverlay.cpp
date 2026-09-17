#include "client/WinlatorXrMenuOverlay.h"

#include "client/MainMenuOverlay.h"
#include "client/QuickMenuArt.h"
#include "client/SettingsMenuArt.h"
#include "stereo/QuickMenuMirrorMath.h"

#include <algorithm>
#include <cstdarg>
#include <cwchar>
#include <utility>

namespace bfvr
{
namespace
{
// PC presenter defaults for the native Deploy/Spawn plane, which also size
// and place the VR Settings panel.
constexpr float kNativeMenuWidthMeters = 1.6F;
constexpr float kNativeMenuDistanceMeters = 1.5F;
constexpr ULONGLONG kRadioSecondKeyDelayMs = 100;

enum TextureIndex : std::size_t
{
    kQuickTexture = 0,
    kUtilityTexture,
    kCommandTexture,
    kCursorTexture,
    kSettingsTexture,
    kVersionTexture,
    kBackToGameTexture,
};

std::wstring JoinPath(const std::wstring& directory, const wchar_t* child)
{
    std::wstring result = directory;
    if (!result.empty() && result.back() != L'\\' && result.back() != L'/')
    {
        result.push_back(L'\\');
    }
    result.append(child);
    return result;
}

void ForwardArtLog(void* context, const wchar_t* message)
{
    // context points at the overlay's LogCallback member.
    const auto* log = static_cast<const WinlatorXrMenuOverlay::LogCallback*>(context);
    if (log != nullptr && *log != nullptr && message != nullptr)
    {
        (*log)(message);
    }
}

UINT QuickMenuVirtualKey(stereo::QuickMenuSelection selection) noexcept
{
    using stereo::QuickMenuSelection;
    switch (selection)
    {
    case QuickMenuSelection::MainMenu: return VK_ESCAPE;
    case QuickMenuSelection::Deploy: return VK_RETURN;
    case QuickMenuSelection::Weapon1: return '1';
    case QuickMenuSelection::Weapon2: return '2';
    case QuickMenuSelection::Weapon3: return '3';
    case QuickMenuSelection::Weapon4: return '4';
    case QuickMenuSelection::Weapon5: return '5';
    case QuickMenuSelection::Weapon6: return '6';
    case QuickMenuSelection::CameraF9: return VK_F9;
    case QuickMenuSelection::CameraF10: return VK_F10;
    case QuickMenuSelection::CameraF11: return VK_F11;
    case QuickMenuSelection::CameraF12: return VK_F12;
    case QuickMenuSelection::SwapKit: return 'G';
    default: return 0;
    }
}

// Same scan-code down/up pair as the PC presenter, sent only while one of
// BF1942's own windows is in the foreground.
bool SendForegroundKeyPress(UINT virtualKey) noexcept
{
    const HWND foregroundWindow = GetForegroundWindow();
    DWORD foregroundProcessId = 0;
    if (virtualKey == 0 || foregroundWindow == nullptr ||
        GetWindowThreadProcessId(foregroundWindow, &foregroundProcessId) == 0 ||
        foregroundProcessId != GetCurrentProcessId())
    {
        return false;
    }
    const UINT scanCode = MapVirtualKeyW(virtualKey, MAPVK_VK_TO_VSC);
    std::array<INPUT, 2> inputs = {};
    for (INPUT& input : inputs)
    {
        input.type = INPUT_KEYBOARD;
        if (scanCode != 0)
        {
            input.ki.wScan = static_cast<WORD>(scanCode);
            input.ki.dwFlags = KEYEVENTF_SCANCODE;
        }
        else
        {
            input.ki.wVk = static_cast<WORD>(virtualKey);
        }
    }
    inputs[1].ki.dwFlags |= KEYEVENTF_KEYUP;
    return SendInput(
        static_cast<UINT>(inputs.size()),
        inputs.data(),
        sizeof(INPUT)) == inputs.size();
}

stereo::QuickMenuMirrorView ToMirrorView(const shared::SharedPresentationView& view) noexcept
{
    stereo::QuickMenuMirrorView result;
    result.pose.position = {view.pose.positionX, view.pose.positionY, view.pose.positionZ};
    result.pose.orientation = {
        view.pose.orientationX,
        view.pose.orientationY,
        view.pose.orientationZ,
        view.pose.orientationW};
    result.angleLeft = view.fov.angleLeft;
    result.angleRight = view.fov.angleRight;
    result.angleUp = view.fov.angleUp;
    result.angleDown = view.fov.angleDown;
    return result;
}
} // namespace

WinlatorXrMenuOverlay::WinlatorXrMenuOverlay() = default;

WinlatorXrMenuOverlay::~WinlatorXrMenuOverlay()
{
    Stop();
    stopLoading_ = true;
    if (loader_.joinable())
    {
        loader_.join();
    }
}

void WinlatorXrMenuOverlay::Start(const std::wstring& payloadDirectory, LogCallback log)
{
    Stop();
    log_ = log;
    const HMODULE translator = GetModuleHandleW(L"BFVRD3D8To9.dll");
    if (translator != nullptr)
    {
        createTexture_ = reinterpret_cast<BFVRD3D8To9CreateOverlayTextureFn>(
            GetProcAddress(translator, "BFVRD3D8To9CreateOverlayTexture"));
        updateTexture_ = reinterpret_cast<BFVRD3D8To9UpdateOverlayTextureFn>(
            GetProcAddress(translator, "BFVRD3D8To9UpdateOverlayTexture"));
        releaseTexture_ = reinterpret_cast<BFVRD3D8To9ReleaseOverlayTextureFn>(
            GetProcAddress(translator, "BFVRD3D8To9ReleaseOverlayTexture"));
    }
    if (createTexture_ == nullptr || updateTexture_ == nullptr || releaseTexture_ == nullptr)
    {
        createTexture_ = nullptr;
        updateTexture_ = nullptr;
        releaseTexture_ = nullptr;
        Log(L"WinlatorXR menus: the d3d8to9 overlay exports are unavailable; Quick Menu and VR Settings are disabled.");
        return;
    }

    auto& runtime = settings::ProcessUserSettingsRuntime();
    startupValues_ = settings::DecodeUserSettings(runtime.Current());
    pointerSmoothing_ = startupValues_.menuPointerSmoothingEnabled;
    settingsInteraction_.Configure(
        kNativeMenuWidthMeters * stereo::kSettingsMenuNativeWidthScale,
        std::max(kNativeMenuDistanceMeters - stereo::kSettingsMenuForwardOffsetMeters, 0.05F));
    payloadDirectory_ = payloadDirectory;
    loadAfterMs_ = 0;
}

void WinlatorXrMenuOverlay::StartLoaderWhenSettled()
{
    // The companion restarts on every device Reset; the art is loaded once.
    // Loading waits until BF1942 has finished starting: WIC/GDI work in
    // parallel with the game's own startup once hung the game under Wine.
    constexpr ULONGLONG kLoadDelayMs = 10000;
    if (loader_.joinable() || createTexture_ == nullptr)
    {
        return;
    }
    const ULONGLONG now = GetTickCount64();
    if (loadAfterMs_ == 0)
    {
        loadAfterMs_ = now + kLoadDelayMs;
        return;
    }
    if (now >= loadAfterMs_)
    {
        loader_ = std::thread(&WinlatorXrMenuOverlay::LoadArt, this, payloadDirectory_);
    }
}

void WinlatorXrMenuOverlay::LoadArt(std::wstring payloadDirectory)
{
    const ULONGLONG started = GetTickCount64();
    const std::wstring assets = JoinPath(payloadDirectory, L"assets");
    auto quick = std::make_unique<QuickMenuArt>();
    if (!quick->InitializeFromDirectory(assets, ForwardArtLog, static_cast<void*>(&log_)))
    {
        Log(L"WinlatorXR menus: Quick Menu art could not be loaded from %s; menus are disabled.", assets.c_str());
        return;
    }
    auto settingsArt = std::make_unique<SettingsMenuArt>();
    const bool settingsLoaded = !stopLoading_ &&
        settingsArt->InitializeFromDirectory(
            JoinPath(assets, L"SettingsMenu"),
            ForwardArtLog,
            static_cast<void*>(&log_));
    auto backToGame = std::make_unique<MainMenuOverlay>();
    if (stopLoading_)
    {
        return;
    }
    if (backToGame->InitializeFromDirectory(assets, log_))
    {
        backToGameArt_ = std::move(backToGame);
    }
    quickArt_ = std::move(quick);
    if (settingsLoaded)
    {
        settingsArt_ = std::move(settingsArt);
    }
    settingsAvailable_ = settingsLoaded;
    artReady_ = true;
    Log(L"WinlatorXR menus: art ready in %llu ms (VR Settings %s); hold right A for the Quick Menu.",
        static_cast<unsigned long long>(GetTickCount64() - started),
        settingsLoaded ? L"available" : L"unavailable");
}

void WinlatorXrMenuOverlay::Stop()
{
    ReleaseTextures();
    interaction_.Reset();
    settingsInteraction_.Reset();
    if (session_.IsActive())
    {
        session_.Cancel();
    }
    lastHoverTarget_ = 0;
    pendingRadioKey_ = 0;
    settingsPixelsValid_ = false;
    createTexture_ = nullptr;
    updateTexture_ = nullptr;
    releaseTexture_ = nullptr;
}

void WinlatorXrMenuOverlay::Update(
    const stereo::QuickMenuFrameInput& frame,
    bool mountedCameraDecoupled,
    WinlatorXrMenuActions& actions)
{
    actions = {};
    DispatchPendingRadioKey();
    if (!artReady_)
    {
        StartLoaderWhenSettled();
        return;
    }
    mountedCameraDecoupled_ = mountedCameraDecoupled;
    stereo::QuickMenuFrameInput input = frame;
    input.menuPointerSmoothingEnabled = pointerSmoothing_;
    if (settingsInteraction_.IsActive())
    {
        UpdateSettings(input, actions);
        // The persistent panel owns right A until Cancel closes it.
        interaction_.Reset();
    }
    else
    {
        interaction_.Update(input);
        const stereo::QuickMenuSelection selection = interaction_.TakeReleasedSelection();
        if (selection != stereo::QuickMenuSelection::None)
        {
            Dispatch(selection, actions);
        }
    }

    const std::uint64_t target = HoverTarget();
    if (target != 0 && target != lastHoverTarget_)
    {
        ++actions.soundHighlight;
        actions.hoverHaptic = HapticsEnabled();
    }
    lastHoverTarget_ = target;
}

void WinlatorXrMenuOverlay::UpdateSettings(
    const stereo::QuickMenuFrameInput& input,
    WinlatorXrMenuActions& actions)
{
    settingsInteraction_.Update(input);
    const stereo::SettingsMenuSelection sound = settingsInteraction_.TakeMenuSoundActivation();
    if (sound == stereo::SettingsMenuSelection::Cancel)
    {
        ++actions.soundCancel;
    }
    else if (sound != stereo::SettingsMenuSelection::None)
    {
        ++actions.soundOk;
    }
    if (settingsInteraction_.TakeValuesChanged())
    {
        settings::EncodeUserSettings(settingsInteraction_.Snapshot().values, session_.Working());
    }
    auto& runtime = settings::ProcessUserSettingsRuntime();
    switch (settingsInteraction_.TakeCommand())
    {
    case stereo::SettingsMenuCommand::ResetDefaults:
        session_.ResetToDefaults();
        settingsInteraction_.SetValues(settings::DecodeUserSettings(session_.Working()));
        settingsInteraction_.SetStatus(stereo::SettingsMenuStatus::DefaultsRestored);
        Log(L"VR Settings replaced only its working copy with the seeded defaults.");
        break;
    case stereo::SettingsMenuCommand::Save:
    {
        const settings::UserSettingsValues saved = settings::DecodeUserSettings(session_.Working());
        const bool committed = runtime.Commit(session_.Working());
        if (committed)
        {
            pointerSmoothing_ = saved.menuPointerSmoothingEnabled;
        }
        const bool restart = committed && settings::UserSettingsRequireRestart(startupValues_, saved);
        settingsInteraction_.SetStatus(
            !committed ? stereo::SettingsMenuStatus::SaveFailed
            : restart  ? stereo::SettingsMenuStatus::SettingsSavedRestartRequired
                       : stereo::SettingsMenuStatus::SettingsSaved);
        Log(committed
                ? L"VR Settings saved its working copy to UserConfig.txt; the menu remains open."
                : L"VR Settings could not save UserConfig.txt; the saved file is unchanged.");
        break;
    }
    case stereo::SettingsMenuCommand::Cancel:
        session_.Cancel();
        Log(L"VR Settings closed and discarded unsaved changes.");
        break;
    case stereo::SettingsMenuCommand::RecenterForward:
        actions.recenterForward = true;
        Log(L"VR Settings requested a forward recenter.");
        break;
    case stereo::SettingsMenuCommand::None:
    default:
        break;
    }
    RefreshSettingsPixels();
}

void WinlatorXrMenuOverlay::Dispatch(
    stereo::QuickMenuSelection selection,
    WinlatorXrMenuActions& actions)
{
    using stereo::QuickMenuSelection;
    ++actions.soundOk;
    switch (selection)
    {
    case QuickMenuSelection::MountedCameraDecouple:
        ++actions.mountedCameraToggles;
        Log(L"Quick Menu toggled the mounted-camera decouple.");
        return;
    case QuickMenuSelection::ToggleHud:
        ++actions.hudToggles;
        Log(L"Quick Menu toggled the native HUD.");
        return;
    case QuickMenuSelection::VrSettings:
        OpenSettingsMenu();
        return;
    default:
        break;
    }

    UINT firstKey = 0;
    UINT secondKey = 0;
    switch (selection)
    {
    case QuickMenuSelection::RadioRoger:
        firstKey = VK_F1;
        secondKey = VK_F1;
        break;
    case QuickMenuSelection::RadioNegative:
        firstKey = VK_F1;
        secondKey = VK_F2;
        break;
    case QuickMenuSelection::RadioGoGoGo:
        firstKey = VK_F7;
        secondKey = VK_F7;
        break;
    default:
        firstKey = QuickMenuVirtualKey(selection);
        break;
    }
    const bool sent = SendForegroundKeyPress(firstKey);
    if (sent && secondKey != 0)
    {
        pendingRadioKey_ = secondKey;
        pendingRadioDueAt_ = GetTickCount64() + kRadioSecondKeyDelayMs;
    }
    Log(sent
            ? L"Quick Menu released %s and sent its key."
            : L"Quick Menu released %s but suppressed its key (BF1942 not foreground or SendInput failed).",
        stereo::QuickMenuSelectionName(selection));
}

void WinlatorXrMenuOverlay::DispatchPendingRadioKey()
{
    if (pendingRadioKey_ == 0 || GetTickCount64() < pendingRadioDueAt_)
    {
        return;
    }
    const UINT key = pendingRadioKey_;
    pendingRadioKey_ = 0;
    (void)SendForegroundKeyPress(key);
}

void WinlatorXrMenuOverlay::OpenSettingsMenu()
{
    if (!settingsAvailable_)
    {
        Log(L"VR Settings open was ignored because its art is unavailable.");
        return;
    }
    interaction_.Reset();
    const settings::UserSettingsLoadStatus status =
        session_.Begin(settings::ProcessUserSettingsRuntime().Store());
    settingsInteraction_.Open();
    settingsInteraction_.SetValues(settings::DecodeUserSettings(session_.Working()));
    switch (status)
    {
    case settings::UserSettingsLoadStatus::Loaded:
        settingsInteraction_.SetStatus(stereo::SettingsMenuStatus::SettingsLoaded);
        break;
    case settings::UserSettingsLoadStatus::InvalidUsedDefaults:
        settingsInteraction_.SetStatus(stereo::SettingsMenuStatus::InvalidConfigDefaultsLoaded);
        break;
    case settings::UserSettingsLoadStatus::IoErrorUsedDefaults:
        settingsInteraction_.SetStatus(stereo::SettingsMenuStatus::ConfigReadFailed);
        break;
    default:
        settingsInteraction_.SetStatus(stereo::SettingsMenuStatus::DefaultsLoaded);
        break;
    }
    settingsPixelsValid_ = false;
    Log(L"VR Settings opened using %s; Cancel closes it.", settings::UserSettingsLoadStatusName(status));
}

void WinlatorXrMenuOverlay::SetForwardRecenterResult(bool succeeded) noexcept
{
    if (settingsInteraction_.IsActive())
    {
        settingsInteraction_.SetStatus(
            succeeded ? stereo::SettingsMenuStatus::ForwardRecentered
                      : stereo::SettingsMenuStatus::ForwardRecenterFailed);
    }
}

void WinlatorXrMenuOverlay::RefreshSettingsPixels()
{
    const stereo::SettingsMenuSnapshot state = settingsInteraction_.Snapshot();
    const bool unchanged = settingsPixelsValid_ &&
        renderedSettings_.controllerLayoutVisible == state.controllerLayoutVisible &&
        renderedSettings_.arrowLeftVisible == state.arrowLeftVisible &&
        renderedSettings_.arrowRightVisible == state.arrowRightVisible &&
        renderedSettings_.tab == state.tab &&
        renderedSettings_.hovered == state.hovered &&
        renderedSettings_.page == state.page &&
        renderedSettings_.values == state.values &&
        renderedSettings_.status == state.status;
    if (unchanged || settingsArt_ == nullptr)
    {
        return;
    }
    UINT width = 0;
    UINT height = 0;
    if (!settingsArt_->Compose(state, settingsPixels_, width, height) ||
        width != stereo::kSettingsMenuTextureSize ||
        height != stereo::kSettingsMenuTextureSize)
    {
        settingsPixels_.clear();
        return;
    }
    renderedSettings_ = state;
    settingsPixelsValid_ = true;
    ++settingsPixelsVersion_;
}

bool WinlatorXrMenuOverlay::IsVisible() const noexcept
{
    if (!artReady_)
    {
        return false;
    }
    return settingsInteraction_.IsActive()
        ? settingsInteraction_.Snapshot().visible
        : interaction_.Snapshot().visible;
}

std::uint64_t WinlatorXrMenuOverlay::HoverTarget() const noexcept
{
    const stereo::SettingsMenuSnapshot settings = settingsInteraction_.Snapshot();
    if (settings.active)
    {
        return settings.visible && settings.hovered != stereo::SettingsMenuSelection::None
            ? 0x1'0000'0000ULL + static_cast<std::uint64_t>(settings.hovered)
            : 0;
    }
    const stereo::QuickMenuInteractionSnapshot quick = interaction_.Snapshot();
    return quick.visible && quick.hovered != stereo::QuickMenuSelection::None
        ? 1ULL + static_cast<std::uint64_t>(quick.hovered)
        : 0;
}

bool WinlatorXrMenuOverlay::HapticsEnabled() const noexcept
{
    const stereo::SettingsMenuSnapshot settings = settingsInteraction_.Snapshot();
    if (settings.active)
    {
        return settings.values.controllerHapticsEnabled;
    }
    return settings::DecodeUserSettings(settings::ProcessUserSettingsRuntime().Current())
        .controllerHapticsEnabled;
}

bool WinlatorXrMenuOverlay::EnsureTexture(void* d3d8Device, std::size_t index, std::uint64_t key)
{
    Texture& slot = textures_[index];
    if (slot.texture != nullptr && slot.key == key)
    {
        return true;
    }
    std::vector<std::uint32_t> copied;
    const std::vector<std::uint32_t>* pixels = &copied;
    UINT width = 0;
    UINT height = 0;
    bool ok = false;
    const auto selection = static_cast<stereo::QuickMenuSelection>(key);
    switch (index)
    {
    case kQuickTexture:
        ok = quickArt_->CopyMenuPixels(selection, copied, width, height);
        break;
    case kUtilityTexture:
        ok = quickArt_->CopyUtilityStripPixels(
            static_cast<stereo::QuickMenuSelection>(key & 0xFFFF),
            (key >> 16) != 0,
            copied,
            width,
            height);
        break;
    case kCommandTexture:
        ok = quickArt_->CopyCommandColumnPixels(selection, copied, width, height);
        break;
    case kCursorTexture:
        ok = quickArt_->CopyCursorPixels(copied, width, height);
        break;
    case kSettingsTexture:
        pixels = &settingsPixels_;
        width = stereo::kSettingsMenuTextureSize;
        height = stereo::kSettingsMenuTextureSize;
        ok = settingsPixels_.size() == static_cast<std::size_t>(width) * height;
        break;
    case kBackToGameTexture:
        ok = backToGameArt_ != nullptr &&
            backToGameArt_->CopyButtonPixels(key != 0, copied, width, height);
        break;
    case kVersionTexture:
        ok = settingsArt_ != nullptr &&
            settingsArt_->ComposeVersionBanner(copied, width, height);
        break;
    default:
        break;
    }
    if (!ok || width == 0 || height == 0 ||
        pixels->size() != static_cast<std::size_t>(width) * height)
    {
        return false;
    }
    if (slot.texture != nullptr && (slot.width != width || slot.height != height))
    {
        releaseTexture_(slot.texture);
        slot = {};
    }
    if (slot.texture == nullptr)
    {
        const HRESULT created = createTexture_(d3d8Device, width, height, &slot.texture);
        if (FAILED(created) || slot.texture == nullptr)
        {
            slot = {};
            if (!textureFailureLogged_)
            {
                Log(L"WinlatorXR menus: overlay texture creation failed (HRESULT=0x%08lX).",
                    static_cast<unsigned long>(created));
                textureFailureLogged_ = true;
            }
            return false;
        }
        slot.width = width;
        slot.height = height;
    }
    const HRESULT updated = updateTexture_(
        slot.texture,
        reinterpret_cast<const DWORD*>(pixels->data()),
        width,
        height);
    if (FAILED(updated))
    {
        releaseTexture_(slot.texture);
        slot = {};
        return false;
    }
    slot.key = key;
    return true;
}

UINT WinlatorXrMenuOverlay::AppendQuads(
    void* d3d8Device,
    const shared::SharedPresentationView& eye,
    float regionLeft,
    float regionWidth,
    BFVRD3D8To9OverlayQuad* quads,
    UINT capacity)
{
    if (!IsVisible() || d3d8Device == nullptr || createTexture_ == nullptr ||
        quads == nullptr)
    {
        return 0;
    }
    if (!UseDevice(d3d8Device))
    {
        return 0;
    }

    std::array<Panel, 4> panels = {};
    std::size_t panelCount = 0;
    std::array<std::uint64_t, 6> keys = {};
    const stereo::SettingsMenuSnapshot settings = settingsInteraction_.Snapshot();
    if (settings.active)
    {
        keys[kSettingsTexture] = settingsPixelsVersion_;
        panels[panelCount++] = {kSettingsTexture, settings.panelPose, settings.widthMeters, settings.heightMeters};
        const float versionHeight = settings.widthMeters *
            static_cast<float>(SettingsMenuArt::kVersionBannerHeight) /
            static_cast<float>(SettingsMenuArt::kVersionBannerWidth);
        constexpr float kVersionGapScale = 0.012F;
        panels[panelCount++] = {
            kVersionTexture,
            stereo::MakeSettingsMenuCursorPose(
                settings.panelPose,
                settings.widthMeters,
                settings.heightMeters,
                0.5F,
                1.0F,
                0.0F,
                versionHeight + settings.widthMeters * kVersionGapScale * 2.0F),
            settings.widthMeters,
            versionHeight};
        const Texture& cursor = textures_[kCursorTexture];
        if (settings.pointerVisible && EnsureTexture(d3d8Device, kCursorTexture, 0))
        {
            const float cursorWidth = settings.widthMeters * static_cast<float>(cursor.width) /
                static_cast<float>(stereo::kSettingsMenuTextureSize) * stereo::kSettingsMenuCursorScale;
            const float cursorHeight = settings.heightMeters * static_cast<float>(cursor.height) /
                static_cast<float>(stereo::kSettingsMenuTextureSize) * stereo::kSettingsMenuCursorScale;
            panels[panelCount++] = {
                kCursorTexture,
                stereo::MakeSettingsMenuCursorPose(
                    settings.panelPose,
                    settings.widthMeters,
                    settings.heightMeters,
                    settings.pointerU,
                    settings.pointerV,
                    cursorWidth,
                    cursorHeight),
                cursorWidth,
                cursorHeight};
        }
    }
    else
    {
        const stereo::QuickMenuInteractionSnapshot quick = interaction_.Snapshot();
        keys[kQuickTexture] = static_cast<std::uint64_t>(quick.hovered);
        keys[kUtilityTexture] =
            (mountedCameraDecoupled_ ? 0x10000ULL : 0ULL) |
            (stereo::IsQuickMenuUtilitySelection(quick.hovered)
                 ? static_cast<std::uint64_t>(quick.hovered)
                 : 0ULL);
        keys[kCommandTexture] = stereo::IsQuickMenuCommandSelection(quick.hovered)
            ? static_cast<std::uint64_t>(quick.hovered)
            : 0ULL;
        panels[panelCount++] = {kQuickTexture, quick.panelPose, stereo::kQuickMenuWidthMeters, stereo::kQuickMenuHeightMeters};
        panels[panelCount++] = {
            kUtilityTexture,
            stereo::MakeQuickMenuUtilityPose(quick.panelPose),
            stereo::kQuickMenuWidthMeters,
            stereo::kQuickMenuUtilityHeightMeters};
        panels[panelCount++] = {
            kCommandTexture,
            stereo::MakeQuickMenuCommandPose(quick.panelPose),
            stereo::kQuickMenuCommandWidthMeters,
            stereo::kQuickMenuCommandHeightMeters};
        const Texture& cursor = textures_[kCursorTexture];
        if (quick.pointerVisible && EnsureTexture(d3d8Device, kCursorTexture, 0))
        {
            const float cursorWidth = stereo::kQuickMenuWidthMeters *
                static_cast<float>(cursor.width) / static_cast<float>(stereo::kQuickMenuTextureSize);
            const float cursorHeight = stereo::kQuickMenuHeightMeters *
                static_cast<float>(cursor.height) / static_cast<float>(stereo::kQuickMenuTextureSize);
            const stereo::Pose cursorPose = quick.pointerOnCommandColumn
                ? stereo::MakeQuickMenuCommandCursorPose(
                      quick.panelPose, quick.pointerU, quick.pointerV, cursorWidth, cursorHeight)
                : quick.pointerOnUtilityStrip
                ? stereo::MakeQuickMenuUtilityCursorPose(
                      quick.panelPose, quick.pointerU, quick.pointerV, cursorWidth, cursorHeight)
                : stereo::MakeQuickMenuCursorPose(
                      quick.panelPose, quick.pointerU, quick.pointerV, cursorWidth, cursorHeight);
            panels[panelCount++] = {kCursorTexture, cursorPose, cursorWidth, cursorHeight};
        }
    }

    const stereo::QuickMenuMirrorView view = ToMirrorView(eye);
    const stereo::QuickMenuMirrorCrop crop = {};
    constexpr std::array<std::array<float, 2>, 4> kUv = {{{0.0F, 1.0F}, {0.0F, 0.0F}, {1.0F, 1.0F}, {1.0F, 0.0F}}};
    UINT count = 0;
    for (std::size_t index = 0; index < panelCount && count < capacity; ++index)
    {
        const Panel& panel = panels[index];
        if (panel.texture != kCursorTexture &&
            !EnsureTexture(d3d8Device, panel.texture, keys[panel.texture]))
        {
            continue;
        }
        std::array<stereo::QuickMenuMirrorVertex, 4> clip = {};
        if (!stereo::ProjectQuickMenuQuadToMirror(
                panel.pose, panel.widthMeters, panel.heightMeters, view, crop, clip))
        {
            continue;
        }
        BFVRD3D8To9OverlayQuad& quad = quads[count++];
        quad.texture = textures_[panel.texture].texture;
        for (std::size_t corner = 0; corner < 4; ++corner)
        {
            const float w = clip[corner].clipW;
            BFVRD3D8To9OverlayVertex& vertex = quad.vertices[corner];
            vertex.x = regionLeft + (clip[corner].clipX / w * 0.5F + 0.5F) * regionWidth;
            vertex.y = 0.5F - clip[corner].clipY / w * 0.5F;
            vertex.z = 0.0F;
            vertex.rhw = 1.0F / w;
            vertex.u = kUv[corner][0];
            vertex.v = kUv[corner][1];
        }
    }
    return count;
}

bool WinlatorXrMenuOverlay::UseDevice(void* d3d8Device)
{
    if (d3d8Device == nullptr || createTexture_ == nullptr)
    {
        return false;
    }
    if (textureDevice_ != d3d8Device)
    {
        ReleaseTextures();
        textureDevice_ = d3d8Device;
    }
    return true;
}

bool WinlatorXrMenuOverlay::BuildBackToGameQuad(
    void* d3d8Device,
    bool hovered,
    float left,
    float top,
    float right,
    float bottom,
    BFVRD3D8To9OverlayQuad& quad)
{
    if (!artReady_ || backToGameArt_ == nullptr || !UseDevice(d3d8Device) ||
        !EnsureTexture(d3d8Device, kBackToGameTexture, hovered ? 1 : 0))
    {
        return false;
    }
    quad.texture = textures_[kBackToGameTexture].texture;
    const std::array<BFVRD3D8To9OverlayVertex, 4> vertices = {{
        {left, bottom, 0.0F, 1.0F, 0.0F, 1.0F},
        {left, top, 0.0F, 1.0F, 0.0F, 0.0F},
        {right, bottom, 0.0F, 1.0F, 1.0F, 1.0F},
        {right, top, 0.0F, 1.0F, 1.0F, 0.0F}}};
    std::copy(vertices.begin(), vertices.end(), quad.vertices);
    return true;
}

void WinlatorXrMenuOverlay::ReleaseTextures() noexcept
{
    for (Texture& slot : textures_)
    {
        if (slot.texture != nullptr && releaseTexture_ != nullptr)
        {
            releaseTexture_(slot.texture);
        }
        slot = {};
    }
    textureDevice_ = nullptr;
}

void WinlatorXrMenuOverlay::Log(const wchar_t* format, ...) const
{
    if (log_ == nullptr || format == nullptr)
    {
        return;
    }
    wchar_t message[512] = {};
    va_list arguments;
    va_start(arguments, format);
    _vsnwprintf_s(message, _TRUNCATE, format, arguments);
    va_end(arguments);
    log_(message);
}

} // namespace bfvr
