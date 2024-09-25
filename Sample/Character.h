#pragma once

#include "MainSystem/Animation/AnimLayer/AnimPlayerLayer.h"
#include "MainSystem/Animation/AnimLayer/AnimBlendLayer.h"
#include "MainSystem/Animation/AnimLayer/AnimTransitLayer.h"
#include "MainSystem/Animation/AnimLayer/AnimJointLayer.h"
#include "MainSystem/Animation/AnimLayer/AnimMixLayer.h"
#include "MainSystem/Animation/Components/AnimatorSkeletalArray.h"
#include "MainSystem/Animation/Utils/Animation.h"

namespace soft
{
	class AnimatorSkeletalArray;
	class AnimPlayerLayer;
	class AnimBlendLayer;
	class AnimTransitLayer;
	class AnimMixLayer;
	class AnimJointLayer;
	class Animation;
}

using namespace soft;

struct Character
{

	struct AnimationID
	{

		constexpr static ID IdleCarefully = 0;
		constexpr static ID IdleNormal = 1;
		constexpr static ID IdleJumpUp = 2;
		constexpr static ID IdleTurnLeft90 = 3;
		constexpr static ID IdleTurnLeft180 = 4;
		constexpr static ID IdleTurnRight90 = 5;
		constexpr static ID IdleTurnRight180 = 6;
		constexpr static ID IdleTurnLeftNoRotationCarefully = 7;
		constexpr static ID IdleTurnRightNoRotationCarefully = 8;
		constexpr static ID RunSlow = 9;
		constexpr static ID RunFast = 10;
		constexpr static ID RunJump = 11;
		constexpr static ID RunJumpInPlace = 12;
		constexpr static ID RunJumpInPlaceShort = 13;
		constexpr static ID FallIdle = 14;
		constexpr static ID FallSoftLanding = 15;
		constexpr static ID FallHardLanding = 16;
		constexpr static ID HangIdle = 17;
		constexpr static ID HangFromFalling = 18;
		constexpr static ID HangClimbUpToCrouch = 19;
		constexpr static ID HangMoveLeft = 20;
		constexpr static ID HangMoveRight = 21;

	};

	struct _Animations
	{

		SharedPtr<Animation> IdleCarefully;
		SharedPtr<Animation> IdleNormal;
		SharedPtr<Animation> IdleJumpUp;
		SharedPtr<Animation> IdleTurnLeft90;
		SharedPtr<Animation> IdleTurnLeft180;
		SharedPtr<Animation> IdleTurnRight90;
		SharedPtr<Animation> IdleTurnRight180;
		SharedPtr<Animation> IdleTurnLeftNoRotationCarefully;
		SharedPtr<Animation> IdleTurnRightNoRotationCarefully;
		SharedPtr<Animation> RunSlow;
		SharedPtr<Animation> RunFast;
		SharedPtr<Animation> RunJump;
		SharedPtr<Animation> RunJumpInPlace;
		SharedPtr<Animation> RunJumpInPlaceShort;
		SharedPtr<Animation> FallIdle;
		SharedPtr<Animation> FallSoftLanding;
		SharedPtr<Animation> FallHardLanding;
		SharedPtr<Animation> HangIdle;
		SharedPtr<Animation> HangFromFalling;
		SharedPtr<Animation> HangClimbUpToCrouch;
		SharedPtr<Animation> HangMoveLeft;
		SharedPtr<Animation> HangMoveRight;

	};

	_Animations Animations;

	struct SrcPlayer0
	{
		constexpr static ID ID = 0;
	};

	AnimPlayerLayer* SrcPlayer0 = nullptr;


	struct Transit0
	{
		constexpr static ID ID = 3;
	};

	AnimTransitLayer* Transit0 = nullptr;


	struct SrcPlayer1
	{
		constexpr static ID ID = 1;
	};

	AnimPlayerLayer* SrcPlayer1 = nullptr;


	struct Blend01
	{
		constexpr static ID ID = 2;
	};

	AnimBlendLayer* Blend01 = nullptr;


	inline void Initialize(AnimatorSkeletalArray* animator)
	{
		Animations.IdleCarefully = animator->m_model3D->GetAnimation(0);
		Animations.IdleNormal = animator->m_model3D->GetAnimation(1);
		Animations.IdleJumpUp = animator->m_model3D->GetAnimation(2);
		Animations.IdleTurnLeft90 = animator->m_model3D->GetAnimation(3);
		Animations.IdleTurnLeft180 = animator->m_model3D->GetAnimation(4);
		Animations.IdleTurnRight90 = animator->m_model3D->GetAnimation(5);
		Animations.IdleTurnRight180 = animator->m_model3D->GetAnimation(6);
		Animations.IdleTurnLeftNoRotationCarefully = animator->m_model3D->GetAnimation(7);
		Animations.IdleTurnRightNoRotationCarefully = animator->m_model3D->GetAnimation(8);
		Animations.RunSlow = animator->m_model3D->GetAnimation(9);
		Animations.RunFast = animator->m_model3D->GetAnimation(10);
		Animations.RunJump = animator->m_model3D->GetAnimation(11);
		Animations.RunJumpInPlace = animator->m_model3D->GetAnimation(12);
		Animations.RunJumpInPlaceShort = animator->m_model3D->GetAnimation(13);
		Animations.FallIdle = animator->m_model3D->GetAnimation(14);
		Animations.FallSoftLanding = animator->m_model3D->GetAnimation(15);
		Animations.FallHardLanding = animator->m_model3D->GetAnimation(16);
		Animations.HangIdle = animator->m_model3D->GetAnimation(17);
		Animations.HangFromFalling = animator->m_model3D->GetAnimation(18);
		Animations.HangClimbUpToCrouch = animator->m_model3D->GetAnimation(19);
		Animations.HangMoveLeft = animator->m_model3D->GetAnimation(20);
		Animations.HangMoveRight = animator->m_model3D->GetAnimation(21);

		
		SrcPlayer0 = (AnimPlayerLayer*)(animator->m_animLayers[0].Get());

		Transit0 = (AnimTransitLayer*)(animator->m_animLayers[3].Get());

		SrcPlayer1 = (AnimPlayerLayer*)(animator->m_animLayers[1].Get());

		Blend01 = (AnimBlendLayer*)(animator->m_animLayers[2].Get());

	}

};
