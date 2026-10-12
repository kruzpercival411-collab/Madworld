#include "PluginProcessor.h"
#include "PluginEditor.h"

using APF = juce::AudioParameterFloat;
using APC = juce::AudioParameterChoice;
using APB = juce::AudioParameterBool;
using API = juce::AudioParameterInt;
using NR  = juce::NormalisableRange<float>;

static const juce::StringArray kKeys   { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
static const juce::StringArray kScales { "Chromatic","Major","Minor","Harm Minor","Pent Major","Pent Minor" };
static const juce::StringArray kModes  { "-","STU","ST+","REV","GATE","STOP","OCT","CRSH" };

juce::AudioProcessorValueTreeState::ParameterLayout MadworldAudioProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> v;
    auto id = [] (const char* s) { return juce::ParameterID { s, 1 }; };

    // Autotune
    v.push_back (std::make_unique<APF> (id ("tuneAmt"), "Tune Amount", NR (0.f, 1.f), 0.f));
    v.push_back (std::make_unique<APF> (id ("tuneSpeed"), "Retune Speed", NR (0.f, 200.f, 0.f, 0.5f), 0.f));
    v.push_back (std::make_unique<APC> (id ("tuneKey"), "Key", kKeys, 0));
    v.push_back (std::make_unique<APC> (id ("tuneScale"), "Scale", kScales, 2));
    v.push_back (std::make_unique<API> (id ("tuneShift"), "Pitch Shift", -12, 12, 0));

    // Glitch
    v.push_back (std::make_unique<APB> (id ("glOn"), "Glitch On", false));
    v.push_back (std::make_unique<APC> (id ("glRate"), "Glitch Rate", juce::StringArray { "1/8","1/16","1/32" }, 1));
    v.push_back (std::make_unique<APF> (id ("glMix"), "Glitch Mix", NR (0.f, 1.f), 1.f));
    v.push_back (std::make_unique<APF> (id ("glProb"), "Glitch Chance", NR (0.f, 1.f), 1.f));
    for (int i = 0; i < 16; ++i)
    {
        auto sid = "step" + juce::String (i);
        v.push_back (std::make_unique<APC> (juce::ParameterID { sid, 1 }, "Step " + juce::String (i + 1), kModes, 0));
    }

    // Distortion
    v.push_back (std::make_unique<APC> (id ("dsMode"), "Dist Mode", juce::StringArray { "Off","Soft","Hard","Fold","Crush","Asym" }, 0));
    v.push_back (std::make_unique<APF> (id ("dsDrive"), "Drive", NR (0.f, 40.f), 12.f));
    v.push_back (std::make_unique<APF> (id ("dsMix"), "Dist Mix", NR (0.f, 1.f), 0.6f));

    // Colour
    v.push_back (std::make_unique<APC> (id ("colFilt"), "Filter", juce::StringArray { "Off","Lowpass","Highpass","Bandpass" }, 0));
    v.push_back (std::make_unique<APF> (id ("colCut"), "Cutoff", NR (40.f, 18000.f, 0.f, 0.25f), 6000.f));
    v.push_back (std::make_unique<APF> (id ("colRes"), "Resonance", NR (0.f, 1.f), 0.2f));
    v.push_back (std::make_unique<APF> (id ("colTilt"), "Tilt", NR (-12.f, 12.f), 0.f));
    v.push_back (std::make_unique<APF> (id ("colAir"), "Air", NR (0.f, 1.f), 0.f));

    // FX
    v.push_back (std::make_unique<APF> (id ("fxChorus"), "Chorus", NR (0.f, 1.f), 0.f));
    v.push_back (std::make_unique<APC> (id ("fxDlyTime"), "Delay Time", juce::StringArray { "1/16","1/8","1/8D","1/4","1/4D","1/2" }, 2));
    v.push_back (std::make_unique<APF> (id ("fxDlyFb"), "Feedback", NR (0.f, 0.92f), 0.35f));
    v.push_back (std::make_unique<APF> (id ("fxDlyMix"), "Delay Mix", NR (0.f, 1.f), 0.f));
    v.push_back (std::make_unique<APF> (id ("fxRevSize"), "Reverb Size", NR (0.f, 1.f), 0.6f));
    v.push_back (std::make_unique<APF> (id ("fxRevMix"), "Reverb Mix", NR (0.f, 1.f), 0.f));
    v.push_back (std::make_unique<APF> (id ("fxWidth"), "Width", NR (0.f, 2.f), 1.f));

    // Output
    v.push_back (std::make_unique<APF> (id ("outGain"), "Output", NR (-24.f, 12.f), 0.f));
    v.push_back (std::make_unique<APF> (id ("mix"), "Mix", NR (0.f, 1.f), 1.f));

    return { v.begin(), v.end() };
}

MadworldAudioProcessor::MadworldAudioProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MADWORLD", createLayout())
{
    for (int i = 0; i < 16; ++i)
        steps[i] = dynamic_cast<APC*> (apvts.getParameter ("step" + juce::String (i)));
}

bool MadworldAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return out == layouts.getMainInputChannelSet();
}

void MadworldAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    const auto nCh = (juce::uint32) juce::jmax (1, getTotalNumOutputChannels());
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, nCh };

    tuner.prepare (sampleRate);
    glitch.prepare (sampleRate);
    dist.prepare (spec);
    delay.prepare (sampleRate);
    chorus.prepare (spec); chorus.reset();
    chorus.setRate (0.9f); chorus.setDepth (0.35f); chorus.setCentreDelay (9.0f); chorus.setFeedback (0.12f);
    reverb.prepare (spec); reverb.reset();
    svf.prepare (spec); svf.reset();
    dryBuf.setSize ((int) nCh, samplesPerBlock);
    for (int c = 0; c < 2; ++c) tiltLp[c] = airLp[c] = dc[c] = dcPrev[c] = 0.0f;
    setLatencySamples (dist.latency());
}

void MadworldAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int nCh = juce::jmin (2, getTotalNumOutputChannels());
    const int n = buffer.getNumSamples();
    for (auto i = nCh; i < buffer.getNumChannels(); ++i) buffer.clear (i, 0, n);
    if (n == 0) return;

    dryBuf.makeCopyOf (buffer, true);

    double bpm = 120.0, ppq = 0.0; bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = juce::jlimit (30.0, 300.0, *b);
            if (pos->getIsPlaying())
                if (auto q = pos->getPpqPosition()) { ppq = *q; playing = true; }
        }

    // 1. Autotune / pitch
    const float tuneAmt = p ("tuneAmt")->load();
    const int shift = (int) p ("tuneShift")->load();
    if (tuneAmt > 0.001f || shift != 0)
        tuner.process (buffer, tuneAmt, p ("tuneSpeed")->load(), (int) p ("tuneKey")->load(), (int) p ("tuneScale")->load(), shift);
    else
        tuner.detectedMidi.store (-1.0f);

    // 2. Glitch sequencer
    if (p ("glOn")->load() > 0.5f)
    {
        int pattern[16];
        for (int i = 0; i < 16; ++i) pattern[i] = steps[i]->getIndex();
        glitch.process (buffer, bpm, ppq, playing, pattern, (int) p ("glRate")->load(), p ("glMix")->load(), p ("glProb")->load());
    }
    else glitch.uiStep.store (-1);

    // 3. Distortion (2x oversampled, always run for constant latency)
    dist.process (buffer, (int) p ("dsMode")->load(), p ("dsDrive")->load(), p ("dsMix")->load());

    // 4. Colour: filter, tilt, air
    {
        const int fmode = (int) p ("colFilt")->load();
        const float tilt = p ("colTilt")->load(), air = p ("colAir")->load();
        if (fmode > 0)
        {
            svf.setType (fmode == 1 ? juce::dsp::StateVariableTPTFilterType::lowpass
                       : fmode == 2 ? juce::dsp::StateVariableTPTFilterType::highpass
                                    : juce::dsp::StateVariableTPTFilterType::bandpass);
            svf.setCutoffFrequency (juce::jmin (p ("colCut")->load(), (float) sr * 0.45f));
            svf.setResonance (0.5f + p ("colRes")->load() * 7.0f);
        }
        const float gl = juce::Decibels::decibelsToGain (-tilt * 0.5f), gh = juce::Decibels::decibelsToGain (tilt * 0.5f);
        const float aTilt = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * 800.0f / (float) sr);
        const float aAir  = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * 4500.0f / (float) sr);
        for (int c = 0; c < nCh; ++c)
        {
            auto* d = buffer.getWritePointer (c);
            for (int i = 0; i < n; ++i)
            {
                float x = d[i];
                if (fmode > 0) x = svf.processSample (c, x);
                if (std::abs (tilt) > 0.01f) { tiltLp[c] += (x - tiltLp[c]) * aTilt; x = tiltLp[c] * gl + (x - tiltLp[c]) * gh; }
                if (air > 0.001f) { airLp[c] += (x - airLp[c]) * aAir; const float hi = x - airLp[c]; x += std::tanh (hi * 4.0f) * air * 0.35f; }
                d[i] = x;
            }
        }
    }

    juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) nCh, (size_t) n);
    juce::dsp::ProcessContextReplacing<float> ctx (block);

    // 5. FX: chorus, ping-pong delay, reverb, width
    const float ch = p ("fxChorus")->load();
    if (ch > 0.001f) { chorus.setMix (ch * 0.6f); chorus.setDepth (0.2f + ch * 0.4f); chorus.process (ctx); }

    {
        static const float beats[6] = { 0.25f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f };
        const float ds = beats[juce::jlimit (0, 5, (int) p ("fxDlyTime")->load())] * (float) (60.0 / bpm * sr);
        const float dm = p ("fxDlyMix")->load();
        delay.process (buffer, ds, p ("fxDlyFb")->load(), dm);
    }

    const float rm = p ("fxRevMix")->load();
    if (rm > 0.001f)
    {
        juce::dsp::Reverb::Parameters rp;
        rp.roomSize = 0.3f + p ("fxRevSize")->load() * 0.69f;
        rp.damping = 0.45f; rp.wetLevel = rm * 0.7f; rp.dryLevel = 1.0f - rm * 0.4f; rp.width = 1.0f;
        reverb.setParameters (rp);
        reverb.process (ctx);
    }

    const float width = p ("fxWidth")->load();
    if (nCh == 2 && std::abs (width - 1.0f) > 0.01f)
    {
        auto* l = buffer.getWritePointer (0); auto* r = buffer.getWritePointer (1);
        for (int i = 0; i < n; ++i) { const float m = 0.5f * (l[i] + r[i]), s = 0.5f * (l[i] - r[i]) * width; l[i] = m + s; r[i] = m - s; }
    }

    // 6. Output: dry/wet, gain, DC block, soft ceiling
    const float mix = p ("mix")->load();
    const float gain = juce::Decibels::decibelsToGain (p ("outGain")->load());
    for (int c = 0; c < nCh; ++c)
    {
        auto* d = buffer.getWritePointer (c);
        const auto* dr = dryBuf.getReadPointer (c);
        for (int i = 0; i < n; ++i)
        {
            float x = (dr[i] + (d[i] - dr[i]) * mix) * gain;
            const float y = x - dcPrev[c] + 0.9995f * dc[c];
            dcPrev[c] = x; dc[c] = y; x = y;
            const float a = std::abs (x);
            if (a > 0.9f) x = std::copysign (0.9f + 0.1f * std::tanh ((a - 0.9f) / 0.1f), x);
            d[i] = x;
        }
    }
}

//==============================================================================
struct Preset { const char* name; std::vector<std::pair<const char*, float>> values; std::vector<int> pattern; };

static const std::vector<Preset>& presets()
{
    static const std::vector<Preset> list {
        { "Init", {}, {} },
        { "Hyperpop Vox", { {"tuneAmt",1}, {"tuneSpeed",0}, {"tuneScale",1}, {"dsMode",1}, {"dsDrive",8}, {"dsMix",0.35f},
                            {"colTilt",3}, {"colAir",0.45f}, {"fxChorus",0.3f}, {"fxDlyTime",2}, {"fxDlyFb",0.3f}, {"fxDlyMix",0.16f},
                            {"fxRevSize",0.6f}, {"fxRevMix",0.18f}, {"fxWidth",1.3f} }, {} },
        { "Glitch Mayhem", { {"glOn",1}, {"glRate",1}, {"glProb",0.85f}, {"dsMode",2}, {"dsDrive",14}, {"dsMix",0.5f}, {"fxWidth",1.2f} },
                           { 0,1,0,3, 0,0,2,4, 0,1,0,5, 0,6,2,7 } },
        { "Crushed Lead", { {"dsMode",4}, {"dsDrive",22}, {"dsMix",0.7f}, {"colFilt",1}, {"colCut",6500}, {"colRes",0.3f},
                            {"fxDlyTime",1}, {"fxDlyFb",0.45f}, {"fxDlyMix",0.25f} }, {} },
        { "Angel Dust", { {"tuneAmt",1}, {"tuneSpeed",25}, {"tuneShift",12}, {"tuneScale",1}, {"fxChorus",0.5f}, {"colAir",0.6f},
                          {"fxRevSize",0.85f}, {"fxRevMix",0.4f}, {"fxWidth",1.6f}, {"fxDlyMix",0.12f} }, {} },
        { "Robot Collapse", { {"tuneAmt",1}, {"tuneSpeed",0}, {"tuneScale",0}, {"dsMode",3}, {"dsDrive",18}, {"dsMix",0.6f},
                              {"glOn",1}, {"glRate",2}, {"glProb",0.5f} }, { 1,0,0,2, 0,0,7,0, 1,0,0,2, 0,4,0,6 } },
        { "Club Wreck", { {"dsMode",5}, {"dsDrive",16}, {"dsMix",0.55f}, {"colFilt",2}, {"colCut",180}, {"colTilt",4},
                          {"glOn",1}, {"glRate",1}, {"glProb",0.7f}, {"fxRevMix",0.15f} }, { 0,0,0,0, 0,0,0,1, 0,0,0,0, 0,5,2,2 } },
    };
    return list;
}

int MadworldAudioProcessor::getNumPrograms() { return (int) presets().size(); }
const juce::String MadworldAudioProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, (int) presets().size()) ? presets()[(size_t) index].name : "";
}

void MadworldAudioProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) presets().size())) return;
    currentProgram = index;
    const auto& pr = presets()[(size_t) index];
    for (auto* prm : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
            r->setValueNotifyingHost (r->getDefaultValue());
    for (const auto& kv : pr.values)
        if (auto* r = apvts.getParameter (kv.first))
            r->setValueNotifyingHost (r->convertTo0to1 (kv.second));
    for (size_t i = 0; i < pr.pattern.size() && i < 16; ++i)
        *steps[i] = pr.pattern[i];
}

void MadworldAudioProcessor::randomisePattern()
{
    juce::Random r;
    for (auto* s : steps)
    {
        const float x = r.nextFloat();
        const int m = x < 0.45f ? 0 : x < 0.62f ? 1 : x < 0.72f ? 2 : x < 0.80f ? 3 : x < 0.86f ? 4 : x < 0.91f ? 5 : x < 0.96f ? 6 : 7;
        *s = m;
    }
}

void MadworldAudioProcessor::clearPattern() { for (auto* s : steps) *s = 0; }

void MadworldAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void MadworldAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* MadworldAudioProcessor::createEditor() { return new MadworldAudioProcessorEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MadworldAudioProcessor(); }
