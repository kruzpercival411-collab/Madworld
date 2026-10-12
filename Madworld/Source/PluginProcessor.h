#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Dsp.h"

class MadworldAudioProcessor : public juce::AudioProcessor
{
public:
    MadworldAudioProcessor();
    ~MadworldAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Madworld"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void randomisePattern();
    void clearPattern();

    juce::AudioProcessorValueTreeState apvts;
    juce::AudioParameterChoice* steps[16] {};
    mad::PitchCorrector tuner;
    mad::GlitchSeq glitch;

private:
    mad::Distortion dist;
    mad::PingPong delay;
    juce::dsp::Chorus<float> chorus;
    juce::dsp::Reverb reverb;
    juce::dsp::StateVariableTPTFilter<float> svf;
    juce::AudioBuffer<float> dryBuf;
    float tiltLp[2] {}, airLp[2] {}, dc[2] {}, dcPrev[2] {};
    double sr = 44100.0;
    int currentProgram = 0;

    std::atomic<float>* p (const char* id) { return apvts.getRawParameterValue (id); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MadworldAudioProcessor)
};
