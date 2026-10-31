// Offline sanity test for the reverb engine: stability, RT60 and loudness per mode.
#include "../Source/ReverbEngine.h"
#include <cstdio>
#include <random>

using namespace qpasa;

static int failures = 0;
#define CHECK(c, ...) do { if (!(c)) { std::printf ("FAIL: " __VA_ARGS__); std::printf ("\n"); ++failures; } } while (0)

int main()
{
    const char* names[] = { "Room", "Hall", "Plate", "Ambient" };
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    for (int m = 0; m < kNumModes; ++m)
    for (float decay : { 0.3f, 2.5f, 20.0f })
    {
        ReverbEngine e;
        Params p; p.mix = 1.0f; p.decaySec = decay; p.mode = (Mode) m; p.mod = 1.0f; p.tone = 1.0f; p.preDelayMs = 0.0f;
        e.setParams (p);
        e.prepare (sr);

        const int block = 256;
        const int total = (int) (sr * std::min (40.0, decay * 2.0 + 1.0));
        std::vector<float> L (block), R (block);
        std::mt19937 rng (1); std::uniform_real_distribution<float> dist (-1.f, 1.f);
        const int burst = (int) (0.05 * sr);
        double maxAbs = 0, energyEarly = 0, rmsNoise = 0; int noiseCount = 0;
        std::vector<double> env;
        bool bad = false;
        const int win = (int) (0.01 * sr);
        double acc = 0; int accN = 0;
        for (int pos = 0; pos < total; pos += block)
        {
            for (int i = 0; i < block; ++i)
            {
                const bool on = pos + i < burst;
                L[i] = on ? dist (rng) * 0.5f : 0.f;
                R[i] = on ? dist (rng) * 0.5f : 0.f;
            }
            e.process (L.data(), R.data(), block);
            for (int i = 0; i < block; ++i)
            {
                if (! std::isfinite (L[i]) || ! std::isfinite (R[i])) bad = true;
                maxAbs = std::max (maxAbs, (double) std::max (std::abs (L[i]), std::abs (R[i])));
                acc += L[i] * L[i] + R[i] * R[i]; ++accN;
                if (accN == win) { env.push_back (acc / accN); acc = 0; accN = 0; }
            }
        }
        (void) energyEarly; (void) rmsNoise; (void) noiseCount;
        // estimate RT60 from envelope: time from peak to -30 dB, doubled
        size_t peakI = 0; for (size_t i = 0; i < env.size(); ++i) if (env[i] > env[peakI]) peakI = i;
        double peakDb = 10 * std::log10 (env[peakI] + 1e-30);
        double t30 = -1;
        for (size_t i = peakI; i < env.size(); ++i)
            if (10 * std::log10 (env[i] + 1e-30) < peakDb - 30) { t30 = (double) (i - peakI) * 0.01; break; }
        double rt = t30 > 0 ? 2 * t30 : -1;
        std::printf ("sr=%6.0f %-8s decay=%5.1f  peak=%.3f  peakRmsDb=%6.1f  RT60~%6.2f s%s\n",
                     sr, names[m], decay, maxAbs, peakDb, rt, bad ? "  NaN!" : "");
        CHECK (! bad, "non-finite output");
        CHECK (maxAbs < 4.0, "too loud");
        if (decay < 21 && t30 > 0) CHECK (rt > decay * 0.4 && rt < decay * 2.0 + 0.5, "RT60 off: %f vs %f", rt, decay);
    }

    // mode switching and parameter sweep must stay finite
    {
        ReverbEngine e; Params p; e.setParams (p); e.prepare (48000);
        std::vector<float> L (64), R (64);
        std::mt19937 rng (2); std::uniform_real_distribution<float> dist (-1.f, 1.f);
        for (int b = 0; b < 20000; ++b)
        {
            if (b % 700 == 0) { p.mode = (Mode) ((b / 700) % kNumModes); }
            p.preDelayMs = (float) (b % 250); p.tone = (b % 100) / 100.f; p.mod = ((b * 7) % 100) / 100.f;
            p.decaySec = 0.2f + (b % 300) / 15.f; p.mix = (b % 50) / 50.f;
            e.setParams (p);
            for (int i = 0; i < 64; ++i) { L[i] = dist (rng); R[i] = dist (rng); }
            e.process (L.data(), R.data(), 64);
            for (int i = 0; i < 64; ++i) if (! std::isfinite (L[i]) || std::abs (L[i]) > 20) { CHECK (false, "sweep unstable"); b = 1 << 30; break; }
        }
    }
    std::printf (failures ? "\n%d FAILURES\n" : "\nALL OK\n", failures);
    return failures ? 1 : 0;
}
