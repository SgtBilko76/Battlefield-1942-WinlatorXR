#include "loader/WinlatorXrGameSetup.h"

#include <windows.h>

#include <cstdarg>
#include <cwchar>
#include <fstream>
#include <iterator>
#include <vector>

namespace bfvr
{
namespace
{
constexpr std::string_view kDisplayModeCommand = "game.setGameDisplayMode";

// BF1942's .con files use CRLF; keep LF only for a file that already does.
std::string_view LineEnding(std::string_view text)
{
    const bool lfOnly = text.find('\n') != std::string_view::npos &&
        text.find("\r\n") == std::string_view::npos;
    return lfOnly ? "\n" : "\r\n";
}

std::string_view Trim(std::string_view text)
{
    const std::size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos)
    {
        return {};
    }
    const std::size_t last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

std::string DisplayModeLine(int width, int height)
{
    return std::string(kDisplayModeCommand) + " " + std::to_string(width) + " " +
        std::to_string(height) + " 32 60";
}

bool Exists(const std::wstring& path)
{
    return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

std::wstring Combine(const std::wstring& directory, const std::wstring& child)
{
    if (directory.empty() || directory.back() == L'\\' || directory.back() == L'/')
    {
        return directory + child;
    }
    return directory + L"\\" + child;
}

bool ReadText(const std::wstring& path, std::string& text)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        return false;
    }
    text.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return true;
}

bool WriteText(const std::wstring& path, const std::string& text)
{
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(text.data(), static_cast<std::streamsize>(text.size()));
    return static_cast<bool>(file);
}

class SetupRun
{
public:
    explicit SetupRun(WinlatorXrSetupLog log) : log_(log) {}

    void Log(const wchar_t* format, ...) const
    {
        if (log_ == nullptr)
        {
            return;
        }
        wchar_t message[512] = {};
        va_list arguments;
        va_start(arguments, format);
        vswprintf_s(message, format, arguments);
        va_end(arguments);
        log_(message);
    }

    // Keeps the first original of every file BFVR changes.
    void Backup(const std::wstring& path) const
    {
        const std::wstring backup = path + L".bfvr-backup";
        if (Exists(path) && !Exists(backup) && !CopyFileW(path.c_str(), backup.c_str(), TRUE))
        {
            Log(L"WinlatorXR setup could not back up %ls (error=%lu).", path.c_str(), GetLastError());
        }
    }

    // Rewrites an existing text file through `edit`; writes only on change.
    template <typename Edit>
    void EditFile(const std::wstring& path, Edit edit) const
    {
        std::string text;
        if (!ReadText(path, text))
        {
            return;
        }
        const std::string edited = edit(std::string_view(text));
        if (edited == text)
        {
            return;
        }
        Backup(path);
        if (!WriteText(path, edited))
        {
            Log(L"WinlatorXR setup could not update %ls.", path.c_str());
        }
    }

    void DisableFile(const std::wstring& path) const
    {
        const std::wstring disabled = path + L".bfvr-disabled";
        if (Exists(path) && !Exists(disabled) &&
            !MoveFileExW(path.c_str(), disabled.c_str(), MOVEFILE_WRITE_THROUGH))
        {
            Log(L"WinlatorXR setup could not disable %ls (error=%lu).", path.c_str(), GetLastError());
        }
    }

private:
    WinlatorXrSetupLog log_ = nullptr;
};

std::vector<std::wstring> Matches(const std::wstring& directory, const wchar_t* pattern, bool directories)
{
    std::vector<std::wstring> result;
    WIN32_FIND_DATAW entry = {};
    const HANDLE find = FindFirstFileW(Combine(directory, pattern).c_str(), &entry);
    if (find == INVALID_HANDLE_VALUE)
    {
        return result;
    }
    do
    {
        const bool isDirectory = (entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        const std::wstring name = entry.cFileName;
        if (isDirectory == directories && name != L"." && name != L"..")
        {
            result.push_back(Combine(directory, name));
        }
    } while (FindNextFileW(find, &entry));
    FindClose(find);
    return result;
}
} // namespace

std::string SetGameDisplayModeLines(
    std::string_view text,
    int width,
    int height,
    bool appendWhenMissing)
{
    const std::string replacement = DisplayModeLine(width, height);
    std::string result;
    result.reserve(text.size() + replacement.size() + 2);
    bool found = false;
    std::size_t start = 0;
    while (start < text.size())
    {
        std::size_t end = text.find('\n', start);
        end = end == std::string_view::npos ? text.size() : end + 1;
        const std::string_view line = text.substr(start, end - start);
        const std::string_view content = Trim(line);
        if (content.substr(0, kDisplayModeCommand.size()) == kDisplayModeCommand)
        {
            found = true;
            const std::size_t contentEnd = line.find_last_not_of("\r\n") + 1;
            result += replacement;
            result += line.substr(contentEnd);
        }
        else
        {
            result += line;
        }
        start = end;
    }
    if (!found && appendWhenMissing)
    {
        return AppendSettingLine(result, replacement);
    }
    return result;
}

std::string AppendSettingLine(std::string_view text, std::string_view line)
{
    const std::string_view trimmed = Trim(text);
    const std::size_t lastBreak = trimmed.find_last_of('\n');
    const std::string_view lastLine =
        Trim(lastBreak == std::string_view::npos ? trimmed : trimmed.substr(lastBreak + 1));
    if (lastLine == line)
    {
        return std::string(text);
    }
    const std::string_view ending = LineEnding(text);
    std::string result(text);
    if (!result.empty() && result.back() != '\n')
    {
        result += ending;
    }
    result += line;
    result += ending;
    return result;
}

std::string QuestProfileVideoCon(int width, int height)
{
    std::string text = "rem *** Generated ***\r\n\r\n";
    for (const std::string& line : {
             DisplayModeLine(width, height),
             std::string("game.setDetailTexture 1"),
             std::string("game.setShadows 0"),
             std::string("game.setEnvironmentMapping 0"),
             std::string("game.setGraphicsQuality 3"),
             std::string("game.setLightmaps 1"),
             std::string("game.setRenderWhenSpawnMenu 0"),
             std::string("game.setMenuViewdistance 60"),
             std::string("game.setEffectsQuality 1"),
             std::string("game.setPerformance 0")})
    {
        text += line;
        text += "\r\n";
    }
    return text;
}

bool ApplyWinlatorXrGameSetup(
    const std::wstring& gameRoot,
    const std::wstring& payloadDirectory,
    int screenWidth,
    int screenHeight,
    WinlatorXrSetupLog log)
{
    const SetupRun run(log);

    // Every start: the game's D3D8.dll is BFVR's translator from this package
    // (dgVoodoo crashes under Wine, and DXVK's D3D8 cannot create its device).
    const std::wstring d3d8 = Combine(gameRoot, L"D3D8.dll");
    run.Backup(d3d8);
    if (!CopyFileW(Combine(payloadDirectory, L"BFVRD3D8To9.dll").c_str(), d3d8.c_str(), FALSE))
    {
        run.Log(L"[FAIL] WinlatorXR setup could not install BFVR's translator as %ls (error=%lu).", d3d8.c_str(), GetLastError());
        return false;
    }
    // A DXVK d3d9.dll in the game folder would replace WinlatorXR's own.
    run.DisableFile(Combine(gameRoot, L"d3d9.dll"));

    const std::wstring settings = Combine(gameRoot, L"Mods\\bf1942\\Settings");
    const std::wstring marker = Combine(payloadDirectory, L"quest-setup.done");
    if (!Exists(marker))
    {
        // The expansions' intro and legal clips only show black; the base
        // game's startup movies are suspended by every player launch.
        for (const wchar_t* clip : {
                 L"Mods\\XPack1\\Movies\\legal.bik",
                 L"Mods\\XPack2\\Movies\\Intro.bik",
                 L"Mods\\XPack2\\Movies\\Legal.bik",
                 L"Mods\\XPack2\\Movies\\LegalF.bik"})
        {
            run.DisableFile(Combine(gameRoot, clip));
        }
        // Normal object detail distance (the MoonGamers package uses 5x).
        run.EditFile(Combine(settings, L"VideoDefault.con"), [](std::string_view text) {
            return AppendSettingLine(text, "renderer.globalLodRadiusScaleFactor 1");
        });
        for (const std::wstring& profile : Matches(Combine(settings, L"Profiles"), L"*", true))
        {
            // Hardware 3D sound crashes BF1942 with WinlatorXR's DirectSound.
            run.EditFile(Combine(profile, L"Sound.con"), [](std::string_view text) {
                return AppendSettingLine(text, "game.setHardware 0");
            });
            const std::wstring video = Combine(profile, L"Video.con");
            if (Exists(video))
            {
                run.Backup(video);
                if (!WriteText(video, QuestProfileVideoCon(screenWidth, screenHeight)))
                {
                    run.Log(L"WinlatorXR setup could not write %ls.", video.c_str());
                }
            }
        }
        if (!WriteText(marker, "BFVR WinlatorXR game setup applied.\r\n"))
        {
            run.Log(L"WinlatorXR setup could not write %ls.", marker.c_str());
        }
        run.Log(L"WinlatorXR setup applied the one-time Quest game settings.");
    }

    // Every start: the game renders at the WinlatorXR screen size. A size
    // that does not fit makes BF1942 fall back to 640x480 and save that.
    const auto displayMode = [&](bool append) {
        return [=](std::string_view text) {
            return SetGameDisplayModeLines(text, screenWidth, screenHeight, append);
        };
    };
    for (const std::wstring& profile : Matches(Combine(settings, L"Profiles"), L"*", true))
    {
        run.EditFile(Combine(profile, L"Video.con"), displayMode(true));
        run.EditFile(Combine(profile, L"VideoCustom.con"), displayMode(true));
    }
    for (const std::wstring& preset : Matches(Combine(settings, L"Default"), L"Video*.con", false))
    {
        run.EditFile(preset, displayMode(true));
    }
    run.Log(L"WinlatorXR setup: BFVR translator installed; game display mode %dx%d.", screenWidth, screenHeight);
    return true;
}

} // namespace bfvr
