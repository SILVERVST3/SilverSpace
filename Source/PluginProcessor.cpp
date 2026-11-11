#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

AudioProcessorValueTreeState::ParameterLayout SilverSpaceProcessor::createLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto pct = [](float v, int) { return String(roundToInt(v * 100.0f)) + " %"; };
    auto hz  = [](float v, int) { return v >= 1000.0f ? String(v / 1000.0f, 1) + " kHz" : String(roundToInt(v)) + " Hz"; };
    auto ms  = [](float v, int) { return String(roundToInt(v)) + " ms"; };
    auto db  = [](float v, int) { return String(v, 1) + " dB"; };

    p.push_back(std::make_unique<AudioParameterFloat>(ParameterID{ "amount", 1 }, "Amount",
        NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.4f, AudioParameterFloatAttributes().withStringFromValueFunction(pct)));
    p.push_back(std::make_unique<AudioParameterFloat>(ParameterID{ "lowcut", 1 }, "Low-Cut",
        NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.3f), 20.0f, AudioParameterFloatAttributes().withStringFromValueFunction(hz)));
    p.push_back(std::make_unique<AudioParameterFloat>(ParameterID{ "highcut", 1 }, "High-Cut",
        NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.3f), 20000.0f, AudioParameterFloatAttributes().withStringFromValueFunction(hz)));
    p.push_back(std::make_unique<AudioParameterBool>(ParameterID{ "freeze", 1 }, "Freeze", false));
    p.push_back(std::make_unique<AudioParameterChoice>(ParameterID{ "mode", 1 }, "L/R - M/S", StringArray{ "L/R", "M/S" }, 0));
    p.push_back(std::make_unique<AudioParameterFloat>(ParameterID{ "pan", 1 }, "Pan",
        NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f, AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float v, int) { return v == 0.0f ? String("Centro") : String(roundToInt(v * 100.0f)); })));
    p.push_back(std::make_unique<AudioParameterFloat>(ParameterID{ "attack", 1 }, "Attack",
        NormalisableRange<float>(1.0f, 500.0f, 0.1f, 0.4f), 20.0f, AudioParameterFloatAttributes().withStringFromValueFunction(ms)));
    p.push_back(std::make_unique<AudioParameterFloat>(ParameterID{ "release", 1 }, "Release",
        NormalisableRange<float>(10.0f, 3000.0f, 1.0f, 0.4f), 200.0f, AudioParameterFloatAttributes().withStringFromValueFunction(ms)));
    p.push_back(std::make_unique<AudioParameterBool>(ParameterID{ "audition", 1 }, "SC Audition", false));
    p.push_back(std::make_unique<AudioParameterFloat>(ParameterID{ "sens", 1 }, "Sensitivity",
        NormalisableRange<float>(-100.0f, -30.0f, 0.1f), -70.0f, AudioParameterFloatAttributes().withStringFromValueFunction(db)));
    p.push_back(std::make_unique<AudioParameterFloat>(ParameterID{ "range", 1 }, "Range",
        NormalisableRange<float>(10.0f, 60.0f, 0.1f), 30.0f, AudioParameterFloatAttributes().withStringFromValueFunction(db)));
    p.push_back(std::make_unique<AudioParameterFloat>(ParameterID{ "mix", 1 }, "Mix",
        NormalisableRange<float>(0.0f, 1.0f, 0.001f), 1.0f, AudioParameterFloatAttributes().withStringFromValueFunction(pct)));
    p.push_back(std::make_unique<AudioParameterFloat>(ParameterID{ "output", 1 }, "Output",
        NormalisableRange<float>(-24.0f, 12.0f, 0.1f), 0.0f, AudioParameterFloatAttributes().withStringFromValueFunction(db)));
    return { p.begin(), p.end() };
}

SilverSpaceProcessor::SilverSpaceProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", AudioChannelSet::stereo(), true)
                         .withOutput("Output", AudioChannelSet::stereo(), true)
                         .withInput("Sidechain", AudioChannelSet::stereo(), false)),
      apvts(*this, nullptr, "SilverSpace", createLayout())
{
    pAmount = apvts.getRawParameterValue("amount");   pLow = apvts.getRawParameterValue("lowcut");
    pHigh = apvts.getRawParameterValue("highcut");    pFreeze = apvts.getRawParameterValue("freeze");
    pMode = apvts.getRawParameterValue("mode");       pPan = apvts.getRawParameterValue("pan");
    pAttack = apvts.getRawParameterValue("attack");   pRelease = apvts.getRawParameterValue("release");
    pAudition = apvts.getRawParameterValue("audition"); pSens = apvts.getRawParameterValue("sens");
    pRange = apvts.getRawParameterValue("range");     pMix = apvts.getRawParameterValue("mix");
    pOutput = apvts.getRawParameterValue("output");

    for (auto& a : scLevelsDb) a.store(-120.0f);
    for (auto& a : reductionDb) a.store(0.0f);
    setLatencySamples(silverspace::SpaceEngine::latency());
}

bool SilverSpaceProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainInputChannelSet() != AudioChannelSet::stereo()
        || layouts.getMainOutputChannelSet() != AudioChannelSet::stereo())
        return false;
    const auto sc = layouts.getChannelSet(true, 1);
    return sc.isDisabled() || sc == AudioChannelSet::stereo() || sc == AudioChannelSet::mono();
}

void SilverSpaceProcessor::prepareToPlay(double sampleRate, int)
{
    engine.prepare(sampleRate);
    setLatencySamples(silverspace::SpaceEngine::latency());
}

void SilverSpaceProcessor::processBlock(AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();

    auto mainBuf = getBusBuffer(buffer, true, 0);
    if (mainBuf.getNumChannels() < 2 || n == 0) return;

    const float* sc0 = nullptr;
    const float* sc1 = nullptr;
    bool scOn = false;
    if (auto* scBus = getBus(true, 1))
    {
        if (scBus->isEnabled())
        {
            auto scBuf = getBusBuffer(buffer, true, 1);
            if (scBuf.getNumChannels() > 0)
            {
                sc0 = scBuf.getReadPointer(0);
                sc1 = scBuf.getNumChannels() > 1 ? scBuf.getReadPointer(1) : sc0;
                scOn = true;
            }
        }
    }
    sidechainConnected.store(scOn);

    silverspace::Params p;
    p.amount = pAmount->load();      p.lowCutHz = pLow->load();     p.highCutHz = pHigh->load();
    p.freeze = pFreeze->load() > 0.5f; p.midSide = pMode->load() > 0.5f; p.pan = pPan->load();
    p.attackMs = pAttack->load();    p.releaseMs = pRelease->load();
    p.scAudition = pAudition->load() > 0.5f;
    p.sensitivityDb = pSens->load(); p.rangeDb = pRange->load();
    p.mix = pMix->load();            p.outputDb = pOutput->load();

    engine.process(mainBuf.getWritePointer(0), mainBuf.getWritePointer(1), sc0, sc1, n, p);

    const auto& lv = engine.bandLevelsDb();
    const auto& rd = engine.bandReductionDb();
    for (int b = 0; b < silverspace::kNumBands; ++b)
    {
        scLevelsDb[(size_t) b].store(lv[(size_t) b]);
        reductionDb[(size_t) b].store(rd[(size_t) b]);
    }
}

AudioProcessorEditor* SilverSpaceProcessor::createEditor() { return new SilverSpaceEditor(*this); }

void SilverSpaceProcessor::getStateInformation(MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void SilverSpaceProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(ValueTree::fromXml(*xml));
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SilverSpaceProcessor(); }
