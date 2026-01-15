#pragma once

#include "../dsp/lmlimiter.h"
#include "SingleMeterUI.h"
#include <JuceHeader.h>

class LMLimiterMeterUI : public juce::Component
{
private:
	SingleMeterUI inputMeter;
	SingleMeterUI reductionMeter;
	SingleMeterUI outputMeter;
	LMLimiter* limiter = nullptr;
	float inputdB = -9999, outputdB = -9999, thresholddB = 0, reductiondB = 0;
public:
	LMLimiterMeterUI(LMLimiter* limiter) : limiter(limiter)
	{
		addAndMakeVisible(inputMeter);
		addAndMakeVisible(outputMeter);
		addAndMakeVisible(reductionMeter);
	}
	LMLimiterMeterUI()
	{
		addAndMakeVisible(inputMeter);
		addAndMakeVisible(outputMeter);
		addAndMakeVisible(reductionMeter);
	}
	void SetProcessor(LMLimiter* limiter)
	{
		this->limiter = limiter;
	}
	~LMLimiterMeterUI() override {}
	void updateMeters()
	{
		if (limiter) limiter->GetMeterValues(inputdB, outputdB, thresholddB, reductiondB);
		inputMeter.SetValueAndRange(inputdB, -30, 30);
		reductionMeter.SetValueAndRange(60 - reductiondB, 0, 60);
		outputMeter.SetValueAndRange(outputdB, -30, 30);
	}
	void paint(juce::Graphics& g) override
	{
		int w = getWidth();
		int h = getHeight();
		int interval = w / 8;
		int metalWidth = w / 4;

		updateMeters();
		/*
		g.setColour(juce::Colour(0xff00ff00));
		g.setFont(juce::Font("FIXEDSYS", 12.0, 1));
		//g.drawRect(0, 0, w, h);
		char tmp[16];
		sprintf(tmp, "%c%02d", inputdB < 0 ? '-' : '+', (int)fabsf(inputdB < -99 ? -99 : inputdB));
		g.drawText(juce::String(tmp), juce::Rectangle<int>(0, 0, metalWidth, 16), juce::Justification::centred);
		sprintf(tmp, "-%02d", (int)fabsf(reductiondB));
		g.drawText(juce::String(tmp), juce::Rectangle<int>(interval + metalWidth, 0, metalWidth, 16), juce::Justification::centred);
		sprintf(tmp, "%c%02d", outputdB < 0 ? '-' : '+', (int)fabsf(outputdB < -99 ? -99 : outputdB));
		g.drawText(juce::String(tmp), juce::Rectangle<int>(2 * interval + 2 * metalWidth, 0, metalWidth, 16), juce::Justification::centred);
		*/
	}
	void resized() override
	{
		int w = getWidth();
		int h = getHeight();
		int interval = w / 8;
		int metalWidth = w / 4;
		inputMeter.setBounds(0, 0, metalWidth, h);
		reductionMeter.setBounds(interval + metalWidth, 0, metalWidth, h);
		outputMeter.setBounds(2 * interval + 2 * metalWidth, 0, metalWidth, h);

		inputMeter.SetColor(juce::Colour(0xff00aaff));
		reductionMeter.SetColor(juce::Colour(0xffff4444));
		outputMeter.SetColor(juce::Colour(0xffff8800));

		reductionMeter.SetFromTop(true);
	}
};