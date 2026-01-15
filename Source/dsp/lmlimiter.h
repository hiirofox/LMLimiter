#pragma once

#define _USE_MATH_DEFINES
#include <math.h>

namespace LMLimiterNamespace
{
	template<int MaxDelaySamples>
	class TinyDelay
	{
	private:
		float buf[MaxDelaySamples] = { 0 };
		int delaySamples = 0;
		int pos = 0;
	public:
		TinyDelay()
		{
			memset(buf, 0, sizeof(buf));
		}
		void SetDelaySamples(int numSamples)
		{
			if (numSamples < 0)numSamples = 0;
			if (numSamples >= MaxDelaySamples) numSamples = MaxDelaySamples - 1;
			delaySamples = numSamples;
		}
		float ProcessSample(float inSample)
		{
			buf[(pos + delaySamples) % MaxDelaySamples] = inSample;
			float outSample = buf[pos];
			pos = (pos + 1) % MaxDelaySamples;
			return outSample;
		}
	};

	class SlidingWindowMax {
	private:
		struct Sample {
			long long index;
			float value;
		};

		std::deque<Sample> deque;
		int windowSize;
		long long currentIndex;

	public:
		SlidingWindowMax() : windowSize(0), currentIndex(0) {}

		void SetWindowSize(int numSamples) {
			if (numSamples < 1) {
				numSamples = 1;
			}
			windowSize = numSamples;
			deque.clear();
			currentIndex = 0;
		}

		float ProcessSample(float x) {
			while (!deque.empty() && deque.back().value <= x) {
				deque.pop_back();
			}

			deque.push_back({ currentIndex, x });

			if (deque.front().index <= currentIndex - windowSize) {
				deque.pop_front();
			}

			currentIndex++;

			if (deque.empty()) return 0;//防止空deque
			return deque.front().value;
		}
	};
}

class LMLimiter {
private:
	float sampleRate = 48000.0;

	LMLimiterNamespace::TinyDelay<2048> delayL;
	LMLimiterNamespace::TinyDelay<2048> delayR;
	LMLimiterNamespace::SlidingWindowMax swmL;
	LMLimiterNamespace::SlidingWindowMax swmR;

	float inputMul = 1.0, outputMul = 1.0;

	float nowMaxL = 0, nowMaxR = 0;
	float riseRateL = 0, riseRateR = 0;
	float gainAddL = 0, gainAddR = 0;

	float attackSamples = 480.0f;
	float releaseTaw = 0.0f;//release是一个一阶低通

	bool isRisingL = false;
	bool isRisingR = false;
public:
	void SetParams(float inputdB, float outputdB, float attackMs, float releaseMs)
	{
		inputMul = powf(10.0f, inputdB / 20.0f);
		outputMul = powf(10.0f, outputdB / 20.0f);

		releaseTaw = expf(-1.0f / ((releaseMs + 0.05) * sampleRate / 1000.0f));//release是一个一阶低通滤波
		attackSamples = 2 + attackMs * sampleRate / 1000.0f;
		delayL.SetDelaySamples(attackSamples);
		delayR.SetDelaySamples(attackSamples);
		swmL.SetWindowSize(attackSamples);
		swmR.SetWindowSize(attackSamples);
	}
	void ProcessBlock(const float* inL, const float* inR, float* outL, float* outR, int numSamples)
	{
		for (int i = 0; i < numSamples; ++i)
		{
			float inl = inL[i] * inputMul;
			float inr = inR[i] * inputMul;
			float vl1 = fabsf(inl) - outputMul;
			float vr1 = fabsf(inr) - outputMul;
			vl1 = (vl1 > 0) ? vl1 : 0;
			vr1 = (vr1 > 0) ? vr1 : 0;

			float smaxL = swmL.ProcessSample(vl1);//计算滑动窗口最大值
			float smaxR = swmR.ProcessSample(vr1);
			float dlyL = delayL.ProcessSample(inl);//延时补偿
			float dlyR = delayR.ProcessSample(inr);

			//主要是这个部分出现问题。
			//////
			if (smaxL > gainAddL)gainAddL += smaxL / attackSamples;//如果是上升则用线性包络
			else gainAddL += releaseTaw * (smaxL - gainAddL);//如果是下降则用指数衰减包络
			if (smaxR > gainAddR)gainAddR += smaxR / attackSamples;
			else gainAddR += releaseTaw * (smaxR - gainAddR);
			//////

			float outl = dlyL / (1.0f + gainAddL);
			float outr = dlyL / (1.0f + gainAddR);
			outL[i] = outl;//应用增益补偿
			outR[i] = outr;
		}
	}
};