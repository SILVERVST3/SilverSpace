#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <array>
#include "SpaceEngine.h"

class SilverSpaceProcessor : public juce::AudioProcessor
{
public:
    SilverSpaceProcessor();
    ~SilverSpaceProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "SilverSpace"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    // dados para o display (escritos no audio thread, lidos pela interface)
    std::array<std::atomic<float>, silverspace::kNumBands> scLevelsDb;
    std::array<std::atomic<float>, silverspace::kNumBands> reductionDb;
    std::atomic<bool> sidechainConnected { false };

private:
    silverspace::SpaceEngine engine;

    std::atomic<float>* pAmount = nullptr; std::atomic<float>* pLow = nullptr;
    std::atomic<float>* pHigh = nullptr;   std::atomic<float>* pFreeze = nullptr;
    std::atomic<float>* pMode = nullptr;   std::atomic<float>* pPan = nullptr;
    std::atomic<float>* pAttack = nullptr; std::atomic<float>* pRelease = nullptr;
    std::atomic<float>* pAudition = nullptr; std::atomic<float>* pSens = nullptr;
    std::atomic<float>* pRange = nullptr;  std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pOutput = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SilverSpaceProcessor)
};
