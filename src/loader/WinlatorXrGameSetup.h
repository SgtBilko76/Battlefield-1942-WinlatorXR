#pragma once

#include <string>
#include <string_view>

namespace bfvr
{

// Quest (WinlatorXR) game setup that BFVR.exe applies before a player launch,
// so a WinlatorXR Dawn package needs no batch files: the BFVR translator as
// the game's D3D8.dll, DXVK's d3d9.dll out of the game folder, the expansion
// intro clips off, software sound, Quest video settings, and the game's
// display mode matched to the WinlatorXR screen. Every changed file is backed
// up once as *.bfvr-backup. The one-time parts leave BFVR\quest-setup.done;
// the translator and the display mode are refreshed on every start.

using WinlatorXrSetupLog = void (*)(const wchar_t* message);

// Replaces every `game.setGameDisplayMode` line with the given size (32-bit,
// 60 Hz) and keeps the text's line endings. Appends the line when there is
// none and appendWhenMissing is set.
[[nodiscard]] std::string SetGameDisplayModeLines(
    std::string_view text,
    int width,
    int height,
    bool appendWhenMissing);

// Appends `line` on its own line unless the text already ends with it as its
// last setting (BF1942 runs .con files top to bottom, so the last value
// wins).
[[nodiscard]] std::string AppendSettingLine(std::string_view text, std::string_view line);

// Profile Video.con with the settings that run well on Quest 3. Detail
// textures stay on: turning them off crashes map loading there.
[[nodiscard]] std::string QuestProfileVideoCon(int width, int height);

// Applies the setup to the game folder. payloadDirectory holds
// BFVRD3D8To9.dll. Returns false only when the translator could not be
// installed as D3D8.dll; other steps log and continue.
bool ApplyWinlatorXrGameSetup(
    const std::wstring& gameRoot,
    const std::wstring& payloadDirectory,
    int screenWidth,
    int screenHeight,
    WinlatorXrSetupLog log);

} // namespace bfvr
