#include "CharacterScript.h"

#include "MainSystem/Rendering/Components/CameraTPP.h"

#include "MainSystem/Scripting/Components/FPPCameraScript.h"

#include "MainSystem/Physics/Components/CharacterController.h"
#include "MainSystem/Physics/Components/CharacterControllerCapsule.h"
#include "MainSystem/Physics/PhysicsSystem.h"
#include "MainSystem/Physics/Query/ActionPhysicsSweep.h"
#include "MainSystem/Physics/Query/ActionPhysicsOverlap.h"
#include "MainSystem/Physics/Shapes/PhysicsShapeCapsule.h"
#include "MainSystem/Physics/Shapes/PhysicsShapeBox.h"

#include "Common/Actions/ActionExecution.h"
#include "Common/Actions/ActionSequence.h"
#include "Common/Actions/ActionDelay.h"
#include "Common/Actions/ActionCallback.h"
#include "Common/Actions/ActionRepeatUntil.h"
#include "Common/Actions/ActionDelayNTicks.h"

#include "Input/Input.h"

#include "Math/Math.h"

#include "Graphics//Graphics.h"
#include "Graphics/DebugGraphics.h"

#include "imgui/imgui.h"

#include "AnimationUtils.h"
#include "TAG.h"
#include "ClimbingBar.h"

#define CharacterScript_CLIMBING_QUERY_BOX_SIZE 3.5f

PhysicsQueryHitType::ENUM CharacterScript::CharacterCollisionFilter::PrevFilter(GameObject* obj, PhysicsShape* shape, PhysicsHitFlags& flags)
{
	auto tag = obj->Tag();
	switch (tag)
	{
	case TAG::CLIMBING_BAR:
		return PhysicsQueryHitType::IGNORE;
	default:
		break;
	}

	return PhysicsQueryHitType::TOUCH;
}

PhysicsQueryHitType::ENUM CharacterScript::CharacterCollisionFilter::PostFilter(GameObject* obj, PhysicsShape* shape, const PhysicsQueryHit& hit)
{
	return PhysicsQueryHitType::ENUM();
}

void CharacterScript::OnStart()
{
	auto camera = GetScene()->FindObjectByIndexedName("#camera");
	m_camera = camera->GetComponent<CameraTPP>();
	m_fppCamScript = camera->GetComponent<FPPCameraScript>();

	m_camera->SetTarget(GetGameObject());

	Base::OnStart();

	m_animator = GetGameObject()->Children()[0]->GetComponent<AnimatorSkeletalArray>();
	m_character.Initialize(m_animator);

	m_actionExecution = ActionExecution::New({});

	m_cctCollisionFilter = std::make_shared<CharacterCollisionFilter>(this);
	m_controller->CCTSetFilterCallback(m_cctCollisionFilter);

	m_climbingQueryShape = std::make_shared<PhysicsShapeBox>(Vec3(CharacterScript_CLIMBING_QUERY_BOX_SIZE), nullptr);
}

void CharacterScript::OnUpdate(float dt)
{
	UpdateCameraDefault(dt);

	if (m_camera && !m_camera->IsTPPEnabled())
	{
		return;
	}

	m_actionExecution->Update(dt);

	ControlMovement(dt);
	ControlAnim(dt);
}

void CharacterScript::OnGUI()
{
	/*{
		auto debugGraphics = Graphics::Get()->GetDebugGraphics();
		if (debugGraphics)
		{
			auto transform = m_controller->GetGameObject()->GetCommittedGlobalTransform();
			transform.Position() += transform.Forward().Normal() * (CharacterScript_CLIMBING_QUERY_BOX_SIZE / 2.0f);
			transform = Mat4::Scaling(Vec3(CharacterScript_CLIMBING_QUERY_BOX_SIZE)) * transform;
			debugGraphics->DrawCube(transform);
		}
	}*/

	return;

	static Transform start = {};
	static Vec3 distance = Vec3(1,1,1);
	static float height = 1.0f;
	static float radius = 1.0f;
	static Vec3 euler = Vec3::ZERO;

	if (!m_testQueryShape)
	{
		m_testQueryShape = std::make_shared<PhysicsShapeCapsule>(height, radius, m_controller->GetShape(0)->GetFirstMaterial());
		m_testQueryShapeActionExecution = ActionExecution::New({});
	}

	ImGui::Begin("CharacterScript");
	
	if (ImGui::DragFloat3("Position", &start.Position()[0], 0.001f, -FLT_MAX, FLT_MAX))
	{

	}

	if (ImGui::DragFloat3("Rotation", &euler[0], 0.001f, -FLT_MAX, FLT_MAX))
	{
		start.Rotation() = Quaternion(euler);
	}

	if (ImGui::DragFloat3("Distance", &distance[0], 0.001f, -FLT_MAX, FLT_MAX))
	{
		
	}

	ImGui::End();

	auto debugGraphics = Graphics::Get()->GetDebugGraphics();
	if (debugGraphics)
	{
		debugGraphics->DrawCapsule(Capsule(
			start.Rotation().ToMat4().Right().Normal(),
			start.Position(),
			height, radius
		));

		debugGraphics->DrawCapsule(Capsule(
			start.Rotation().ToMat4().Right().Normal(),
			start.Position() + distance,
			height, radius
		));

		debugGraphics->DrawRay(start.Position(), distance);
	}

	m_testQueryShapeActionExecution->Update(0);

	m_testQueryShapeActionExecution->RunAction(
		Physics()->Sweep(
			[&, debugGraphics](const ActionPhysicsSweep* action, const PhysicsSweepResult& result)
			{
				if (!action->HitOrTouchAnything())
				{
					return;
				}

				if (debugGraphics)
				{
					if (result.hasBlock)
					{
						debugGraphics->DrawRay(result.block.position, result.block.normal);
					}
					
					for (auto& touch : result.touches)
					{
						debugGraphics->DrawRay(touch.position, touch.normal);
					}
				}
			},
			m_testQueryShape.get(),
			start,
			distance
		)
	);
}

Handle<ClassMetadata> CharacterScript::GetMetadata(size_t sign)
{
	auto meta = mheap::New<ClassMetadata>(GetClassName(), this);

	{
		auto accessor = Accessor(
			"Reset Default State",
			1,
			[](const Variant& input, UnknownAddress& var, Serializable* instance) -> void
			{
				auto script = (CharacterScript*)instance;
				script->ResetDefaultState(0);
			},

			[](UnknownAddress& var, Serializable* instance) -> Variant
			{
				return Variant::Of(true);
			},
			this
		);
		meta->AddProperty(accessor);
	}

	return meta;
}

void CharacterScript::ResetDefaultState(float dt)
{
	m_currentMovingSpeed = 0.0f;

	m_character.Blend01->SetEnabled(false);
	m_character.SrcPlayer0->SetEnabled(false);
	m_character.SrcPlayer1->SetEnabled(false);

	m_character.Transit0->GetInput()->SetEnabled(true);

	m_actionExecution->StopAllActions();
	m_character.Transit0->FadeTo(AnimTransitLayer::TransitDirection::FORWARD, 0.2f, m_character.Animations.IdleCarefully, -1, -1);

	m_currentBodyState = STATE::IDLE;
	m_nextBodyState = STATE::IDLE;
}

void CharacterScript::ControlCCTRotation(float dt, float angleSpeed)
{
	if (m_lastExpectedMovingDir == Vec3::ZERO)
	{
		return;
	}

	if (!m_characterForward.Equals(m_lastExpectedMovingDir, 0.0001f))
	{
		auto angle = std::abs(AngleBetween(m_characterForward, m_lastExpectedMovingDir));

		auto& cctRotation = m_controller->CCTGetRotation();
		auto cctRotationMat = Mat4::Rotation(cctRotation);
		auto up = cctRotationMat.Up().Normal();
		auto destRotation = Mat4({
			Vec4(up.Cross(m_lastExpectedMovingDir).Normal(), 0.0f),
			Vec4(up, 0.0f),
			Vec4(m_lastExpectedMovingDir, 0.0f),
			Vec4(0,0,0,1.0f)
			});

		m_controller->CCTSetRotation(SLerp(cctRotation, destRotation, std::clamp((angleSpeed * dt) / angle, 0.0f, 1.0f)));
	}
}

void CharacterScript::ControlMovement(float dt)
{
	{
		auto temp = Mat4::Rotation(m_controller->CCTGetRotation());
		m_characterForward = temp.Forward().Normal();
		m_characterRight = temp.Right().Normal();
		m_characterUp = temp.Up().Normal();
	}

	auto right = Vec3::UP.Cross(-m_viewPoint.Normal());
	right.Normalize();
	auto forward = right.Cross(Vec3::UP);
	forward.Normalize();

	m_currentCameraToCharacterRight = right;
	m_currentCameraToCharacterForward = forward;

	Vec3 motion = { 0,0,0 };
	if (Input()->IsKeyDown('W'))
	{
		motion += forward;
	}

	if (Input()->IsKeyDown('S'))
	{
		motion -= forward;
	}

	if (Input()->IsKeyDown('A'))
	{
		motion -= right;
	}

	if (Input()->IsKeyDown('D'))
	{
		motion += right;
	}

	if (motion != Vec3::ZERO)
	{
		m_lastExpectedMovingDir = motion.Normalize();
		m_currentExpectedMovingDir = motion;
	}
	else
	{
		m_currentExpectedMovingDir = Vec3::ZERO;
	}

	if (m_currentMovingSpeed != 0.0f && !IsBlockingControlCCT())
	{
		auto disp = m_currentMovingSpeed * dt * m_characterForward;
		if (!TrySlideOverFirstPlane(disp))
		{
			m_controller->Move(disp);
		}
	}

	if (m_currentMovingSpeed != 0.0f && !IsBlockingControlCCT())
	{
		ControlCCTRotation(dt, 1.6f * PI);
	}

	if (IsBlockingControlCCT() && (m_currentBodyState == STATE::JUMP || m_currentBodyState == STATE::FALL 
		|| (m_currentBodyState == STATE::LANDING && (Clock::ms::now() - m_landingStartTime) <= m_landingAllowMovingDeltaTime))
		)
	{
		m_fallingMovingMotion = Vec3::ZERO;
		if (motion != Vec3::ZERO)
		{
			m_fallingMovingMotion = m_currentJumpingMovingSpeed * motion;
			{
				assert(std::abs(m_fallingMovingMotion.y) < 0.00001f);
				m_controller->Move(m_fallingMovingMotion * dt);
			}
		}

		ControlCCTRotation(dt, 2.0f * PI);
	}

	if (m_currentBodyState == STATE::CLIMBING)
	{
		if (Input()->IsKeyDown(KEYBOARD::SPACE))
		{
			if (Input()->IsKeyDown('S'))
			{
				m_controller->SetGravity(GetScene()->GetPhysicsSystem()->GetGravity());
				//m_currentBodyState = STATE::IDLE;
				//m_nextBodyState = STATE::IDLE;
				TimeoutTransitingBodyState(STATE::IDLE, 0.2f);
				//m_controller->Move(m_currentClimbingBar->GetCommittedGlobalTransform().Forward().Normal() * 5.0f);
				m_lastExpectedMovingDir = m_characterForward;
			}
			else
			{
				PlayAnimClimbUp(dt);
			}
		}
	}
}

bool CharacterScript::TrySlideOverFirstPlane(const Vec3& disp)
{
	if (m_controller->GetCollision()->contacts.empty())
	{
		return false;
	}

	CollisionContactPoint* touchPoint = nullptr;
	Vec3 touchNormal = {};

	auto slopLimit = m_controller->CCTGetSlopeLimit();
	auto invG = -m_controller->GetGravity().Normal();
	bool hasGround = false;
	for (auto& contact : m_controller->GetCollision()->contacts)
	{
		bool isA = contact->A == m_controller->GetGameObject() ? true : false;
		for (auto& pair : contact->contactPairs)
		{
			for (auto& point : pair->contactPoints)
			{
				auto normal = isA ? point.normal : -point.normal;
				auto isGround = invG.Dot(normal) >= slopLimit;
				if (isGround)
				{
					touchPoint = &point;
					touchNormal = normal;
					goto End;
				}
			}
		}
	}

End:
	if (touchPoint)
	{
		auto dispDir = disp.Normal();
		auto& normal = touchNormal;
		auto t = normal.Dot(dispDir);
		if (normal.Dot(dispDir) < 0.00001f)
		{
			// disp is trying to move cct upward to the top of the slope
			return false;
		}

		// move down on the slope
		auto v = normal.Cross(dispDir).Normal();
		v = v.Cross(normal).Normal();

		if (v.Dot(dispDir) < -0.00001f)
		{
			v = -v;
		}

		m_controller->Move(v * disp.Length());

		return true;
	}

	return touchPoint != nullptr;
}

void CharacterScript::ControlAnim(float dt)
{
	if (IsPlayingMotionAnim())
	{
		ControlMotionAnim(dt);
	}
}

bool CharacterScript::IsBlockingControlCCT()
{
	return m_currentBodyState == STATE::TURN || m_nextBodyState == STATE::TURN
		|| (m_nextBodyState == STATE::IDLE && m_currentBodyState != STATE::IDLE)
		|| (m_nextBodyState == STATE::JUMP || m_currentBodyState == STATE::JUMP)
		|| (m_nextBodyState == STATE::FALL || m_currentBodyState == STATE::FALL)
		|| (m_nextBodyState == STATE::LANDING || m_currentBodyState == STATE::LANDING);
}

bool CharacterScript::IsAnimTransiting()
{
	return m_nextBodyState != m_currentBodyState;
}

bool CharacterScript::IsPlayingMotionAnim()
{
	return 
		m_currentBodyState == STATE::IDLE
		|| m_currentBodyState == STATE::MOVE_SLOW
		|| m_currentBodyState == STATE::MOVE_FAST
		|| m_currentBodyState == STATE::JUMP;
}

void CharacterScript::ControlMotionAnim(float dt)
{
	auto iter = GetScene()->GetIterationCount();
	if (iter > 20 && !IsAnimTransiting() && !IsOnGround(m_controller->CCTGetSlopeLimit())
		&& (m_currentBodyState == STATE::IDLE
			|| m_currentBodyState == STATE::MOVE_FAST
			|| m_currentBodyState == STATE::MOVE_SLOW
			|| m_currentBodyState == STATE::LANDING
			)
		)
	{
		PlayAnimFalling(dt);
	}

	if (m_currentExpectedMovingDir != Vec3::ZERO && m_currentBodyState == STATE::IDLE)
	{
		PlayAnimTurnFromIdle(dt);
	}

	if (!IsBlockingControlCCT() && !IsAnimTransiting() && m_currentExpectedMovingDir != Vec3::ZERO && m_currentMovingSpeed == 0.0f && m_currentBodyState == STATE::IDLE)
	{
		// player request moving character from idling state

		PlayAnimRun(dt);
	}

	if (!IsBlockingControlCCT() /*&& !IsAnimTransiting()*/ && m_currentExpectedMovingDir == Vec3::ZERO && m_currentMovingSpeed != 0.0f 
		&& (m_currentBodyState == STATE::MOVE_SLOW || m_currentBodyState == STATE::MOVE_FAST) 
		&& (m_nextBodyState == STATE::MOVE_SLOW || m_nextBodyState == STATE::MOVE_FAST))
	{
		// player request stoping character from moving state

		PlayAnimIdle(dt);
	}

	if (!IsBlockingControlCCT() && !IsAnimTransiting()
		&& (m_currentBodyState == STATE::MOVE_SLOW || m_currentBodyState == STATE::MOVE_FAST))
	{
		// player request moving character from idling state

		PlayAnimSwitchRunSlowFast(dt);
	}

	if (!IsBlockingControlCCT() /*&& !IsAnimTransiting()*/ && Input()->IsKeyDown(KEYBOARD::SPACE)
		&& (m_currentBodyState == STATE::IDLE || m_currentBodyState == STATE::MOVE_SLOW || m_currentBodyState == STATE::MOVE_FAST))
	{
		// player request jumping character

		PlayAnimJump(dt);
	}
}

void CharacterScript::TimeoutTransitingBodyState(STATE::ENUM nextState, float timeout)
{
	if (m_switchBodyStateAction)
	{
		m_actionExecution->StopAction(m_switchBodyStateAction);
		m_switchBodyStateAction = nullptr;
	}

	m_nextBodyState = nextState;
	m_actionExecution->RunAction(
		m_switchBodyStateAction = ActionSequence::New({
			ActionDelay::New(timeout),
			ActionCallback::New([&, nextState]()
				{
					m_currentBodyState = nextState;
					m_switchBodyStateAction = nullptr;
				}
			)
		})
	);
}

void CharacterScript::StopTimeoutTransitingBodyState(STATE::ENUM nextState)
{
	if (m_switchBodyStateAction)
	{
		m_actionExecution->StopAction(m_switchBodyStateAction);
		m_switchBodyStateAction = nullptr;
	}

	m_nextBodyState = nextState;
	m_currentBodyState = nextState;
}

void CharacterScript::ModifyMovingSpeed(const ActionInterpolation<float>::KeyFrame& start, const ActionInterpolation<float>::KeyFrame& end)
{
	if (m_modifyingMovingSpeedAction)
	{
		m_actionExecution->StopAction(m_modifyingMovingSpeedAction);
		m_modifyingMovingSpeedAction = nullptr;
	}

	auto endValue = end.value;
	m_actionExecution->RunAction(
		m_modifyingMovingSpeedAction = ActionInterpolation<float>::New(
			{
				start,
				end
			},
			[&, endValue](const float& v)
			{
				m_currentMovingSpeed = v;
				if (m_currentMovingSpeed == endValue)
				{
					m_modifyingMovingSpeedAction = nullptr;
				}
			}
		)
	);
}

void CharacterScript::StopModifyMovingSpeed()
{
	if (m_modifyingMovingSpeedAction)
	{
		m_actionExecution->StopAction(m_modifyingMovingSpeedAction);
		m_modifyingMovingSpeedAction = nullptr;
	}
}

void CharacterScript::PlayAnimIdle(float dt)
{
	if (!IsAnimTransiting())
	{
		float transitTime = 0.15f;
		float instantSpeed = 2.0f;
		if (m_currentBodyState == STATE::MOVE_FAST)
		{
			transitTime = 0.3f;
			instantSpeed = 5.0f;
		}

		m_character.Transit0->FadeTo(AnimTransitLayer::TransitDirection::BACKWARD, transitTime, m_character.Animations.IdleCarefully, -1, -1);

		ModifyMovingSpeed({ instantSpeed, 0.0f }, { 0.0f, transitTime });
		TimeoutTransitingBodyState(STATE::IDLE, transitTime);
	}
	else if (m_nextBodyState != STATE::IDLE)
	{
		const float TRANSIT_TIME_FROM_SLOW = 0.15f;
		const float TRANSIT_TIME_FROM_FAST = 0.3f;

		const float SPEED_FROM_SLOW = 2.0f;
		const float SPEED_FROM_FAST = 5.0f;

		auto srcPlayer0 = dynamic_cast<AnimPlayerLayer*>(m_character.Blend01->GetInput(0));

		auto currentBlendFactor = m_character.Blend01->GetBlendFactor();

		float srcTransitTime, destTransitTime, srcInstantSpeed, destInstantSpeed; int dir;
		if (srcPlayer0->GetAnimation() == m_character.Animations.RunSlow && m_currentBodyState == STATE::MOVE_SLOW)
		{
			// blend from 0->1, slow->fast
			srcTransitTime = TRANSIT_TIME_FROM_SLOW;
			destTransitTime = TRANSIT_TIME_FROM_FAST;
			srcInstantSpeed = SPEED_FROM_SLOW;
			destInstantSpeed = SPEED_FROM_FAST;
			dir = 0;
		}
		else if (srcPlayer0->GetAnimation() != m_character.Animations.RunSlow && m_currentBodyState == STATE::MOVE_SLOW)
		{
			// blend from 1->0, slow->fast
			srcTransitTime = TRANSIT_TIME_FROM_SLOW;
			destTransitTime = TRANSIT_TIME_FROM_FAST;
			srcInstantSpeed = SPEED_FROM_SLOW;
			destInstantSpeed = SPEED_FROM_FAST;
			currentBlendFactor = 1 - currentBlendFactor;
			dir = 0;
		}
		else if (srcPlayer0->GetAnimation() == m_character.Animations.RunSlow && m_currentBodyState != STATE::MOVE_SLOW)
		{
			// blend from 1->0, fast->slow
			srcTransitTime = TRANSIT_TIME_FROM_FAST;
			destTransitTime = TRANSIT_TIME_FROM_SLOW;
			srcInstantSpeed = SPEED_FROM_FAST;
			destInstantSpeed = SPEED_FROM_SLOW;
			currentBlendFactor = 1 - currentBlendFactor;
			dir = 1;
		}
		else if (srcPlayer0->GetAnimation() != m_character.Animations.RunSlow && m_currentBodyState != STATE::MOVE_SLOW)
		{
			// blend from 0->1, fast->slow
			srcTransitTime = TRANSIT_TIME_FROM_FAST;
			destTransitTime = TRANSIT_TIME_FROM_SLOW;
			srcInstantSpeed = SPEED_FROM_FAST;
			destInstantSpeed = SPEED_FROM_SLOW;
			dir = 1;
		}

		auto transitTime = Lerp(srcTransitTime, destTransitTime, currentBlendFactor);
		auto instantSpeed = Lerp(srcInstantSpeed, destInstantSpeed, currentBlendFactor);

		m_character.Transit0->FadeTo(AnimTransitLayer::TransitDirection::BACKWARD, transitTime, m_character.Animations.IdleCarefully, -1, -1);

		//AnimationUtils::TransitChangeDirection(m_actionExecution, m_character.Blend01, transitTime, dir);
		m_character.Blend01->StartBlending(
			std::make_shared<FunctionLinear1D>(0, m_character.Blend01->GetBlendFactor()),
			0, transitTime
		);

		m_actionExecution->StopAction(m_switchRunSlowFastActionAnim);

		SetTimeout(transitTime, 
			[&]()
			{
				auto playerLayer = dynamic_cast<AnimPlayerLayer*>(m_character.Blend01->GetMainLayer());
				assert(playerLayer);
				playerLayer->SetAnimation(m_character.Animations.IdleCarefully, -1, -1);
			}
		);

		ModifyMovingSpeed({ instantSpeed, 0.0f }, { 0.0f, transitTime });
		TimeoutTransitingBodyState(STATE::IDLE, transitTime + MIN_FRAMETIME);
	}
}

void CharacterScript::PlayAnimRun(float dt)
{
	float transitTime = 0.15f;

	m_character.Transit0->FadeTo(AnimTransitLayer::TransitDirection::FORWARD, transitTime, m_character.Animations.RunSlow, -1, -1);

	ModifyMovingSpeed({ 0.0f, 0.0f }, { m_slowRunMovingSpeed, transitTime });
	TimeoutTransitingBodyState(STATE::MOVE_SLOW, transitTime);
}

void CharacterScript::PlayAnimSwitchRunSlowFast(float dt)
{
	// mode 1: hold LSHIFT to sprint
	{
		if (Input()->IsKeyDown(KEYBOARD::LSHIFT) && m_currentBodyState == STATE::MOVE_SLOW)
		{
			// switch to fast run
			float transitTime = 0.5f;

			m_switchRunSlowFastActionAnim = AnimationUtils::Transit(m_actionExecution, m_character.Blend01, m_character.Animations.RunSlow, m_character.Animations.RunFast, transitTime);

			TimeoutTransitingBodyState(STATE::MOVE_FAST, transitTime + MIN_FRAMETIME);

			ModifyMovingSpeed({ m_currentMovingSpeed, 0.0f }, { m_fastRunMovingSpeed, transitTime });
		}

		if (!Input()->IsKeyDown(KEYBOARD::LSHIFT) && m_currentBodyState == STATE::MOVE_FAST)
		{
			// switch to slow run
			float transitTime = 0.5f;

			m_switchRunSlowFastActionAnim = AnimationUtils::Transit(m_actionExecution, m_character.Blend01, m_character.Animations.RunFast, m_character.Animations.RunSlow, transitTime);

			TimeoutTransitingBodyState(STATE::MOVE_SLOW, transitTime + MIN_FRAMETIME);

			ModifyMovingSpeed({ m_currentMovingSpeed, 0.0f }, { m_slowRunMovingSpeed, transitTime });
		}
	}
}

void CharacterScript::PlayAnimTurnFromIdle(float dt)
{
	float transitTime = 0.15f;

	auto angle = std::abs(AngleBetween(m_characterForward, m_currentExpectedMovingDir));
	if (angle >= PI / 3.0f && m_nextBodyState == m_currentBodyState && (m_currentBodyState == STATE::IDLE))
	{
		//std::cout << angle << "\n";

		auto& right = Mat4::Rotation(m_controller->CCTGetRotation()).Right();
		int dir = right.Dot(m_currentExpectedMovingDir) > 0 ? 1 : 0;

		SharedPtr<Animation> turnAnimation;
		float duration = 0.0f;
		float coeff = 0.0f;
		if (angle <= PI / 2.0f)
		{
			turnAnimation = dir == 0 ? m_character.Animations.IdleTurnLeft90 : m_character.Animations.IdleTurnRight90;
			duration = 0.35f - transitTime;
			coeff = angle / (PI / 2.0f);
		}
		else
		{
			turnAnimation = dir == 0 ? m_character.Animations.IdleTurnLeft180 : m_character.Animations.IdleTurnRight180;
			duration = 0.7f - transitTime;
			coeff = angle / (PI);
		}

		// play turn animation
		//if ()
		{
			m_character.Transit0->FadeTo(AnimTransitLayer::TransitDirection::FORWARD, transitTime, turnAnimation, -1, -1);

			// let animator control this cct
			m_animator->SetForwardCCT(m_controller, m_characterUp);

			auto srcPlayer = dynamic_cast<AnimPlayerLayer*>(m_character.Transit0->GetInput());
			assert(srcPlayer);

			srcPlayer->SetDuration(duration);

			TimeoutTransitingBodyState(STATE::TURN, 0.15f);

			SetTimeout(duration * coeff - MIN_FRAMETIME,
				[&]()
				{
					m_animator->SetForwardCCT(nullptr);

					m_character.Transit0->FadeTo(AnimTransitLayer::TransitDirection::FORWARD, 0.15f,
						m_character.Animations.IdleCarefully, -1, -1);

					TimeoutTransitingBodyState(STATE::IDLE, 0.17f);
				}
			);
		}
	}
}

void CharacterScript::PlayAnimJump(float dt)
{
	if (m_fallingUpdateAction)
	{
		return;
	}

	if (m_currentBodyState == STATE::MOVE_SLOW && !IsAnimTransiting())
	{
		// jump from slow run
		m_currentJumpingMovingSpeed = 6.0f;
		PlayAnimJumpFromRunning(dt, m_character.Animations.RunJumpInPlace, 0.15f, 0.377f, 6.0f);
	}

	if (m_currentBodyState == STATE::MOVE_FAST && !IsAnimTransiting())
	{
		// jump from fast run
		m_currentJumpingMovingSpeed = 6.0f;
		PlayAnimJumpFromRunning(dt, m_character.Animations.RunJumpInPlace, 0.15f, 0.377f, 7.0f);
	}

	if ((m_currentBodyState == STATE::MOVE_SLOW || m_currentBodyState == STATE::MOVE_FAST 
		|| (m_currentBodyState == STATE::IDLE && m_nextBodyState == STATE::MOVE_SLOW)) 
		&& IsAnimTransiting())
	{
		m_character.Blend01->SetEnabled(false);
		StopTimeoutTransitingBodyState(STATE::JUMP);
		StopModifyMovingSpeed();

		if (m_switchRunSlowFastActionAnim)
		{
			m_actionExecution->StopAction(m_switchRunSlowFastActionAnim);
			m_switchRunSlowFastActionAnim = nullptr;
		}

		auto upVelocity = m_currentBodyState == STATE::IDLE ? 6.0f : 7.0f;

		m_currentJumpingMovingSpeed = 6.0f;
		PlayAnimJumpFromRunning(dt, m_character.Animations.RunJumpInPlace, 0.15f, 0.377f, upVelocity);
	}

	if (m_currentBodyState == STATE::IDLE && !IsAnimTransiting())
	{
		// jump from idle
		m_currentJumpingMovingSpeed = 4.0f;
		PlayAnimJumpFromIdle(dt);
	}
}

void CharacterScript::PlayAnimJumpFromRunning(float dt, const SharedPtr<Animation>& anim, float transitTime, float animReachedTopTime, float jumpUpVelocityLength, 
	float extTimeTillApplyingJumpUpVelocity)
{
	auto& g = GetScene()->GetPhysicsSystem()->GetGravity();

	//const float transitTime = 0.15f;
	m_character.Transit0->FadeTo(AnimTransitLayer::TransitDirection::BACKWARD, transitTime, anim, -1, -1);

	m_nextBodyState = STATE::JUMP;
	m_currentBodyState = STATE::JUMP;

	//constexpr static float jumpUpVelocityLength = 6.0f;

	SetTimeout(transitTime + extTimeTillApplyingJumpUpVelocity,
		[&, jumpUpVelocityLength]()
		{
			//m_animator->SetForwardCCT(m_controller, m_characterUp);
			auto srcPlayer = dynamic_cast<AnimPlayerLayer*>(m_character.Transit0->GetInput());
			if (srcPlayer)
			{
				srcPlayer->SetEnableRootMotion(true, true, false);
			}

			//m_controller->CCTSetAdditionRotationEnabled(true);
			m_controller->CCTApplyVelocity(/*m_characterForward * 6.0f +*/  Vec3(0, jumpUpVelocityLength, 0));

			StartJumpingToClimpingUpdate();
		}
	);

	//const float reachedTopTime = 0.377f + transitTime;
	const float reachedTopTime = animReachedTopTime + transitTime;

	m_prepareFallingUpdateActions[0] = SetTimeout(reachedTopTime,
		[&]()
		{
			auto srcPlayer = dynamic_cast<AnimPlayerLayer*>(m_character.Transit0->GetInput());
			if (srcPlayer)
			{
				srcPlayer->SetEnableRootMotion(true, true, true);
			}
			//m_animator->SetForwardCCT(nullptr);
		}
	);

	m_prepareFallingUpdateActions[1] = SetTimeout(reachedTopTime - 0.15f,
		[&]()
		{
			m_character.Transit0->FadeTo(AnimTransitLayer::TransitDirection::FORWARD, 0.6f, m_character.Animations.FallIdle, -1, -1);
		}
	);

	m_currentMovingSpeed = 0.0f;
	float t0 = jumpUpVelocityLength / g.Length() + transitTime;
	//TimeoutTransitingBodyState(STATE::FALL, t0);
	m_prepareFallingUpdateActions[2] = SetTimeout(t0,
		[&]()
		{
			m_nextBodyState = STATE::FALL;
			m_currentBodyState = STATE::FALL;

			m_actionExecution->RunAction(
				m_fallingUpdateAction = ActionRepeatUntil::New(
					ActionCallback::New(
						[&]()
						{
							FallingUpdate(GetScene()->Dt());
						}
					),
					nullptr
				)
			);
		}
	);
}

void CharacterScript::PlayAnimJumpFromIdle(float dt)
{
	auto& g = GetScene()->GetPhysicsSystem()->GetGravity();

	const float transitTime = 0.15f;
	m_character.Transit0->FadeTo(AnimTransitLayer::TransitDirection::BACKWARD, transitTime, m_character.Animations.IdleJumpUp, -1, -1);

	m_nextBodyState = STATE::JUMP_FROM_IDLE;
	m_currentBodyState = STATE::JUMP_FROM_IDLE;

	constexpr static float jumpUpVelocityLength = 6.0f;

	SetTimeout(0.15f + 0.533f,
		[&]()
		{
			m_nextBodyState = STATE::JUMP;
			m_currentBodyState = STATE::JUMP;
			m_controller->CCTApplyVelocity(/*m_characterForward * 6.0f +*/  Vec3(0, jumpUpVelocityLength, 0));

			StartJumpingToClimpingUpdate();
		}
	);

	m_prepareFallingUpdateActions[0] = SetTimeout(m_character.Animations.IdleJumpUp->GetDuration() + transitTime - 0.2f,
		[&]()
		{
			m_character.Transit0->FadeTo(AnimTransitLayer::TransitDirection::FORWARD, 0.4f, m_character.Animations.FallIdle, -1, -1);
		}
	);

	m_currentMovingSpeed = 0.0f;
	float t0 = jumpUpVelocityLength / g.Length() + transitTime;
	t0 = std::max(t0, m_character.Animations.IdleJumpUp->GetDuration() - 0.533f + transitTime) + 0.533f;
	//TimeoutTransitingBodyState(STATE::FALL, t0);
	m_prepareFallingUpdateActions[1] = SetTimeout(t0,
		[&]()
		{
			m_nextBodyState = STATE::FALL;
			m_currentBodyState = STATE::FALL;
			m_actionExecution->RunAction(
				m_fallingUpdateAction = ActionRepeatUntil::New(
					ActionCallback::New(
						[&]()
						{
							FallingUpdate(GetScene()->Dt());
						}
					),
					nullptr
				)
			);
		}
	);
}

PhysicsQueryHitType::ENUM CharacterScript::FallingSweepFilter::PrevFilter(GameObject* obj, PhysicsShape* shape, PhysicsHitFlags& flags)
{
	if (obj->GetRoot() == m_script->GetGameObject()->GetRoot())
	{
		return PhysicsQueryHitType::ENUM::IGNORE;
	}

	return PhysicsQueryHitType::ENUM::BLOCK;
}

PhysicsQueryHitType::ENUM CharacterScript::FallingSweepFilter::PostFilter(GameObject* obj, PhysicsShape* shape, const PhysicsQueryHit& hit)
{
	return PhysicsQueryHitType::ENUM();
}

void CharacterScript::FallingUpdate(float dt)
{
	std::vector<Vec3> points;
	points.reserve(10);
	const float dtSample = 0.16f;
	//const size_t numSamples = 100;

	float deltaHeight = 0.0f;
	auto g = m_controller->GetGravity();
	auto velocity = m_controller->GetVelocity() + m_fallingMovingMotion;

	Transform start = Transform::FromTransformMatrix(GetGameObject()->GetCommittedGlobalTransform());
	auto startRotationMat = start.Rotation().ToMat4();
	start.Rotation() = Mat4(
		Vec4(startRotationMat.Up(), 0.0f),
		Vec4(startRotationMat.Forward(), 0.0f),
		Vec4(startRotationMat.Right(), 0.0f),
		Vec4(0.0f, 0.0f, 0.0f, 1.0f)
	);

	//std::cout << "groundCount: " << m_controller->CCTGetCollisionPlanes().groundCount << "\n";

	auto startPosition = start.Position();
	auto pos = start.Position();

	points.push_back(pos);

	/*if (m_fallingSweepFilter)
	{
		int x = 3;
	}*/

	while (deltaHeight < 2.0f)
	{
		pos += velocity * dtSample;

		if (velocity.Length() > 0.0001f)
			points.push_back(pos);

		velocity += g * dtSample;

		deltaHeight = startPosition.y - pos.y;

		//assert(deltaHeight >= 0);
	}

	if (points.size() > 1)
	{
		auto physicsSystem = GetScene()->GetPhysicsSystem();
		auto shape = m_controller->GetShape(0);

		auto localState = std::make_shared<FallingSweepLocalState>();

		auto serialId = Physics()->BeginSerialQuery(
			[&, localState](PhysicsSystem* sys, const ActionPhysicsQuery* prev, const ActionPhysicsQuery*) -> bool
			{
				auto sweepPrev = dynamic_cast<const ActionPhysicsSweep*>(prev);
				if (localState->stopSerialQuery || (sweepPrev && sweepPrev->HitOrTouchAnything()))
				{
					localState->stopSerialQuery = true;
					return false;
				}
				return true;
			}
		);

		for (size_t i = 0; i < points.size() - 1; i++)
		{
			auto& begin = points[i];
			auto& end = points[i + 1];

			Transform transform = start;
			transform.Position() = begin;

			assert(begin != end);

			m_actionExecution->RunAction(
				Physics()->SerialSweep(
					serialId,
					[&](const ActionPhysicsSweep* action, const PhysicsSweepResult& result)
					{
						LandingUpdate(GetScene()->Dt(), action, result);
					},
					shape, transform, end - begin, m_fallingSweepFilter
				)
			);
		}

		Physics()->EndSerialQuery(serialId);
	}

	//auto debugGraphics = Graphics::Get()->GetDebugGraphics();
	//if (debugGraphics)
	//{
	//	if (points.size() > 1)
	//	{
	//		auto cct = ((CharacterControllerCapsule*)m_controller);

	//		//std::cout << "Num points: " << points.size() << '\n';
	//		for (size_t i = 0; i < points.size() - 1; i++)
	//		{
	//			auto& begin = points[i];
	//			auto& end = points[i + 1];

	//			if (begin != end)
	//			{
	//				debugGraphics->DrawLineSegment(begin, end);
	//			}

	//			debugGraphics->DrawCapsule(
	//				Capsule(start.Rotation().ToMat4().Right().Normal(), begin, cct->CCTGetHeight(), cct->CCTGetRadius())
	//			);
	//		}
	//	}
	//}
}

void CharacterScript::LandingUpdate(float dt, const ActionPhysicsSweep* action, const PhysicsSweepResult& result)
{
	auto FnSwitchBackToFalling = [&]()
	{
		if (m_landingTransitAction)
		{
			m_actionExecution->StopAction(m_landingTransitAction);
			m_landingTransitAction = nullptr;
		}

		m_currentBodyState = STATE::FALL;
		m_nextBodyState = STATE::FALL;
		m_currentMovingSpeed = 0;
		m_character.Transit0->FadeTo(AnimTransitLayer::TransitDirection::FORWARD, 0.2f, m_character.Animations.FallIdle, -1, -1);
	};

	if (!action->HitOrTouchAnything())
	{
		if (m_currentBodyState == STATE::LANDING)
		{
			FnSwitchBackToFalling();
		}

		return;
	}

	/*static CollisionContactPoint cpoint;
	auto debugGraphics = Graphics::Get()->GetDebugGraphics();
	if (debugGraphics)
	{
		debugGraphics->DrawRay(cpoint.position, cpoint.normal);
	}*/

	if (!(m_currentBodyState == STATE::LANDING || m_currentBodyState == STATE::FALL))
	{
		return;
	}

	//std::cout << "Will touch ground\n";

	auto& pos = GetGameObject()->GetCommittedGlobalTransform().Position();
	auto g = m_controller->GetGravity();
	auto nG = g.Normal();
	auto invG = -nG;

	auto slopeLimit = std::cos(ToRadians(80));//m_controller->CCTGetSlopeLimit();

	auto capsuleRadius = ((CharacterControllerCapsule*)m_controller)->CCTGetRadius();
	auto capsuleRadiusToAllowLanding = capsuleRadius * 0.4f;

	auto capsuleHeight = ((CharacterControllerCapsule*)m_controller)->CCTGetHeight();

	bool hasGround = false;
	if (!m_controller->GetCollision()->contacts.empty())
	{
		//auto slopeLimit = m_controller->CCTGetSlopeLimit();

		for (auto& contact : m_controller->GetCollision()->contacts)
		{
			bool isA = contact->A == m_controller->GetGameObject() ? true : false;
			for (auto& pair : contact->contactPairs)
			{
				for (auto& point : pair->contactPoints)
				{
					auto normal = isA ? point.normal : -point.normal;
					auto isGround = invG.Dot(normal) >= slopeLimit;

					auto distanceFromUpAxis = (Vec3(pos.x, 0, pos.z) - Vec3(point.position.x, 0, point.position.z)).Length();

					if (isGround && distanceFromUpAxis > capsuleRadiusToAllowLanding)
					{
						auto capsule = m_controller->GetShape(0);
						auto touchShape = isA ? pair->BShape : pair->AShape;

						PhysicsShapeRaycastResult result;
						if (touchShape->Raycast(result, pos, nG * (capsuleHeight / 2.0f + capsuleRadius + 0.7f)))
						{
							distanceFromUpAxis = 0.0f;
						}
					}

					if (isGround && distanceFromUpAxis <= capsuleRadiusToAllowLanding)
					{
						//cpoint = point;
						//cpoint.normal = normal;
						hasGround = true;
						goto End;
					}
				}
			}
		}

	End:
		if (hasGround && m_currentBodyState == STATE::FALL)
		{
			PlayAnimSoftLanding(dt, 0);
			return;
		}
	}

	auto sweepDir = action->GetSweepDistance().Normal();
	auto sweepStartPos = action->GetStartTransform().Position();

	float nearestGroundDistance = INFINITY;
	float distanceFromUpAxis = INFINITY;
	//int nearestGroundIdx = -1;

	Vec3 touchPos;
	Vec3 touchNormal;

	auto FnMinDistance = [&](const PhysicsSweepHit& touch)
	{
		if (touch.distance <= 0)
		{
			return;
		}

		auto d = std::abs(touch.position.y - pos.y);//.Length();
		if (d < nearestGroundDistance)
		{
			bool isGround = invG.Dot(touch.normal) >= slopeLimit;
			if (isGround)
			{
				//nearestGroundIdx = &touch - result.touches.data();
				nearestGroundDistance = d;

				auto p = sweepStartPos + sweepDir * touch.distance;

				distanceFromUpAxis = (Vec3(p.x, 0, p.z) - Vec3(touch.position.x, 0, touch.position.z)).Length();
				touchPos = touch.position;
				touchNormal = touch.normal;
			}
		}
	};

	if (result.hasBlock)
	{
		FnMinDistance(result.block);
	}

	for (auto& touch : result.touches)
	{
		FnMinDistance(touch);
	}

	assert(m_currentBodyState == STATE::LANDING || m_currentBodyState == STATE::FALL);

	if (nearestGroundDistance != INFINITY && !hasGround)
	{
		// has ground

		//std::cout << "Has ground\n";

		//std::cout << "distanceFromUpAxis: " << distanceFromUpAxis << "\n";
		auto debugGraphics = Graphics::Get()->GetDebugGraphics();
		if (debugGraphics)
		{
			debugGraphics->DrawRay(touchPos, touchNormal);
		}

		if (nearestGroundDistance <= 2.0f && distanceFromUpAxis <= capsuleRadiusToAllowLanding && m_currentBodyState == STATE::FALL)
		{
			//m_controller->CCTSetAdditionRotationEnabled(false);
			//m_controller->CCTApplyVelocity(m_currentExpectedMovingDir * 2.0f);

			PlayAnimSoftLanding(dt, nearestGroundDistance);
		}
		
		if (m_currentBodyState == STATE::LANDING && (nearestGroundDistance > 2.0f || distanceFromUpAxis > capsuleRadiusToAllowLanding))
		{
			FnSwitchBackToFalling();
		}
	}
	else
	{
		// doesn't have ground

		//std::cout << "Doesn't have ground\n";

	
	}
}

void CharacterScript::PlayAnimSoftLanding(float dt, float dY)
{
	m_landingStartTime = Clock::ms::now();
	m_landingAllowMovingDeltaTime = size_t(0.3f * dY / 2.0f * 1000);//size_t(0.3f * 1000);// size_t(0.1f * (2.0f - dY) * 1000);

	m_nextBodyState = STATE::LANDING;
	m_currentBodyState = STATE::LANDING;

	m_character.Transit0->FadeTo(AnimTransitLayer::TransitDirection::FORWARD, 0.3f, m_character.Animations.FallSoftLanding, -1, -1);

	auto duration = m_character.Animations.FallSoftLanding->GetDuration();
	m_landingTransitAction = SetTimeout(duration - 0.45f,
		[&]()
		{
			m_character.Transit0->FadeTo(AnimTransitLayer::TransitDirection::BACKWARD, 0.15f, m_character.Animations.IdleCarefully, -1, -1);
			TimeoutTransitingBodyState(STATE::IDLE, 0.16f);

			m_actionExecution->StopAction(m_fallingUpdateAction);
			m_fallingUpdateAction = nullptr;

			m_landingTransitAction = nullptr;

			StopJumpingToClimbingUpdate();
		}
	);
}

bool CharacterScript::IsOnGround(float slopLimit) const
{
	if (!m_controller->GetCollision()->contacts.empty())
	{
		auto invG = -m_controller->GetGravity().Normal();
		bool hasGround = false;
		for (auto& contact : m_controller->GetCollision()->contacts)
		{
			bool isA = contact->A == m_controller->GetGameObject() ? true : false;
			for (auto& pair : contact->contactPairs)
			{
				for (auto& point : pair->contactPoints)
				{
					auto normal = isA ? point.normal : -point.normal;
					//std::cout << "dot: " << invG.Dot(normal) << "\n";

					auto debugGraphics = Graphics::Get()->GetDebugGraphics();
					if (debugGraphics)
					{
						debugGraphics->DrawRay(point.position, normal);
					}

					auto isGround = invG.Dot(normal) >= slopLimit;
					if (isGround)
					{
						return true;
					}
				}
			}
		}
	}

	return false;
}

void CharacterScript::PlayAnimFalling(float dt)
{
	m_currentBodyState = STATE::FALL;
	m_nextBodyState = STATE::FALL;
	m_currentMovingSpeed = 0;

	m_character.Transit0->FadeTo(AnimTransitLayer::TransitDirection::FORWARD, 0.5f, m_character.Animations.FallIdle, -1, -1);

	assert(m_fallingUpdateAction == nullptr);
	m_actionExecution->RunAction(
		m_fallingUpdateAction = ActionRepeatUntil::New(
			ActionCallback::New(
				[&]()
				{
					FallingUpdate(GetScene()->Dt());
				}
			),
			nullptr
		)
	);
}

void CharacterScript::StopJumpingOrFalling()
{
	for (auto& a : m_prepareFallingUpdateActions)
	{
		if (a && m_actionExecution->Contains(a))
		{
			m_actionExecution->StopAction(a);
			a = nullptr;
		}
	}

	auto srcPlayer = dynamic_cast<AnimPlayerLayer*>(m_character.Transit0->GetInput());
	if (srcPlayer)
	{
		srcPlayer->SetEnableRootMotion(true, true, true);
	}

	if (m_fallingUpdateAction)
	{
		m_actionExecution->StopAction(m_fallingUpdateAction);
		m_fallingUpdateAction = nullptr;
	}
}

// find the best climbing bar
GameObject* CharacterScript::GetClimbingBar(const PhysicsOverlapResult& result)
{
	GameObject* climbingBar = nullptr;
	float nearestDist2 = FLT_MAX;

	auto& cctGlobal = m_controller->GetGameObject()->GetCommittedGlobalTransform();
	auto& cctOrigin = cctGlobal.Position();
	auto cctForward = cctGlobal.Forward().Normal();

	auto FnChooseBest = [&](GameObject* obj)
	{
		auto& global = obj->GetCommittedGlobalTransform();
		auto& barPos = global.Position();

		auto bar = obj->GetComponentRaw<ClimbingBar>();
		auto halfDistance = bar->GetHalfDistance();

		Line line = Line(barPos, global.Right());
		Plane p = Plane(cctOrigin, global.Right());

		Vec3 point;
		if (line.Intersect(p, point))
		{
			bool isPointInsideBar = (barPos - point).Length2() <= halfDistance * halfDistance;
			if (isPointInsideBar)
			{
				auto v = point - cctOrigin;
				if (cctForward.Dot(v.Normal()) < 0.5f)
				{
					// get the climbing bar that is forward to cct
					return;
				}

				auto d2 = v.Length2();
				if (d2 <= (1.0f * 1.0f) && nearestDist2 > d2)
				{
					climbingBar = obj;
					nearestDist2 = d2;

					m_currentClimbingBarAnchorPoint = point;
				}
			}
		}
	};

	auto FnFindClimbingBar = [&](GameObject* obj)
	{
		if (obj->Tag() == TAG::CLIMBING_BAR)
		{
			FnChooseBest(obj);
		}

		for (auto& c : obj->Children())
		{
			if (c->Tag() == TAG::CLIMBING_BAR)
			{
				FnChooseBest(c);
			}
		}
	};

	if (result.hasBlock)
	{
		FnFindClimbingBar(result.block.obj);
	}

	for (auto& touch : result.touches)
	{
		FnFindClimbingBar(touch.obj);
	}

	return climbingBar;
}

void CharacterScript::JumpngToClimbingUpdate(float dt, const ActionPhysicsOverlap* action, const PhysicsOverlapResult& result)
{
	if ((m_currentBodyState != STATE::FALL
		&& m_currentBodyState != STATE::CLIMBING
		&& m_currentBodyState != STATE::TRANSIT_TO_CLIMBING
		&& m_currentBodyState != STATE::JUMP
		) || IsAnimTransiting())
	{
		return;
	}

	//bool isClimbable = false;
	m_currentClimbingBar = GetClimbingBar(result);
	if (m_currentClimbingBar && m_currentBodyState != STATE::TRANSIT_TO_CLIMBING && m_currentBodyState != STATE::CLIMBING)
	{
		StopJumpingOrFalling();
		StopJumpingToClimbingUpdate();

		m_currentBodyState = STATE::TRANSIT_TO_CLIMBING;
		m_nextBodyState = STATE::TRANSIT_TO_CLIMBING;

		auto& pos = m_currentClimbingBarAnchorPoint;//m_currentClimbingBar->GetCommittedGlobalTransform().Position();
		auto normal = m_currentClimbingBar->GetCommittedGlobalTransform().Forward().Normal();

		auto cctCapsule = (CharacterControllerCapsule*)m_controller;
		auto r = cctCapsule->CCTGetRadius();
		auto h = cctCapsule->CCTGetHeight();
		auto dG = cctCapsule->GetGravity().Normal();

		const auto legLengthHorizontal = 0.17f;
		const auto legLengthVertical = 0.32f;

		auto& srcPos = m_controller->GetGameObject()->GetCommittedGlobalTransform().Position();
		auto destPos = pos + normal * (r + legLengthHorizontal) + dG * (h / 2.0f + r + legLengthVertical);

		m_controller->SetGravity({ 0,0,0 });
		m_controller->CCTSetVelocity({ 0,0,0 });

		const auto moveSpeed = 5.0f;
		const auto rotSpeed = 2.0f * PI;

		auto length = (srcPos - destPos).Length();
		auto Tm = length / moveSpeed + 0.01f;

		Tm = std::clamp(Tm, 0.25f, 0.6f);

		m_climbingToBarPrevPos = srcPos;
		m_actionExecution->RunAction(
			ActionInterpolation<Vec3>::New(
				{
					{ srcPos, 0 },
					{ destPos, Tm }
				},
				[&](const Vec3& value)
				{
					auto& cctPos = m_climbingToBarPrevPos;
					auto d = value - cctPos;
					m_controller->Move(d);
					m_climbingToBarPrevPos = value;

					//auto debugGraphics = Graphics::Get()->GetDebugGraphics();
					//debugGraphics->DrawRay(pos, destPos - pos);
				}
			)
		);

		auto& srcRot = m_controller->CCTGetRotation();
		Quaternion destRot = Mat4(
			Vec4(-Vec3::UP.Cross(normal).Normal(), 0.0f),
			Vec4(Vec3::UP, 0.0f),
			Vec4(-normal, 0.0f),
			Vec4(0, 0, 0, 1)
		);

		auto angle = AngularDistance(destRot, srcRot);
		auto Ta = angle / rotSpeed + 0.01f;

		Ta = std::clamp(Ta, 0.25f, 0.6f);

		m_actionExecution->RunAction(
			ActionInterpolation<Quaternion>::New(
				{
					{ srcRot, 0 },
					{ destRot, Ta }
				},
				[&](const Quaternion& value)
				{
					m_controller->CCTSetRotation(value);
				}
			)
		);

		auto animTransitTime = std::max(Ta, Tm);
		m_character.Transit0->FadeTo(AnimTransitLayer::TransitDirection::FORWARD, animTransitTime, m_character.Animations.HangIdle, -1, -1);

		SetTimeout(animTransitTime + MIN_FRAMETIME,
			[&]() 
			{
				m_currentBodyState = STATE::CLIMBING;
				m_nextBodyState = STATE::CLIMBING;
			}
		);
	}
}

void CharacterScript::ClimbingUpdate(float dt)
{
}

void CharacterScript::StartJumpingToClimpingUpdate()
{
	assert(m_jumpingToClimbingUpdateAction == nullptr);
	m_actionExecution->RunAction(
		m_jumpingToClimbingUpdateAction = ActionRepeatUntil::New(
			ActionCallback::New(
				[&]()
				{
					auto transform = m_controller->GetGameObject()->GetCommittedGlobalTransform();
					transform.Position() += transform.Forward().Normal() * (CharacterScript_CLIMBING_QUERY_BOX_SIZE / 2.0f);
					m_actionExecution->RunAction(
						Physics()->Overlap(
							[&](const ActionPhysicsOverlap* action, const PhysicsOverlapResult& result)
							{
								if (!action->HitOrTouchAnything())
								{
									return;
								}
								JumpngToClimbingUpdate(GetScene()->Dt(), action, result);
							},
							m_climbingQueryShape.get(), Transform::FromTransformMatrix(transform), m_fallingSweepFilter
						)
					);
				}
			),
			nullptr
		)
	);
}

void CharacterScript::StopJumpingToClimbingUpdate()
{
	if (m_jumpingToClimbingUpdateAction)
	{
		m_actionExecution->StopAction(m_jumpingToClimbingUpdateAction);
		m_jumpingToClimbingUpdateAction = nullptr;
	}
}

void CharacterScript::PlayAnimClimbUp(float dt)
{
	auto animTransitTime = 0.1f;

	m_character.Transit0->FadeTo(AnimTransitLayer::TransitDirection::FIXED, animTransitTime, m_character.Animations.HangClimbUpToCrouch, -1, -1);
	m_animator->SetForwardCCT(m_controller, m_characterUp);

	auto cct = (CharacterControllerCapsule*)m_controller;
	m_cctOriginalHeight = cct->CCTGetHeight();
	m_cctOriginalRadius = cct->CCTGetRadius();

	cct->CCTSetHeight(0.0f);
	cct->CCTSetRadius(0.1f);

	m_currentBodyState = STATE::CLIMBING_UP;
	m_nextBodyState = STATE::CLIMBING_UP;

	SetTimeout(m_character.Animations.HangClimbUpToCrouch->GetDuration() + animTransitTime - MIN_FRAMETIME * 2.0f,
		[&]()
		{
			m_animator->SetForwardCCT(nullptr);

			constexpr static float restoreTime = 0.3f;

			m_character.Transit0->FadeTo(AnimTransitLayer::TransitDirection::FORWARD, restoreTime, m_character.Animations.IdleCarefully, -1, -1);

			m_actionExecution->RunAction(
				ActionSequence::New(
					{
						ActionDelayNTicks::New(2),
						ActionCallback::New(
							[&]()
							{
								PlayAnimClimbUpRestoreCCTDimensions(restoreTime);
							}
						)
					}
				)
			);
		}
	);
}

void CharacterScript::PlayAnimClimbUpRestoreCCTDimensions(float restoreTime)
{
	//m_character.SrcPlayer0->SetEnabled(false);
	//m_character.SrcPlayer1->SetEnabled(false);

	auto cct = (CharacterControllerCapsule*)m_controller;
	m_actionExecution->RunAction(
		ActionInterpolation<float>::New(
			{
				{ cct->CCTGetHeight(), 0.0f},
				{ m_cctOriginalHeight, restoreTime }
			},
			[&, cct](const float& value)
			{
				cct->CCTSetHeight(value);
			}
		)
	);

	m_actionExecution->RunAction(
		ActionInterpolation<float>::New(
			{
				{ cct->CCTGetRadius(), 0.0f },
				{ m_cctOriginalRadius, restoreTime }
			},
			[&, cct](const float& value)
			{
				cct->CCTSetRadius(value);
			}
		)
	);

	m_climbingToBarPrevPos.y = m_controller->GetGameObject()->GetCommittedGlobalTransform().Position().y;
	auto oriY = m_cctOriginalRadius + m_cctOriginalHeight / 2.0f + m_controller->CCTGetContactOffset();
	oriY += m_currentClimbingBar->GetCommittedGlobalTransform().Position().y - 0.05f;
	m_actionExecution->RunAction(
		ActionInterpolation<float>::New(
			{
				{ m_climbingToBarPrevPos.y, 0.0f},
				{ oriY, restoreTime }
			},
			[&](const float& value)
			{
				auto v = value - m_climbingToBarPrevPos.y;
				m_controller->Move(Vec3(0, v, 0));
				m_climbingToBarPrevPos.y = value;
			}
		)
	);

	SetTimeout(restoreTime,
		[&]()
		{
			m_controller->SetGravity(GetScene()->GetPhysicsSystem()->GetGravity());
			TimeoutTransitingBodyState(STATE::IDLE, MIN_FRAMETIME * 2.0f);
		}
	);
}

void CharacterScript::DeserializeFromJson(Serializer* serializer, const json& j)
{
	Base::DeserializeFromJson(serializer, j);
}
