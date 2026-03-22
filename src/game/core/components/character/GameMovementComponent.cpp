#include "GameMovementComponent.h"

#include <algorithm>
#include <cmath>

#include "base/BaseCharacterComponent.h"

#include "core/ComponentRegistry.h"
#include "core/json/JsonReader.h"
#include "core/json/JsonWriter.h"

#include "engine/Engine.h"
#include "engine/EngineServices.h"
#include "engine/scene/Scene.h"
#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/components/kinematic/KinematicMoverComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/unnamed/subsystem/console/ConsoleSystem.h"
#include "engine/unnamed/subsystem/console/Log.h"
#include "engine/unnamed/subsystem/console/concommand/ConVar.h"
#include "engine/unnamed/subsystem/input/InputSystem.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"
#include "engine/world/World.h"

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
			.input                 = input,
			.transform             = transform,
			.velocity              = mVelocity,
			.resolver              = mCollisionResolver.get(),
			.halfExtents           = mBoxHalfExtents,
			.requestedState        = "",
			.isGrounded            = mSupportCache.grounded,
			.supportEntityGuid     = mSupportCache.supportEntityGuid,
			.supportLinearVelocity = mSupportCache.supportLinearVelocity,
			.supportStepDelta      = mSupportCache.supportStepDelta,
			.jumpSnapDisableRemaining = mJumpSnapDisableRemaining
		};

		UpdateCollisionHull(transform);
		mStateMachine->Tick(context, stepSeconds);
		mVelocity = context.velocity;
		mGrounded = context.isGrounded;
		mJumpSnapDisableRemaining = std::max(
			0.0f,
			context.jumpSnapDisableRemaining
		);

		if (context.isGrounded) {
			mSupportCache.grounded          = true;
			mSupportCache.supportEntityGuid = context.supportEntityGuid;
			mSupportCache.supportLinearVelocity = ResolveSupportLinearVelocity(
				context.supportEntityGuid
			);
			mSupportCache.supportStepDelta = ResolveSupportStepDelta(
				context.supportEntityGuid, stepSeconds
			);
		} else {
			mSupportCache.grounded            = false;
			mSupportCache.supportEntityGuid   = 0;
			mSupportCache.supportLinearVelocity = Vec3::zero;
			mSupportCache.supportStepDelta      = Vec3::zero;
		}

		Physics::Hit hit;
		auto*        phys = mCollisionResolver->GetPhysics();

		if (
			phys->SphereCast(
				transform->Position(), mBoxHalfExtents.x,
				mMoveFrameInput.forward, 65535.0f, &hit
			)
		) {
			GetWorld()->GetDebugDraw().DrawSphere(
				hit.pos + hit.normal * mBoxHalfExtents.x,
				Quaternion::identity,
				mBoxHalfExtents.x,
				Vec4::red
			);

			GetWorld()->GetDebugDraw().DrawRay(
				hit.pos, hit.normal, Vec4::magenta
			);
		}
	}

	void GameMovementComponent::OnAttached() {
		BaseCharacterComponent::OnAttached();

		mInput   = ServiceLocator::Get<InputSystem>();
		mConsole = ServiceLocator::Get<ConsoleSystem>();

		mStateMachine = std::make_unique<GameMovementStateMachine>();
		mStateMachine->Init();

		RegisterMovementStates(*mStateMachine);
		mStateMachine->ChangeState(GetInitialStateName());

		auto* physicsEngine = GetWorld() ?
			                      &GetWorld()->GetPhysicsEngine() :
			                      nullptr;
		mCollisionResolver = std::make_unique<BoxKinematicCollisionResolver>(
			physicsEngine
		);

		mSupportCache = {};
		mJumpSnapDisableRemaining = 0.0f;
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

		GetWorld()->GetDebugDraw().DrawArrow(
			transform->Position(),
			mVelocity * 0.25f,
			Vec4::yellow,
			0.125f
		);

		const float    stepSec       = std::max(mSimStepSec, 1.0e-6f);
		const uint32_t requiredSteps = static_cast<uint32_t>(
			std::ceil(std::max(0.0f, deltaTime) / stepSec)
		);
		const uint32_t frameMaxSteps = std::clamp(
			std::max(mMaxSubSteps, requiredSteps),
			1u,
			32u
		);

		mAccumulator               += deltaTime;
		const float maxAccumulated =
			stepSec * static_cast<float>(frameMaxSteps);
		mAccumulator = std::min(mAccumulator, maxAccumulated);

		if (mSupportCache.grounded && mSupportCache.supportEntityGuid != 0) {
			mSupportCache.supportLinearVelocity = ResolveSupportLinearVelocity(
				mSupportCache.supportEntityGuid
			);
			mSupportCache.supportStepDelta = ResolveSupportStepDelta(
				mSupportCache.supportEntityGuid, stepSec
			);

			// 動床コライダーはこの時点で当フレーム最終位置へ更新済みなので、
			// プレイヤー追従も同フレームで一括適用して時差由来のガタつきを防ぐ。
			const Vec3 supportFrameDelta = ResolveSupportStepDelta(
				mSupportCache.supportEntityGuid,
				std::max(0.0f, deltaTime)
			);
			if (!supportFrameDelta.IsZero()) {
				transform->SetPosition(transform->Position() + supportFrameDelta);
			}
		} else {
			mSupportCache.supportLinearVelocity = Vec3::zero;
			mSupportCache.supportStepDelta      = Vec3::zero;
		}

		UpdateCollisionHull(transform);

		uint32_t steps = 0;
		while (mAccumulator >= stepSec && steps < frameMaxSteps) {
			SimulateStep(transform, mMoveFrameInput, stepSec);
			mAccumulator -= stepSec;
			steps++;
		}

		// GameMovement は Transform の TickGroup より後で位置を書き換えるため、
		// 同フレーム描画での1フレーム遅延を防ぐためにここで行列を確定する。
		transform->OnTick(0.0f);
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

	Vec3 GameMovementComponent::ResolveSupportLinearVelocity(
		const uint64_t supportEntityGuid
	) const {
		const Entity* owner = GetOwner();
		if (supportEntityGuid == 0 ||
		    (owner && supportEntityGuid == owner->GetGuid())) {
			return Vec3::zero;
		}

		const World* world = GetWorld();
		const Scene* scene = world ? world->GetScenePtr() : nullptr;
		if (!scene) {
			return Vec3::zero;
		}

		const Entity* supportEntity = scene->FindEntity(supportEntityGuid);
		if (!supportEntity || !supportEntity->IsActive()) {
			return Vec3::zero;
		}

		const auto* mover = supportEntity->GetComponent<KinematicMoverComponent>(
		);
		if (!mover || mover->WasTeleported()) {
			return Vec3::zero;
		}

		return mover->GetLinearVelocity();
	}

	Vec3 GameMovementComponent::ResolveSupportStepDelta(
		const uint64_t supportEntityGuid, const float stepSeconds
	) const {
		const Entity* owner = GetOwner();
		if (supportEntityGuid == 0 ||
		    (owner && supportEntityGuid == owner->GetGuid())) {
			return Vec3::zero;
		}

		const World* world = GetWorld();
		const Scene* scene = world ? world->GetScenePtr() : nullptr;
		if (!scene) {
			return Vec3::zero;
		}

		const Entity* supportEntity = scene->FindEntity(supportEntityGuid);
		if (!supportEntity || !supportEntity->IsActive()) {
			return Vec3::zero;
		}

		const auto* mover = supportEntity->GetComponent<KinematicMoverComponent>(
		);
		if (!mover || mover->WasTeleported()) {
			return Vec3::zero;
		}

		return mover->GetStepDelta(stepSeconds);
	}

	REGISTER_COMPONENT(GameMovementComponent);
}
