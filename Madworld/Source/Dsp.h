#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <cmath>
#include <vector>

namespace mad
{
static inline float interp (const std::vector<float>& b, int mask, double pos)
{
    const int i0 = (int) std::floor (pos);
    const float f = (float) (pos - i0);
    const float a = b[(size_t) (i0 & mask)];
    const float c = b[(size_t) ((i0 + 1) & mask)];
    return a + (c - a) * f;
}

//==============================================================================
// Pitch correction: YIN detector (decimated) + dual-tap delay-line pitch shifter
class PitchCorrector
{
public:
    std::atomic<float> detectedMidi { -1.0f };

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        srD = sr * 0.5;
        tauMax = juce::jlimit (100, 700, (int) (srD / 70.0));
        tauMin = juce::jmax (2, (int) (srD / 1200.0));
        anaRing.assign (ringSize, 0.0f);
        frame.assign ((size_t) (W + tauMax + 2), 0.0f);
        cmnd.assign ((size_t) (tauMax + 2), 1.0f);
        for (auto& d : dl) d.assign (DL, 0.0f);
        winLen = (float) (sr * 0.028);
        reset();
    }

    void reset()
    {
        std::fill (anaRing.begin(), anaRing.end(), 0.0f);
        for (auto& d : dl) std::fill (d.begin(), d.end(), 0.0f);
        anaWrite = 0; decimFlag = false; decimAcc = 0.0f; hop = 0;
        wpos = 0; phase = 0.0f; curRatio = 1.0f; targetRatio = 1.0f; lastNote = -100;
        detectedMidi.store (-1.0f);
    }

    void process (juce::AudioBuffer<float>& buf, float amount, float speedMs, int key, int scale, int shiftSemis)
    {
        const int nCh = juce::jmin (2, buf.getNumChannels());
        const int n = buf.getNumSamples();
        const float alpha = speedMs <= 0.5f ? 1.0f : 1.0f - std::exp (-1.0f / (speedMs * 0.001f * (float) sr));
        corrAmount = amount; key_ = key; scale_ = scale; shift_ = shiftSemis;

        for (int i = 0; i < n; ++i)
        {
            float mono = 0.0f;
            for (int c = 0; c < nCh; ++c) mono += buf.getSample (c, i);
            mono /= (float) nCh;

            // decimate by 2 into analysis ring
            if (decimFlag) { anaRing[(size_t) anaWrite] = 0.5f * (decimAcc + mono); anaWrite = (anaWrite + 1) & (ringSize - 1); }
            else decimAcc = mono;
            decimFlag = ! decimFlag;

            if (++hop >= 256) { hop = 0; analyse(); }

            curRatio += (targetRatio - curRatio) * alpha;
            phase += (1.0f - curRatio) / winLen;
            phase -= std::floor (phase);
            const float d1 = phase * winLen + 2.0f;
            float p2 = phase + 0.5f; if (p2 >= 1.0f) p2 -= 1.0f;
            const float d2 = p2 * winLen + 2.0f;
            const float s = std::sin (juce::MathConstants<float>::pi * phase);
            const float w1 = s * s;

            for (int c = 0; c < nCh; ++c)
            {
                auto& d = dl[(size_t) c];
                d[(size_t) wpos] = buf.getSample (c, i);
                const float a = interp (d, DL - 1, (double) wpos - d1);
                const float b = interp (d, DL - 1, (double) wpos - d2);
                buf.setSample (c, i, a * w1 + b * (1.0f - w1));
            }
            wpos = (wpos + 1) & (DL - 1);
        }
    }

private:
    static constexpr int ringSize = 2048, W = 512, DL = 8192;
    double sr = 44100.0, srD = 22050.0;
    int tauMin = 18, tauMax = 315;
    std::vector<float> anaRing, frame, cmnd;
    std::vector<float> dl[2];
    int anaWrite = 0, hop = 0, wpos = 0, lastNote = -100;
    bool decimFlag = false;
    float decimAcc = 0.0f, phase = 0.0f, winLen = 1200.0f, curRatio = 1.0f, targetRatio = 1.0f;
    float corrAmount = 1.0f; int key_ = 0, scale_ = 0, shift_ = 0;

    static bool inScale (int note, int key, int scale)
    {
        static const int masks[6] = { 0xFFF,
            (1<<0)|(1<<2)|(1<<4)|(1<<5)|(1<<7)|(1<<9)|(1<<11),
            (1<<0)|(1<<2)|(1<<3)|(1<<5)|(1<<7)|(1<<8)|(1<<10),
            (1<<0)|(1<<2)|(1<<3)|(1<<5)|(1<<7)|(1<<8)|(1<<11),
            (1<<0)|(1<<2)|(1<<4)|(1<<7)|(1<<9),
            (1<<0)|(1<<3)|(1<<5)|(1<<7)|(1<<10) };
        const int pc = ((note - key) % 12 + 12) % 12;
        return (masks[juce::jlimit (0, 5, scale)] >> pc) & 1;
    }

    void analyse()
    {
        const int len = W + tauMax + 1;
        int start = (anaWrite - len) & (ringSize - 1);
        double energy = 0.0;
        for (int j = 0; j < len; ++j)
        {
            frame[(size_t) j] = anaRing[(size_t) ((start + j) & (ringSize - 1))];
            if (j < W) energy += frame[(size_t) j] * frame[(size_t) j];
        }
        const float shiftRatio = std::pow (2.0f, (float) shift_ / 12.0f);
        const float rms = (float) std::sqrt (energy / W);

        if (rms < 0.008f) { targetRatio = shiftRatio; detectedMidi.store (-1.0f); return; }

        double running = 0.0;
        cmnd[0] = 1.0f;
        int found = -1;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            double s = 0.0;
            for (int j = 0; j < W; ++j)
            {
                const float df = frame[(size_t) j] - frame[(size_t) (j + tau)];
                s += df * df;
            }
            running += s;
            cmnd[(size_t) tau] = running > 0.0 ? (float) (s * tau / running) : 1.0f;
        }
        for (int tau = tauMin; tau < tauMax; ++tau)
        {
            if (cmnd[(size_t) tau] < 0.15f)
            {
                while (tau + 1 < tauMax && cmnd[(size_t) (tau + 1)] < cmnd[(size_t) tau]) ++tau;
                found = tau; break;
            }
        }
        if (found < 0) { targetRatio = shiftRatio; detectedMidi.store (-1.0f); return; }

        double t = found;
        {
            const float a = cmnd[(size_t) (found - 1)], b = cmnd[(size_t) found], c = cmnd[(size_t) (found + 1)];
            const float den = a - 2.0f * b + c;
            if (std::abs (den) > 1e-9f) t += 0.5 * (a - c) / den;
        }
        const double f = srD / t;
        const float midi = (float) (69.0 + 12.0 * std::log2 (f / 440.0));
        detectedMidi.store (midi);

        int target = lastNote;
        if (std::abs (midi - (float) lastNote) > 0.6f || ! inScale (lastNote, key_, scale_))
        {
            const int r = (int) std::lround (midi);
            float best = 1e9f;
            for (int k = r - 6; k <= r + 6; ++k)
                if (inScale (k, key_, scale_) && std::abs ((float) k - midi) < best) { best = std::abs ((float) k - midi); target = k; }
            lastNote = target;
        }
        const float corr = ((float) target - midi) * corrAmount;
        targetRatio = std::pow (2.0f, corr / 12.0f) * shiftRatio;
    }
};

//==============================================================================
// 16-step tempo-synced glitch sequencer
class GlitchSeq
{
public:
    enum Mode { Off = 0, Stutter, StutterFast, Reverse, Gate, TapeStop, OctUp, Crush };
    std::atomic<int> uiStep { -1 };

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        size = juce::nextPowerOfTwo ((int) (sr * 6.0));
        mask = size - 1;
        for (auto& b : buf) b.assign ((size_t) size, 0.0f);
        gCoef = 1.0f - std::exp (-1.0f / (0.003f * (float) sr));
        reset();
    }

    void reset()
    {
        for (auto& b : buf) std::fill (b.begin(), b.end(), 0.0f);
        wpos = 0; stepStart = 0; lastStepAbs = -1; active = false; g = 0.0f; curMode = 0; holdCount = 0;
        hold[0] = hold[1] = 0.0f;
    }

    void process (juce::AudioBuffer<float>& b, double bpm, double ppqStart, bool playing,
                  const int* pattern, int rateIdx, float mix, float prob)
    {
        const int nCh = juce::jmin (2, b.getNumChannels());
        const int n = b.getNumSamples();
        const double stepBeats = rateIdx == 0 ? 0.5 : rateIdx == 1 ? 0.25 : 0.125;
        const double ppqPerSample = bpm / 60.0 / sr;
        const int L = juce::jlimit (64, size / 3, (int) (stepBeats / ppqPerSample));
        const double ppq0 = playing ? ppqStart : internalPpq;

        for (int i = 0; i < n; ++i)
        {
            const double p = ppq0 + i * ppqPerSample;
            const long long stepAbs = (long long) std::floor (p / stepBeats);
            if (stepAbs != lastStepAbs)
            {
                lastStepAbs = stepAbs;
                const int step = (int) (((stepAbs % 16) + 16) % 16);
                curMode = pattern[step];
                active = curMode != Off && rng.nextFloat() < prob;
                stepStart = wpos;
                uiStep.store (step);
            }

            for (int c = 0; c < nCh; ++c) buf[(size_t) c][(size_t) wpos] = b.getSample (c, i);

            g += ((active ? 1.0f : 0.0f) - g) * gCoef;
            if (g > 1.0e-4f)
            {
                const int s = (wpos - stepStart) & mask;
                for (int c = 0; c < nCh; ++c)
                {
                    const float dry = b.getSample (c, i);
                    const float wet = render (c, s, L, dry);
                    b.setSample (c, i, dry + (wet - dry) * g * mix);
                }
            }
            wpos = (wpos + 1) & mask;
        }
        internalPpq = ppq0 + n * ppqPerSample;
    }

private:
    double sr = 44100.0, internalPpq = 0.0;
    std::vector<float> buf[2];
    int size = 1 << 18, mask = (1 << 18) - 1, wpos = 0, stepStart = 0, curMode = 0, holdCount = 0;
    long long lastStepAbs = -1;
    bool active = false;
    float g = 0.0f, gCoef = 0.01f, hold[2] {};
    juce::Random rng;

    static float edge (int q, int len)
    {
        const float F = (float) juce::jmin (96, juce::jmax (1, len / 4));
        return juce::jlimit (0.0f, 1.0f, juce::jmin ((float) q / F, (float) (len - q) / F));
    }

    float render (int c, int s, int L, float dry)
    {
        const auto& b = buf[(size_t) c];
        switch (curMode)
        {
            case Stutter:
            case StutterFast:
            {
                const int rep = juce::jmax (32, curMode == Stutter ? L / 2 : L / 4);
                const int q = s % rep;
                return b[(size_t) ((stepStart + q) & mask)] * edge (q, rep);
            }
            case Reverse:
                return b[(size_t) ((stepStart - 1 - s) & mask)] * edge (juce::jmin (s, L), L);
            case Gate:
                return 0.0f;
            case TapeStop:
            {
                const double ss = juce::jmin (s, L);
                const double pos = stepStart + ss - 0.5 * ss * ss / L;
                return interp (b, mask, pos) * (s >= L ? 0.0f : 1.0f);
            }
            case OctUp:
            {
                const int half = juce::jmax (32, L / 2);
                const int q = s % half;
                return b[(size_t) ((stepStart - L + 2 * q) & mask)] * edge (q, half);
            }
            case Crush:
            {
                if (c == 0 && ++holdCount >= 7) holdCount = 0;
                if (holdCount == 0) hold[c] = std::round (dry * 7.0f) / 7.0f;
                return hold[c];
            }
            default: return dry;
        }
    }
};

//==============================================================================
// Oversampled multi-mode distortion
class Distortion
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        os = std::make_unique<juce::dsp::Oversampling<float>> ((size_t) spec.numChannels, 1,
                 juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
        os->initProcessing (spec.maximumBlockSize);
    }
    void reset() { if (os) os->reset(); }
    int latency() const { return os ? (int) std::round (os->getLatencyInSamples()) : 0; }

    void process (juce::AudioBuffer<float>& buffer, int mode, float driveDb, float mix)
    {
        juce::dsp::AudioBlock<float> block (buffer);
        auto up = os->processSamplesUp (block);
        if (mode > 0 && mix > 0.0f)
        {
            const float drive = juce::Decibels::decibelsToGain (driveDb);
            const float makeup = juce::jlimit (0.3f, 1.0f, 1.8f / std::sqrt (drive));
            const float bits = juce::jmap (driveDb, 0.0f, 40.0f, 8.0f, 2.0f);
            const float steps = std::pow (2.0f, bits);
            for (size_t c = 0; c < up.getNumChannels(); ++c)
            {
                auto* d = up.getChannelPointer (c);
                for (size_t i = 0; i < up.getNumSamples(); ++i)
                {
                    const float x = d[i];
                    const float v = x * drive;
                    float y;
                    switch (mode)
                    {
                        case 1: y = std::tanh (v); break;
                        case 2: y = juce::jlimit (-0.8f, 0.8f, v) * 1.25f; break;
                        case 3: y = std::sin (v * 1.3f); break;
                        case 4: y = std::round (std::tanh (x * 1.5f) * steps) / steps; break;
                        default: y = std::tanh (v + 0.35f) - std::tanh (0.35f); break;
                    }
                    if (mode != 4) y *= makeup;
                    d[i] = x + (y - x) * mix;
                }
            }
        }
        os->processSamplesDown (block);
    }

private:
    std::unique_ptr<juce::dsp::Oversampling<float>> os;
};

//==============================================================================
// Ping-pong tempo delay
class PingPong
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        size = juce::nextPowerOfTwo ((int) (sr * 4.0));
        for (auto& b : buf) b.assign ((size_t) size, 0.0f);
        reset();
    }
    void reset() { for (auto& b : buf) std::fill (b.begin(), b.end(), 0.0f); wpos = 0; cur = -1.0f; lp[0] = lp[1] = 0.0f; }

    void process (juce::AudioBuffer<float>& b, float delaySamples, float fb, float mix)
    {
        const int nCh = juce::jmin (2, b.getNumChannels());
        const int mask = size - 1;
        delaySamples = juce::jlimit (16.0f, (float) size - 4.0f, delaySamples);
        if (cur < 0.0f) cur = delaySamples;
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            cur += (delaySamples - cur) * 0.0005f;
            const double rp = (double) wpos - cur;
            const float dL = interp (buf[0], mask, rp);
            const float dR = nCh > 1 ? interp (buf[1], mask, rp) : dL;
            lp[0] += (dL - lp[0]) * 0.35f;
            lp[1] += (dR - lp[1]) * 0.35f;
            const float inL = b.getSample (0, i);
            if (nCh > 1)
            {
                const float inR = b.getSample (1, i);
                buf[0][(size_t) wpos] = 0.5f * (inL + inR) + lp[1] * fb;
                buf[1][(size_t) wpos] = lp[0] * fb;
                b.setSample (0, i, inL + dL * mix);
                b.setSample (1, i, inR + dR * mix);
            }
            else
            {
                buf[0][(size_t) wpos] = inL + lp[0] * fb;
                b.setSample (0, i, inL + dL * mix);
            }
            wpos = (wpos + 1) & mask;
        }
    }

private:
    double sr = 44100.0;
    std::vector<float> buf[2];
    int size = 1 << 18, wpos = 0;
    float cur = -1.0f, lp[2] {};
};
} // namespace mad
