#pragma once

#include "ReverbEngine.h"

namespace qpasa
{

struct Preset
{
    const char* name;
    Mode  mode;
    float mix;        // %
    float decay;      // s
    float preDelay;   // ms
    float tone;       // %
    float mod;        // %
};

inline constexpr Preset kPresets[] =
{
    { "Default Hall",     Mode::Hall,    30.0f,  2.5f,  20.0f, 60.0f, 30.0f },
    { "Small Room",       Mode::Room,    20.0f,  0.6f,   5.0f, 55.0f, 15.0f },
    { "Vocal Room",       Mode::Room,    25.0f,  1.2f,  25.0f, 65.0f, 25.0f },
    { "Drum Room",        Mode::Room,    18.0f,  0.9f,   0.0f, 70.0f, 10.0f },
    { "Concert Hall",     Mode::Hall,    30.0f,  3.2f,  30.0f, 55.0f, 30.0f },
    { "Big Dark Hall",    Mode::Hall,    35.0f,  5.0f,  40.0f, 25.0f, 40.0f },
    { "Bright Plate",     Mode::Plate,   25.0f,  1.8f,  10.0f, 85.0f, 35.0f },
    { "Vintage Plate",    Mode::Plate,   28.0f,  2.4f,  15.0f, 40.0f, 50.0f },
    { "Snare Plate",      Mode::Plate,   22.0f,  1.2f,   0.0f, 75.0f, 20.0f },
    { "Ambient Wash",     Mode::Ambient, 45.0f,  8.0f,  60.0f, 45.0f, 60.0f },
    { "Endless Space",    Mode::Ambient, 55.0f, 18.0f,  80.0f, 50.0f, 80.0f },
    { "Dreamy Pad",       Mode::Ambient, 50.0f,  6.0f, 120.0f, 35.0f, 90.0f },
};

inline constexpr int kNumPresets = (int) (sizeof (kPresets) / sizeof (kPresets[0]));

} // namespace qpasa
