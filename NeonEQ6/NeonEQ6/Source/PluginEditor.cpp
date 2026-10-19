#include "PluginEditor.h"

NeonEQEditor::NeonEQEditor (NeonEQProcessor& p)
    : AudioProcessorEditor (&p), proc (p), graph (p)
{
    setLookAndFeel (&laf);

    addAndMakeVisible (graph);
    graph.onSelect = [this] (int b) { selectBand (b); };

    for (int i = 0; i < 6; ++i)
    {
        auto& c = chips[(size_t) i];
        c.text = juce::String (i + 1);
        c.colour = Neon::bandColour (i);
        c.onClick = [this, i] { selectBand (i); };
        addAndMakeVisible (c);
    }

    for (auto* c : std::initializer_list<juce::Component*> { &segOn, &segType, &segSlope, &segDyn, &segSrc, &segMode,
                                                              &segSpeed, &segAn, &fFreq, &fGain, &fQ, &fThr, &fRange,
                                                              &fAtk, &fRel, &fOut, &linkBtn })
        addAndMakeVisible (c);

    linkBtn.colour = Neon::purple;
    linkBtn.selected = true;
    linkBtn.onClick = [this] { showLinkMenu(); };

    fOut.setParameter (P ("out_gain"), Neon::white);
    segSpeed.setParameter (P ("an_speed"), Neon::pink);
    segAn.setParameter (P ("an_on"), Neon::green);

    selectBand (0);

    setResizable (true, true);
    setResizeLimits (820, 600, 2400, 1400);
    setSize (proc.editorW.load(), proc.editorH.load());

    startTimerHz (15);
}

NeonEQEditor::~NeonEQEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void NeonEQEditor::selectBand (int b)
{
    selected = juce::jlimit (0, 5, b);
    const auto c = Neon::bandColour (selected);

    segOn.setParameter (BP ("on"), c);
    segType.setParameter (BP ("type"), c);
    segSlope.setParameter (BP ("slope"), c);
    segDyn.setParameter (BP ("dyn"), c);
    segSrc.setParameter (BP ("src"), c);
    segMode.setParameter (BP ("mode"), c);

    fFreq.setParameter (BP ("freq"), c);
    fGain.setParameter (BP ("gain"), c);
    fQ.setParameter (BP ("q"), c);
    fThr.setParameter (BP ("thr"), c);
    fRange.setParameter (BP ("range"), c);
    fAtk.setParameter (BP ("atk"), c);
    fRel.setParameter (BP ("rel"), c);

    graph.setSelected (selected);
    timerCallback();
    repaint();
}

void NeonEQEditor::showLinkMenu()
{
    auto others = proc.getOtherInstances();
    const auto current = proc.getLinkedName();

    juce::PopupMenu m;
    m.addItem (1, "None", true, current.isEmpty());
    m.addSeparator();

    if (others.empty())
        m.addItem (1000, "(no other instances found)", false, false);

    int id = 2;
    for (auto& e : others)
        m.addItem (id++, e->getName(), true, e->getName() == current);

    auto* pr = &proc;
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&linkBtn),
                     [pr, others] (int r)
                     {
                         if (r == 1)
                             pr->setLink (nullptr);
                         else if (r >= 2 && (size_t) (r - 2) < others.size())
                             pr->setLink (others[(size_t) (r - 2)]);
                     });
}

void NeonEQEditor::timerCallback()
{
    const int type = (int) (raw ("type") + 0.5f);
    const bool isCut = type >= 3;
    const bool dynAvail = type <= 2;
    const bool dynOn = dynAvail && raw ("dyn") > 0.5f;
    const int src = (int) (raw ("src") + 0.5f);

    fGain.setEnabled (! isCut);
    fGain.setAlpha (isCut ? 0.3f : 1.f);
    segSlope.setVisible (isCut);

    segDyn.setEnabled (dynAvail);
    segDyn.setAlpha (dynAvail ? 1.f : 0.3f);

    for (auto* c : std::initializer_list<juce::Component*> { &segSrc, &segMode, &fThr, &fRange, &fAtk, &fRel, &linkBtn })
    {
        c->setEnabled (dynOn);
        c->setAlpha (dynOn ? 1.f : 0.3f);
    }
    linkBtn.setVisible (src == 2);

    const auto linked = proc.getLinkedName();
    linkBtn.text = "LINK:  " + (linked.isEmpty() ? juce::String ("choose a track") : linked);
    linkBtn.repaint();

    for (int i = 0; i < 6; ++i)
    {
        const bool on = proc.apvts.getRawParameterValue (bid (i, "on"))->load() > 0.5f;
        auto& c = chips[(size_t) i];
        if (c.selected != (i == selected) || c.dimmed != ! on)
        {
            c.selected = i == selected;
            c.dimmed = ! on;
            c.repaint();
        }
    }

    const auto name = proc.getTrackName();
    if (name != lastTrackName)
    {
        lastTrackName = name;
        repaint (trackRect);
    }
}

void NeonEQEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0d0d16), 0.f, 0.f,
                                             Neon::bg, 0.f, (float) getHeight(), false));
    g.fillAll();

    g.setFont (Neon::font (15.f, true));
    g.setColour (Neon::white);
    g.drawText ("NEON EQ", 18, 12, 90, 24, juce::Justification::centredLeft);
    g.setColour (Neon::purple);
    g.fillEllipse (14.f, 21.f, 4.f, 4.f);

    g.setFont (Neon::font (12.f));
    g.setColour (Neon::pink);
    g.drawText (lastTrackName, trackRect, juce::Justification::centredLeft, true);
}

void NeonEQEditor::resized()
{
    proc.editorW.store (getWidth());
    proc.editorH.store (getHeight());

    auto r = getLocalBounds().reduced (16, 12);

    // top bar
    auto top = r.removeFromTop (34);
    auto right = top;
    fOut.setBounds (right.removeFromRight (150));
    right.removeFromRight (14);
    segAn.setBounds (right.removeFromRight (64).withSizeKeepingCentre (64, 24));
    right.removeFromRight (8);
    segSpeed.setBounds (right.removeFromRight (190).withSizeKeepingCentre (190, 24));
    trackRect = juce::Rectangle<int> (116, top.getY() + 5, juce::jmax (40, right.getRight() - 116 - 12), 24);
    r.removeFromTop (6);

    // bottom panel
    auto bottom = r.removeFromBottom (166);
    graph.setBounds (r.withTrimmedBottom (8));

    auto row1 = bottom.removeFromTop (26);
    bottom.removeFromTop (6);
    auto row2 = bottom.removeFromTop (38);
    bottom.removeFromTop (10);
    auto row3 = bottom.removeFromTop (26);
    bottom.removeFromTop (6);
    auto row4 = bottom.removeFromTop (38);

    for (auto& c : chips)
    {
        c.setBounds (row1.removeFromLeft (32));
        row1.removeFromLeft (4);
    }
    row1.removeFromLeft (12);
    segOn.setBounds (row1.removeFromLeft (86));
    row1.removeFromLeft (10);
    segType.setBounds (row1.removeFromLeft (juce::jmin (380, row1.getWidth() - 130)));
    row1.removeFromLeft (10);
    segSlope.setBounds (row1.removeFromLeft (110));

    auto thirds = [] (juce::Rectangle<int> row, int n, std::initializer_list<juce::Component*> comps)
    {
        const int w = row.getWidth() / n;
        for (auto* c : comps)
        {
            c->setBounds (row.removeFromLeft (w).reduced (8, 0));
        }
    };
    thirds (row2, 3, { &fFreq, &fGain, &fQ });
    thirds (row4, 4, { &fThr, &fRange, &fAtk, &fRel });

    segDyn.setBounds (row3.removeFromLeft (150));
    row3.removeFromLeft (10);
    segSrc.setBounds (row3.removeFromLeft (230));
    row3.removeFromLeft (10);
    linkBtn.setBounds (row3.removeFromLeft (230));
    row3.removeFromLeft (10);
    segMode.setBounds (row3.removeFromLeft (150));
}
