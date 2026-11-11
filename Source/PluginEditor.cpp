#include "PluginEditor.h"

using namespace juce;
using namespace silvercolours;

static Font makeFont(float h, bool bold = false)
{
    return Font(FontOptions(h, bold ? Font::bold : Font::plain));
}

// ====================================================================== LookAndFeel
SilverLookAndFeel::SilverLookAndFeel()
{
    setColour(Slider::textBoxTextColourId, navy);
    setColour(Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour(Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    setColour(Slider::textBoxHighlightColourId, accent.withAlpha(0.35f));
    setColour(Label::textColourId, navy);
    setColour(ComboBox::backgroundColourId, panelTop);
    setColour(ComboBox::textColourId, navy);
    setColour(ComboBox::outlineColourId, grid);
    setColour(ComboBox::arrowColourId, navy);
    setColour(PopupMenu::backgroundColourId, panelTop);
    setColour(PopupMenu::textColourId, navy);
    setColour(PopupMenu::highlightedBackgroundColourId, accent);
    setColour(PopupMenu::highlightedTextColourId, Colours::white);
}

Font SilverLookAndFeel::getLabelFont(Label&) { return makeFont(13.0f); }
Font SilverLookAndFeel::getComboBoxFont(ComboBox&) { return makeFont(13.0f); }
Font SilverLookAndFeel::getPopupMenuFont() { return makeFont(14.0f); }

void SilverLookAndFeel::drawRotarySlider(Graphics& g, int x, int y, int w, int h, float pos,
                                         float startAngle, float endAngle, Slider&)
{
    auto bounds = Rectangle<float>((float) x, (float) y, (float) w, (float) h).reduced(4.0f);
    const float d = jmin(bounds.getWidth(), bounds.getHeight());
    auto c = bounds.withSizeKeepingCentre(d, d);
    const auto centre = c.getCentre();
    const float radius = d * 0.5f;
    const float angle = startAngle + pos * (endAngle - startAngle);

    // trilho + arco de valor
    const float trackR = radius - 2.0f;
    Path track; track.addCentredArc(centre.x, centre.y, trackR, trackR, 0.0f, startAngle, endAngle, true);
    g.setColour(grid);
    g.strokePath(track, PathStrokeType(3.0f, PathStrokeType::curved, PathStrokeType::rounded));
    Path val; val.addCentredArc(centre.x, centre.y, trackR, trackR, 0.0f, startAngle, angle, true);
    g.setColour(accent);
    g.strokePath(val, PathStrokeType(3.0f, PathStrokeType::curved, PathStrokeType::rounded));

    // corpo prateado
    auto body = c.reduced(radius * 0.22f);
    g.setColour(Colours::black.withAlpha(0.18f));
    g.fillEllipse(body.translated(0.0f, 1.5f));
    g.setGradientFill(ColourGradient(Colour(0xfffbfcfe), body.getX(), body.getY(),
                                     Colour(0xffa9b0bc), body.getRight(), body.getBottom(), false));
    g.fillEllipse(body);
    g.setColour(Colour(0xff8a92a0));
    g.drawEllipse(body, 1.0f);

    // ponteiro
    Path p;
    const float len = body.getWidth() * 0.5f;
    p.addRoundedRectangle(-1.8f, -len + 3.0f, 3.6f, len * 0.5f, 1.8f);
    g.setColour(navy);
    g.fillPath(p, AffineTransform::rotation(angle).translated(centre.x, centre.y));
}

void SilverLookAndFeel::drawToggleButton(Graphics& g, ToggleButton& b, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced(1.5f);
    const bool on = b.getToggleState();
    if (on)
        g.setGradientFill(ColourGradient(accent.brighter(0.2f), r.getX(), r.getY(), accent.darker(0.3f), r.getX(), r.getBottom(), false));
    else
        g.setGradientFill(ColourGradient(panelTop, r.getX(), r.getY(), panelBot, r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle(r, r.getHeight() * 0.5f);
    g.setColour(on ? accent.darker(0.5f) : Colour(0xff8a92a0));
    g.drawRoundedRectangle(r, r.getHeight() * 0.5f, 1.0f);
    if (highlighted || down) { g.setColour(Colours::white.withAlpha(down ? 0.25f : 0.15f)); g.fillRoundedRectangle(r, r.getHeight() * 0.5f); }
    g.setColour(on ? Colours::white : navy);
    g.setFont(makeFont(13.0f, true));
    g.drawText(b.getButtonText(), r, Justification::centred);
}

// ====================================================================== SpectrumView
SpectrumView::SpectrumView(SilverSpaceProcessor& p) : proc(p) { startTimerHz(30); }

Rectangle<float> SpectrumView::plotArea() const { return getLocalBounds().toFloat().reduced(1.0f).withTrimmedLeft(34).withTrimmedRight(40).withTrimmedTop(12).withTrimmedBottom(20); }

float SpectrumView::freqToX(float f) const
{
    auto a = plotArea();
    const float t = std::log(jlimit(20.0f, 20000.0f, f) / 20.0f) / std::log(1000.0f);
    return a.getX() + t * a.getWidth();
}
float SpectrumView::xToFreq(float x) const
{
    auto a = plotArea();
    const float t = jlimit(0.0f, 1.0f, (x - a.getX()) / a.getWidth());
    return 20.0f * std::pow(1000.0f, t);
}
float SpectrumView::levelToY(float db) const
{
    auto a = plotArea();
    const float t = jlimit(0.0f, 1.0f, (db + 100.0f) / 80.0f); // -100 .. -20 dB
    return a.getBottom() - t * a.getHeight();
}
Point<float> SpectrumView::advancedDotCentre() const { return { (float) getWidth() - 16.0f, 16.0f }; }

void SpectrumView::paint(Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced(1.0f);
    g.setGradientFill(ColourGradient(Colour(0xfffdfdff), 0, b.getY(), Colour(0xffe4e8ee), 0, b.getBottom(), false));
    g.fillRoundedRectangle(b, 8.0f);
    g.setColour(Colour(0xff8a92a0));
    g.drawRoundedRectangle(b, 8.0f, 1.0f);

    auto a = plotArea();
    g.setFont(makeFont(11.0f));

    // grade vertical (frequencia)
    for (float f : { 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f })
    {
        const float x = freqToX(f);
        g.setColour(grid.withAlpha(0.55f));
        g.drawVerticalLine((int) x, a.getY(), a.getBottom());
        g.setColour(navySoft);
        g.drawText(f >= 1000.0f ? String((int) (f / 1000.0f)) + "k" : String((int) f),
                   Rectangle<float>(x - 20, a.getBottom() + 3, 40, 14), Justification::centred);
    }
    // grade horizontal (nivel do sidechain, esquerda)
    for (int db = -80; db <= -20; db += 20)
    {
        const float y = levelToY((float) db);
        g.setColour(grid.withAlpha(0.55f));
        g.drawHorizontalLine((int) y, a.getX(), a.getRight());
        g.setColour(navySoft);
        g.drawText(String(db), Rectangle<float>(1, y - 7, 30, 14), Justification::centredRight);
    }
    // escala da reducao (direita)
    for (int r = 0; r >= -30; r -= 10)
    {
        const float y = a.getY() + ((float) -r / 30.0f) * a.getHeight() * 0.8f;
        g.setColour(navy.withAlpha(0.8f));
        g.drawText(String(r), Rectangle<float>(a.getRight() + 4, y - 7, 34, 14), Justification::centredLeft);
    }

    // pontos das bandas
    std::array<juce::Point<float>, silverspace::kNumBands> sc, rd;
    for (int i = 0; i < silverspace::kNumBands; ++i)
    {
        const float fc = 20.0f * std::pow(1000.0f, ((float) i + 0.5f) / (float) silverspace::kNumBands);
        const float x = freqToX(fc);
        sc[(size_t) i] = { x, levelToY(proc.scLevelsDb[(size_t) i].load()) };
        rd[(size_t) i] = { x, a.getY() + (-proc.reductionDb[(size_t) i].load() / 30.0f) * a.getHeight() * 0.8f };
    }
    auto smoothPath = [&](const std::array<juce::Point<float>, silverspace::kNumBands>& pts, Path& path)
    {
        path.startNewSubPath(pts[0]);
        for (size_t i = 0; i + 1 < pts.size(); ++i)
            path.quadraticTo(pts[i], (pts[i] + pts[i + 1]) * 0.5f);
        path.lineTo(pts.back());
    };

    {   // sidechain (azul)
        Path line; smoothPath(sc, line);
        Path fill(line);
        fill.lineTo(sc.back().x, a.getBottom()); fill.lineTo(sc.front().x, a.getBottom()); fill.closeSubPath();
        g.saveState(); g.reduceClipRegion(a.toNearestInt());
        g.setGradientFill(ColourGradient(accent.withAlpha(0.45f), 0, a.getY(), accent.withAlpha(0.08f), 0, a.getBottom(), false));
        g.fillPath(fill);
        g.setColour(accent);
        g.strokePath(line, PathStrokeType(1.6f));
        g.restoreState();
    }
    {   // reducao (navy), pendurada do topo
        Path line; smoothPath(rd, line);
        Path fill(line);
        fill.lineTo(rd.back().x, a.getY()); fill.lineTo(rd.front().x, a.getY()); fill.closeSubPath();
        g.saveState(); g.reduceClipRegion(a.toNearestInt());
        g.setColour(navy.withAlpha(0.16f)); g.fillPath(fill);
        g.setColour(navy); g.strokePath(line, PathStrokeType(2.0f));
        g.restoreState();
    }

    // Low / High cut
    const float low = proc.apvts.getRawParameterValue("lowcut")->load();
    const float high = proc.apvts.getRawParameterValue("highcut")->load();
    const float xl = freqToX(low), xh = freqToX(high);
    g.setColour(Colour(0xff8a92a0).withAlpha(0.35f));
    if (xl > a.getX()) g.fillRect(a.getX(), a.getY(), xl - a.getX(), a.getHeight());
    if (xh < a.getRight()) g.fillRect(xh, a.getY(), a.getRight() - xh, a.getHeight());
    g.setColour(navy.withAlpha(0.7f));
    g.drawVerticalLine((int) xl, a.getY(), a.getBottom());
    g.drawVerticalLine((int) xh, a.getY(), a.getBottom());
    for (float x : { xl, xh })
    {
        Path t; t.addTriangle(x - 5, a.getBottom(), x + 5, a.getBottom(), x, a.getBottom() - 8);
        g.fillPath(t);
    }

    // estado
    g.setFont(makeFont(11.0f, true));
    g.setColour(navySoft);
    g.drawText(proc.sidechainConnected.load() ? "Sidechain externo" : "Sidechain interno (a propria faixa)",
               Rectangle<float>(a.getX() + 6, a.getY() + 2, 260, 14), Justification::centredLeft);
    if (proc.apvts.getRawParameterValue("freeze")->load() > 0.5f)
    {
        g.setColour(accent);
        g.drawText("FREEZE", Rectangle<float>(a.getX() + 6, a.getY() + 16, 100, 14), Justification::centredLeft);
    }

    // ponto "avancado"
    const auto dc = advancedDotCentre();
    g.setColour(accent); g.fillEllipse(dc.x - 6, dc.y - 6, 12, 12);
    g.setColour(Colours::white.withAlpha(0.6f)); g.drawEllipse(dc.x - 6, dc.y - 6, 12, 12, 1.0f);

    // leitura ao passar o mouse
    if (hovering && a.contains(hover))
    {
        const float f = xToFreq(hover.x);
        int band = jlimit(0, silverspace::kNumBands - 1,
                          (int) std::floor(std::log(f / 20.0f) / std::log(1000.0f) * (float) silverspace::kNumBands));
        const String txt = (f >= 1000.0f ? String(f / 1000.0f, 2) + " kHz" : String(f, 0) + " Hz")
                         + "   SC " + String(proc.scLevelsDb[(size_t) band].load(), 1) + " dB"
                         + "   Red " + String(proc.reductionDb[(size_t) band].load(), 1) + " dB";
        g.setFont(makeFont(12.0f, true));
        GlyphArrangement ga; ga.addLineOfText(g.getCurrentFont(), txt, 0.0f, 0.0f);
        const int tw = (int) ga.getBoundingBox(0, -1, true).getWidth() + 14;
        auto box = Rectangle<float>((float) tw, 20.0f).withCentre({ jlimit(a.getX() + tw * 0.5f, a.getRight() - tw * 0.5f, hover.x),
                                                                    jmax(a.getY() + 30.0f, hover.y - 20.0f) });
        g.setColour(Colour(0xf0ffffff)); g.fillRoundedRectangle(box, 4.0f);
        g.setColour(grid); g.drawRoundedRectangle(box, 4.0f, 1.0f);
        g.setColour(navy); g.drawText(txt, box, Justification::centred);
        g.setColour(navy.withAlpha(0.35f)); g.drawVerticalLine((int) hover.x, a.getY(), a.getBottom());
    }
}

void SpectrumView::mouseMove(const MouseEvent& e) { hover = e.position; hovering = true; }
void SpectrumView::mouseExit(const MouseEvent&) { hovering = false; }

void SpectrumView::mouseDown(const MouseEvent& e)
{
    if (e.position.getDistanceFrom(advancedDotCentre()) < 10.0f) { if (onAdvancedClicked) onAdvancedClicked(); return; }
    const float xl = freqToX(proc.apvts.getRawParameterValue("lowcut")->load());
    const float xh = freqToX(proc.apvts.getRawParameterValue("highcut")->load());
    const float dl = std::abs(e.position.x - xl), dh = std::abs(e.position.x - xh);
    drag = Drag::None;
    if (jmin(dl, dh) < 9.0f) drag = dl <= dh ? Drag::Low : Drag::High;
    if (drag != Drag::None)
    {
        dragParam = proc.apvts.getParameter(drag == Drag::Low ? "lowcut" : "highcut");
        dragParam->beginChangeGesture();
    }
}

void SpectrumView::mouseDrag(const MouseEvent& e)
{
    hover = e.position;
    if (drag == Drag::None || dragParam == nullptr) return;
    float f = xToFreq(e.position.x);
    const float low = proc.apvts.getRawParameterValue("lowcut")->load();
    const float high = proc.apvts.getRawParameterValue("highcut")->load();
    f = drag == Drag::Low ? jmin(f, high * 0.8f) : jmax(f, low * 1.25f);
    dragParam->setValueNotifyingHost(dragParam->convertTo0to1(jlimit(20.0f, 20000.0f, f)));
}

void SpectrumView::mouseUp(const MouseEvent&)
{
    if (dragParam != nullptr) dragParam->endChangeGesture();
    dragParam = nullptr; drag = Drag::None;
}

void SpectrumView::mouseDoubleClick(const MouseEvent& e)
{
    const float xl = freqToX(proc.apvts.getRawParameterValue("lowcut")->load());
    const float xh = freqToX(proc.apvts.getRawParameterValue("highcut")->load());
    const bool nearLow = std::abs(e.position.x - xl) < 9.0f, nearHigh = std::abs(e.position.x - xh) < 9.0f;
    if (nearLow || nearHigh)
    {
        auto* prm = proc.apvts.getParameter(nearLow ? "lowcut" : "highcut");
        prm->beginChangeGesture(); prm->setValueNotifyingHost(nearLow ? 0.0f : 1.0f); prm->endChangeGesture();
    }
}

// ====================================================================== Editor
SilverSpaceEditor::SilverSpaceEditor(SilverSpaceProcessor& p)
    : AudioProcessorEditor(&p), proc(p), view(p)
{
    setLookAndFeel(&lnf);
    addAndMakeVisible(view);
    view.onAdvancedClicked = [this] { toggleAdvanced(); };

    setupKnob(amount, "Amount", "amount");
    setupKnob(lowCut, "Low-Cut", "lowcut");
    setupKnob(highCut, "High-Cut", "highcut");
    setupKnob(pan, "Pan", "pan");
    setupKnob(attack, "Attack", "attack");
    setupKnob(release, "Release", "release");
    setupKnob(sens, "Sensitivity", "sens");
    setupKnob(range, "Range", "range");
    setupKnob(mix, "Mix", "mix");
    setupKnob(output, "Output", "output");

    addAndMakeVisible(freeze);
    freezeAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "freeze", freeze);
    addChildComponent(audition);
    auditionAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "audition", audition);

    mode.addItem("L/R", 1);
    mode.addItem("M/S", 2);
    addChildComponent(mode);
    modeAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(proc.apvts, "mode", mode);
    modeLabel.setText("Modo", dontSendNotification);
    modeLabel.setJustificationType(Justification::centredRight);
    addChildComponent(modeLabel);

    for (auto* k : { &pan, &attack, &release, &sens, &range, &mix, &output })
    { k->slider.setVisible(false); k->label.setVisible(false); }

    setResizable(true, true);
    setResizeLimits(560, 480, 1200, 1000);
    setSize(650, 540);
}

SilverSpaceEditor::~SilverSpaceEditor() { setLookAndFeel(nullptr); }

void SilverSpaceEditor::setupKnob(Knob& k, const String& name, const String& id)
{
    k.slider.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle(Slider::TextBoxBelow, false, 84, 18);
    k.slider.setRotaryParameters(MathConstants<float>::pi * 1.2f, MathConstants<float>::pi * 2.8f, true);
    k.slider.setDoubleClickReturnValue(true, proc.apvts.getParameter(id)->convertFrom0to1(proc.apvts.getParameter(id)->getDefaultValue()));
    addAndMakeVisible(k.slider);
    k.label.setText(name, dontSendNotification);
    k.label.setJustificationType(Justification::centred);
    k.label.setFont(makeFont(13.0f, true));
    addAndMakeVisible(k.label);
    k.attach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, id, k.slider);
}

void SilverSpaceEditor::placeKnob(Knob& k, Rectangle<int> area)
{
    k.label.setBounds(area.removeFromTop(18));
    k.slider.setBounds(area);
}

void SilverSpaceEditor::toggleAdvanced()
{
    advancedOpen = !advancedOpen;
    for (auto* k : { &pan, &attack, &release, &sens, &range, &mix, &output })
    { k->slider.setVisible(advancedOpen); k->label.setVisible(advancedOpen); }
    audition.setVisible(advancedOpen);
    mode.setVisible(advancedOpen);
    modeLabel.setVisible(advancedOpen);
    setSize(getWidth(), getHeight() + (advancedOpen ? advancedHeight : -advancedHeight));
}

void SilverSpaceEditor::paint(Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setGradientFill(ColourGradient(bgTop, 0, 0, bgBot, 0, b.getBottom(), false));
    g.fillAll();
    g.setColour(Colour(0xff8a92a0)); g.drawRect(b, 1.0f);

    g.setColour(navy);
    g.setFont(makeFont(24.0f, true));
    g.drawText("SilverSpace", Rectangle<int>(18, 8, 220, 32), Justification::centredLeft);
    g.setColour(navySoft);
    g.setFont(makeFont(12.0f));
    g.drawText("carve space for the sidechain  |  32-band spectral ducker", Rectangle<int>(170, 14, getWidth() - 190, 24), Justification::centredRight);
}

void SilverSpaceEditor::resized()
{
    auto r = getLocalBounds().reduced(14);
    r.removeFromTop(36);

    auto adv = advancedOpen ? r.removeFromBottom(advancedHeight) : Rectangle<int>();
    auto mainRow = r.removeFromBottom(170);
    view.setBounds(r.withTrimmedBottom(6));

    // linha principal: Amount | Low-Cut | High-Cut | Freeze
    const int w = mainRow.getWidth();
    placeKnob(amount, mainRow.removeFromLeft(w * 30 / 100));
    placeKnob(lowCut, mainRow.removeFromLeft(w * 22 / 100));
    placeKnob(highCut, mainRow.removeFromLeft(w * 22 / 100));
    freeze.setBounds(mainRow.withSizeKeepingCentre(juce::jmin(mainRow.getWidth() - 8, 110), 30));

    if (advancedOpen)
    {
        auto top = adv.removeFromTop(34);
        modeLabel.setBounds(top.removeFromLeft(50));
        mode.setBounds(top.removeFromLeft(80).reduced(4, 4));
        top.removeFromLeft(16);
        audition.setBounds(top.removeFromLeft(120).reduced(2, 3));

        const int cw = adv.getWidth() / 7;
        for (auto* k : { &pan, &attack, &release, &sens, &range, &mix, &output })
            placeKnob(*k, adv.removeFromLeft(cw));
    }
}
