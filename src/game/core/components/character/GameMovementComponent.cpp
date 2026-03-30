#include "GameMovementComponent.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>

#ifdef _DEBUG
#include <imgui.h>
#endif

#include "base/BaseCharacterComponent.h"

#include "core/ComponentRegistry.h"
#include "core/json/JsonReader.h"
#include "core/json/JsonWriter.h"

#include "engine/scene/Scene.h"
#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/components/kinematic/KinematicMoverComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/unnamed/subsystem/console/ConsoleSystem.h"
#include "engine/unnamed/subsystem/console/Log.h"
#include "engine/unnamed/subsystem/console/concommand/ConVar.h"
#include "engine/unnamed/subsystem/input/InputSystem.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"
#include "engine/world/GameplayCueBus.h"
#include "engine/world/World.h"

#include "game/core/collision/kinematic/BoxKinematicCollisionResolver.h"
#include "game/core/components/character/state/GameMovementStateMachine.h"

#include "state/AirMove.h"
#include "state/GroundMove.h"
#include "state/NoclipMove.h"

namespace Unnamed {
	namespace {
		constexpr char kStateAirMove[]        = "AirMove";
		constexpr int  kMaxPassiveOverlapHits = 64;

		struct PassiveMoverInfluence {
			uint64_t guid      = 0;
			Vec3     stepDelta = Vec3::zero;
		};
	}

	GameMovementComponent::~GameMovementComponent() = default;

	void GameMovementComponent::SimulateStep(
		TransformComponent* transform, const MovementFrameInput& input,
		const float         stepSeconds
	) {
		if (!transform || !mCollisionResolver || !mStateMachine) {
			return;
		}

		const bool        wasGrounded            = mSupportCache.grounded;
		const float       preStepVerticalSpeedHu = Math::MtoH(mVelocity.y);
		const std::string previousStateName      = ToLowerCopy(
			mStateMachine->GetCurrentState() ?
				mStateMachine->GetCurrentState()->GetStateName() :
				""
		);

		(void)ApplyPassiveMotionStep(transform, stepSeconds);

		MovementContext context = {
			.input                    = input,
			.transform                = transform,
			.velocity                 = mVelocity,
			.resolver                 = mCollisionResolver.get(),
			.halfExtents              = mBoxHalfExtents,
			.requestedState           = "",
			.isGrounded               = mSupportCache.grounded,
			.supportEntityGuid        = mSupportCache.supportEntityGuid,
			.supportLinearVelocity    = mSupportCache.supportLinearVelocity,
			.supportStepDelta         = mSupportCache.supportStepDelta,
			.jumpSnapDisableRemaining = mJumpSnapDisableRemaining,
			.movementComponent        = this
		};

		UpdateCollisionHull(transform);
		mStateMachine->Tick(context, stepSeconds);
		mVelocity                 = context.velocity;
		mGrounded                 = context.isGrounded;
		mJumpSnapDisableRemaining = std::max(
			0.0f,
			context.jumpSnapDisableRemaining
		);

		const std::string currentStateName = ToLowerCopy(
			mStateMachine->GetCurrentState() ?
				mStateMachine->GetCurrentState()->GetStateName() :
				""
		);

		if (!previousStateName.empty() && previousStateName != currentStateName) {
			PublishCue("movement.state.exit." + previousStateName);
			if (!currentStateName.empty()) {
				PublishCue("movement.state.enter." + currentStateName);
			}
		}

		if (
			wasGrounded &&
			!context.isGrounded &&
			input.jumpPressed &&
			context.velocity.y > 0.0f
		) {
			PublishCue("movement.jump");
		}

		if (!wasGrounded && context.isGrounded) {
			const float downSpeedHu  = std::max(0.0f, -preStepVerticalSpeedHu);
			const float landStrength = std::clamp(
				downSpeedHu / 400.0f, 0.0f, 200.0f
			);
			PublishCue("movement.land", landStrength, downSpeedHu);
		}

		OnAfterCoreCueDispatch(
			previousStateName,
			currentStateName,
			input,
			wasGrounded,
			context.isGrounded,
			stepSeconds
		);

		if (context.isGrounded) {
			mSupportCache.grounded              = true;
			mSupportCache.supportEntityGuid     = context.supportEntityGuid;
			mSupportCache.supportLinearVelocity = ResolveSupportLinearVelocity(
				context.supportEntityGuid
			);
			mSupportCache.supportStepDelta = ResolveSupportStepDelta(
				context.supportEntityGuid, stepSeconds
			);
		} else {
			mSupportCache.grounded              = false;
			mSupportCache.supportEntityGuid     = 0;
			mSupportCache.supportLinearVelocity = Vec3::zero;
			mSupportCache.supportStepDelta      = Vec3::zero;
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

		mSupportCache             = {};
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
				transform->SetPosition(
					transform->Position() + supportFrameDelta
				);
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

	void GameMovementComponent::PostPhysicsTick(float) {
		TransformComponent* transform = GetTransform();
		if (!transform) {
			Error(
				GetComponentName(), "TransformComponentが見つからないため、移動できません。"
			);
			return;
		}
		// デバッグ描画: 現在の速度を矢印で表示
		GetWorld()->GetDebugDraw().DrawArrow(
			transform->Position(),
			mVelocity * 0.25f, // 普通に出したら長いので縮める
			Vec4::yellow,
			0.125f
		);

		MovementContext context = {
			.input = mMoveFrameInput,
		};

		const Vec3 wishDir = mStateMachine->GetCurrentState()->GetWishDirHoriz(
			context
		);

		// デバッグ描画: 移動方向をレイで表示
		transform->GetWorld()->GetDebugDraw().DrawRay(
			transform->Position(),
			wishDir.Normalized(),
			Vec4::white
		);
	}

	void GameMovementComponent::OnEditorTick(float) {
		auto* world = GetWorld();

		world->GetDebugDraw().DrawBox(
			GetTransform()->Position(),
			GetTransform()->Rotation(),
			mBoxHalfExtents * 2.0f,
			Vec4::cyan
		);
	}

	std::string_view GameMovementComponent::GetStableName() const {
		return "game.GameMovement";
	}

	std::string_view GameMovementComponent::GetComponentName() const {
		return "GameMovement";
	}

	void GameMovementComponent::RegisterMovementStates(
		GameMovementStateMachine& stateMachine
	) {
		stateMachine.AddState(std::make_shared<NoclipMove>());
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

	void GameMovementComponent::OnAfterCoreCueDispatch(
		const std::string_view,
		const std::string_view,
		const MovementFrameInput&,
		const bool,
		const bool,
		const float
	) {}

#ifdef _DEBUG
	void GameMovementComponent::DrawInspectorImGui() {
		BaseCharacterComponent::DrawInspectorImGui();

		ImGui::SeparatorText("Gameplay Cue Debug");
		ImGui::Text(
			"Published Cues: %llu",
			static_cast<unsigned long long>(mDebugPublishedCueCount)
		);
		ImGui::Text("Last Cue ID: %s", mDebugLastPublishedCueId.c_str());
		ImGui::Text(
			"Last Cue Payload: %.3f / %.3f",
			mDebugLastPublishedCueValue,
			mDebugLastPublishedCueValue2
		);
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

		const auto* mover = supportEntity->GetComponent<
			KinematicMoverComponent>(
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

		const auto* mover = supportEntity->GetComponent<
			KinematicMoverComponent>(
		);
		if (!mover || mover->WasTeleported()) {
			return Vec3::zero;
		}

		return mover->GetStepDelta(stepSeconds);
	}

	bool GameMovementComponent::ApplyPassiveMotionStep(
		TransformComponent* transform,
		const float         stepSeconds
	) {
		if (!transform || !mCollisionResolver || stepSeconds <= 0.0f) {
			return false;
		}

		const Entity*  owner     = GetOwner();
		const uint64_t ownerGuid = owner ? owner->GetGuid() : 0;
		const World*   world     = GetWorld();
		const Scene*   scene     = world ? world->GetScenePtr() : nullptr;
		if (!scene) {
			return false;
		}

		const auto resolveMover = [scene](
			const uint64_t entityGuid
		)
			-> const KinematicMoverComponent* {
			if (entityGuid == 0) {
				return nullptr;
			}

			const Entity* entity = scene->FindEntity(entityGuid);
			for (int depth = 0; entity && depth < 8; ++depth) {
				if (!entity->IsActive()) {
					return nullptr;
				}

				const auto* mover = entity->GetComponent<
					KinematicMoverComponent>();
				if (mover && !mover->WasTeleported()) {
					return mover;
				}

				const auto* transform = entity->GetComponent<
					TransformComponent>();
				if (!transform || !transform->Parent()) {
					break;
				}
				entity = transform->Parent()->GetOwner();
			}
			return nullptr;
		};

		const uint64_t supportGuid =
			mSupportCache.grounded ? mSupportCache.supportEntityGuid : 0;
		const float contactSkinHu = mConsole->GetConVarValueOr(
			"sv_passivepush_contactskin", 4.0f
		);
		const float contactSkinM = Math::HtoM(std::max(0.0f, contactSkinHu));
		const Vec3  overlapHalfExtents =
			mBoxHalfExtents + Vec3(contactSkinM, contactSkinM, contactSkinM);
		const float sweepExtra = contactSkinM + Math::HtoM(0.5f);

		Vec3 position = transform->Position();
		std::array<Physics::Hit, kMaxPassiveOverlapHits> overlapHits{};
		std::array<PassiveMoverInfluence, kMaxPassiveOverlapHits> influences{};
		int influenceCount = 0;
		const auto findInfluenceIndex = [&influences, &influenceCount](
			const uint64_t guid
		) -> int {
			for (int i = 0; i < influenceCount; ++i) {
				if (influences[i].guid == guid) {
					return i;
				}
			}
			return -1;
		};
		const auto tryAddInfluence =
			[&influences, &influenceCount, &findInfluenceIndex](
			const uint64_t guid,
			const Vec3&    stepDelta
		) -> void {
			if (guid == 0 || stepDelta.SqrLength() <= 1.0e-12f) {
				return;
			}

			const int existing = findInfluenceIndex(guid);
			if (existing >= 0) {
				influences[existing].stepDelta = stepDelta;
				return;
			}

			if (influenceCount >= static_cast<int>(influences.size())) {
				return;
			}
			influences[influenceCount++] = PassiveMoverInfluence{
				.guid      = guid,
				.stepDelta = stepDelta
			};
		};

		const int overlapCount = mCollisionResolver->CollectOverlaps(
			position,
			overlapHalfExtents,
			overlapHits.data(),
			static_cast<int>(overlapHits.size())
		);
		for (int i = 0; i < overlapCount; ++i) {
			const Physics::Hit& hit = overlapHits[i];
			if (hit.hitEntityGuid == 0 || hit.hitEntityGuid == ownerGuid) {
				continue;
			}

			const auto* mover = resolveMover(hit.hitEntityGuid);
			if (!mover) {
				continue;
			}

			const Entity*  moverOwner = mover->GetOwner();
			const uint64_t moverGuid  =
				moverOwner ? moverOwner->GetGuid() : hit.hitEntityGuid;
			if (moverGuid == 0 || moverGuid == supportGuid) {
				continue;
			}

			const Vec3 moverStepDelta = mover->GetStepDelta(stepSeconds);
			if (moverStepDelta.SqrLength() <= 1.0e-12f) {
				continue;
			}

			bool pushing = true;
			if (hit.normal.SqrLength() > 1.0e-12f) {
				const Vec3 n = hit.normal.Normalized();
				pushing      = moverStepDelta.Dot(n) > 1.0e-6f;
			}
			if (!pushing) {
				continue;
			}

			tryAddInfluence(moverGuid, moverStepDelta);
		}

		Physics::Engine* physics = mCollisionResolver->GetPhysics();
		for (const auto& entityPtr : scene->GetEntities()) {
			const Entity* moverEntity = entityPtr.get();
			if (!moverEntity || !moverEntity->IsActive() ||
			    moverEntity->GetGuid() == ownerGuid) {
				continue;
			}

			const auto* mover = moverEntity->GetComponent<
				KinematicMoverComponent>();
			if (!mover || mover->WasTeleported()) {
				continue;
			}

			const uint64_t moverGuid = moverEntity->GetGuid();
			if (moverGuid == supportGuid || findInfluenceIndex(moverGuid) >=
			    0) {
				continue;
			}

			const Vec3 moverStepDelta = mover->GetStepDelta(stepSeconds);
			if (moverStepDelta.SqrLength() <= 1.0e-12f) {
				continue;
			}

			if (!physics) {
				continue;
			}

			const float moverDeltaLen = moverStepDelta.Length();
			if (moverDeltaLen <= 1.0e-6f) {
				continue;
			}

			const Vec3 castDir = (-moverStepDelta) / moverDeltaLen;
			const float castLength = moverDeltaLen + sweepExtra;
			const float yOffset = mBoxHalfExtents.y * 0.5f;
			const std::array<Vec3, 3> offsets = {
				Vec3::zero,
				Vec3::up * yOffset,
				Vec3::down * yOffset
			};

			bool moverWouldTouch = false;
			for (const Vec3& offset : offsets) {
				Physics::Hit sweepHit{};
				const Box    sweepBox = {
					.center   = position + offset,
					.halfSize = overlapHalfExtents
				};
				if (!physics->BoxCast(
					sweepBox,
					castDir,
					castLength,
					&sweepHit
				)) {
					continue;
				}

				if (resolveMover(sweepHit.hitEntityGuid) == mover) {
					moverWouldTouch = true;
					break;
				}
			}
			if (!moverWouldTouch) {
				continue;
			}

			tryAddInfluence(moverGuid, moverStepDelta);
		}

		if (influenceCount <= 0) {
			return false;
		}

		std::sort(
			influences.begin(),
			influences.begin() + influenceCount,
			[](
			const PassiveMoverInfluence& lhs, const PassiveMoverInfluence& rhs
		) {
				const float lhsLenSq = lhs.stepDelta.SqrLength();
				const float rhsLenSq = rhs.stepDelta.SqrLength();
				if (lhsLenSq == rhsLenSq) {
					return lhs.guid < rhs.guid;
				}
				return lhsLenSq > rhsLenSq;
			}
		);

		UpdateCollisionHull(transform);
		bool positionChanged = false;
		for (int i = 0; i < influenceCount; ++i) {
			const Vec3 desiredDelta = influences[i].stepDelta;
			if (desiredDelta.SqrLength() <= 1.0e-12f) {
				continue;
			}

			const Vec3 beforePosition = position;
			Vec3       pseudoVelocity = desiredDelta;
			mCollisionResolver->SlideMove(position, pseudoVelocity, 1.0f);

			const Vec3 appliedDelta = position - beforePosition;
			if (!appliedDelta.IsZero(1.0e-6f)) {
				positionChanged = true;
			}

			const Vec3 residual = desiredDelta - appliedDelta;
			if (residual.SqrLength() > 1.0e-10f) {
				const Vec3 blockNormal = residual.Normalized();
				if (mVelocity.Dot(blockNormal) < 0.0f) {
					mVelocity = BaseKinematicCollisionResolver::ClipVelocity(
						mVelocity,
						blockNormal,
						1.0f
					);
				}
			}
		}

		int maxDepenetrationIters = mConsole->GetConVarValueOr<int>(
			"sv_passivepush_maxdepenetrationiters", 2
		);
		maxDepenetrationIters = std::clamp(maxDepenetrationIters, 0, 8);
		for (int iter = 0; iter < maxDepenetrationIters; ++iter) {
			const int depenHitCount = mCollisionResolver->CollectOverlaps(
				position,
				mBoxHalfExtents,
				overlapHits.data(),
				static_cast<int>(overlapHits.size())
			);

			float deepestDepth  = 0.0f;
			Vec3  deepestNormal = Vec3::zero;
			for (int i = 0; i < depenHitCount; ++i) {
				const Physics::Hit& hit = overlapHits[i];
				if (hit.hitEntityGuid == 0 || hit.hitEntityGuid == ownerGuid) {
					continue;
				}

				if (hit.depth > deepestDepth && hit.normal.SqrLength() >
				    1.0e-12f) {
					deepestDepth  = hit.depth;
					deepestNormal = hit.normal.Normalized();
				}
			}

			if (deepestDepth <= contactSkinM + 1.0e-6f || deepestNormal.
			    IsZero()) {
				break;
			}

			const float depenBias  = Math::HtoM(0.05f);
			const float correction =
				std::max(0.0f, deepestDepth - contactSkinM) + depenBias;
			position        += deepestNormal * correction;
			positionChanged = true;
		}

		if (positionChanged) {
			transform->SetPosition(position);
			UpdateCollisionHull(transform);
		}
		return positionChanged;
	}

	void GameMovementComponent::PublishCue(
		std::string id, const float value, const float value2
	) {
		World* world = GetWorld();
		if (!world) {
			return;
		}

		const Entity* owner = GetOwner();
		if (!owner) {
			return;
		}

		GameplayCue cue      = {};
		cue.id               = std::move(id);
		cue.sourceEntityGuid = owner->GetGuid();
		cue.value            = value;
		cue.value2           = value2;
		world->GetGameplayCueBus().Publish(cue);

#ifdef _DEBUG
		mDebugLastPublishedCueId     = cue.id;
		mDebugLastPublishedCueValue  = cue.value;
		mDebugLastPublishedCueValue2 = cue.value2;
		++mDebugPublishedCueCount;
#endif
	}

	std::string GameMovementComponent::ToLowerCopy(
		const std::string_view text
	) {
		std::string lower(text);
		std::transform(
			lower.begin(),
			lower.end(),
			lower.begin(),
			[](const unsigned char c) {
				return static_cast<char>(std::tolower(c));
			}
		);
		return lower;
	}

	REGISTER_COMPONENT(GameMovementComponent);
}
