/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

struct RotaryLookAndFeel : juce::LookAndFeel_V4
{
    void drawRotarySlider (juce::Graphics&, // from juce::LookAndFeel_V4 class line 206
                        int x, int y, int width, int height,
                        float sliderPosProportional,
                        float rotaryStartAngle,
                        float rotaryEndAngle,
                        juce::Slider&) override;

    void drawToggleButton (juce::Graphics &g, // line 119
                        juce::ToggleButton &toggleButton, 
                        bool shouldDrawButtonAsHighlighted, 
                        bool shouldDrawButtonAsDown) override;

    //Font getLabelFont (Label&) override;
    
private:

const juce::Typeface::Ptr typeface = juce::Typeface::createSystemTypefaceFor(BinaryData::Orbitron_ttf, BinaryData::Orbitron_ttfSize);
    // g.setFont(juce::Font(typeface).withHeight(15.5f)); // slider labels
};

struct RotarySliderWithLabels : juce::Slider
{
  RotarySliderWithLabels(juce::RangedAudioParameter& rap, const juce::String& unitSuffix) : 
  juce::Slider(juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag, juce::Slider::TextEntryBoxPosition::NoTextBox),
  param(&rap),
  suffix(unitSuffix)
  {
    setLookAndFeel(&lnf);
  }

  // setLookAndFeel, so unset it with destructor
  ~RotarySliderWithLabels()
  {
    setLookAndFeel(nullptr);
  }

  struct LabelPos
  {
    float pos;
    juce::String label;
  };

  juce::Array<LabelPos> labels;

  void paint(juce::Graphics& g) override;
  juce::Rectangle<int> getSliderBounds() const;
  int getTextHeight() const { return 14; }
  juce::String getDisplayString() const;
private:
  RotaryLookAndFeel lnf; // Calling this "LookAndFeel" throws ambiguous symbol error as could be juce::LookAndFeel

  juce::RangedAudioParameter* param;
  juce::String suffix;
};

struct EnableButton : juce::ToggleButton {};

//==============================================================================
/**
*/
class ChorusAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    ChorusAudioProcessorEditor (ChorusAudioProcessor&);
    ~ChorusAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    ChorusAudioProcessor& audioProcessor;

    const juce::Typeface::Ptr typeface = juce::Typeface::createSystemTypefaceFor(BinaryData::Orbitron_ttf, BinaryData::Orbitron_ttfSize);
    // g.setFont(juce::Font(typeface).withHeight(15.5f)); // slider labels

    RotarySliderWithLabels
    delayTimeSliderLeft,
    delayTimeSliderRight,
    depthSlider,
    rateSlider;

    using APVTS = juce::AudioProcessorValueTreeState;
    using Attachment = APVTS::SliderAttachment;

    Attachment
    delayTimeSliderAttachmentLeft,
    delayTimeSliderAttachmentRight,
    depthSliderAttachment,
    rateSliderAttachment;

    using ButtonAttachment = APVTS::ButtonAttachment;
    EnableButton dualDelayButton, chorusButton;
    ButtonAttachment dualDelayButtonAttachment, chorusButtonAttachment;

    std::vector<juce::Component*> getComps();

    //juce::Image background; // just used for drawing bbox rects for ui layout

    //juce::Slider delayTimeSlider; // add comps without subclass or lookandfeel
    //std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> delayTimeSliderAttachment;
    //juce::Label delayTimeLabelLeft;

    RotaryLookAndFeel lnf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChorusAudioProcessorEditor)
};
