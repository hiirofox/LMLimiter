/*
  ==============================================================================

	This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
LModelAudioProcessorEditor::LModelAudioProcessorEditor(LModelAudioProcessor& p)
	: AudioProcessorEditor(&p), audioProcessor(p)
{
	// Make sure that before the constructor has finished, you've set the
	// editor's size to whatever you need it to be.
	setResizable(true, true); // 允许窗口调整大小

	setOpaque(false);  // 允许在边框外面绘制

	//setResizeLimits(64 * 11, 64 * 5, 10000, 10000); // 设置最小宽高为300x200，最大宽高为800x600
	setSize(64 * 7, 64 * 2);
	setResizeLimits(64 * 7, 64 * 2, 64 * 13, 64 * 2);

	//constrainer.setFixedAspectRatio(11.0 / 4.0);  // 设置为16:9比例
	//setConstrainer(&constrainer);  // 绑定窗口的宽高限制

	K_Lookahead.setText("lookahead", "ms");
	K_Lookahead.ParamLink(audioProcessor.GetParams(), "lookahead");
	addAndMakeVisible(K_Lookahead);
	K_Attack.setText("attack", "ms");
	K_Attack.ParamLink(audioProcessor.GetParams(), "attack");
	addAndMakeVisible(K_Attack);
	K_Release.setText("release", "ms");
	K_Release.ParamLink(audioProcessor.GetParams(), "release");
	addAndMakeVisible(K_Release);
	K_Input.setText("input", "dB");
	K_Input.ParamLink(audioProcessor.GetParams(), "input");
	addAndMakeVisible(K_Input);
	K_Output.setText("output", "dB");
	K_Output.ParamLink(audioProcessor.GetParams(), "output");
	addAndMakeVisible(K_Output);
	K_Threshold.setText("Threshold", "dB");
	K_Threshold.ParamLink(audioProcessor.GetParams(), "threshold");
	addAndMakeVisible(K_Threshold);


	startTimerHz(30);

}

LModelAudioProcessorEditor::~LModelAudioProcessorEditor()
{
}

//==============================================================================
void LModelAudioProcessorEditor::paint(juce::Graphics& g)
{
	g.fillAll(juce::Colour(0x00, 0x00, 0x00));

	g.fillAll();
	g.setFont(juce::Font("FIXEDSYS", 16.0, 1));
	g.setColour(juce::Colour(0xff00ff00));;

	int w = getBounds().getWidth(), h = getBounds().getHeight();

	g.drawText("LMLimiter 260115 13:15", juce::Rectangle<float>(0, h - 16, w, 16), 1);
}

void LModelAudioProcessorEditor::resized()
{
	juce::Rectangle<int> bound = getBounds();
	int x = bound.getX(), y = bound.getY(), w = bound.getWidth(), h = bound.getHeight();
	auto convXY = juce::Rectangle<int>::leftTopRightBottom;

	K_Lookahead.setBounds(32 + 64 * 0, 32, 64, 64);
	K_Attack.setBounds(32 + 64 * 1, 32, 64, 64);
	K_Release.setBounds(32 + 64 * 2, 32, 64, 64);
	K_Input.setBounds(32 + 64 * 3, 32, 64, 64);
	K_Output.setBounds(32 + 64 * 4, 32, 64, 64);
	K_Threshold.setBounds(32 + 64 * 5, 32, 64, 64);
}

void LModelAudioProcessorEditor::timerCallback()
{
	repaint();
}
