#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>

// Pure parts of the WinlatorXR keyboard filter. WinlatorXR types keyboard
// keys for controller buttons; when its mapping falls back to Enter, a
// button press opens BF1942's spawn screen during gameplay. The client drops
// Enter from the DirectInput keyboard while a head-tracked gameplay frame is
// shown, except briefly after BFVR itself sends Enter (Quick Menu Deploy).
namespace bfvr::winlatorxr
{

// DIK_RETURN and DIK_NUMPADENTER; DirectInput reports them as these offsets.
inline constexpr std::uint32_t kDikReturn = 0x1C;
inline constexpr std::uint32_t kDikNumpadEnter = 0x9C;
// How long an Enter that BFVR sent itself may pass the filter.
inline constexpr std::uint64_t kOwnEnterAllowanceMs = 750;

[[nodiscard]] inline bool IsEnterKeyOffset(std::uint32_t offset) noexcept
{
    return offset == kDikReturn || offset == kDikNumpadEnter;
}

[[nodiscard]] inline bool ShouldDropEnter(
    bool gameplayShown,
    std::uint64_t nowMs,
    std::uint64_t ownEnterAllowedUntilMs) noexcept
{
    return gameplayShown && nowMs >= ownEnterAllowedUntilMs;
}

// Releases both Enter keys in a DirectInput keyboard state (256 bytes, one
// per DIK code). Returns true when either was down.
inline bool ClearEnterKeys(std::uint8_t* state, std::size_t size) noexcept
{
    bool cleared = false;
    for (const std::uint32_t key : {kDikReturn, kDikNumpadEnter})
    {
        if (state != nullptr && key < size && state[key] != 0)
        {
            state[key] = 0;
            cleared = true;
        }
    }
    return cleared;
}

// Removes the Enter events from buffered DirectInput data
// (DIDEVICEOBJECTDATA records of `stride` bytes whose first DWORD is the key
// offset) and keeps the others in order. Returns the new record count.
inline std::uint32_t RemoveEnterEvents(
    std::uint8_t* records,
    std::uint32_t stride,
    std::uint32_t count) noexcept
{
    if (records == nullptr || stride < sizeof(std::uint32_t))
    {
        return count;
    }
    std::uint32_t kept = 0;
    for (std::uint32_t index = 0; index < count; ++index)
    {
        std::uint8_t* const record = records + static_cast<std::size_t>(index) * stride;
        std::uint32_t offset = 0;
        std::memcpy(&offset, record, sizeof(offset));
        if (IsEnterKeyOffset(offset))
        {
            continue;
        }
        if (kept != index)
        {
            std::memmove(records + static_cast<std::size_t>(kept) * stride, record, stride);
        }
        ++kept;
    }
    return kept;
}

} // namespace bfvr::winlatorxr
