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
			if (delaySamples == numSamples) return;
			if (numSamples < 0)numSamples = 0;
			if (numSamples >= MaxDelaySamples) numSamples = MaxDelaySamples - 1;
			delaySamples = numSamples;
			memset(buf, 0, sizeof(buf));
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
			if (windowSize == numSamples) {
				return;
			}

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

	class SampleToPeak //之后再加上去了
	{
	private:
		float buffer[4]; // 移位寄存器：s[0]是最旧的, s[3]是最新的
		bool filled;     // 缓冲是否填满

	public:
		SampleToPeak() {
			for (int i = 0; i < 4; ++i) buffer[i] = 0.0f;
		}

		float ProcessSampleHermite4x(float x) {
			buffer[0] = buffer[1];
			buffer[1] = buffer[2];
			buffer[2] = buffer[3];
			buffer[3] = x;

			const float y0 = buffer[0];
			const float y1 = buffer[1];
			const float y2 = buffer[2];
			const float y3 = buffer[3];

			float c0 = y1;
			float c1 = 0.5f * (y2 - y0);
			float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
			float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);

			float val_25 = c0 + 0.25f * (c1 + 0.25f * (c2 + 0.25f * c3));
			float val_50 = c0 + 0.50f * (c1 + 0.50f * (c2 + 0.50f * c3));
			float val_75 = c0 + 0.75f * (c1 + 0.75f * (c2 + 0.75f * c3));

			float max_peak = fabsf(y1);
			float abs_25 = fabsf(val_25);
			if (abs_25 > max_peak) max_peak = abs_25;
			float abs_50 = fabsf(val_50);
			if (abs_50 > max_peak) max_peak = abs_50;
			float abs_75 = fabsf(val_75);
			if (abs_75 > max_peak) max_peak = abs_75;

			return max_peak;
		}
		float ProcessSampleSimple(float x)
		{
			return fabsf(x);
		}
	};
}

#define WithEditor 1

class LMLimiter {
private:
	float sampleRate = 48000.0;

	LMLimiterNamespace::TinyDelay<4800> delayL;//最大100ms延时
	LMLimiterNamespace::TinyDelay<4800> delayR;
	LMLimiterNamespace::SlidingWindowMax swmL;
	LMLimiterNamespace::SlidingWindowMax swmR;

	float inputMul = 1.0, outputMul = 1.0, thresholdMul = 1.0;

	float nowMaxL = 0, nowMaxR = 0;
	float riseRateL = 0, riseRateR = 0;
	float gainAddL = 0, gainAddR = 0;

	float lookaheadSamples = 480.0f;
	float attackTaw = 0.0f;
	float releaseTaw = 0.0f;//release是一个一阶低通

	bool isRisingL = false;
	bool isRisingR = false;

#if WithEditor
	using ThreadSafeFloat = std::atomic<float>;
	float maxInputdB = 0.0f;//输入电平（包含插件inputdB的增益）
	float maxOutputdB = 0.0f;//总输出电平（包含outputdB增益）
	float maxThresholddB = 0.0f;//阈值电平
	float maxReductiondB = 0.0f;//压下去的量
	ThreadSafeFloat lastMaxInputdB = -1000.0f;
	ThreadSafeFloat lastMaxOutputdB = -1000.0f;
	ThreadSafeFloat lastMaxThresholddB = -1000.0f;
	ThreadSafeFloat lastMaxReductiondB = -1000.0f;
	int updateCounter = 0;
#endif

public:
	void SetParams(float lookahead, float inputdB, float outputdB, float thresholddB, float attackMs, float releaseMs)
	{
		inputMul = powf(10.0f, inputdB / 20.0f);
		outputMul = powf(10.0f, outputdB / 20.0f);
		thresholdMul = powf(10.0f, thresholddB / 20.0f);

		attackTaw = 1.0 / lookaheadSamples;//在lookahead时间内上升到目标值,然后速度再乘以一个attack时间常数
		attackTaw *= (1.0 + attackMs / 1000.0);//应用attackMs(别信这个单位)
		releaseTaw = 1.0f / (releaseMs * sampleRate / 1000.0f);

		float lookaheadSamples = lookahead * sampleRate / 1000.0f + 2.0;
		if (lookaheadSamples > 4800.0f) lookaheadSamples = 4800.0f;
		delayL.SetDelaySamples(lookaheadSamples);
		delayR.SetDelaySamples(lookaheadSamples);
		swmL.SetWindowSize(lookaheadSamples);
		swmR.SetWindowSize(lookaheadSamples);
	}
	void ProcessBlock(const float* inL, const float* inR, float* outL, float* outR, int numSamples)
	{
		//到时候优化，把循环里面的乘法在外面预先计算好
		for (int i = 0; i < numSamples; ++i)
		{
			float inl = inL[i] * inputMul / thresholdMul;
			float inr = inR[i] * inputMul / thresholdMul;
			float vl1 = fabsf(inl) - 1.0;
			float vr1 = fabsf(inr) - 1.0;
			vl1 = (vl1 > 0) ? vl1 : 0;
			vr1 = (vr1 > 0) ? vr1 : 0;

			float smaxL = swmL.ProcessSample(vl1);//计算滑动窗口最大值
			float smaxR = swmR.ProcessSample(vr1);
			float dlyL = delayL.ProcessSample(inl);//延时补偿
			float dlyR = delayR.ProcessSample(inr);

			if (smaxL > gainAddL)
			{
				gainAddL += smaxL * attackTaw;//在lookahead时间内上升到目标值
				if (gainAddL > smaxL) gainAddL = smaxL;
			}
			else
			{
				gainAddL += releaseTaw * (smaxL - gainAddL);
				//if (gainAddL < smaxL) gainAddL = smaxL;
			}
			if (smaxR > gainAddR)
			{
				gainAddR += smaxR * attackTaw;//在lookahead时间内上升到目标值
				if (gainAddR > smaxR) gainAddR = smaxR;
			}
			else
			{
				gainAddR += releaseTaw * (smaxR - gainAddR);
				//if (gainAddR < smaxR) gainAddR = smaxR;
			}

			//最终保护：如果存在lookahead还没准备好的情况，则强制限制
			float dlyvl = fabsf(dlyL) - 1.0;
			float dlyvr = fabsf(dlyR) - 1.0;
			if (dlyvl > gainAddL) gainAddL = dlyvl;
			if (dlyvr > gainAddR) gainAddR = dlyvr;

			float outl = dlyL / (1.0f + gainAddL);
			float outr = dlyR / (1.0f + gainAddR);
			if (outl > 1.0f) outl = 1.0f;//削波吧，我力竭了
			if (outl < -1.0f) outl = -1.0f;

			outL[i] = outl * thresholdMul * outputMul;//应用增益补偿
			outR[i] = outr * thresholdMul * outputMul;

#if WithEditor
			float absInL = fabsf(inl * thresholdMul);
			float inLdB = 20.0f * log10f(absInL + 1e-60);
			if (inLdB > maxInputdB) maxInputdB = inLdB;

			float absInR = fabsf(inr * thresholdMul);
			float inRdB = 20.0f * log10f(absInR + 1e-60);
			if (inRdB > maxInputdB) maxInputdB = inRdB;

			float absOutL = fabsf(outL[i]);
			float outLdB = 20.0f * log10f(absOutL + 1e-60);
			if (outLdB > maxOutputdB) maxOutputdB = outLdB;

			float absOutR = fabsf(outR[i]);
			float outRdB = 20.0f * log10f(absOutR + 1e-60);
			if (outRdB > maxOutputdB) maxOutputdB = outRdB;

			float thresholddB = 20.0f * log10f(thresholdMul + 1e-60);
			if (thresholddB > maxThresholddB) maxThresholddB = thresholddB;
			float reductionL = 20.0f * log10f(1.0f + gainAddL);
			if (reductionL > maxReductiondB) maxReductiondB = reductionL;
			float reductionR = 20.0f * log10f(1.0f + gainAddR);
			if (reductionR > maxReductiondB) maxReductiondB = reductionR;
			updateCounter++;
#endif

		}
	}

#if WithEditor
	void GetMeterValues(float& inputdB, float& outputdB, float& thresholddB, float& reductiondB)
	{
		if (updateCounter >= 1024)
		{
			lastMaxInputdB.store(maxInputdB);
			lastMaxOutputdB.store(maxOutputdB);
			lastMaxThresholddB.store(maxThresholddB);
			lastMaxReductiondB.store(maxReductiondB);
			maxInputdB = -1000.0f;
			maxOutputdB = -1000.0f;
			maxThresholddB = -1000.0f;
			maxReductiondB = -1000.0f;
			updateCounter = 0;
		}
		inputdB = lastMaxInputdB.load();
		outputdB = lastMaxOutputdB.load();
		thresholddB = lastMaxThresholddB.load();
		reductiondB = lastMaxReductiondB.load();
	}
#endif


};