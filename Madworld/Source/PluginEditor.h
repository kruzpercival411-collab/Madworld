#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

namespace madui
{
namespace col
{
    const juce::Colour bg     { 0xff0b0a10 };
    const juce::Colour panel  { 0xff15131d };
    const juce::Colour line   { 0xff2a2638 };
    const juce::Colour pink   { 0xffff3ea5 };
    const juce::Colour cyan   { 0xff3ef0ff };
    const juce::Colour lime   { 0xffc6ff3e };
    const juce::Colour text   { 0xffeae6f5 };
    const juce::Colour muted  { 0xff8a85a0 };
}

struct LnF : juce::LookAndFeel_V4
{
    LnF();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int, int, int, int, juce::ComboBox&) override;
};

struct Knob : juce::Component
{
    Knob (juce::AudioProcessorValueTreeState& st, const juce::String& id, const juce::String& name, juce::Colour c);
    void resized() override;
    juce::Slider slider;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

struct Choice : juce::Component
{
    Choice (juce::AudioProcessorValueTreeState& st, const juce::String& id, const juce::String& name);
    void resized() override;
    juce::ComboBox box;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
};

struct StepGrid : juce::Component
{
    explicit StepGrid (MadworldAudioProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    MadworldAudioProcessor& proc;
};
}

class MadworldAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit MadworldAudioProcessorEditor (MadworldAudioProcessor&);
    ~MadworldAudioProcessorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void panel (juce::Graphics&, juce::Rectangle<int> r, const juce::String& title, juce::Colour c);

    MadworldAudioProcessor& proc;
    madui::LnF lnf;

    std::vector<std::unique_ptr<madui::Knob>> knobs;
    std::vector<std::unique_ptr<madui::Choice>> choices;
    madui::Knob* k (const juce::String& id, const juce::String& name, juce::Colour c);
    madui::Choice* ch (const juce::String& id, const juce::String& name);

    madui::Knob *tuneAmt, *tuneSpeed, *tuneShift, *glMix, *glProb, *dsDrive, *dsMix,
                *colCut, *colRes, *colTilt, *colAir, *fxChorus, *fxDlyFb, *fxDlyMix, *fxRevSize, *fxRevMix, *fxWidth, *outGain, *mix;
    madui::Choice *tuneKey, *tuneScale, *glRate, *dsMode, *colFilt, *fxDlyTime;

    juce::TextButton glOn { "GLITCH ON" }, chaos { "CHAOS" }, clear { "CLEAR" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> glOnAtt;
    juce::ComboBox presetBox;
    juce::Label noteLabel;
    madui::StepGrid grid;

    juce::Rectangle<int> rTune, rDist, rCol, rGlitch, rFx;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MadworldAudioProcessorEditor)
};
