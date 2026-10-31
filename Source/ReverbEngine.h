// Qpasa Reverb - reverb engine (plain C++, no JUCE dependency).
//
// Algorithms used (all from public literature):
//  - Room / Hall / Ambient: 8-line feedback delay network (Jot-style FDN)
//    with a Hadamard feedback matrix, input allpass diffusion,
//    per-line damping and slowly modulated delay lines.
//  - Plate: the figure-of-eight tank from Jon Dattorro,
//    "Effect Design Part 1: Reverberator and Other Filters" (JAES, 1997).

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace qpasa
{

constexpr float kPi    = 3.14159265358979323846f;
constexpr float kTwoPi = 2.0f * kPi;

enum class Mode : int { Room = 0, Hall = 1, Plate = 2, Ambient = 3 };
constexpr int kNumModes = 4;

//==============================================================================
// Circular delay buffer. tap(n) returns the sample written n pushes ago (n >= 1).
class DelayBuffer
{
public:
    void prepare (int maxDelaySamples)
    {
        int size = 1;
        while (size < maxDelaySamples + 8)
            size <<= 1;
        buffer.assign ((size_t) size, 0.0f);
        mask = size - 1;
        writePos = 0;
    }

    void clear() { std::fill (buffer.begin(), buffer.end(), 0.0f); }

    void push (float x)
    {
        buffer[(size_t) writePos] = x;
        writePos = (writePos + 1) & mask;
    }

    float tap (int n) const { return buffer[(size_t) ((writePos - n) & mask)]; }

    // Cubic (Hermite) interpolated read, d >= 2.
    float tapFrac (float d) const
    {
        const int i = (int) d;
        const float f = d - (float) i;
        const float xm1 = tap (i - 1), x0 = tap (i), x1 = tap (i + 1), x2 = tap (i + 2);
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }

private:
    std::vector<float> buffer;
    int mask = 0;
    int writePos = 0;
};

//==============================================================================
// Schroeder allpass with optional modulated length.
class Allpass
{
public:
    void prepare (int maxLen) { line.prepare (maxLen + 64); }
    void clear() { line.clear(); }

    float process (float x, float delay, float g)
    {
        const float d = line.tapFrac (delay);
        const float v = x - g * d;
        line.push (v);
        return d + g * v;
    }

    float processInt (float x, int delay, float g)
    {
        const float d = line.tap (delay);
        const float v = x - g * d;
        line.push (v);
        return d + g * v;
    }

    float tap (int n) const { return line.tap (n); }

private:
    DelayBuffer line;
};

//==============================================================================
struct OnePoleLP
{
    float a = 0.0f, z = 0.0f;
    void setCoef (float coef) { a = coef; }
    float process (float x) { z = x + a * (z - x); return z; }
    void clear() { z = 0.0f; }
};

struct OnePoleHP
{
    float a = 0.0f, z = 0.0f;
    void setCutoff (float hz, float sr) { a = std::exp (-kTwoPi * hz / sr); }
    float process (float x) { z = x + a * (z - x); return x - z; }
    void clear() { z = 0.0f; }
};

struct Lfo
{
    float phase = 0.0f, inc = 0.0f;
    void set (float hz, float sr) { inc = hz / sr; }
    float next()
    {
        phase += inc;
        if (phase >= 1.0f) phase -= 1.0f;
        // cheap smooth sine-like shape (parabolic)
        const float p = phase * 2.0f - 1.0f;           // -1..1
        return 4.0f * p * (1.0f - std::abs (p)) ;       // approx sin, range -1..1
    }
};

//==============================================================================
struct Params
{
    float mix       = 0.3f;  // 0..1
    float decaySec  = 2.5f;  // RT60 seconds
    float preDelayMs = 20.0f;
    float tone      = 0.6f;  // 0 = dark, 1 = bright
    float mod       = 0.3f;  // 0..1
    Mode  mode      = Mode::Hall;
};

//==============================================================================
// 8-line FDN used for Room, Hall and Ambient.
class FdnReverb
{
public:
    static constexpr int N = 8;

    void prepare (float sampleRate)
    {
        sr = sampleRate;
        const float maxScale = 2.0f;
        for (int i = 0; i < N; ++i)
            lines[(size_t) i].prepare ((int) std::ceil (baseMs[(size_t) i] * 0.001f * sr * maxScale) + 64 + 128);

        for (int i = 0; i < 4; ++i)
        {
            diffL[(size_t) i].prepare ((int) (diffMs[(size_t) i] * 0.001f * sr * 2.0f) + 8);
            diffR[(size_t) i].prepare ((int) (diffMs[(size_t) i] * 0.001f * sr * 2.0f * 1.1f) + 8);
        }
        clear();
    }

    void clear()
    {
        for (auto& l : lines) l.clear();
        for (auto& d : diffL) d.clear();
        for (auto& d : diffR) d.clear();
        for (auto& f : damp)  f.clear();
    }

    void configure (Mode m)
    {
        float scale = 1.0f, diffG = 0.7f, modDepthMs = 0.3f, rateMul = 1.0f, diffScale = 1.0f;
        switch (m)
        {
            case Mode::Room:    scale = 0.42f; diffG = 0.62f; modDepthMs = 0.12f; rateMul = 1.3f; diffScale = 0.6f; outGain = 0.60f; break;
            case Mode::Hall:    scale = 1.25f; diffG = 0.70f; modDepthMs = 0.30f; rateMul = 1.0f; diffScale = 1.0f; outGain = 1.00f; break;
            case Mode::Ambient: scale = 1.95f; diffG = 0.76f; modDepthMs = 0.70f; rateMul = 0.6f; diffScale = 1.8f; outGain = 1.25f; break;
            case Mode::Plate:   break; // not used
        }

        diffusion = diffG;
        maxModSamples = modDepthMs * 0.001f * sr;

        for (int i = 0; i < N; ++i)
        {
            lengths[(size_t) i] = baseMs[(size_t) i] * 0.001f * sr * scale;
            lfo[(size_t) i].set ((0.11f + 0.093f * (float) i) * rateMul, sr);
            lfo[(size_t) i].phase = (float) i / (float) N;
        }

        for (int i = 0; i < 4; ++i)
        {
            diffLenL[(size_t) i] = std::max (2, (int) (diffMs[(size_t) i] * 0.001f * sr * diffScale));
            diffLenR[(size_t) i] = std::max (2, (int) (diffMs[(size_t) i] * 0.001f * sr * diffScale * 1.09f));
        }
    }

    void update (float decaySec, float dampCoef, float modAmount)
    {
        for (int i = 0; i < N; ++i)
            gains[(size_t) i] = std::pow (10.0f, -3.0f * lengths[(size_t) i] / (decaySec * sr));
        for (auto& f : damp) f.setCoef (dampCoef);
        modDepth = maxModSamples * modAmount;
    }

    void process (float inL, float inR, float& outL, float& outR)
    {
        // input diffusion
        for (int i = 0; i < 4; ++i)
        {
            const float g = (i < 2 ? diffusion : diffusion * 0.85f);
            inL = diffL[(size_t) i].processInt (inL, diffLenL[(size_t) i], g);
            inR = diffR[(size_t) i].processInt (inR, diffLenR[(size_t) i], g);
        }

        std::array<float, N> y {};
        for (int i = 0; i < N; ++i)
        {
            const float d = lengths[(size_t) i] + modDepth * lfo[(size_t) i].next();
            y[(size_t) i] = damp[(size_t) i].process (lines[(size_t) i].tapFrac (std::max (4.0f, d))) * gains[(size_t) i];
        }

        // Fast Walsh-Hadamard transform, normalised (orthogonal => lossless)
        std::array<float, N> h = y;
        for (int len = 1; len < N; len <<= 1)
            for (int i = 0; i < N; i += len << 1)
                for (int j = i; j < i + len; ++j)
                {
                    const float a = h[(size_t) j], b = h[(size_t) (j + len)];
                    h[(size_t) j] = a + b;
                    h[(size_t) (j + len)] = a - b;
                }
        const float norm = 0.35355339f; // 1/sqrt(8)

        for (int i = 0; i < N; ++i)
        {
            const float in = (i & 1) ? inR : inL;
            const float sign = ((i >> 1) & 1) ? -1.0f : 1.0f;
            lines[(size_t) i].push (h[(size_t) i] * norm + sign * in * 0.5f);
        }

        // two orthogonal output combinations for a wide, decorrelated stereo image
        outL = (y[0] + y[1] - y[2] - y[3] + y[4] + y[5] - y[6] - y[7]) * outGain * 0.5f;
        outR = (y[0] - y[1] + y[2] - y[3] + y[4] - y[5] + y[6] - y[7]) * outGain * 0.5f;
    }

private:
    float sr = 44100.0f;
    static constexpr std::array<float, N> baseMs { 31.3f, 37.9f, 43.1f, 49.7f, 56.3f, 61.9f, 71.3f, 79.1f };
    static constexpr std::array<float, 4> diffMs { 4.77f, 3.59f, 12.73f, 9.31f };

    std::array<DelayBuffer, N> lines;
    std::array<OnePoleLP, N> damp;
    std::array<Lfo, N> lfo;
    std::array<float, N> lengths {};
    std::array<float, N> gains {};
    std::array<Allpass, 4> diffL, diffR;
    std::array<int, 4> diffLenL {}, diffLenR {};
    float diffusion = 0.7f, maxModSamples = 0.0f, modDepth = 0.0f, outGain = 0.7f;
};

//==============================================================================
// Dattorro plate.
class PlateReverb
{
public:
    void prepare (float sampleRate)
    {
        sr = sampleRate;
        k = sr / 29761.0f;
        for (int i = 0; i < 4; ++i) inAp[(size_t) i].prepare (sc (inApLen[(size_t) i]));
        modApL.prepare (sc (672) + sc (32));
        modApR.prepare (sc (908) + sc (32));
        apL2.prepare (sc (1800));
        apR2.prepare (sc (2656));
        dL1.prepare (sc (4453)); dL2.prepare (sc (3720));
        dR1.prepare (sc (4217)); dR2.prepare (sc (3163));
        lfoL.set (0.5f, sr); lfoR.set (0.37f, sr); lfoR.phase = 0.25f;
        clear();
    }

    void clear()
    {
        for (auto& a : inAp) a.clear();
        modApL.clear(); modApR.clear(); apL2.clear(); apR2.clear();
        dL1.clear(); dL2.clear(); dR1.clear(); dR2.clear();
        bw.clear(); dampL.clear(); dampR.clear();
        fbL = fbR = 0.0f;
    }

    void update (float decaySec, float dampCoef, float modAmount)
    {
        // one full trip around the tank passes 4 decay gains in ~0.725 s (at 29761 Hz)
        const float loopSec = 21589.0f / 29761.0f;
        decay = std::min (0.985f, std::pow (10.0f, -3.0f * loopSec / (4.0f * decaySec)));
        decayDiff2 = std::clamp (decay + 0.15f, 0.25f, 0.5f);
        dampL.setCoef (dampCoef);
        dampR.setCoef (dampCoef);
        bw.setCoef (0.0005f);
        modDepth = 16.0f * k * modAmount * 1.5f;
    }

    void process (float inL, float inR, float& outL, float& outR)
    {
        float x = bw.process (0.5f * (inL + inR));
        x = inAp[0].processInt (x, sc (142), 0.75f);
        x = inAp[1].processInt (x, sc (107), 0.75f);
        x = inAp[2].processInt (x, sc (379), 0.625f);
        x = inAp[3].processInt (x, sc (277), 0.625f);

        // left half of the tank
        float l = x + fbR;
        l = modApL.process (l, (float) sc (672) + modDepth * lfoL.next() + 4.0f, -0.7f);
        const float l1 = dL1.tap (sc (4453));
        dL1.push (l);
        float lt = dampL.process (l1) * decay;
        lt = apL2.processInt (lt, sc (1800), decayDiff2);
        const float l2 = dL2.tap (sc (3720));
        dL2.push (lt);

        // right half of the tank
        float r = x + fbL;
        r = modApR.process (r, (float) sc (908) + modDepth * lfoR.next() + 4.0f, -0.7f);
        const float r1 = dR1.tap (sc (4217));
        dR1.push (r);
        float rt = dampR.process (r1) * decay;
        rt = apR2.processInt (rt, sc (2656), decayDiff2);
        const float r2 = dR2.tap (sc (3163));
        dR2.push (rt);

        fbR = r2 * decay; // right feeds left
        fbL = l2 * decay; // left feeds right

        float yl = dR1.tap (sc (266)) + dR1.tap (sc (2974)) - apR2.tap (sc (1913))
                 + dR2.tap (sc (1996)) - dL1.tap (sc (1990)) - apL2.tap (sc (187)) - dL2.tap (sc (1066));
        float yr = dL1.tap (sc (353)) + dL1.tap (sc (3627)) - apL2.tap (sc (1228))
                 + dL2.tap (sc (2673)) - dR1.tap (sc (2111)) - apR2.tap (sc (335)) - dR2.tap (sc (121));

        outL = yl * 0.6f;
        outR = yr * 0.6f;
    }

private:
    int sc (int n) const { return std::max (1, (int) ((float) n * k)); }

    float sr = 44100.0f, k = 1.0f;
    static constexpr std::array<int, 4> inApLen { 142, 107, 379, 277 };
    std::array<Allpass, 4> inAp;
    Allpass modApL, modApR, apL2, apR2;
    DelayBuffer dL1, dL2, dR1, dR2;
    OnePoleLP bw, dampL, dampR;
    Lfo lfoL, lfoR;
    float fbL = 0.0f, fbR = 0.0f;
    float decay = 0.5f, decayDiff2 = 0.5f, modDepth = 0.0f;
};

//==============================================================================
// Complete stereo reverb: pre-delay, algorithm, low-cut, dry/wet mix.
class ReverbEngine
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        preL.prepare ((int) (0.26f * sr) + 8);
        preR.prepare ((int) (0.26f * sr) + 8);
        fdn.prepare (sr);
        plate.prepare (sr);
        hpL.setCutoff (70.0f, sr);
        hpR.setCutoff (70.0f, sr);
        activeMode = params.mode;
        fdn.configure (activeMode == Mode::Plate ? Mode::Hall : activeMode);
        reset();
        smoothedMix = params.mix;
        smoothedPre = params.preDelayMs * 0.001f * sr;
        fade = 1.0f;
    }

    void reset()
    {
        preL.clear(); preR.clear();
        fdn.clear(); plate.clear();
        hpL.clear(); hpR.clear();
    }

    void setParams (const Params& p) { params = p; }

    // Processes in place. right may equal left for mono.
    void process (float* left, float* right, int numSamples)
    {
        const float dampHz   = 1200.0f * std::pow (15.0f, std::clamp (params.tone, 0.0f, 1.0f));
        const float dampCoef = std::exp (-kTwoPi * std::min (dampHz, 0.45f * sr) / sr);
        const float decay    = std::clamp (params.decaySec, 0.1f, 30.0f);

        fdn.update (decay, dampCoef, params.mod);
        plate.update (decay, dampCoef, params.mod);

        const float mixTarget = std::clamp (params.mix, 0.0f, 1.0f);
        const float preTarget = std::clamp (params.preDelayMs, 0.0f, 250.0f) * 0.001f * sr;
        const float smooth = 1.0f - std::exp (-1.0f / (0.03f * sr)); // ~30 ms
        const float fadeStep = 1.0f / (0.015f * sr);                 // 15 ms fades on mode change

        for (int n = 0; n < numSamples; ++n)
        {
            const float dryL = left[n];
            const float dryR = right[n];

            // mode change: fade wet out, switch, fade back in
            if (params.mode != activeMode)
            {
                fade -= fadeStep;
                if (fade <= 0.0f)
                {
                    fade = 0.0f;
                    activeMode = params.mode;
                    if (activeMode != Mode::Plate)
                        fdn.configure (activeMode);
                    fdn.update (decay, dampCoef, params.mod);
                    fdn.clear(); plate.clear();
                }
            }
            else if (fade < 1.0f)
            {
                fade = std::min (1.0f, fade + fadeStep);
            }

            smoothedMix += (mixTarget - smoothedMix) * smooth;
            smoothedPre += (preTarget - smoothedPre) * smooth;

            preL.push (dryL);
            preR.push (dryR);
            const float pd = std::max (2.0f, smoothedPre);
            const float inL = preL.tapFrac (pd);
            const float inR = preR.tapFrac (pd);

            float wl = 0.0f, wr = 0.0f;
            if (activeMode == Mode::Plate)
                plate.process (inL, inR, wl, wr);
            else
                fdn.process (inL, inR, wl, wr);

            wl = hpL.process (wl) * fade;
            wr = hpR.process (wr) * fade;

            const float dryGain = std::cos (smoothedMix * 0.5f * kPi);
            const float wetGain = std::sin (smoothedMix * 0.5f * kPi);
            left[n]  = dryL * dryGain + wl * wetGain;
            right[n] = dryR * dryGain + wr * wetGain;
        }
    }

private:
    float sr = 44100.0f;
    Params params;
    Mode activeMode = Mode::Hall;
    DelayBuffer preL, preR;
    FdnReverb fdn;
    PlateReverb plate;
    OnePoleHP hpL, hpR;
    float smoothedMix = 0.3f, smoothedPre = 0.0f, fade = 1.0f;
};

} // namespace qpasa
