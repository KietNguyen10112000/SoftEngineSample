#pragma once

#include "Character.h"

#include "MainSystem/Scripting/Components/TPPCameraScript.h"

#ifdef IGNORE
#undef IGNORE
#endif
#include "MainSystem/Physics/Query/PhysicsQueryFilterCallback.h"

#include "Common/Actions/ActionExecution.h"
#include "Common/Actions/ActionSequence.h"
#include "Common/Actions/ActionDelay.h"
#include "Common/Actions/ActionCallback.h"

using namespace soft;

namespace soft
{
	class AnimatorSkeletalArray;
	class ActionExecution;
	class Action;
	class ActionPhysicsOverlap;
}

class CharacterScript : public TPPCameraScript
{
	using Base = TPPCameraScript;
protected:
	SCRIPT_DEFAULT_METHOD(CharacterScript);

	TRACEABLE_FRIEND();
	void Trace(Tracer* tracer)
	{
		Base::Trace(tracer);
	}

	struct STATE
	{
		enum ENUM
		{
			IDLE,
			MOVE_SLOW,
			MOVE_FAST,
			JUMP,
			JUMP_FROM_IDLE,
			FALL,
			LANDING,
			TRANSIT_TO_CLIMBING,
			CLIMBING,
			CLIMBING_UP,
			TURN,
			RAGDOLL
		};
	};

	class FallingSweepFilter : public PhysicsQueryFilterCallback
	{
	public:
		CharacterScript* m_script;
		inline FallingSweepFilter(CharacterScript* script) : m_script(script) {};

		// Inherited via PhysicsQueryFilterCallback
		PhysicsQueryHitType::ENUM PrevFilter(GameObject* obj, PhysicsShape* shape, PhysicsHitFlags& flags) override;
		PhysicsQueryHitType::ENUM PostFilter(GameObject* obj, PhysicsShape* shape, const PhysicsQueryHit& hit) override;
	};

	struct FallingSweepLocalState
	{
		bool stopSerialQuery = false;
	};

	class CharacterCollisionFilter : public PhysicsQueryFilterCallback
	{
	public:
		CharacterScript* m_script;
		inline CharacterCollisionFilter(CharacterScript* script) : m_script(script) {};

		// Inherited via PhysicsQueryFilterCallback
		PhysicsQueryHitType::ENUM PrevFilter(GameObject* obj, PhysicsShape* shape, PhysicsHitFlags& flags) override;
		PhysicsQueryHitType::ENUM PostFilter(GameObject* obj, PhysicsShape* shape, const PhysicsQueryHit& hit) override;
	};

protected:
	SharedPtr<ActionExecution> m_actionExecution;

	AnimatorSkeletalArray* m_animator = nullptr;
	Character m_character;

	float m_slowRunMovingSpeed = 5.0f;
	float m_fastRunMovingSpeed = 10.0f;

	float m_currentJumpingMovingSpeed = 0.0f;
	float m_currentMovingSpeed = 0.0f;

	Vec3 m_characterForward = Vec3::ZERO; 
	Vec3 m_characterRight = Vec3::ZERO;
	Vec3 m_characterUp = Vec3::ZERO;

	Vec3 m_currentCameraToCharacterRight = Vec3::ZERO;
	Vec3 m_currentCameraToCharacterForward = Vec3::ZERO;

	Vec3 m_currentExpectedMovingDir = Vec3::ZERO;
	Vec3 m_lastExpectedMovingDir = Vec3::ZERO;

	STATE::ENUM m_currentBodyState = STATE::IDLE; 
	STATE::ENUM m_nextBodyState = STATE::IDLE;

	SharedPtr<ActionBase> m_switchRunSlowFastActionAnim;

	SharedPtr<ActionBase> m_switchBodyStateAction = nullptr;
	SharedPtr<ActionBase> m_modifyingMovingSpeedAction = nullptr;

	SharedPtr<ActionBase> m_prepareFallingUpdateActions[3] = {};
	SharedPtr<ActionBase> m_landingTransitAction = nullptr;
	SharedPtr<ActionBase> m_fallingUpdateAction = nullptr;
	SharedPtr<FallingSweepFilter> m_fallingSweepFilter = std::make_shared<FallingSweepFilter>(this);
	Vec3 m_fallingMovingMotion = Vec3::ZERO;
	size_t m_landingStartTime = 0;
	size_t m_landingAllowMovingDeltaTime = size_t(0.5f * 1000); // 0.5s

	SharedPtr<PhysicsShape> m_testQueryShape;
	SharedPtr<ActionExecution> m_testQueryShapeActionExecution;

	SharedPtr<PhysicsQueryFilterCallback> m_cctCollisionFilter = nullptr;

	SharedPtr<ActionBase> m_jumpingToClimbingUpdateAction = nullptr;
	SharedPtr<PhysicsShape> m_climbingQueryShape = nullptr;
	GameObject* m_currentClimbingBar = nullptr;
	Vec3 m_currentClimbingBarAnchorPoint = {};
	Vec3 m_climbingToBarPrevPos = {};

	float m_cctOriginalHeight = 0;
	float m_cctOriginalRadius = 0;


public:
	void OnStart() override;
	void OnUpdate(float dt) override;
	void OnGUI() override;

	Handle<ClassMetadata> GetMetadata(size_t sign) override;

private:
	void ResetDefaultState(float dt);

	void ControlCCTRotation(float dt, float rotationSpeed);
	void ControlMovement(float dt);
	// return true if moved
	bool TrySlideOverFirstPlane(const Vec3& disp);
	void ControlAnim(float dt);

	bool IsBlockingControlCCT();
	bool IsAnimTransiting();

	// return true if current state of character is idling or moving
	bool IsPlayingMotionAnim();
	void ControlMotionAnim(float dt);

	void TimeoutTransitingBodyState(STATE::ENUM nextState, float timeout);
	void StopTimeoutTransitingBodyState(STATE::ENUM nextState);

	template <typename Fn>
	SharedPtr<ActionBase> SetTimeout(float sec, const Fn& callback)
	{
		SharedPtr<ActionBase> ret;
		m_actionExecution->RunAction(
			ret = ActionSequence::New({
				ActionDelay::New(sec),
				ActionCallback::New(callback)
			})
		); 
		return ret;
	}

	void ModifyMovingSpeed(
		const ActionInterpolation<float>::KeyFrame& start,
		const ActionInterpolation<float>::KeyFrame& end);

	void StopModifyMovingSpeed();

	void PlayAnimIdle(float dt);
	void PlayAnimRun(float dt);
	void PlayAnimSwitchRunSlowFast(float dt);
	void PlayAnimTurnFromIdle(float dt);

	void PlayAnimJump(float dt);
	void PlayAnimJumpFromRunning(float dt, const SharedPtr<Animation>& anim, float transitTime, float animReachedTopTime, 
		float jumpUpVelocityLength, float extTimeTillApplyingJumpUpVelocity = 0);
	void PlayAnimJumpFromIdle(float dt);
	void FallingUpdate(float dt);
	void LandingUpdate(float dt, const ActionPhysicsSweep* action, const PhysicsSweepResult& result);
	void PlayAnimSoftLanding(float dt, float dY);

	bool IsOnGround(float slopLimit = 0.0f) const;
	void PlayAnimFalling(float dt);

	void StopJumpingOrFalling();
	GameObject* GetClimbingBar(const PhysicsOverlapResult& result);
	void JumpngToClimbingUpdate(float dt, const ActionPhysicsOverlap* action, const PhysicsOverlapResult& result);
	void ClimbingUpdate(float dt);
	void StartJumpingToClimpingUpdate();
	void StopJumpingToClimbingUpdate();
	void PlayAnimClimbUp(float dt);
	void PlayAnimClimbUpRestoreCCTDimensions(float dt);

protected:
	void DeserializeFromJson(Serializer* serializer, const json& j) override;

};