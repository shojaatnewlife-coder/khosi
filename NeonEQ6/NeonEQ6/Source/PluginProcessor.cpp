#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    juce::String fmtFreq (float v, int)
    {
        if (v >= 1000.f) return juce::String (v / 1000.f, 2) + " kHz";
        if (v < 100.f)   return juce::String (v, 1) + " Hz";
        return juce::String (juce::roundToInt (v)) + " Hz";
    }
    juce::String fmtDb (float v, int)  { return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " dB"; }
    juce::String fmtQ (float v, int)   { return juce::String (v, 2); }
    juce::String fmtMs (float v, int)  { return juce::String (v, v < 10.f ? 1 : 0) + " ms"; }

    float parseNum (const juce::String& t)
    {
        auto s = t.trim().toLowerCase();
        float v = s.getFloatValue();
        if (s.contains ("k")) v *= 1000.f;
        return v;
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout NeonEQProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto addFloat = [&] (const String& id, const String& name, NormalisableRange<float> r, float def,
                         std::function<String (float, int)> toStr)
    {
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { id, 1 }, name, r, def,
            AudioParameterFloatAttributes().withStringFromValueFunction (toStr).withValueFromStringFunction (parseNum)));
    };

    const float defFreq[6] = { 60.f, 180.f, 500.f, 1500.f, 4500.f, 12000.f };

    for (int b = 0; b < 6; ++b)
    {
        const String n = "Band " + String (b + 1) + " ";

        layout.add (std::make_unique<AudioParameterBool> (ParameterID { bid (b, "on"), 1 }, n + "On", true));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { bid (b, "type"), 1 }, n + "Type",
                    StringArray { "Bell", "Low Shelf", "High Shelf", "Low Cut", "High Cut" }, 0));

        NormalisableRange<float> fr (20.f, 20000.f, 0.01f);
        fr.setSkewForCentre (1000.f);
        addFloat (bid (b, "freq"), n + "Freq", fr, defFreq[b], fmtFreq);

        addFloat (bid (b, "gain"), n + "Gain", { -18.f, 18.f, 0.01f }, 0.f, fmtDb);

        NormalisableRange<float> qr (0.1f, 18.f, 0.01f);
        qr.setSkewForCentre (1.f);
        addFloat (bid (b, "q"), n + "Q", qr, 1.0f, fmtQ);

        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { bid (b, "slope"), 1 }, n + "Slope",
                    StringArray { "12", "24", "48" }, 1));

        layout.add (std::make_unique<AudioParameterBool> (ParameterID { bid (b, "dyn"), 1 }, n + "Dynamic", false));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { bid (b, "src"), 1 }, n + "Source",
                    StringArray { "Self", "Sidechain", "Link" }, 0));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { bid (b, "mode"), 1 }, n + "Mode",
                    StringArray { "Above", "Below" }, 0));

        addFloat (bid (b, "thr"), n + "Threshold", { -60.f, 0.f, 0.1f }, -30.f, fmtDb);
        addFloat (bid (b, "range"), n + "Range", { -18.f, 18.f, 0.1f }, -6.f, fmtDb);

        NormalisableRange<float> ar (0.1f, 200.f, 0.1f);
        ar.setSkewForCentre (10.f);
        addFloat (bid (b, "atk"), n + "Attack", ar, 10.f, fmtMs);

        NormalisableRange<float> rr (5.f, 2000.f, 0.1f);
        rr.setSkewForCentre (120.f);
        addFloat (bid (b, "rel"), n + "Release", rr, 120.f, fmtMs);
    }

    addFloat ("out_gain", "Output", { -18.f, 18.f, 0.1f }, 0.f, fmtDb);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "an_speed", 1 }, "Analyzer Speed",
                StringArray { "Fast", "Normal", "Slow" }, 1));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "an_on", 1 }, "Analyzer", true));

    return layout;
}

NeonEQProcessor::NeonEQProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    static std::atomic<int> counter { 0 };
    instanceId = ++counter;

    shared = std::make_shared<SharedEntry>();
    shared->setName ("Track " + juce::String (instanceId));
    Registry::get().add (shared);

    for (int b = 0; b < 6; ++b)
    {
        auto& p = bp[(size_t) b];
        p.on    = apvts.getRawParameterValue (bid (b, "on"));
        p.type  = apvts.getRawParameterValue (bid (b, "type"));
        p.freq  = apvts.getRawParameterValue (bid (b, "freq"));
        p.gain  = apvts.getRawParameterValue (bid (b, "gain"));
        p.q     = apvts.getRawParameterValue (bid (b, "q"));
        p.slope = apvts.getRawParameterValue (bid (b, "slope"));
        p.dyn   = apvts.getRawParameterValue (bid (b, "dyn"));
        p.src   = apvts.getRawParameterValue (bid (b, "src"));
        p.thr   = apvts.getRawParameterValue (bid (b, "thr"));
        p.range = apvts.getRawParameterValue (bid (b, "range"));
        p.atk   = apvts.getRawParameterValue (bid (b, "atk"));
        p.rel   = apvts.getRawParameterValue (bid (b, "rel"));
        p.mode  = apvts.getRawParameterValue (bid (b, "mode"));
        dynDb[(size_t) b].store (0.f);
    }
    outParam = apvts.getRawParameterValue ("out_gain");

    startTimer (500);
}

NeonEQProcessor::~NeonEQProcessor()
{
    stopTimer();
    shared->alive.store (false);
    Registry::get().remove (shared.get());
}

// ---------------------------------------------------------------- names / link

void NeonEQProcessor::setTrackName (const juce::String& n)
{
    {
        juce::ScopedLock l (metaLock);
        trackName = n;
    }
    shared->setName (n.isNotEmpty() ? n : "Track " + juce::String (instanceId));
}

void NeonEQProcessor::updateTrackProperties (const TrackProperties& p)
{
    if (p.name.has_value() && p.name->isNotEmpty())
        setTrackName (*p.name);
}

juce::String NeonEQProcessor::getTrackName() const
{
    return shared->getName();
}

juce::String NeonEQProcessor::getLinkedName() const
{
    juce::ScopedLock l (metaLock);
    return linkedName;
}

std::vector<std::shared_ptr<SharedEntry>> NeonEQProcessor::getOtherInstances()
{
    auto all = Registry::get().snapshot();
    all.erase (std::remove_if (all.begin(), all.end(),
                               [this] (const std::shared_ptr<SharedEntry>& e) { return e.get() == shared.get(); }),
               all.end());
    return all;
}

void NeonEQProcessor::setLink (std::shared_ptr<SharedEntry> e)
{
    {
        juce::SpinLock::ScopedLockType l (linkLock);
        linked = e;
    }
    juce::ScopedLock l (metaLock);
    linkedName = e ? e->getName() : juce::String();
}

void NeonEQProcessor::timerCallback()
{
    std::shared_ptr<SharedEntry> cur;
    {
        juce::SpinLock::ScopedLockType l (linkLock);
        cur = linked;
    }

    if (cur)
    {
        if (! cur->alive.load())
        {
            juce::SpinLock::ScopedLockType l (linkLock);
            linked = nullptr;           // keep linkedName so it reconnects by name
        }
        else
        {
            juce::ScopedLock l (metaLock);
            linkedName = cur->getName();
        }
    }
    else
    {
        const auto wanted = getLinkedName();
        if (wanted.isNotEmpty())
            for (auto& e : getOtherInstances())
                if (e->getName() == wanted)
                {
                    juce::SpinLock::ScopedLockType l (linkLock);
                    linked = e;
                    break;
                }
    }
}

// ---------------------------------------------------------------- audio

void NeonEQProcessor::prepareToPlay (double sampleRate, int)
{
    fs = sampleRate;
    sampleRateAtomic.store (sampleRate);

    for (int b = 0; b < 6; ++b)
    {
        auto& s = bands[(size_t) b];
        auto& p = bp[(size_t) b];

        s.freq.reset (sampleRate, 0.03);  s.freq.setCurrentAndTargetValue (*p.freq);
        s.q.reset (sampleRate, 0.03);     s.q.setCurrentAndTargetValue (*p.q);
        s.gain.reset (sampleRate, 0.03);  s.gain.setCurrentAndTargetValue (*p.gain);

        for (int ch = 0; ch < 2; ++ch)
            for (int st = 0; st < 4; ++st)
                s.z1[ch][st] = s.z2[ch][st] = 0.0;

        s.dz1 = s.dz2 = s.env = 0.0;
        dynDb[(size_t) b].store (0.f);
    }

    outGain.reset (sampleRate, 0.03);
    outGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (outParam->load()));
    subCounter = 0;
}

bool NeonEQProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getMainInputChannelSet() != out)
        return false;

    const auto sc = layouts.getChannelSet (true, 1);
    return sc.isDisabled() || sc == juce::AudioChannelSet::mono() || sc == juce::AudioChannelSet::stereo();
}

void NeonEQProcessor::refreshParams()
{
    for (int b = 0; b < 6; ++b)
    {
        auto& s = bands[(size_t) b];
        auto& p = bp[(size_t) b];

        s.on    = p.on->load() > 0.5f;
        s.type  = (int) (p.type->load() + 0.5f);
        s.slope = (int) (p.slope->load() + 0.5f);
        s.dyn   = p.dyn->load() > 0.5f;
        s.src   = juce::jlimit (0, 2, (int) (p.src->load() + 0.5f));
        s.mode  = (int) (p.mode->load() + 0.5f);
        s.thr   = p.thr->load();
        s.range = p.range->load();

        s.freq.setTargetValue (p.freq->load());
        s.gain.setTargetValue (p.gain->load());
        s.q.setTargetValue (p.q->load());

        s.atkC = std::exp (-1.0 / (std::max (0.05f, p.atk->load()) * 0.001 * fs));
        s.relC = std::exp (-1.0 / (std::max (1.0f, p.rel->load()) * 0.001 * fs));
    }
    outGain.setTargetValue (juce::Decibels::decibelsToGain (outParam->load()));
}

void NeonEQProcessor::updateCoefficients (int chunk)
{
    for (int b = 0; b < 6; ++b)
    {
        auto& s = bands[(size_t) b];
        const double f = s.freq.skip (chunk);
        const double g = s.gain.skip (chunk);
        const double q = s.q.skip (chunk);

        if (! s.on)
        {
            dynDb[(size_t) b].store (0.f);
            continue;
        }

        double dynGain = 0.0;
        if (s.dynActive())
        {
            const double envDb = 20.0 * std::log10 (s.env + 1e-6);
            const double over = s.mode == 0 ? (envDb - s.thr) : (s.thr - envDb);
            double t = juce::jlimit (0.0, 1.0, over / 10.0);
            t = t * t * (3.0 - 2.0 * t);
            dynGain = s.range * t;

            // detector filter
            const int dt = s.type == 0 ? 5 : (s.type == 1 ? 4 : 3);
            s.det = makeBiquad (dt, fs, f, s.type == 0 ? q : 0.7071, 0.0);
        }
        dynDb[(size_t) b].store ((float) dynGain);

        s.nst = buildBandCoeffs (s.type, s.slope, fs, f, q, g + dynGain, s.c);
    }
}

void NeonEQProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto mainBus = getBusBuffer (buffer, true, 0);
    const int n = mainBus.getNumSamples();
    const int chans = juce::jmin (2, mainBus.getNumChannels());
    if (n == 0 || chans == 0)
        return;

    refreshParams();

    const bool scOn = getBusCount (true) > 1 && getBus (true, 1) != nullptr && getBus (true, 1)->isEnabled();
    auto scBuf = scOn ? getBusBuffer (buffer, true, 1) : juce::AudioBuffer<float>();

    bool needLink = false;
    for (auto& s : bands)
        if (s.dynActive() && s.src == 2)
            needLink = true;

    std::shared_ptr<SharedEntry> linkPtr;
    juce::SpinLock::ScopedTryLockType tryLock (linkLock);
    if (tryLock.isLocked() && needLink && linked && linked->alive.load())
        linkPtr = linked;          // (shared_ptr copy only when linking is actually used)

    float* data[2] = { mainBus.getWritePointer (0), chans > 1 ? mainBus.getWritePointer (1) : nullptr };
    const float* keys[3] = { keySelf, keySC, keyLink };

    const uint64_t wp0 = shared->ring.wp.load (std::memory_order_relaxed);

    for (int s0 = 0; s0 < n; s0 += kChunk)
    {
        const int m = juce::jmin (kChunk, n - s0);

        // ---- key signals (mono)
        for (int i = 0; i < m; ++i)
        {
            float sum = data[0][s0 + i];
            if (data[1] != nullptr) sum = 0.5f * (sum + data[1][s0 + i]);
            keySelf[i] = sum;
        }

        if (scOn && scBuf.getNumChannels() > 0)
        {
            const int nc = scBuf.getNumChannels();
            for (int i = 0; i < m; ++i)
            {
                float sum = 0.f;
                for (int c = 0; c < nc; ++c) sum += scBuf.getReadPointer (c)[s0 + i];
                keySC[i] = sum / (float) nc;
            }
        }
        else
            std::fill (keySC, keySC + m, 0.f);

        if (linkPtr)
            linkPtr->ring.readBlock (keyLink, n, s0, m);
        else
            std::fill (keyLink, keyLink + m, 0.f);

        shared->ring.put (wp0 + (uint64_t) s0, keySelf, m);

        // ---- processing
        for (int i = 0; i < m; ++i)
        {
            if (subCounter == 0)
                updateCoefficients (kSub);
            subCounter = (subCounter + 1) % kSub;

            for (auto& s : bands)
            {
                if (! s.dynActive())
                    continue;

                const double x = keys[s.src][i];
                const double y = s.det.b0 * x + s.dz1;
                s.dz1 = s.det.b1 * x - s.det.a1 * y + s.dz2;
                s.dz2 = s.det.b2 * x - s.det.a2 * y;

                const double a = std::abs (y);
                s.env = a > s.env ? s.atkC * s.env + (1.0 - s.atkC) * a
                                  : s.relC * s.env + (1.0 - s.relC) * a;
            }

            const float og = outGain.getNextValue();

            for (int ch = 0; ch < chans; ++ch)
            {
                double x = data[ch][s0 + i];

                for (auto& s : bands)
                {
                    if (! s.on)
                        continue;

                    for (int st = 0; st < s.nst; ++st)
                    {
                        const auto& c = s.c[st];
                        const double y = c.b0 * x + s.z1[ch][st];
                        s.z1[ch][st] = c.b1 * x - c.a1 * y + s.z2[ch][st];
                        s.z2[ch][st] = c.b2 * x - c.a2 * y;
                        x = y;
                    }
                }

                data[ch][s0 + i] = (float) (x * og);
            }

            tmpOut[i] = data[1] != nullptr ? 0.5f * (data[0][s0 + i] + data[1][s0 + i]) : data[0][s0 + i];
        }

        scope.push (tmpOut, m);
    }

    shared->ring.wp.store (wp0 + (uint64_t) n, std::memory_order_release);
}

// ---------------------------------------------------------------- state / editor

juce::AudioProcessorEditor* NeonEQProcessor::createEditor()
{
    return new NeonEQEditor (*this);
}

void NeonEQProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("editorW", editorW.load(), nullptr);
    state.setProperty ("editorH", editorH.load(), nullptr);
    state.setProperty ("linkName", getLinkedName(), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void NeonEQProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = juce::ValueTree::fromXml (*xml);
            apvts.replaceState (vt);

            editorW.store (juce::jlimit (820, 2400, (int) vt.getProperty ("editorW", 980)));
            editorH.store (juce::jlimit (600, 1400, (int) vt.getProperty ("editorH", 660)));

            juce::ScopedLock l (metaLock);
            linkedName = vt.getProperty ("linkName", "").toString();
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NeonEQProcessor();
}
