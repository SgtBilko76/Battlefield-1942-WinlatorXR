#include "client/WinlatorXrWatchdog.h"

#include <windows.h>
#include <tlhelp32.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cwchar>
#include <string>
#include <vector>

namespace bfvr
{
namespace
{
constexpr ULONGLONG kStallMs = 15000;
constexpr ULONGLONG kPollMs = 2000;
constexpr ULONGLONG kModuleRefreshMs = 10000;
constexpr int kMaximumStallDumps = 3;
constexpr int kMaximumFaultDumps = 5;
constexpr size_t kStackBytes = 32 * 1024;
constexpr size_t kMaximumFrames = 48;

struct CodeRange
{
    DWORD begin = 0;
    DWORD end = 0;
    DWORD moduleBase = 0;
    wchar_t name[32] = {};
};

std::atomic<WinlatorXrWatchdogLogCallback> g_log = nullptr;
std::atomic<bool> g_started = false;
std::atomic<DWORD> g_frameThreadId = 0;
std::atomic<ULONGLONG> g_lastFrameMs = 0;
std::atomic<int> g_faultDumps = 0;
SRWLOCK g_rangesLock = SRWLOCK_INIT;
std::vector<CodeRange> g_ranges;

// Collects the executable sections of every loaded module. Never called
// while another thread is suspended, because it uses loader-backed APIs.
void RefreshCodeRanges()
{
    std::vector<CodeRange> ranges;
    const HANDLE snapshot = CreateToolhelp32Snapshot(
        TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32,
        GetCurrentProcessId());
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        return;
    }
    MODULEENTRY32W entry = {};
    entry.dwSize = sizeof(entry);
    for (BOOL more = Module32FirstW(snapshot, &entry); more; more = Module32NextW(snapshot, &entry))
    {
        const auto base = static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(entry.modBaseAddr));
        IMAGE_DOS_HEADER dos = {};
        IMAGE_NT_HEADERS32 nt = {};
        SIZE_T read = 0;
        if (!ReadProcessMemory(GetCurrentProcess(), entry.modBaseAddr, &dos, sizeof(dos), &read) ||
            dos.e_magic != IMAGE_DOS_SIGNATURE ||
            !ReadProcessMemory(
                GetCurrentProcess(),
                static_cast<const BYTE*>(entry.modBaseAddr) + dos.e_lfanew,
                &nt,
                sizeof(nt),
                &read) ||
            nt.Signature != IMAGE_NT_SIGNATURE)
        {
            continue;
        }
        const auto* const firstSection = reinterpret_cast<const BYTE*>(entry.modBaseAddr) +
            dos.e_lfanew + offsetof(IMAGE_NT_HEADERS32, OptionalHeader) +
            nt.FileHeader.SizeOfOptionalHeader;
        for (WORD index = 0; index < nt.FileHeader.NumberOfSections; ++index)
        {
            IMAGE_SECTION_HEADER section = {};
            if (!ReadProcessMemory(
                    GetCurrentProcess(),
                    firstSection + index * sizeof(IMAGE_SECTION_HEADER),
                    &section,
                    sizeof(section),
                    &read) ||
                (section.Characteristics & IMAGE_SCN_MEM_EXECUTE) == 0)
            {
                continue;
            }
            CodeRange range;
            range.moduleBase = base;
            range.begin = base + section.VirtualAddress;
            range.end = range.begin + std::max(section.Misc.VirtualSize, section.SizeOfRawData);
            wcsncpy_s(range.name, entry.szModule, _TRUNCATE);
            ranges.push_back(range);
        }
    }
    CloseHandle(snapshot);
    AcquireSRWLockExclusive(&g_rangesLock);
    g_ranges.swap(ranges);
    ReleaseSRWLockExclusive(&g_rangesLock);
}

// Caller holds g_rangesLock (shared).
const CodeRange* FindCodeRange(DWORD address)
{
    for (const CodeRange& range : g_ranges)
    {
        if (address >= range.begin && address < range.end)
        {
            return &range;
        }
    }
    return nullptr;
}

// Formats EIP and the plausible return addresses in a stack copy.
std::wstring DescribeStack(DWORD eip, DWORD esp, const std::vector<DWORD>& stack)
{
    std::wstring text;
    wchar_t item[96] = {};
    AcquireSRWLockShared(&g_rangesLock);
    const CodeRange* range = FindCodeRange(eip);
    swprintf_s(
        item,
        L"eip=%08lX (%s+0x%lX) esp=%08lX; stack code refs:",
        eip,
        range != nullptr ? range->name : L"?",
        range != nullptr ? eip - range->moduleBase : 0UL,
        esp);
    text += item;
    size_t frames = 0;
    for (size_t index = 0; index < stack.size() && frames < kMaximumFrames; ++index)
    {
        const CodeRange* hit = FindCodeRange(stack[index]);
        if (hit == nullptr)
        {
            continue;
        }
        swprintf_s(item, L" [+%zX]%s+0x%lX", index * 4, hit->name, stack[index] - hit->moduleBase);
        text += item;
        ++frames;
    }
    ReleaseSRWLockShared(&g_rangesLock);
    return text;
}

std::vector<DWORD> CopyStack(DWORD esp)
{
    std::vector<DWORD> stack(kStackBytes / sizeof(DWORD));
    SIZE_T read = 0;
    // Stop at the first unreadable page instead of faulting.
    size_t words = 0;
    for (; words < stack.size(); words += 256)
    {
        const size_t count = std::min<size_t>(256, stack.size() - words);
        if (!ReadProcessMemory(
                GetCurrentProcess(),
                reinterpret_cast<LPCVOID>(static_cast<ULONG_PTR>(esp) + words * sizeof(DWORD)),
                stack.data() + words,
                count * sizeof(DWORD),
                &read))
        {
            break;
        }
    }
    stack.resize(words);
    return stack;
}

// Reports also go to BFVR\logs\watchdog.log, which is written even when
// BFVR's diagnostics are off.
void AppendToWatchdogFile(const std::wstring& message)
{
    static wchar_t path[MAX_PATH] = {};
    if (path[0] == L'\0')
    {
        HMODULE module = nullptr;
        if (!GetModuleHandleExW(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(&AppendToWatchdogFile),
                &module) ||
            GetModuleFileNameW(module, path, MAX_PATH) == 0)
        {
            return;
        }
        wchar_t* const separator = wcsrchr(path, L'\\');
        if (separator == nullptr)
        {
            path[0] = L'\0';
            return;
        }
        *separator = L'\0';
        wcscat_s(path, L"\\logs");
        CreateDirectoryW(path, nullptr);
        wcscat_s(path, L"\\watchdog.log");
    }
    SYSTEMTIME now = {};
    GetLocalTime(&now);
    char prefix[40] = {};
    const int prefixLength = sprintf_s(
        prefix,
        "%02u:%02u:%02u.%03u ",
        now.wHour,
        now.wMinute,
        now.wSecond,
        now.wMilliseconds);
    std::string line(prefix, prefixLength > 0 ? static_cast<size_t>(prefixLength) : 0);
    for (const wchar_t character : message)
    {
        line.push_back(character < 0x80 ? static_cast<char>(character) : '?');
    }
    line += "\r\n";
    const HANDLE file = CreateFileW(
        path,
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return;
    }
    DWORD written = 0;
    WriteFile(file, line.data(), static_cast<DWORD>(line.size()), &written, nullptr);
    CloseHandle(file);
}

void Log(const std::wstring& message)
{
    AppendToWatchdogFile(message);
    const WinlatorXrWatchdogLogCallback log = g_log.load();
    if (log == nullptr)
    {
        return;
    }
    // AppendLog truncates long lines; split into chunks.
    constexpr size_t kChunk = 900;
    for (size_t offset = 0; offset < message.size(); offset += kChunk)
    {
        log(message.substr(offset, kChunk).c_str());
    }
}

void DumpStalledThread(DWORD threadId, ULONGLONG stalledMs)
{
    RefreshCodeRanges();
    const HANDLE thread = OpenThread(
        THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION,
        FALSE,
        threadId);
    if (thread == nullptr)
    {
        Log(L"WinlatorXR watchdog: could not open the stalled frame thread.");
        return;
    }
    // Nothing that may take a lock runs while the thread is suspended.
    CONTEXT context = {};
    context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
    std::vector<DWORD> stack;
    bool captured = false;
    if (SuspendThread(thread) != static_cast<DWORD>(-1))
    {
        captured = GetThreadContext(thread, &context) != FALSE;
        if (captured)
        {
            stack = CopyStack(context.Esp);
        }
        ResumeThread(thread);
    }
    CloseHandle(thread);
    if (!captured)
    {
        Log(L"WinlatorXR watchdog: the stalled frame thread's context is unavailable.");
        return;
    }
    wchar_t header[160] = {};
    swprintf_s(
        header,
        L"WinlatorXR watchdog: no frame composed for %llu ms; frame thread %lu eax=%08lX ecx=%08lX edx=%08lX ebp=%08lX ",
        static_cast<unsigned long long>(stalledMs),
        threadId,
        context.Eax,
        context.Ecx,
        context.Edx,
        context.Ebp);
    Log(header + DescribeStack(context.Eip, context.Esp, stack));
}

DWORD WINAPI WatchdogThread(void*)
{
    int dumps = 0;
    bool stallReported = false;
    ULONGLONG lastRefresh = 0;
    for (;;)
    {
        Sleep(static_cast<DWORD>(kPollMs));
        const ULONGLONG now = GetTickCount64();
        if (now - lastRefresh >= kModuleRefreshMs)
        {
            RefreshCodeRanges();
            lastRefresh = now;
        }
        const ULONGLONG last = g_lastFrameMs.load();
        // A frame composed after `now` was read is not a stall.
        if (last == 0 || last >= now)
        {
            continue;
        }
        if (now - last < kStallMs)
        {
            stallReported = false;
            continue;
        }
        if (!stallReported && dumps < kMaximumStallDumps)
        {
            stallReported = true;
            ++dumps;
            DumpStalledThread(g_frameThreadId.load(), now - last);
        }
    }
}

LONG WINAPI FaultHandler(EXCEPTION_POINTERS* exception)
{
    if (exception == nullptr || exception->ExceptionRecord == nullptr ||
        exception->ContextRecord == nullptr ||
        exception->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION ||
        g_faultDumps.fetch_add(1) >= kMaximumFaultDumps)
    {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    const EXCEPTION_RECORD& record = *exception->ExceptionRecord;
    const CONTEXT& context = *exception->ContextRecord;
    wchar_t header[200] = {};
    swprintf_s(
        header,
        L"WinlatorXR watchdog: access violation (%s %08lX) on thread %lu eax=%08lX ecx=%08lX edx=%08lX ebp=%08lX ",
        record.NumberParameters >= 2 && record.ExceptionInformation[0] == 1 ? L"write" : L"read",
        record.NumberParameters >= 2 ? static_cast<unsigned long>(record.ExceptionInformation[1]) : 0UL,
        GetCurrentThreadId(),
        context.Eax,
        context.Ecx,
        context.Edx,
        context.Ebp);
    Log(header + DescribeStack(context.Eip, context.Esp, CopyStack(context.Esp)));
    return EXCEPTION_CONTINUE_SEARCH;
}
} // namespace

void StartWinlatorXrWatchdog(WinlatorXrWatchdogLogCallback log) noexcept
{
    g_log = log;
    if (g_started.exchange(true))
    {
        return;
    }
    RefreshCodeRanges();
    AddVectoredExceptionHandler(0, &FaultHandler);
    const HANDLE thread = CreateThread(nullptr, 0, &WatchdogThread, nullptr, 0, nullptr);
    if (thread != nullptr)
    {
        CloseHandle(thread);
    }
    Log(L"WinlatorXR watchdog armed: frame stalls over 15 s and access violations are logged with code addresses.");
}

void NoteWinlatorXrFrameComposed() noexcept
{
    g_frameThreadId = GetCurrentThreadId();
    g_lastFrameMs = GetTickCount64();
}

} // namespace bfvr
