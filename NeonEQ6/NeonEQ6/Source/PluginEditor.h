#pragma once
#include "Ui.h"
#include "Graph.h"
#include "PluginProcessor.h"

class NeonEQEditor : public juce::AudioProcessorEditor,
                     private juce::Timer
{
public:
    explicit NeonEQEditor (NeonEQProcessor&);
    ~NeonEQEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void selectBand (int b);
    void showLinkMenu();
    juce::RangedAudioParameter* P (const char* name) { return proc.apvts.getParameter (name); }
    juce::RangedAudioParameter* BP (const char* name) { return proc.apvts.getParameter (bid (selected, name)); }
    float raw (const char* name) { return proc.apvts.getRawParameterValue (bid (selected, name))->load(); }

    NeonEQProcessor& proc;
    Neon::NeonLookAndFeel laf;
    GraphView graph;
    int selected = 0;
    juce::String lastTrackName;
    juce::Rectangle<int> trackRect;

    std::array<PillButton, 6> chips;
    PillButton linkBtn;

    SegmentedChoice segOn    { juce::StringArray { "OFF", "ON" } };
    SegmentedChoice segType  { juce::StringArray { "BELL", "LO SHELF", "HI SHELF", "LO CUT", "HI CUT" } };
    SegmentedChoice segSlope { juce::StringArray { "12", "24", "48" } };
    SegmentedChoice segDyn   { juce::StringArray { "DYN OFF", "DYN ON" } };
    SegmentedChoice segSrc   { juce::StringArray { "SELF", "SIDECHAIN", "LINK" } };
    SegmentedChoice segMode  { juce::StringArray { "ABOVE", "BELOW" } };
    SegmentedChoice segSpeed { juce::StringArray { "FAST", "NORMAL", "SLOW" } };
    SegmentedChoice segAn    { juce::StringArray { "OFF", "ON" } };

    MiniFader fFreq { "FREQUENCY" }, fGain { "GAIN" }, fQ { "Q" };
    MiniFader fThr { "THRESHOLD" }, fRange { "RANGE" }, fAtk { "ATTACK" }, fRel { "RELEASE" };
    MiniFader fOut { "OUTPUT" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NeonEQEditor)
};
