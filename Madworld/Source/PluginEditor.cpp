#include "PluginEditor.h"

using namespace madui;

static juce::Font font (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

//==============================================================================
LnF::LnF()
{
    setColour (juce::Slider::textBoxTextColourId, col::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, col::muted);
    setColour (juce::ComboBox::backgroundColourId, col::bg);
    setColour (juce::ComboBox::textColourId, col::text);
    setColour (juce::ComboBox::outlineColourId, col::line);
    setColour (juce::ComboBox::arrowColourId, col::cyan);
    setColour (juce::PopupMenu::backgroundColourId, col::panel);
    setColour (juce::PopupMenu::textColourId, col::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::pink);
    setColour (juce::PopupMenu::highlightedTextColourId, col::bg);
    setColour (juce::TextButton::textColourOffId, col::text);
    setColour (juce::TextButton::textColourOnId, col::bg);
}

void LnF::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    auto r = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (6.0f);
    const float size = juce::jmin (r.getWidth(), r.getHeight());
    r = r.withSizeKeepingCentre (size, size);
    const auto c = r.getCentre();
    const float rad = size * 0.5f;
    const float ang = a0 + pos * (a1 - a0);
    const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);

    juce::Path track; track.addCentredArc (c.x, c.y, rad - 3, rad - 3, 0, a0, a1, true);
    g.setColour (col::line); g.strokePath (track, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float from = bipolar ? (a0 + a1) * 0.5f : a0;
    juce::Path val; val.addCentredArc (c.x, c.y, rad - 3, rad - 3, 0, juce::jmin (from, ang), juce::jmax (from, ang), true);
    g.setColour (accent); g.strokePath (val, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (col::panel.brighter (0.15f));
    g.fillEllipse (r.reduced (9.0f));
    g.setColour (col::line); g.drawEllipse (r.reduced (9.0f), 1.0f);
    juce::Path ptr;
    ptr.addRoundedRectangle (-1.5f, -(rad - 11), 3.0f, rad * 0.42f, 1.5f);
    g.setColour (col::text);
    g.fillPath (ptr, juce::AffineTransform::rotation (ang).translated (c.x, c.y));
}

void LnF::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    const auto accent = b.getProperties().contains ("accent") ? juce::Colour ((juce::uint32) (juce::int64) b.getProperties()["accent"]) : col::pink;
    g.setColour (on ? accent : (down ? col::line.brighter (0.2f) : col::bg));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (on ? accent : (over ? accent.withAlpha (0.8f) : col::line));
    g.drawRoundedRectangle (r, 5.0f, 1.2f);
}

void LnF::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<int> (0, 0, w, h).toFloat().reduced (0.5f);
    g.setColour (col::bg); g.fillRoundedRectangle (r, 5.0f);
    g.setColour (box.hasKeyboardFocus (true) ? col::cyan : col::line); g.drawRoundedRectangle (r, 5.0f, 1.0f);
    juce::Path tri; const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    tri.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
    g.setColour (col::cyan); g.fillPath (tri);
}

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& st, const juce::String& id, const juce::String& name, juce::Colour c)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 16);
    slider.setColour (juce::Slider::rotarySliderFillColourId, c);
    addAndMakeVisible (slider);
    label.setText (name.toUpperCase(), juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (font (11.0f, true));
    addAndMakeVisible (label);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (st, id, slider);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (16));
    slider.setBounds (r);
}

Choice::Choice (juce::AudioProcessorValueTreeState& st, const juce::String& id, const juce::String& name)
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (st.getParameter (id)))
        box.addItemList (p->choices, 1);
    addAndMakeVisible (box);
    label.setText (name.toUpperCase(), juce::dontSendNotification);
    label.setFont (font (11.0f, true));
    addAndMakeVisible (label);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (st, id, box);
}

void Choice::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (16));
    box.setBounds (r.removeFromTop (26));
}

//==============================================================================
static juce::Colour modeColour (int m)
{
    switch (m)
    {
        case 1: case 2: return col::pink;
        case 3: return col::cyan;
        case 4: return col::muted;
        case 5: return juce::Colour (0xffffb23e);
        case 6: return col::lime;
        case 7: return juce::Colour (0xffb06bff);
        default: return col::bg;
    }
}

void StepGrid::paint (juce::Graphics& g)
{
    const float cw = (float) getWidth() / 16.0f;
    const int play = proc.glitch.uiStep.load();
    for (int i = 0; i < 16; ++i)
    {
        auto r = juce::Rectangle<float> (i * cw, 0.0f, cw, (float) getHeight()).reduced (2.0f);
        const int m = proc.steps[i]->getIndex();
        const auto c = modeColour (m);
        g.setColour (m == 0 ? (i % 4 == 0 ? col::panel.brighter (0.06f) : col::bg) : c.withAlpha (0.85f));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (i == play ? col::text : col::line);
        g.drawRoundedRectangle (r, 4.0f, i == play ? 2.0f : 1.0f);
        g.setColour (m == 0 ? col::muted : col::bg);
        g.setFont (font (11.0f, true));
        g.drawText (proc.steps[i]->getCurrentChoiceName(), r.toNearestInt(), juce::Justification::centred);
        g.setColour (col::muted.withAlpha (0.6f));
        g.setFont (font (9.0f));
        g.drawText (juce::String (i + 1), r.withTrimmedTop (r.getHeight() - 14).toNearestInt(), juce::Justification::centred);
    }
}

void StepGrid::mouseDown (const juce::MouseEvent& e)
{
    const int i = juce::jlimit (0, 15, (int) (e.position.x / ((float) getWidth() / 16.0f)));
    auto* prm = proc.steps[i];
    const int n = prm->choices.size();
    int m = prm->getIndex();
    if (e.mods.isShiftDown()) m = 0;
    else if (e.mods.isPopupMenu()) m = (m + n - 1) % n;
    else m = (m + 1) % n;
    prm->beginChangeGesture();
    *prm = m;
    prm->endChangeGesture();
    repaint();
}

//==============================================================================
Knob* MadworldAudioProcessorEditor::k (const juce::String& id, const juce::String& name, juce::Colour c)
{
    knobs.push_back (std::make_unique<Knob> (proc.apvts, id, name, c));
    addAndMakeVisible (*knobs.back());
    return knobs.back().get();
}

Choice* MadworldAudioProcessorEditor::ch (const juce::String& id, const juce::String& name)
{
    choices.push_back (std::make_unique<Choice> (proc.apvts, id, name));
    addAndMakeVisible (*choices.back());
    return choices.back().get();
}

MadworldAudioProcessorEditor::MadworldAudioProcessorEditor (MadworldAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p), grid (p)
{
    setLookAndFeel (&lnf);

    tuneAmt   = k ("tuneAmt", "Amount", col::pink);
    tuneSpeed = k ("tuneSpeed", "Speed ms", col::pink);
    tuneShift = k ("tuneShift", "Shift st", col::cyan);
    tuneKey   = ch ("tuneKey", "Key");
    tuneScale = ch ("tuneScale", "Scale");

    dsMode  = ch ("dsMode", "Mode");
    dsDrive = k ("dsDrive", "Drive dB", col::pink);
    dsMix   = k ("dsMix", "Mix", col::cyan);

    colFilt = ch ("colFilt", "Filter");
    colCut  = k ("colCut", "Cutoff", col::cyan);
    colRes  = k ("colRes", "Reso", col::cyan);
    colTilt = k ("colTilt", "Tilt dB", col::lime);
    colAir  = k ("colAir", "Air", col::lime);

    glRate = ch ("glRate", "Rate");
    glMix  = k ("glMix", "Mix", col::pink);
    glProb = k ("glProb", "Chance", col::cyan);

    fxChorus  = k ("fxChorus", "Chorus", col::cyan);
    fxDlyTime = ch ("fxDlyTime", "Delay");
    fxDlyFb   = k ("fxDlyFb", "Feedback", col::pink);
    fxDlyMix  = k ("fxDlyMix", "Dly Mix", col::pink);
    fxRevSize = k ("fxRevSize", "Rev Size", col::lime);
    fxRevMix  = k ("fxRevMix", "Rev Mix", col::lime);
    fxWidth   = k ("fxWidth", "Width", col::cyan);
    outGain   = k ("outGain", "Output", col::text);
    mix       = k ("mix", "Mix", col::text);

    colCut->slider.setTextValueSuffix (" Hz");
    colCut->slider.setNumDecimalPlacesToDisplay (0);
    tuneSpeed->slider.setNumDecimalPlacesToDisplay (0);
    dsDrive->slider.setNumDecimalPlacesToDisplay (1);

    glOn.setClickingTogglesState (true);
    addAndMakeVisible (glOn);
    glOnAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "glOn", glOn);

    chaos.getProperties().set ("accent", (juce::int64) col::cyan.getARGB());
    chaos.onClick = [this] { proc.randomisePattern(); grid.repaint(); };
    clear.onClick = [this] { proc.clearPattern(); grid.repaint(); };
    addAndMakeVisible (chaos);
    addAndMakeVisible (clear);
    addAndMakeVisible (grid);

    for (int i = 0; i < proc.getNumPrograms(); ++i) presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this] { proc.setCurrentProgram (presetBox.getSelectedId() - 1); grid.repaint(); };
    addAndMakeVisible (presetBox);

    noteLabel.setFont (font (22.0f, true));
    noteLabel.setColour (juce::Label::textColourId, col::cyan);
    noteLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (noteLabel);

    setSize (1000, 660);
    startTimerHz (30);
}

MadworldAudioProcessorEditor::~MadworldAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void MadworldAudioProcessorEditor::timerCallback()
{
    grid.repaint();
    const float m = proc.tuner.detectedMidi.load();
    static const char* names[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
    if (m < 0.0f) noteLabel.setText ("--", juce::dontSendNotification);
    else
    {
        const int n = (int) std::lround (m);
        const int cents = (int) std::lround ((m - (float) n) * 100.0f);
        noteLabel.setText (juce::String (names[((n % 12) + 12) % 12]) + juce::String (n / 12 - 1)
                           + "  " + (cents >= 0 ? "+" : "") + juce::String (cents) + "c", juce::dontSendNotification);
    }
}

void MadworldAudioProcessorEditor::panel (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title, juce::Colour c)
{
    g.setColour (col::panel);
    g.fillRoundedRectangle (r.toFloat(), 8.0f);
    g.setColour (col::line);
    g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 8.0f, 1.0f);
    g.setColour (c);
    g.fillRect (r.getX() + 14, r.getY() + 14, 8, 8);
    g.setColour (col::text);
    g.setFont (font (13.0f, true));
    g.drawText (title, r.getX() + 30, r.getY() + 8, 300, 20, juce::Justification::centredLeft);
}

void MadworldAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);

    // scanlines
    g.setColour (juce::Colours::white.withAlpha (0.018f));
    for (int y = 0; y < getHeight(); y += 3) g.drawHorizontalLine (y, 0.0f, (float) getWidth());

    // glitch logo
    auto logo = juce::Rectangle<int> (16, 10, 400, 46);
    g.setFont (font (40.0f, true));
    g.setColour (col::pink.withAlpha (0.9f));  g.drawText ("MADWORLD", logo.translated (-3, 1), juce::Justification::centredLeft);
    g.setColour (col::cyan.withAlpha (0.9f));  g.drawText ("MADWORLD", logo.translated (3, -1), juce::Justification::centredLeft);
    g.setColour (col::text);                   g.drawText ("MADWORLD", logo, juce::Justification::centredLeft);
    g.setColour (col::bg);
    g.fillRect (16, 30, 250, 2);
    g.fillRect (16, 41, 250, 1);
    g.setColour (col::muted);
    g.setFont (font (11.0f, true));
    g.drawText ("TUNE / GLITCH / DIRT / COLOUR / SPACE", 270, 26, 300, 20, juce::Justification::centredLeft);
    g.drawText ("PRESET", getWidth() - 300, 22, 70, 20, juce::Justification::centredRight);

    panel (g, rTune, "AUTOTUNE", col::pink);
    panel (g, rDist, "DISTORT", col::pink);
    panel (g, rCol, "COLOUR", col::lime);
    panel (g, rGlitch, "GLITCH SEQUENCER", col::cyan);
    panel (g, rFx, "EFFECTS + OUTPUT", col::cyan);

    g.setColour (col::muted);
    g.setFont (font (10.0f));
    g.drawText ("Click: next mode   Right-click: previous   Shift-click: clear",
                grid.getX(), grid.getBottom() + 8, 400, 16, juce::Justification::centredLeft);
}

void MadworldAudioProcessorEditor::resized()
{
    presetBox.setBounds (getWidth() - 222, 20, 206, 26);

    rTune   = { 12, 66, 372, 206 };
    rDist   = { rTune.getRight() + 10, 66, 220, 206 };
    rCol    = { rDist.getRight() + 10, 66, getWidth() - 12 - (rDist.getRight() + 10), 206 };
    rGlitch = { 12, rTune.getBottom() + 10, getWidth() - 24, 178 };
    rFx     = { 12, rGlitch.getBottom() + 10, getWidth() - 24, getHeight() - rGlitch.getBottom() - 22 };

    const int kw = 82, kh = 112;

    {   auto r = rTune.reduced (12).withTrimmedTop (24);
        auto right = r.removeFromRight (112);
        auto row = r.removeFromTop (kh);
        tuneAmt->setBounds (row.removeFromLeft (kw));
        tuneSpeed->setBounds (row.removeFromLeft (kw));
        tuneShift->setBounds (row.removeFromLeft (kw));
        noteLabel.setBounds (r.removeFromTop (40));
        tuneKey->setBounds (right.removeFromTop (46));
        right.removeFromTop (6);
        tuneScale->setBounds (right.removeFromTop (46)); }

    {   auto r = rDist.reduced (12).withTrimmedTop (24);
        dsMode->setBounds (r.removeFromTop (46));
        r.removeFromTop (6);
        auto row = r.removeFromTop (kh);
        dsDrive->setBounds (row.removeFromLeft (row.getWidth() / 2));
        dsMix->setBounds (row); }

    {   auto r = rCol.reduced (12).withTrimmedTop (24);
        colFilt->setBounds (r.removeFromTop (46).withWidth (140));
        r.removeFromTop (6);
        auto row = r.removeFromTop (kh);
        const int w = row.getWidth() / 4;
        colCut->setBounds (row.removeFromLeft (w));
        colRes->setBounds (row.removeFromLeft (w));
        colTilt->setBounds (row.removeFromLeft (w));
        colAir->setBounds (row); }

    {   auto r = rGlitch.reduced (12).withTrimmedTop (24);
        auto left = r.removeFromLeft (290);
        auto col1 = left.removeFromLeft (110);
        glOn.setBounds (col1.removeFromTop (30));
        col1.removeFromTop (8);
        glRate->setBounds (col1.removeFromTop (46));
        auto knobsRow = left.removeFromTop (kh);
        glMix->setBounds (knobsRow.removeFromLeft (kw));
        glProb->setBounds (knobsRow.removeFromLeft (kw));
        r.removeFromLeft (10);
        auto btns = r.removeFromBottom (30);
        clear.setBounds (btns.removeFromRight (80));
        btns.removeFromRight (8);
        chaos.setBounds (btns.removeFromRight (80));
        r.removeFromBottom (24);
        grid.setBounds (r); }

    {   auto r = rFx.reduced (12).withTrimmedTop (24);
        const int w = r.getWidth() / 10;
        fxChorus->setBounds (r.removeFromLeft (w));
        fxDlyTime->setBounds (r.removeFromLeft (w).reduced (4, 0).withHeight (46).translated (0, 20));
        fxDlyFb->setBounds (r.removeFromLeft (w));
        fxDlyMix->setBounds (r.removeFromLeft (w));
        fxRevSize->setBounds (r.removeFromLeft (w));
        fxRevMix->setBounds (r.removeFromLeft (w));
        fxWidth->setBounds (r.removeFromLeft (w));
        r.removeFromLeft (w / 2);
        outGain->setBounds (r.removeFromLeft (w));
        mix->setBounds (r.removeFromLeft (w)); }
}
