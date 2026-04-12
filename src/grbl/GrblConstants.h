#pragma once

#include <cstdint>

/// GRBL realtime command bytes (single-character, no newline)
namespace GrblRealtime
{
    constexpr char StatusReport   = '?';   ///< Request status report
    constexpr char CycleStart     = '~';   ///< Resume/continue motion
    constexpr char FeedHold       = '!';   ///< Pause motion
    constexpr char Reset          = 0x18;  ///< Soft reset
    constexpr char SafetyDoor     = 0x84;  ///< Safety door ajar
    constexpr char JogCancel      = 0x85;  ///< Cancel jog motion
    constexpr char DebugReport    = 0x86;  ///< Debug report (requires $D)
} // namespace GrblRealtime

/// GRBL parser state strings
namespace GrblState
{
    constexpr const char *Idle     = "Idle";
    constexpr const char *Run      = "Run";
    constexpr const char *Hold     = "Hold";
    constexpr const char *Jog      = "Jog";
    constexpr const char *Alarm    = "Alarm";
    constexpr const char *Door     = "Door";
    constexpr const char *Check    = "Check";
    constexpr const char *Home     = "Home";
    constexpr const char *Sleep    = "Sleep";
} // namespace GrblState

/// GRBL response prefixes
namespace GrblResponse
{
    constexpr const char *OkPrefix     = "ok";
    constexpr const char *ErrorPrefix  = "error:";
    constexpr const char *StatusStart  = "<";
    constexpr const char *StatusEnd    = ">";
    constexpr const char *WelcomeStart = "Grbl";
} // namespace GrblResponse
