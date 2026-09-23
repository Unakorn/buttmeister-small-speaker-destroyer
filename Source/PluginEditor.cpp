#include "PluginEditor.h"

HoneyBadgerHolyGrailAudioProcessorEditor::HoneyBadgerHolyGrailAudioProcessorEditor(HoneyBadgerHolyGrailAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(980, 620);

    titleLabel.setText("Buttmeister Small Speaker Destroyer", juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(34.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    subtitleLabel.setText("Auto EQ for tracks, voices, sends, and 808s that need to survive tiny speakers.",
                          juce::dontSendNotification);
    subtitleLabel.setColour(juce::Label::textColourId, juce::Colour(0xffd7d0c2));
    addAndMakeVisible(subtitleLabel);

    slotTitleLabel.setText("Slots: click one to capture, click several for Stack Destroy", juce::dontSendNotification);
    slotTitleLabel.setColour(juce::Label::textColourId, juce::Colour(0xffffd25f));
    slotTitleLabel.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    addAndMakeVisible(slotTitleLabel);

    readoutLabel.setColour(juce::Label::textColourId, juce::Colour(0xffb7f7ee));
    readoutLabel.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    addAndMakeVisible(readoutLabel);

    setupCombo(missionBox, missionLabel, HoneyBadgerHolyGrailAudioProcessor::missions(), "Mission");
    setupCombo(genreBox, genreLabel, HoneyBadgerHolyGrailAudioProcessor::genres(), "Genre");
    setupCombo(instrumentBox, instrumentLabel, HoneyBadgerHolyGrailAudioProcessor::instruments(), "Instrument");
    setupCombo(styleBox, styleLabel, HoneyBadgerHolyGrailAudioProcessor::styles(), "Style");
    setupCombo(intensityBox, intensityLabel, HoneyBadgerHolyGrailAudioProcessor::intensities(), "Intensity");

    autoToneButton.setButtonText("Destroyer Rack");
    autoToneButton.setColour(juce::ToggleButton::textColourId, juce::Colour(0xfff4efe5));
    autoToneButton.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xffffd25f));
    autoToneButton.setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour(0xff4a4f55));
    addAndMakeVisible(autoToneButton);

    auto setupSlider = [this](juce::Slider& slider, juce::Label& label, const juce::String& labelText)
    {
        label.setText(labelText, juce::dontSendNotification);
        label.setColour(juce::Label::textColourId, juce::Colours::white);
        label.setFont(juce::FontOptions(14.0f, juce::Font::bold));
        addAndMakeVisible(label);

        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 58, 22);
        slider.setColour(juce::Slider::trackColourId, juce::Colour(0xffffd25f));
        slider.setColour(juce::Slider::thumbColourId, juce::Colour(0xfff4efe5));
        slider.setColour(juce::Slider::backgroundColourId, juce::Colour(0xff11161b));
        slider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
        slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff080a0c));
        addAndMakeVisible(slider);
    };

    setupSlider(toneAmountSlider, toneAmountLabel, "Destroy Amount");
    setupSlider(outputTrimSlider, outputTrimLabel, "Output Trim");

    auto& state = audioProcessor.getValueTreeState();
    missionAttachment = std::make_unique<ComboBoxAttachment>(state, "mission", missionBox);
    genreAttachment = std::make_unique<ComboBoxAttachment>(state, "genre", genreBox);
    instrumentAttachment = std::make_unique<ComboBoxAttachment>(state, "instrument", instrumentBox);
    styleAttachment = std::make_unique<ComboBoxAttachment>(state, "style", styleBox);
    intensityAttachment = std::make_unique<ComboBoxAttachment>(state, "intensity", intensityBox);
    autoToneAttachment = std::make_unique<ButtonAttachment>(state, "autoTone", autoToneButton);
    toneAmountAttachment = std::make_unique<SliderAttachment>(state, "toneAmount", toneAmountSlider);
    outputTrimAttachment = std::make_unique<SliderAttachment>(state, "outputTrim", outputTrimSlider);

    for (int i = 0; i < static_cast<int>(slotButtons.size()); ++i)
    {
        auto& button = slotButtons[static_cast<size_t>(i)];
        button.setButtonText(juce::String(i + 1));
        button.setClickingTogglesState(true);
        button.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
        button.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xffffd25f));
        button.setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour(0xff4a4f55));
        button.onClick = [this, i]
        {
            audioProcessor.setActiveSlot(i);
            updateSlotButtons();
        };
        addAndMakeVisible(button);
    }

    setupActionButton(fightButton, "Destroy It", juce::Colour(0xffffd25f));
    setupActionButton(recheckButton, "Recheck Slot", juce::Colour(0xff2fdbc8));
    setupActionButton(approveButton, "Run Away", juce::Colour(0xff73e26b));
    setupActionButton(stackButton, "Stack Destroy", juce::Colour(0xffff5f5f));

    fightButton.onClick = [this] { setChatText(audioProcessor.captureSlot(false)); updateSlotButtons(); };
    recheckButton.onClick = [this] { setChatText(audioProcessor.captureSlot(true)); updateSlotButtons(); };
    approveButton.onClick = [this] { setChatText(audioProcessor.approveSlot()); updateSlotButtons(); };
    stackButton.onClick = [this] { setChatText(audioProcessor.analyzeStack(selectedSlots())); updateSlotButtons(); };

    chatBox.setMultiLine(true);
    chatBox.setReadOnly(true);
    chatBox.setCaretVisible(false);
    chatBox.setScrollbarsShown(true);
    chatBox.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff080a0c));
    chatBox.setColour(juce::TextEditor::textColourId, juce::Colour(0xfff4efe5));
    chatBox.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff343b42));
    chatBox.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xffffd25f));
    chatBox.setFont(juce::FontOptions(16.0f));
    addAndMakeVisible(chatBox);

    setChatText(audioProcessor.getLastMessage());
    updateSlotButtons();
    startTimerHz(12);
}

void HoneyBadgerHolyGrailAudioProcessorEditor::setupCombo(juce::ComboBox& box, juce::Label& label,
                                                          const juce::StringArray& items,
                                                          const juce::String& labelText)
{
    label.setText(labelText, juce::dontSendNotification);
    label.setColour(juce::Label::textColourId, juce::Colours::white);
    label.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    addAndMakeVisible(label);

    box.addItemList(items, 1);
    box.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff11161b));
    box.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    box.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xffffd25f));
    addAndMakeVisible(box);
}

void HoneyBadgerHolyGrailAudioProcessorEditor::setupActionButton(juce::TextButton& button,
                                                                 const juce::String& text,
                                                                 juce::Colour colour)
{
    button.setButtonText(text);
    button.setColour(juce::TextButton::buttonColourId, colour);
    button.setColour(juce::TextButton::buttonOnColourId, colour.brighter(0.2f));
    button.setColour(juce::TextButton::textColourOffId, juce::Colours::black);
    button.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
    addAndMakeVisible(button);
}

void HoneyBadgerHolyGrailAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff070809));

    juce::ColourGradient bg(juce::Colour(0xff111315), 0.0f, 0.0f,
                            juce::Colour(0xff1f1710), static_cast<float>(getWidth()), static_cast<float>(getHeight()), false);
    g.setGradientFill(bg);
    g.fillAll();

    auto panel = getLocalBounds().toFloat().reduced(18.0f);
    g.setColour(juce::Colour(0xe0090c0e));
    g.fillRoundedRectangle(panel, 8.0f);
    g.setColour(juce::Colour(0x88ffd25f));
    g.drawRoundedRectangle(panel, 8.0f, 1.2f);

    g.setColour(juce::Colour(0xff10151a));
    g.fillRoundedRectangle(34.0f, 154.0f, static_cast<float>(getWidth() - 68), 150.0f, 7.0f);
    g.fillRoundedRectangle(34.0f, 328.0f, static_cast<float>(getWidth() - 68), static_cast<float>(getHeight() - 362), 7.0f);
    g.setColour(juce::Colour(0xff333b43));
    g.drawRoundedRectangle(34.0f, 154.0f, static_cast<float>(getWidth() - 68), 150.0f, 7.0f, 1.0f);
    g.drawRoundedRectangle(34.0f, 328.0f, static_cast<float>(getWidth() - 68), static_cast<float>(getHeight() - 362), 7.0f, 1.0f);

    auto latest = audioProcessor.getLatestFingerprint();
    drawMeter(g, { 630.0f, 96.0f, 110.0f, 22.0f }, juce::jmap(latest.lowBuild, 0.0f, 0.7f, 0.0f, 1.0f), juce::Colour(0xff72d2ff), "Low");
    drawMeter(g, { 752.0f, 96.0f, 110.0f, 22.0f }, juce::jmap(latest.midDensity, 0.0f, 0.7f, 0.0f, 1.0f), juce::Colour(0xffffd25f), "Body");
    drawMeter(g, { 874.0f, 96.0f, 70.0f, 22.0f }, latest.width, juce::Colour(0xffff5f5f), "Wide");
}

void HoneyBadgerHolyGrailAudioProcessorEditor::drawMeter(juce::Graphics& g, juce::Rectangle<float> bounds,
                                                         float value, juce::Colour colour, const juce::String& label)
{
    g.setColour(juce::Colour(0xff06080a));
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(colour);
    g.fillRoundedRectangle(bounds.withWidth(bounds.getWidth() * juce::jlimit(0.0f, 1.0f, value)), 4.0f);
    g.setColour(juce::Colours::white.withAlpha(0.84f));
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawText(label, bounds.reduced(5.0f, 0.0f), juce::Justification::centredLeft);
}

void HoneyBadgerHolyGrailAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(34);
    titleLabel.setBounds(area.removeFromTop(42));
    subtitleLabel.setBounds(area.removeFromTop(26));

    auto comboRow = area.removeFromTop(64);
    auto placeCombo = [&comboRow](juce::Label& label, juce::ComboBox& box, int width)
    {
        auto cell = comboRow.removeFromLeft(width).reduced(4);
        label.setBounds(cell.removeFromTop(20));
        box.setBounds(cell.removeFromTop(30));
    };

    placeCombo(missionLabel, missionBox, 210);
    placeCombo(genreLabel, genreBox, 150);
    placeCombo(instrumentLabel, instrumentBox, 160);
    placeCombo(styleLabel, styleBox, 150);
    placeCombo(intensityLabel, intensityBox, 130);

    area.removeFromTop(16);
    auto toneRow = area.removeFromTop(52);
    autoToneButton.setBounds(toneRow.removeFromLeft(180).reduced(5, 10));
    auto placeSlider = [&toneRow](juce::Label& label, juce::Slider& slider, int width)
    {
        auto cell = toneRow.removeFromLeft(width).reduced(4);
        label.setBounds(cell.removeFromTop(18));
        slider.setBounds(cell.removeFromTop(28));
    };
    placeSlider(toneAmountLabel, toneAmountSlider, 230);
    placeSlider(outputTrimLabel, outputTrimSlider, 230);

    area.removeFromTop(12);
    slotTitleLabel.setBounds(area.removeFromTop(24));

    auto slotRow = area.removeFromTop(48);
    for (auto& button : slotButtons)
        button.setBounds(slotRow.removeFromLeft(64).reduced(5, 5));

    readoutLabel.setBounds(slotRow.reduced(8, 5));

    area.removeFromTop(18);
    auto actionRow = area.removeFromTop(52);
    fightButton.setBounds(actionRow.removeFromLeft(150).reduced(5, 7));
    recheckButton.setBounds(actionRow.removeFromLeft(160).reduced(5, 7));
    approveButton.setBounds(actionRow.removeFromLeft(150).reduced(5, 7));
    stackButton.setBounds(actionRow.removeFromLeft(160).reduced(5, 7));

    area.removeFromTop(28);
    chatBox.setBounds(area.reduced(10));
}

std::array<bool, 8> HoneyBadgerHolyGrailAudioProcessorEditor::selectedSlots() const
{
    std::array<bool, 8> selected {};
    for (size_t i = 0; i < slotButtons.size(); ++i)
        selected[i] = slotButtons[i].getToggleState();
    return selected;
}

void HoneyBadgerHolyGrailAudioProcessorEditor::setChatText(const juce::String& message)
{
    displayedMessage = message;
    chatBox.setText(message, juce::dontSendNotification);
    chatBox.moveCaretToTop(false);
}

void HoneyBadgerHolyGrailAudioProcessorEditor::updateSlotButtons()
{
    for (size_t i = 0; i < slotButtons.size(); ++i)
    {
        auto fp = audioProcessor.getSlot(static_cast<int>(i));
        auto& button = slotButtons[i];
        auto label = juce::String(static_cast<int>(i) + 1);
        if (fp.captured)
            label << " " << fp.instrument.substring(0, 3).toUpperCase();
        button.setButtonText(label);
        button.setAlpha(fp.captured ? 1.0f : 0.62f);
        button.setColour(juce::ToggleButton::textColourId,
                         fp.approved ? juce::Colour(0xff73e26b)
                                     : (fp.captured ? juce::Colour(0xffffd25f) : juce::Colours::white));
    }
}

void HoneyBadgerHolyGrailAudioProcessorEditor::timerCallback()
{
    auto latest = audioProcessor.getLatestFingerprint();
    readoutLabel.setText("Live: peak " + juce::String(latest.peakDb, 1) + " dB | rms "
                             + juce::String(latest.rmsDb, 1) + " dB | active slot "
                             + juce::String(audioProcessor.getActiveSlot() + 1) + "\n"
                             + audioProcessor.getToneRackReadout(),
                         juce::dontSendNotification);
    updateSlotButtons();
    repaint();
}
