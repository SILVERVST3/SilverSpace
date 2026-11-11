#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

namespace silvercolours
{
    const juce::Colour navy      { 0xff14213d };
    const juce::Colour navySoft  { 0xff3a4a6b };
    const juce::Colour accent    { 0xff2f6fe0 };
    const juce::Colour panelTop  { 0xfff6f8fb };
    const juce::Colour panelBot  { 0xffdfe3ea };
    const juce::Colour bgTop     { 0xffeef0f4 };
    const juce::Colour bgBot     { 0xffc6ccd6 };
    const juce::Colour grid      { 0xffb4bbc7 };
}

class SilverLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SilverLookAndFeel();
    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h, float pos,
                          float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    juce::Font getLabelFont(juce::Label&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
};

// Display: espectro do sidechain (azul) + curva de reducao (navy), Low/High-Cut arrastaveis
class SpectrumView : public juce::Component, private juce::Timer
{
public:
    explicit SpectrumView(SilverSpaceProcessor&);
    ~SpectrumView() override { stopTimer(); }

    void paint(juce::Graphics&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;

    std::function<void()> onAdvancedClicked;

private:
    void timerCallback() override { repaint(); }
    juce::Rectangle<float> plotArea() const;
    float freqToX(float f) const;
    float xToFreq(float x) const;
    float levelToY(float db) const;
    juce::Point<float> advancedDotCentre() const;
    void setCutParam(const char* id, float hz);

    SilverSpaceProcessor& proc;
    juce::Point<float> hover { -1.0f, -1.0f };
    bool hovering = false;
    enum class Drag { None, Low, High } drag = Drag::None;
    juce::RangedAudioParameter* dragParam = nullptr;
};

class SilverSpaceEditor : public juce::AudioProcessorEditor
{
public:
    explicit SilverSpaceEditor(SilverSpaceProcessor&);
    ~SilverSpaceEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
    };

    void setupKnob(Knob&, const juce::String& name, const juce::String& paramId);
    static void placeKnob(Knob&, juce::Rectangle<int> area);
    void toggleAdvanced();

    SilverSpaceProcessor& proc;
    SilverLookAndFeel lnf;
    SpectrumView view;

    Knob amount, lowCut, highCut, pan, attack, release, sens, range, mix, output;
    juce::ToggleButton freeze { "Freeze" }, audition { "SC Audition" };
    juce::ComboBox mode;
    juce::Label modeLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeAtt, auditionAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAtt;

    bool advancedOpen = false;
    static constexpr int advancedHeight = 176;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SilverSpaceEditor)
};
