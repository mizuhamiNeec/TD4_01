#include "GameMovementComponent.h"

#include <algorithm>
#include <cmath>
#include <imgui.h>

#include "base/BaseCharacterComponent.h"

#include "core/ComponentRegistry.h"
#include "core/json/JsonReader.h"
#include "core/json/JsonWriter.h"

#include "engine/Engine.h"
#include "engine/EngineServices.h"
#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/unnamed/subsystem/console/ConsoleSystem.h"
#include "engine/unnamed/subsystem/console/Log.h"
#include "engine/unnamed/subsystem/console/concommand/ConVar.h"
#include "engine/unnamed/subsystem/input/InputSystem.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"

#include "game/core/collision/kinematic/BoxKinematicCollisionResolver.h"
#include "game/core/components/character/state/GameMovementStateMachine.h"

#include "state/AirMove.h"
#include "state/GroundMove.h"

namespace Unnamed {
	namespace {
		constexpr char kStateAirMove[] = "AirMove";
	}

	GameMovementComponent::~GameMovementComponent() = default;

	void GameMovementComponent::SimulateStep(
		TransformComponent* transform, const MovementFrameInput& input,
		const float         stepSeconds
	) {
		if (!transform || !mCollisionResolver || !mStateMachine) {
			return;
		}

		MovementContext context = {
			.input          = input,
			.transform      = transform,
			.velocity       = mVelocity,
			.resolver       = mCollisionResolver.get(),
			.halfExtents    = mBoxHalfExtents,
			.requestedState = ""
		};

		UpdateCollisionHull(transform);
		mStateMachine->Tick(context, stepSeconds);
		mVelocity = context.velocity;
	}

	void GameMovementComponent::OnAttached() {
		BaseCharacterComponent::OnAttached();

		mInput   = ServiceLocator::Get<InputSystem>();
		mConsole = ServiceLocator::Get<ConsoleSystem>();

		mStateMachine = std::make_unique<GameMovementStateMachine>();
		mStateMachine->Init();

		RegisterMovementStates(*mStateMachine);
		mStateMachine->ChangeState(GetInitialStateName());

		auto* physicsEngine = EngineServices::Get()->GetPhysicsEngine();
		mCollisionResolver = std::make_unique<BoxKinematicCollisionResolver>(
			physicsEngine
		);

		if (mConsole) {
			mNoclip = GetConVarSafe<bool>(mConsole, "noclip");

			mAccelerate    = GetConVarSafe<float>(mConsole, "sv_accelerate");
			mAirAccelerate = GetConVarSafe<float>(mConsole, "sv_airaccelerate");
			mMaxSpeed      = GetConVarSafe<float>(mConsole, "sv_maxspeed");
			mStopSpeed     = GetConVarSafe<float>(mConsole, "sv_stopspeed");
			mAirSpeedCap   = GetConVarSafe<float>(mConsole, "sv_airspeedcap");
			mFriction      = GetConVarSafe<float>(mConsole, "sv_friction");

			mDuckSpeed   = GetConVarSafe<float>(mConsole, "sv_duckspeed");
			mWalkSpeed   = GetConVarSafe<float>(mConsole, "sv_walkspeed");
			mSprintSpeed = GetConVarSafe<float>(mConsole, "sv_sprintspeed");

			if (mSprintSpeed) {
				mCurrentMaxWalkSpeed = mSprintSpeed->GetValue();
			}
		}
	}

	void GameMovementComponent::PrePhysicsTick(const float deltaTime) {
		BaseCharacterComponent::PrePhysicsTick(deltaTime);
	}

	void GameMovementComponent::OnTick(const float deltaTime) {
		TransformComponent* transform = GetTransform();
		if (!transform) {
			Error(
				GetComponentName(), "TransformComponentが見つからないため、移動できません。"
			);
			return;
		}

		const float    stepSec       = std::max(mSimStepSec, 1.0e-6f);
		const uint32_t requiredSteps = static_cast<uint32_t>(
			std::ceil(std::max(0.0f, deltaTime) / stepSec)
		);
		const uint32_t frameMaxSteps = std::clamp(
			std::max(mMaxSubSteps, requiredSteps),
			1u,
			32u
		);

		mAccumulator += deltaTime;
		const float maxAccumulated = stepSec * static_cast<float>(frameMaxSteps);
		mAccumulator               = std::min(mAccumulator, maxAccumulated);

		uint32_t steps = 0;
		while (mAccumulator >= stepSec && steps < frameMaxSteps) {
			SimulateStep(transform, mMoveFrameInput, stepSec);
			mAccumulator -= stepSec;
			steps++;
		}
	}

	void GameMovementComponent::PostPhysicsTick(float) {}

	std::string_view GameMovementComponent::GetStableName() const {
		return "game.GameMovement";
	}

	std::string_view GameMovementComponent::GetComponentName() const {
		return "GameMovement";
	}

	void GameMovementComponent::RegisterMovementStates(
		GameMovementStateMachine& stateMachine
	) {
		stateMachine.AddState(std::make_shared<AirMove>());
		stateMachine.AddState(std::make_shared<GroundMove>());
	}

	std::string GameMovementComponent::GetInitialStateName() const {
		return kStateAirMove;
	}

	void GameMovementComponent::UpdateCollisionHull(
		TransformComponent* transform
	) const {
		if (!transform) {
			return;
		}

		auto* boxResolver = dynamic_cast<BoxKinematicCollisionResolver*>(
			mCollisionResolver.get()
		);
		if (boxResolver) {
			boxResolver->UpdateHull(transform->Position(), mBoxHalfExtents);
		}
	}

#ifdef _DEBUG
	void GameMovementComponent::DrawInspectorImGui() {
		BaseCharacterComponent::DrawInspectorImGui();

		const bool noclip = mNoclip ? mNoclip->GetValue() : false;
		ImGui::Text("noclip: %s", noclip ? "true" : "false");
	}
#endif

	void GameMovementComponent::Deserialize(const JsonReader& reader) {
		BaseCharacterComponent::Deserialize(reader);
	}

	void GameMovementComponent::Serialize(JsonWriter& writer) const {
		BaseCharacterComponent::Serialize(writer);
	}

	TransformComponent* GameMovementComponent::GetTransform() const {
		Entity* owner = GetOwner();
		return owner ? owner->GetComponent<TransformComponent>() : nullptr;
	}

	REGISTER_COMPONENT(GameMovementComponent);
}
