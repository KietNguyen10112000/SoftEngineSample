#pragma once

#include "MainSystem/Animation/AnimLayer/AnimPlayerLayer.h"
#include "MainSystem/Animation/AnimLayer/AnimBlendLayer.h"

#include "Common/Actions/ActionExecution.h"

using namespace soft;

class AnimationUtils
{
public:
	static SharedPtr<ActionBase> Transit(const SharedPtr<ActionExecution>& exe, AnimBlendLayer* blendLayer, const SharedPtr<Animation>& srcAnimation, const SharedPtr<Animation>& destAnimation, float transitTime)
	{
		auto srcPlayer0 = dynamic_cast<AnimPlayerLayer*>(blendLayer->GetOutput());
		ID srcPlayer0Index = blendLayer->GetInput(0) == srcPlayer0 ? 0 : 1;
		assert(srcPlayer0);

		ID srcPlayer1Index = srcPlayer0Index == 0 ? 1 : 0;
		auto srcPlayer1 = dynamic_cast<AnimPlayerLayer*>(blendLayer->GetInput(srcPlayer1Index));
		assert(srcPlayer1);

		SharedPtr<Function1D> controlFunction;
		if (srcPlayer0Index != 0)
		{
			// fade from Blend01.inputIndex1 -> Blend01.inputIndex0
			// f(x) = 1 - x
			controlFunction = std::make_shared<FunctionLinear1D>(-1.0f / transitTime, 1.0f);
		}
		else
		{
			// fade from Blend01.inputIndex0 -> Blend01.inputIndex1
			// f(x) = x
			controlFunction = std::make_shared<FunctionLinear1D>(1.0f / transitTime, 0.0f);
		}

		auto time = srcPlayer0->GetTime();
		auto srcDuration = srcAnimation->GetTickDuration() / srcAnimation->GetTicksPerSecond();
		auto destDuration = destAnimation->GetTickDuration() / destAnimation->GetTicksPerSecond();

		srcPlayer1->SetAnimation(destAnimation, -1, -1);
		srcPlayer1->SetDuration(srcDuration);
		srcPlayer1->SetCurrentTime(time);

		// start blending
		blendLayer->StartBlending(controlFunction, 0, transitTime);

		auto ret = ActionInterpolation<float>::New(
			{
				{ srcDuration, 0.0f },
				{ destDuration, transitTime },
			},
			[&, srcPlayer0, srcPlayer1](const float& v)
			{
				srcPlayer0->SetDuration(v);
				srcPlayer1->SetDuration(v);
			}
		);

		exe->RunAction(ret);
		return ret;
	}

	// forwardOrBackward: 0 - backward, 1 - forward
	static void TransitChangeDirection(const SharedPtr<ActionExecution>& exe, AnimBlendLayer* blendLayer, float transitTime, int forwardOrBackward)
	{
		assert(forwardOrBackward == 0 || forwardOrBackward == 1);
		auto controlFunction = std::dynamic_pointer_cast<FunctionLinear1D>(blendLayer->GetControlFunction());

		if (!controlFunction)
		{
			return;
		}

		ID srcPlayer0Index = 0;
		if (controlFunction->m_a < 0)
		{
			srcPlayer0Index = 1;
		}

		auto srcPlayer0 = dynamic_cast<AnimPlayerLayer*>(blendLayer->GetInput(srcPlayer0Index));
		assert(srcPlayer0);

		ID srcPlayer1Index = srcPlayer0Index == 0 ? 1 : 0;
		auto srcPlayer1 = dynamic_cast<AnimPlayerLayer*>(blendLayer->GetInput(srcPlayer1Index));
		assert(srcPlayer1);

		auto currentTime = blendLayer->GetTime();
		auto currentBlendFactor = controlFunction->Test(currentTime);

		Vec2 p0 = { 0,currentBlendFactor };
		Vec2 p1 = { transitTime,float(forwardOrBackward) };

		auto newControlFunction = std::make_shared<FunctionLinear1D>(p0, p1);

		// start blending
		blendLayer->StartBlending(newControlFunction, 0, transitTime);

		auto& srcAnimation = srcPlayer0->GetAnimation();
		auto& destAnimation = srcPlayer1->GetAnimation();

		auto srcDuration = srcPlayer0->GetDuration();
		auto destDuration = forwardOrBackward ?
			destAnimation->GetTickDuration() / destAnimation->GetTicksPerSecond()
			: srcAnimation->GetTickDuration() / srcAnimation->GetTicksPerSecond();

		exe->RunAction(
			ActionInterpolation<float>::New(
				{
					{ srcDuration, 0.0f },
					{ destDuration, transitTime },
				},
				[&, srcPlayer0, srcPlayer1](const float& v)
				{
					srcPlayer0->SetDuration(v);
					srcPlayer1->SetDuration(v);
				}
			)
		);
	}

};

