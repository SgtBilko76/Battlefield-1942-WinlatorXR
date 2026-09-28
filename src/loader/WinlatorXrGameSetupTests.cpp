#include "loader/WinlatorXrGameSetup.h"

#include <cstdio>
#include <string>

namespace
{
bool Expect(bool condition, const char* message)
{
    if (!condition)
    {
        std::fprintf(stderr, "WinlatorXR game setup test failed: %s\n", message);
    }
    return condition;
}
} // namespace

int main()
{
    using namespace bfvr;
    bool passed = true;

    passed &= Expect(
        SetGameDisplayModeLines(
            "rem x\r\ngame.setGameDisplayMode 640 480 32 60\r\ngame.setShadows 0\r\n",
            1792,
            1624,
            true) ==
            "rem x\r\ngame.setGameDisplayMode 1792 1624 32 60\r\ngame.setShadows 0\r\n",
        "the display mode line is replaced in place with CRLF kept");
    passed &= Expect(
        SetGameDisplayModeLines(
            "game.setGameDisplayMode 1280 960 32 60\n\ngame.setGameDisplayMode 640 480 32",
            1792,
            1624,
            false) ==
            "game.setGameDisplayMode 1792 1624 32 60\n\ngame.setGameDisplayMode 1792 1624 32 60",
        "every display mode line is replaced, including an unterminated last line");
    passed &= Expect(
        SetGameDisplayModeLines("game.setShadows 0\r\n", 1792, 1624, true) ==
            "game.setShadows 0\r\ngame.setGameDisplayMode 1792 1624 32 60\r\n",
        "a missing display mode is appended when requested");
    passed &= Expect(
        SetGameDisplayModeLines("game.setShadows 0\r\n", 1792, 1624, false) == "game.setShadows 0\r\n",
        "a missing display mode is left out otherwise");

    passed &= Expect(
        AppendSettingLine("game.setHardware 1\r\n", "game.setHardware 0") ==
            "game.setHardware 1\r\ngame.setHardware 0\r\n",
        "a setting is appended so it overrides the earlier value");
    passed &= Expect(
        AppendSettingLine("game.setHardware 1\r\ngame.setHardware 0\r\n\r\n", "game.setHardware 0") ==
            "game.setHardware 1\r\ngame.setHardware 0\r\n\r\n",
        "a setting that is already last is not appended again");
    passed &= Expect(
        AppendSettingLine("a 1", "b 2") == "a 1\r\nb 2\r\n" &&
            AppendSettingLine("a 1\n", "b 2") == "a 1\nb 2\n",
        "appending follows the file's line endings and ends an open last line");

    const std::string video = QuestProfileVideoCon(1792, 1624);
    passed &= Expect(
        video.find("game.setGameDisplayMode 1792 1624 32 60\r\n") != std::string::npos &&
            video.find("game.setDetailTexture 1\r\n") != std::string::npos &&
            video.find("game.setGraphicsQuality 3\r\n") != std::string::npos,
        "the Quest profile video settings carry the size and keep detail textures on");

    return passed ? 0 : 1;
}
