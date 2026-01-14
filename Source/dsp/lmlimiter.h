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
}

class LMLimiter {
private:
	float sampleRate = 48000.0;

	LMLimiterNamespace::TinyDelay<2048> delayL;
	LMLimiterNamespace::TinyDelay<2048> delayR;
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

			//维护最大值。
			//每次发现新最大值，更新上升速度，在attackMs之前使gainAdd达到目标值。
			float targetGainL = vl1 / outputMul;
			if (targetGainL > nowMaxL)
			{
				nowMaxL = targetGainL;
				isRisingL = true;
				riseRateL = (nowMaxL - gainAddL) / attackSamples;
			}
			else if (isRisingL && gainAddL >= nowMaxL)
			{
				isRisingL = false;
				nowMaxL = gainAddL;
			}
			if (!isRisingL)
			{
				nowMaxL = gainAddL;
			}
			float targetGainR = vr1 / outputMul;
			if (targetGainR > nowMaxR)
			{
				nowMaxR = targetGainR;
				isRisingR = true;
				riseRateR = (nowMaxR - gainAddR) / attackSamples;
			}
			else if (isRisingR && gainAddR >= nowMaxR)
			{
				isRisingR = false;
				nowMaxR = gainAddR;
			}
			if (!isRisingR)
			{
				nowMaxR = gainAddR;
			}


			if (isRisingL) gainAddL += riseRateL;//给gainAdd应用riseRate和releaseTaw
			else gainAddL *= releaseTaw;
			if (isRisingR) gainAddR += riseRateR;
			else gainAddR *= releaseTaw;

			float outl = delayL.ProcessSample(inl);//应用延迟
			float outr = delayR.ProcessSample(inr);
			outL[i] = outl / (1.0f + gainAddL);//应用增益补偿
			outR[i] = outr / (1.0f + gainAddR);
		}
	}
};