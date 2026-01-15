#pragma once

#include <JuceHeader.h>

// 1. 继承 juce::Timer 以实现动画循环
class SingleMeterUI : public juce::Component, public juce::Timer
{
private:
	// ... 原有的绘图函数 drawSingleMeter 保持不变 ...
	void drawSingleMeter(juce::Graphics& g, int x, int y, int width, int height,
		float value, float peakValue, float minDb, float maxDb, juce::Colour color, bool fromTop = false)
	{
		// 计算电平位置
		float normalizedLevel = juce::jlimit(0.0f, 1.0f, (value - minDb) / (maxDb - minDb));
		float levelHeight;
		if (fromTop) levelHeight = (1.0f - normalizedLevel) * height;
		else levelHeight = normalizedLevel * height;

		// 计算峰值位置
		float normalizedPeak = juce::jlimit(0.0f, 1.0f, (peakValue - minDb) / (maxDb - minDb));
		if (fromTop) normalizedPeak = 1.0f - normalizedPeak;

		// 绘制电平条
		if (levelHeight > 0)
		{
			juce::ColourGradient gradient;
			if (fromTop)
			{
				gradient = juce::ColourGradient(
					color.withBrightness(0.8f), x, y,
					color.withBrightness(1.0f), x, y + height, false
				);
				g.setGradientFill(gradient);
				g.fillRoundedRectangle((float)x + 1, (float)y, (float)width - 2, levelHeight, 1.0f);
			}
			else
			{
				gradient = juce::ColourGradient(
					color.withBrightness(0.5f), x, y + height,
					color.withBrightness(1.0f), x, y, false
				);
				g.setGradientFill(gradient);
				g.fillRoundedRectangle((float)x + 1, (float)y + height - levelHeight, (float)width - 2, levelHeight, 1.0f);
			}
		}

		// 绘制峰值指示器
		if (normalizedPeak > 0.01f || (fromTop && normalizedPeak < 0.99f))
		{
			float peakY;
			if (fromTop) peakY = y + normalizedPeak * height;
			else peakY = y + height - normalizedPeak * height;

			g.setColour(color.brighter(0.7f));
			g.fillRect(juce::Rectangle<float>((float)x + 1, peakY - 1, (float)width - 2, 2.0f));
		}
	}

	juce::Colour meterColor = juce::Colour(0xff00ffff);
	float currentValue = -60.0f;
	float peakValue = -60.0f;
	float minValue = -60.0f; // 修改默认值以匹配通常的静音电平
	float maxValue = 6.0f;
	bool isFromTop = false;

	// --- Peak Hold 相关的变量 ---
	juce::int64 lastPeakUpdateTime = 0; // 记录上一次峰值更新的时间戳
	const int holdTimeMs = 1000;        // 峰值保持时间：1000毫秒 (1秒)
	const float decayRate = 0.75f;       // 跌落速度：每帧跌落多少dB (基于60Hz刷新率)

public:
	SingleMeterUI()
	{
		// 启动定时器，60Hz (每秒60帧) 用于平滑的跌落动画
		startTimerHz(60);
	}

	~SingleMeterUI() override
	{
		stopTimer();
	}

	void SetColor(juce::Colour newColor)
	{
		meterColor = newColor;
	}

	void SetValueAndRange(float newValue, float minV = -60, float maxV = 6)
	{
		currentValue = newValue;
		minValue = minV;
		maxValue = maxV;

		auto now = juce::Time::getMillisecondCounter();

		if (isFromTop)
		{
			if (currentValue < peakValue)
			{
				peakValue = currentValue;
				lastPeakUpdateTime = now;
			}
		}
		else
		{
			if (currentValue > peakValue)
			{
				peakValue = currentValue;
				lastPeakUpdateTime = now;
			}
		}
	}

	void SetFromTop(bool fromTop)
	{
		if (isFromTop != fromTop)
		{
			isFromTop = fromTop;
			peakValue = isFromTop ? maxValue : minValue;
			currentValue = peakValue; 
		}
	}

	void timerCallback() override
	{
		auto now = juce::Time::getMillisecondCounter();
		if (now > lastPeakUpdateTime + holdTimeMs)
		{
			if (isFromTop)
			{
				if (peakValue < currentValue)
				{
					peakValue += decayRate; 
					if (peakValue > currentValue)
						peakValue = currentValue;

					repaint();
				}
			}
			else
			{
				if (peakValue > currentValue)
				{
					peakValue -= decayRate; 
					if (peakValue < currentValue)
						peakValue = currentValue;

					repaint();
				}
			}
		}
		if (std::abs(peakValue - currentValue) > 0.001f)
		{
			repaint();
		}
	}

	void paint(juce::Graphics& g) override
	{
		float w = (float)getWidth();
		float h = (float)getHeight();
		g.fillAll(juce::Colour(0xff333333));
		drawSingleMeter(g, 0, 0, (int)w, (int)h, currentValue, peakValue,
			minValue, maxValue, meterColor, isFromTop);
	}
};