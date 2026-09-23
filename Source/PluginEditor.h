#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class HoneyBadgerHolyGrailAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                       private juce::Timer
{
public:
    explicit HoneyBadgerHolyGrailAudioProcessorEditor(HoneyBadgerHolyGrailAudioProcessor&);
    ~HoneyBadgerHolyGrailAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    void timerCallback() override;
    void setupCombo(juce::ComboBox& box, juce::Label& label, const juce::StringArray& items, const juce::String& labelText);
    void setupActionButton(juce::TextButton& button, const juce::String& text, juce::Colour colour);
    void updateSlotButtons();
    void setChatText(const juce::String& message);
    std::array<bool, 8> selectedSlots() const;
    void drawMeter(juce::Graphics& g, juce::Rectangle<float> bounds, float value, juce::Colour colour, const juce::String& label);

    HoneyBadgerHolyGrailAudioProcessor& audioProcessor;

    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label slotTitleLabel;
    juce::Label readoutLabel;
    juce::Label missionLabel;
    juce::Label genreLabel;
    juce::Label instrumentLabel;
    juce::Label styleLabel;
    juce::Label intensityLabel;
    juce::Label toneAmountLabel;
    juce::Label outputTrimLabel;

    juce::ComboBox missionBox;
    juce::ComboBox genreBox;
    juce::ComboBox instrumentBox;
    juce::ComboBox styleBox;
    juce::ComboBox intensityBox;
    juce::ToggleButton autoToneButton;
    juce::Slider toneAmountSlider;
    juce::Slider outputTrimSlider;

    std::array<juce::ToggleButton, 8> slotButtons;
    juce::TextButton fightButton;
    juce::TextButton recheckButton;
    juce::TextButton approveButton;
    juce::TextButton stackButton;
    juce::TextEditor chatBox;

    std::unique_ptr<ComboBoxAttachment> genreAttachment;
    std::unique_ptr<ComboBoxAttachment> missionAttachment;
    std::unique_ptr<ComboBoxAttachment> instrumentAttachment;
    std::unique_ptr<ComboBoxAttachment> styleAttachment;
    std::unique_ptr<ComboBoxAttachment> intensityAttachment;
    std::unique_ptr<ButtonAttachment> autoToneAttachment;
    std::unique_ptr<SliderAttachment> toneAmountAttachment;
    std::unique_ptr<SliderAttachment> outputTrimAttachment;

    juce::String displayedMessage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HoneyBadgerHolyGrailAudioProcessorEditor)
};
