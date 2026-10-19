#pragma once
#include <JuceHeader.h>
#include "Dsp.h"
#include "SharedBus.h"

class NeonEQProcessor : public juce::AudioProcessor,
                        private juce::Timer
{
public:
    NeonEQProcessor();
    ~NeonEQProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Neon EQ 6"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    void updateTrackProperties (const TrackProperties& properties) override;

    // ---- track names / linking (message thread) ----
    juce::String getTrackName() const;
    juce::String getLinkedName() const;
    std::vector<std::shared_ptr<SharedEntry>> getOtherInstances();
    void setLink (std::shared_ptr<SharedEntry> e);

    // ---- shared with the editor ----
    juce::AudioProcessorValueTreeState apvts;
    AudioRing<16384> scope;                       // post-EQ mono signal for the analyzer
    std::array<std::atomic<float>, 6> dynDb;      // current dynamic gain per band
    std::atomic<int> editorW { 980 }, editorH { 660 };
    std::atomic<double> sampleRateAtomic { 44100.0 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void timerCallback() override;
    void setTrackName (const juce::String& n);
    void refreshParams();
    void updateCoefficients (int chunk);

    static constexpr int kChunk = 512;
    static constexpr int kSub = 16;

    using SmoothLin = juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>;
    using SmoothMul = juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative>;

    struct BandParams
    {
        std::atomic<float> *on, *type, *freq, *gain, *q, *slope, *dyn, *src, *thr, *range, *atk, *rel, *mode;
    };

    struct BandState
    {
        SmoothMul freq, q;
        SmoothLin gain;
        Coeffs c[4];
        int nst = 1;
        double z1[2][4] {}, z2[2][4] {};
        Coeffs det;
        double dz1 = 0, dz2 = 0, env = 0;
        bool on = false, dyn = false;
        int type = 0, slope = 0, src = 0, mode = 0;
        float thr = -30.f, range = -6.f;
        double atkC = 0, relC = 0;
        bool dynActive() const { return on && dyn && type <= 2; }
    };

    std::array<BandParams, 6> bp {};
    std::array<BandState, 6> bands;
    SmoothLin outGain;
    std::atomic<float>* outParam = nullptr;
    double fs = 44100.0;
    int subCounter = 0;

    float keySelf[kChunk] {}, keySC[kChunk] {}, keyLink[kChunk] {}, tmpOut[kChunk] {};

    // sharing
    std::shared_ptr<SharedEntry> shared;
    std::shared_ptr<SharedEntry> linked;
    juce::SpinLock linkLock;
    mutable juce::CriticalSection metaLock;
    juce::String trackName, linkedName;
    int instanceId = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NeonEQProcessor)
};
