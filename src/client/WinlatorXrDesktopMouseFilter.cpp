#include "client/WinlatorXrDesktopMouseFilter.h"

#include "winlatorxr/WinlatorXrClient.h"

#include <windows.h>

#include <atomic>
#include <cstring>
#include <cwchar>

namespace bfvr
{
namespace
{
// IDirectInput8 vtable: IUnknown (0-2), CreateDevice (3).
constexpr std::size_t kDirectInput8CreateDeviceSlot = 3;
// IDirectInputDevice8 vtable: ... Unacquire (8), GetDeviceState (9),
// GetDeviceData (10).
constexpr std::size_t kDeviceGetDeviceStateSlot = 9;
constexpr std::size_t kDeviceGetDeviceDataSlot = 10;
constexpr std::size_t kMaximumMouseDevices = 4;

// GUID_SysMouse, GUID_SysMouseEm and GUID_SysMouseEm2 from dinput.h.
constexpr GUID kSysMouse =
    {0x6F1D2B60, 0xD5A0, 0x11CF, {0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}};
constexpr GUID kSysMouseEm =
    {0x6F1D2B80, 0xD5A0, 0x11CF, {0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}};
constexpr GUID kSysMouseEm2 =
    {0x6F1D2B81, 0xD5A0, 0x11CF, {0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}};

using DirectInput8CreateFn = HRESULT(WINAPI*)(
    HINSTANCE instance, DWORD version, REFIID iid, void** out, void* outer);
using CreateDeviceFn = HRESULT(STDMETHODCALLTYPE*)(
    void* self, REFGUID guid, void** device, void* outer);
using GetDeviceStateFn = HRESULT(STDMETHODCALLTYPE*)(
    void* self, DWORD size, void* data);
using GetDeviceDataFn = HRESULT(STDMETHODCALLTYPE*)(
    void* self, DWORD objectDataSize, void* data, DWORD* count, DWORD flags);

constexpr std::size_t kMaximumVtables = 8;

// Wine uses separate vtables for the ANSI and Unicode interface flavours, so
// every patched vtable keeps its own original entry.
struct PatchedVtable
{
    std::atomic<void**> vtable{nullptr};
    std::atomic<void*> original{nullptr};
};

WinlatorXrMouseFilterLogCallback g_log = nullptr;
DirectInput8CreateFn g_originalDirectInput8Create = nullptr;
PatchedVtable g_createDevice[kMaximumVtables];
PatchedVtable g_getDeviceState[kMaximumVtables];
PatchedVtable g_getDeviceData[kMaximumVtables];
std::atomic<bool> g_blocked{false};
std::atomic<long> g_directInputCreations{0};
std::atomic<void*> g_mouseDevices[kMaximumMouseDevices] = {};
std::atomic<bool> g_blockLogged{false};

void Log(const wchar_t* message)
{
    if (g_log != nullptr)
    {
        g_log(message);
    }
}

bool IsMouseDevice(void* device)
{
    for (const auto& mouse : g_mouseDevices)
    {
        if (device != nullptr && mouse.load() == device)
        {
            return true;
        }
    }
    return false;
}

bool ShouldDrop(void* device)
{
    if (!g_blocked.load() || !IsMouseDevice(device))
    {
        return false;
    }
    if (!g_blockLogged.exchange(true))
    {
        Log(L"WinlatorXR: dropping the emulated desktop mouse during VR gameplay (menus keep it).");
    }
    return true;
}

void* OriginalFor(const PatchedVtable table[], void* self)
{
    void** const vtable = self == nullptr ? nullptr : *static_cast<void***>(self);
    for (std::size_t index = 0; index < kMaximumVtables; ++index)
    {
        if (vtable != nullptr && table[index].vtable.load() == vtable)
        {
            return table[index].original.load();
        }
    }
    return nullptr;
}

// Patches vtable[slot] once and remembers that vtable's original entry.
bool PatchSlot(PatchedVtable table[], void** vtable, std::size_t slot, void* replacement)
{
    std::size_t freeIndex = kMaximumVtables;
    for (std::size_t index = 0; index < kMaximumVtables; ++index)
    {
        const void** const known = const_cast<const void**>(table[index].vtable.load());
        if (known == const_cast<const void**>(vtable))
        {
            return true;
        }
        if (known == nullptr && freeIndex == kMaximumVtables)
        {
            freeIndex = index;
        }
    }
    if (freeIndex == kMaximumVtables || vtable[slot] == replacement)
    {
        return false;
    }
    DWORD previousProtection = 0;
    if (!VirtualProtect(&vtable[slot], sizeof(void*), PAGE_READWRITE, &previousProtection))
    {
        return false;
    }
    table[freeIndex].original.store(vtable[slot]);
    table[freeIndex].vtable.store(vtable);
    vtable[slot] = replacement;
    DWORD ignored = 0;
    VirtualProtect(&vtable[slot], sizeof(void*), previousProtection, &ignored);
    return true;
}

HRESULT STDMETHODCALLTYPE HookGetDeviceState(void* self, DWORD size, void* data)
{
    const auto original = static_cast<GetDeviceStateFn>(OriginalFor(g_getDeviceState, self));
    if (original == nullptr)
    {
        return E_FAIL;
    }
    const HRESULT result = original(self, size, data);
    if (SUCCEEDED(result) && data != nullptr && ShouldDrop(self))
    {
        // DIMOUSESTATE / DIMOUSESTATE2: axes and buttons, all zero = idle.
        std::memset(data, 0, size);
    }
    return result;
}

HRESULT STDMETHODCALLTYPE HookGetDeviceData(
    void* self,
    DWORD objectDataSize,
    void* data,
    DWORD* count,
    DWORD flags)
{
    const auto original = static_cast<GetDeviceDataFn>(OriginalFor(g_getDeviceData, self));
    if (original == nullptr)
    {
        return E_FAIL;
    }
    const HRESULT result = original(self, objectDataSize, data, count, flags);
    if (SUCCEEDED(result) && count != nullptr && ShouldDrop(self))
    {
        // The buffered events were consumed by the call above; report none.
        *count = 0;
    }
    return result;
}

HRESULT STDMETHODCALLTYPE HookCreateDevice(void* self, REFGUID guid, void** device, void* outer)
{
    const auto original = static_cast<CreateDeviceFn>(OriginalFor(g_createDevice, self));
    if (original == nullptr)
    {
        return E_FAIL;
    }
    const HRESULT result = original(self, guid, device, outer);
    if (FAILED(result) || device == nullptr || *device == nullptr)
    {
        return result;
    }
    const bool mouse = IsEqualGUID(guid, kSysMouse) ||
        IsEqualGUID(guid, kSysMouseEm) ||
        IsEqualGUID(guid, kSysMouseEm2);
    if (!mouse)
    {
        return result;
    }
    for (auto& slot : g_mouseDevices)
    {
        void* expected = nullptr;
        if (slot.compare_exchange_strong(expected, *device))
        {
            break;
        }
    }
    // Wine gives all devices of one interface flavour the same vtable, so the
    // hooks are installed once and compare the device pointer.
    auto** const vtable = *static_cast<void***>(*device);
    const bool patched =
        PatchSlot(g_getDeviceState, vtable, kDeviceGetDeviceStateSlot, reinterpret_cast<void*>(&HookGetDeviceState)) &&
        PatchSlot(g_getDeviceData, vtable, kDeviceGetDeviceDataSlot, reinterpret_cast<void*>(&HookGetDeviceData));
    Log(patched
        ? L"WinlatorXR: DirectInput mouse device observed; its data is filtered during VR gameplay."
        : L"WinlatorXR: the DirectInput mouse device could not be hooked; the emulated mouse stays active.");
    return result;
}

HRESULT WINAPI HookDirectInput8Create(
    HINSTANCE instance,
    DWORD version,
    REFIID iid,
    void** out,
    void* outer)
{
    const HRESULT result = g_originalDirectInput8Create(instance, version, iid, out, outer);
    if (SUCCEEDED(result) && out != nullptr && *out != nullptr)
    {
        auto** const vtable = *static_cast<void***>(*out);
        const bool patched = PatchSlot(
            g_createDevice,
            vtable,
            kDirectInput8CreateDeviceSlot,
            reinterpret_cast<void*>(&HookCreateDevice));
        wchar_t message[160] = {};
        swprintf_s(
            message,
            L"WinlatorXR: DirectInput8Create call %ld version=0x%lx iid=%08lX vtable=%p observer=%d.",
            g_directInputCreations.fetch_add(1) + 1,
            static_cast<unsigned long>(version),
            static_cast<unsigned long>(iid.Data1),
            static_cast<void*>(vtable),
            patched ? 1 : 0);
        Log(message);
    }
    return result;
}

bool RouteExecutableImport(const char* moduleName, const char* functionName, void* replacement, void*& previous)
{
    auto* const imageBase = reinterpret_cast<BYTE*>(GetModuleHandleW(nullptr));
    const auto* const dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(imageBase);
    if (imageBase == nullptr || dos->e_magic != IMAGE_DOS_SIGNATURE)
    {
        return false;
    }
    const auto* const nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(imageBase + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE)
    {
        return false;
    }
    const IMAGE_DATA_DIRECTORY imports = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (imports.VirtualAddress == 0)
    {
        return false;
    }
    for (auto* descriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(imageBase + imports.VirtualAddress);
         descriptor->Name != 0;
         ++descriptor)
    {
        if (_stricmp(reinterpret_cast<const char*>(imageBase + descriptor->Name), moduleName) != 0 ||
            descriptor->OriginalFirstThunk == 0)
        {
            continue;
        }
        auto* names = reinterpret_cast<IMAGE_THUNK_DATA32*>(imageBase + descriptor->OriginalFirstThunk);
        auto* functions = reinterpret_cast<IMAGE_THUNK_DATA32*>(imageBase + descriptor->FirstThunk);
        for (; names->u1.AddressOfData != 0; ++names, ++functions)
        {
            if (IMAGE_SNAP_BY_ORDINAL32(names->u1.Ordinal))
            {
                continue;
            }
            const auto* const import =
                reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(imageBase + names->u1.AddressOfData);
            if (std::strcmp(reinterpret_cast<const char*>(import->Name), functionName) != 0)
            {
                continue;
            }
            auto** const slot = reinterpret_cast<void**>(&functions->u1.Function);
            DWORD previousProtection = 0;
            if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &previousProtection))
            {
                return false;
            }
            previous = *slot;
            *slot = replacement;
            DWORD ignored = 0;
            VirtualProtect(slot, sizeof(void*), previousProtection, &ignored);
            return true;
        }
    }
    return false;
}
} // namespace

void InstallWinlatorXrDesktopMouseFilter(WinlatorXrMouseFilterLogCallback log) noexcept
{
    if (g_originalDirectInput8Create != nullptr || !winlatorxr::DetectWinlatorXrEnvironment())
    {
        return;
    }
    wchar_t disabled[4] = {};
    if (GetEnvironmentVariableW(L"BFVR_WINLATORXR_MOUSE_FILTER", disabled, 4) == 1 &&
        disabled[0] == L'0')
    {
        if (log != nullptr)
        {
            log(L"WinlatorXR: desktop mouse filter disabled by BFVR_WINLATORXR_MOUSE_FILTER=0.");
        }
        return;
    }
    g_log = log;
    void* previous = nullptr;
    if (!RouteExecutableImport(
            "dinput8.dll",
            "DirectInput8Create",
            reinterpret_cast<void*>(&HookDirectInput8Create),
            previous) ||
        previous == nullptr)
    {
        Log(L"WinlatorXR: BF1942 does not import dinput8!DirectInput8Create; the emulated desktop mouse cannot be filtered.");
        return;
    }
    g_originalDirectInput8Create = static_cast<DirectInput8CreateFn>(previous);
    Log(L"WinlatorXR: routed dinput8!DirectInput8Create to filter the emulated desktop mouse during VR gameplay.");
}

void SetWinlatorXrDesktopMouseBlocked(bool blocked) noexcept
{
    g_blocked.store(blocked);
}

} // namespace bfvr
