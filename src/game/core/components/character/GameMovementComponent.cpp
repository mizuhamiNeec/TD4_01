#include "GameMovementComponent.h"

#include <algorithm>
#include <array>

#ifdef _DEBUG
#include <imgui.h>
#endif

#include "../controller/PlayerCharacterController.h"

#include "base/BaseCharacterComponent.h"

#include "core/ComponentRegistry.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"
#include "core/string/StrUtil.h"

#include "engine/scene/Scene.h"
#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/components/kinematic/KinematicMoverComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/unnamed/subsystem/console/ConsoleSystem.h"
#include "engine/unnamed/subsystem/console/Log.h"
#include "engine/unnamed/subsystem/console/concommand/ConVar.h"
#include "engine/unnamed/subsystem/input/InputSystem.h"
#include "engine/world/GameplayCueBus.h"
#include "engine/world/World.h"

#include "game/core/collision/kinematic/BoxKinematicCollisionResolver.h"
#include "game/core/components/character/state/GameMovementStateMachine.h"
#include "game/core/replay/DemoManager.h"
#include "game/core/replay/ReplayHash.h"

#include "state/ability/CoreMovementAbilities.h"
#include "state/mode/AirMovementMode.h"
#include "state/mode/GroundMovementMode.h"
#include "state/mode/NoclipMovementMode.h"

namespace Unnamed {
	namespace {
		constexpr int kMaxPassiveOverlapHits = 64;

		struct PassiveMoverInfluence {
			uint64_t guid      = 0;
			Vec3     stepDelta = Vec3::zero;
		};
	}

	GameMovementComponent::~GameMovementComponent() = default;

	void GameMovementComponent::TeleportAndResetMotion(
		TransformComponent* transform,
		const Vec3&         worldPosition
	) {
		if (!transform) {
			return;
		}

		// リスポーン直後の慣性や支持床キャッシュを引きずらないように初期化します。
		transform->SetPosition(worldPosition);
		transform->RequestInterpolationResync();
		mVelocity                 = Vec3::zero;
		mGrounded                 = false;
		mSupportCache             = {};
		mJumpSnapDisableRemaining = 0.0f;
		UpdateCollisionHull(transform);
	}

	void GameMovementComponent::SimulateStep(
		TransformComponent* transform, const MovementFrameInput& input,
		const float         stepSeconds
	) {
		if (!transform || !mCollisionResolver || !mStateMachine) {
			return;
		}

		const bool        wasGrounded            = mSupportCache.grounded;
		const float       preStepVerticalSpeedHu = Math::MtoH(mVelocity.y);
		const std::string previousStateName      = StrUtil::ToLowerCase(
			ResolvePresentationStateName()
		);

		(void)ApplyPassiveMotionStep(transform, stepSeconds);

		mCurrentModeId = mStateMachine->GetCurrentModeId();
		mActiveAbilityMask = mStateMachine->GetActiveAbilityMask();
		MovementContext context = {};
		context.input = input;
		context.transform = transform;
		context.velocity = mVelocity;
		context.resolver = mCollisionResolver.get();
		context.halfExtents = mBoxHalfExtents;
		context.isGrounded = mSupportCache.grounded;
		context.supportEntityGuid = mSupportCache.supportEntityGuid;
		context.supportLinearVelocity = mSupportCache.supportLinearVelocity;
		context.supportStepDelta = mSupportCache.supportStepDelta;
		context.jumpSnapDisableRemaining = mJumpSnapDisableRemaining;
		context.defaultAirMode = GetAirModeForTransitions();
		context.movementComponent = this;
		context.capabilitySet = &mCapabilitySet;
		context.modeState.currentMode = mCurrentModeId;
		context.modeState.previousMode = mCurrentModeId;
		context.modeState.changedThisTick = false;
		context.activeAbilityMask = mActiveAbilityMask;

		UpdateCollisionHull(transform);
		mStateMachine->Tick(context, stepSeconds);
		mCurrentModeId            = mStateMachine->GetCurrentModeId();
		mActiveAbilityMask        = mStateMachine->GetActiveAbilityMask();
		mVelocity                 = context.velocity;
		mGrounded                 = context.isGrounded;
		mJumpSnapDisableRemaining = std::max(
			0.0f,
			context.jumpSnapDisableRemaining
		);

		const std::string currentStateName = StrUtil::ToLowerCase(
			ResolvePresentationStateName()
		);

		if (
			!previousStateName.empty() &&
			previousStateName != currentStateName
		) {
			// movement.state.exit.* payload contract:
			// value/value2 are currently unused (default 0).
			PublishCue("movement.state.exit." + previousStateName);
			if (!currentStateName.empty()) {
				// movement.state.enter.* payload contract:
				// value/value2 are currently unused (default 0).
				PublishCue("movement.state.enter." + currentStateName);
			}
		}

		if (
			wasGrounded &&
			!context.isGrounded &&
			input.jumpPressed &&
			context.velocity.y > 0.0f
		) {
			// movement.jump: 現状は payload 未使用（value/value2 は 0）。
			PublishCue("movement.jump");
		}

		if (!wasGrounded && context.isGrounded) {
			const float downSpeedHu  = std::max(0.0f, -preStepVerticalSpeedHu);
			const float landStrength = std::clamp(
				downSpeedHu / 400.0f, 0.0f, 200.0f
			);
			// movement.land payload contract:
			// value  = normalized land strength [0..200]
			// value2 = impactSpeed (downward speed in HU/s, >= 0)
			// named:
			//   payload.landStrength
			//   payload.impactSpeedHuPerSec
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

		mInput   = GetInputSystem();
		mConsole = GetConsoleSystem();

		mStateMachine = std::make_unique<GameMovementStateMachine>();
		mStateMachine->Init(mConsole);

		RegisterMovementModes(*mStateMachine);
		RegisterMovementAbilities(*mStateMachine);
		mStateMachine->SetInitialMode(GetInitialMode());
		mCurrentModeId     = mStateMachine->GetCurrentModeId();
		mActiveAbilityMask = mStateMachine->GetActiveAbilityMask();

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

		DeterministicInputPacket deterministicPacket = {};
		if (TryDequeueDeterministicInput(deterministicPacket)) {
			const float stepSeconds = std::max(
				deterministicPacket.stepSeconds,
				1.0e-6f
			);

			if (mSupportCache.grounded && mSupportCache.supportEntityGuid !=
			    0) {
				mSupportCache.supportLinearVelocity =
					ResolveSupportLinearVelocity(
						mSupportCache.supportEntityGuid
					);
				mSupportCache.supportStepDelta = ResolveSupportStepDelta(
					mSupportCache.supportEntityGuid, stepSeconds
				);
			} else {
				mSupportCache.supportLinearVelocity = Vec3::zero;
				mSupportCache.supportStepDelta      = Vec3::zero;
			}

			UpdateCollisionHull(transform);
			mMoveFrameInput = deterministicPacket.input;
			SimulateStep(transform, deterministicPacket.input, stepSeconds);

			if (Entity* owner = GetOwner()) {
				if (auto* demoManager = GetDemoManager()) {
					demoManager->RecordOrVerifySnapshot(
						deterministicPacket.tick,
						*owner
					);
				}
			}

			transform->OnTick(0.0f);
			return;
		}

		// PlayerCharacterController が接続されている場合は、
		// deterministic 入力キューのみで移動更新する。
		// キュー空フレームで旧フレーム依存フォールバックへ入ると
		// FPS によって移動量が変わってしまうため、ここで抑止する。
		if (const Entity* owner = GetOwner()) {
			if (owner->GetComponent<PlayerCharacterController>() != nullptr) {
				transform->OnTick(0.0f);
				return;
			}
		}

		const float stepSec = std::max(deltaTime, 1.0e-6f);

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
					transform->GetPosition() + supportFrameDelta
				);
			}
		} else {
			mSupportCache.supportLinearVelocity = Vec3::zero;
			mSupportCache.supportStepDelta      = Vec3::zero;
		}

		UpdateCollisionHull(transform);
		SimulateStep(transform, mMoveFrameInput, stepSec);

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
		auto& worldDebugDraw = GetWorld()->GetDebugDraw();

		// デバッグ描画: 現在の速度を矢印で表示
		worldDebugDraw.DrawArrow(
			transform->GetPosition(),
			mVelocity * 0.25f, // 普通に出したら長いので縮める
			Vec4::yellow,
			0.125f
		);

		const MovementContext context = {
			.input = mMoveFrameInput,
		};

		if (!mStateMachine) {
			return;
		}

		const Vec3 right   = context.input.right.Normalized();
		Vec3       forward = context.input.forward.Normalized();
		forward.y          = 0.0f;
		forward.Normalize();
		Vec3 wishDir =
			right * context.input.moveAxis.x +
			forward * context.input.moveAxis.z;
		wishDir.y = 0.0f;

		// デバッグ描画: 移動方向をレイで表示
		worldDebugDraw.DrawRay(
			transform->GetPosition(),
			wishDir.Normalized(),
			Vec4::white
		);

		// デバッグ描画: 当たり判定のボックスを表示
		worldDebugDraw.DrawBox(
			transform->GetPosition(),
			transform->GetRotation(),
			mBoxHalfExtents * 2.0f,
			Vec4::cyan
		);
	}

	void GameMovementComponent::OnEditorTick(float) {
		auto*                     world     = GetWorld();
		const TransformComponent* transform = GetTransform();
		if (!world || !transform) {
			return;
		}

		world->GetDebugDraw().DrawBox(
			transform->GetPosition(),
			transform->GetRotation(),
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

	bool GameMovementComponent::UseSprintSpeedAsDefaultGroundSpeed() const {
		return false;
	}

	void GameMovementComponent::RegisterMovementModes(
		GameMovementStateMachine& stateMachine
	) {
		stateMachine.AddMode(std::make_shared<NoclipMovementMode>());
		stateMachine.AddMode(std::make_shared<AirMovementMode>());
		stateMachine.AddMode(std::make_shared<GroundMovementMode>());
	}

	void GameMovementComponent::RegisterMovementAbilities(
		GameMovementStateMachine& stateMachine
	) {
		stateMachine.AddAbility(std::make_shared<JumpMovementAbility>());
		stateMachine.AddAbility(std::make_shared<CrouchMovementAbility>());
	}

	MOVEMENT_MODE_ID GameMovementComponent::GetInitialMode() const {
		return MOVEMENT_MODE_ID::AIR;
	}

	MOVEMENT_MODE_ID GameMovementComponent::GetAirModeForTransitions() const {
		return MOVEMENT_MODE_ID::AIR;
	}

	std::string GameMovementComponent::ResolvePresentationStateName() const {
		return std::string(ToString(mCurrentModeId));
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
			boxResolver->UpdateHull(transform->GetPosition(), mBoxHalfExtents);
		}
	}

	MovementCapabilitySet& GameMovementComponent::GetCapabilitySet() {
		return mCapabilitySet;
	}

	const MovementCapabilitySet& GameMovementComponent::GetCapabilitySet()
	const {
		return mCapabilitySet;
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

		ImGui::SeparatorText("Movement Capabilities");
		ImGui::Checkbox("Jump", &mCapabilitySet.jump);
		ImGui::Checkbox("Crouch", &mCapabilitySet.crouch);
		ImGui::Checkbox("Slide", &mCapabilitySet.slide);
		ImGui::Checkbox("WallRun", &mCapabilitySet.wallRun);
		ImGui::Checkbox("DoubleJump", &mCapabilitySet.doubleJump);
		ImGui::Checkbox("SpeedVault", &mCapabilitySet.speedVault);
		ImGui::Checkbox("Blink", &mCapabilitySet.blink);
		ImGui::Checkbox("Grapple", &mCapabilitySet.grapple);
		ImGui::Text(
			"Mode: %s | ActiveAbilities: 0x%016llX",
			ToString(mCurrentModeId).data(),
			static_cast<unsigned long long>(mActiveAbilityMask)
		);

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
		if (const JsonReader caps = reader["movementCapabilities"]; caps.
			Valid()) {
			mCapabilitySet.jump   = caps["jump"].GetBool(mCapabilitySet.jump);
			mCapabilitySet.crouch = caps["crouch"].GetBool(
				mCapabilitySet.crouch
			);
			mCapabilitySet.slide = caps["slide"].GetBool(mCapabilitySet.slide);
			mCapabilitySet.wallRun = caps["wallRun"].GetBool(
				mCapabilitySet.wallRun
			);
			mCapabilitySet.doubleJump = caps["doubleJump"].GetBool(
				mCapabilitySet.doubleJump
			);
			mCapabilitySet.speedVault = caps["speedVault"].GetBool(
				mCapabilitySet.speedVault
			);
			mCapabilitySet.blink = caps["blink"].GetBool(mCapabilitySet.blink);
			mCapabilitySet.grapple = caps["grapple"].GetBool(
				mCapabilitySet.grapple
			);
		}
	}

	void GameMovementComponent::Serialize(JsonWriter& writer) const {
		BaseCharacterComponent::Serialize(writer);
		writer.Key("movementCapabilities");
		writer.BeginObject();
		writer.Key("jump");
		writer.Write(mCapabilitySet.jump);
		writer.Key("crouch");
		writer.Write(mCapabilitySet.crouch);
		writer.Key("slide");
		writer.Write(mCapabilitySet.slide);
		writer.Key("wallRun");
		writer.Write(mCapabilitySet.wallRun);
		writer.Key("doubleJump");
		writer.Write(mCapabilitySet.doubleJump);
		writer.Key("speedVault");
		writer.Write(mCapabilitySet.speedVault);
		writer.Key("blink");
		writer.Write(mCapabilitySet.blink);
		writer.Key("grapple");
		writer.Write(mCapabilitySet.grapple);
		writer.EndObject();
	}

	void GameMovementComponent::WriteReplayState(
		nlohmann::json& outState
	) const {
		outState["velocity"] = nlohmann::json::array(
			{mVelocity.x, mVelocity.y, mVelocity.z}
		);
		outState["grounded"]                 = mGrounded;
		outState["collisionEnabled"]         = mCollisionEnabled;
		outState["jumpSnapDisableRemaining"] = mJumpSnapDisableRemaining;
		outState["support"]                  = nlohmann::json::object(
			{
				{"grounded", mSupportCache.grounded},
				{"supportEntityGuid", mSupportCache.supportEntityGuid},
				{
					"supportLinearVelocity",
					nlohmann::json::array(
						{
							mSupportCache.supportLinearVelocity.x,
							mSupportCache.supportLinearVelocity.y,
							mSupportCache.supportLinearVelocity.z
						}
					)
				},
				{
					"supportStepDelta",
					nlohmann::json::array(
						{
							mSupportCache.supportStepDelta.x,
							mSupportCache.supportStepDelta.y,
							mSupportCache.supportStepDelta.z
						}
					)
				}
			}
		);
		outState["boxHalfExtents"] = nlohmann::json::array(
			{mBoxHalfExtents.x, mBoxHalfExtents.y, mBoxHalfExtents.z}
		);
		outState["movementRuntimeVersion"] = kMovementRuntimeVersion;
		outState["modeId"] = static_cast<uint32_t>(mCurrentModeId);
		outState["activeAbilityMask"] = mActiveAbilityMask;
		outState["capabilities"] = nlohmann::json::object(
			{
				{"jump", mCapabilitySet.jump},
				{"crouch", mCapabilitySet.crouch},
				{"slide", mCapabilitySet.slide},
				{"wallRun", mCapabilitySet.wallRun},
				{"doubleJump", mCapabilitySet.doubleJump},
				{"speedVault", mCapabilitySet.speedVault},
				{"blink", mCapabilitySet.blink},
				{"grapple", mCapabilitySet.grapple}
			}
		);

		if (TransformComponent* transform = GetTransform()) {
			const Vec3       position = transform->GetPosition();
			const Quaternion rotation = transform->GetRotation();
			outState["position"]      = nlohmann::json::array(
				{position.x, position.y, position.z}
			);
			outState["rotation"] = nlohmann::json::array(
				{rotation.x, rotation.y, rotation.z, rotation.w}
			);
		}
	}

	void GameMovementComponent::ReadReplayState(const nlohmann::json& inState) {
		if (!inState.is_object()) {
			return;
		}

		if (const auto it = inState.find("velocity");
			it != inState.end() && it->is_array() && it->size() == 3) {
			mVelocity = Vec3(
				(*it)[0].get<float>(),
				(*it)[1].get<float>(),
				(*it)[2].get<float>()
			);
		}

		mGrounded         = inState.value("grounded", mGrounded);
		mCollisionEnabled = inState.value(
			"collisionEnabled", mCollisionEnabled
		);
		mJumpSnapDisableRemaining = inState.value(
			"jumpSnapDisableRemaining",
			mJumpSnapDisableRemaining
		);

		if (const auto it = inState.find("boxHalfExtents");
			it != inState.end() && it->is_array() && it->size() == 3) {
			mBoxHalfExtents = Vec3(
				(*it)[0].get<float>(),
				(*it)[1].get<float>(),
				(*it)[2].get<float>()
			);
		}

		if (const auto it = inState.find("support");
			it != inState.end() && it->is_object()) {
			mSupportCache.grounded = it->value(
				"grounded", mSupportCache.grounded
			);
			mSupportCache.supportEntityGuid = it->value(
				"supportEntityGuid",
				mSupportCache.supportEntityGuid
			);
			if (const auto itVel = it->find("supportLinearVelocity");
				itVel != it->end() && itVel->is_array() && itVel->size() == 3) {
				mSupportCache.supportLinearVelocity = Vec3(
					(*itVel)[0].get<float>(),
					(*itVel)[1].get<float>(),
					(*itVel)[2].get<float>()
				);
			}
			if (const auto itDelta = it->find("supportStepDelta");
				itDelta != it->end() && itDelta->is_array() && itDelta->size()
				==
				3) {
				mSupportCache.supportStepDelta = Vec3(
					(*itDelta)[0].get<float>(),
					(*itDelta)[1].get<float>(),
					(*itDelta)[2].get<float>()
				);
			}
		}

		if (TransformComponent* transform = GetTransform()) {
			if (const auto itPos = inState.find("position");
				itPos != inState.end() && itPos->is_array() && itPos->size() ==
				3) {
				transform->SetPosition(
					Vec3(
						(*itPos)[0].get<float>(),
						(*itPos)[1].get<float>(),
						(*itPos)[2].get<float>()
					)
				);
			}

			if (const auto itRot = inState.find("rotation");
				itRot != inState.end() && itRot->is_array() && itRot->size() ==
				4) {
				transform->SetRotation(
					Quaternion(
						(*itRot)[0].get<float>(),
						(*itRot)[1].get<float>(),
						(*itRot)[2].get<float>(),
						(*itRot)[3].get<float>()
					)
				);
			}
		}

		const bool hasRuntimeVersion =
			inState.contains("movementRuntimeVersion");
		if (hasRuntimeVersion) {
			if (const auto it = inState.find("modeId");
				it != inState.end() && it->is_number_unsigned()) {
				const uint32_t rawMode = it->get<uint32_t>();
				if (rawMode < static_cast<uint32_t>(MOVEMENT_MODE_ID::COUNT)) {
					mCurrentModeId = static_cast<MOVEMENT_MODE_ID>(rawMode);
				}
			}
			mActiveAbilityMask = inState.value(
				"activeAbilityMask",
				mActiveAbilityMask
			);
			if (const auto it = inState.find("capabilities");
				it != inState.end() && it->is_object()) {
				mCapabilitySet.jump   = it->value("jump", mCapabilitySet.jump);
				mCapabilitySet.crouch = it->value(
					"crouch",
					mCapabilitySet.crouch
				);
				mCapabilitySet.slide = it->value("slide", mCapabilitySet.slide);
				mCapabilitySet.wallRun = it->value(
					"wallRun",
					mCapabilitySet.wallRun
				);
				mCapabilitySet.doubleJump = it->value(
					"doubleJump",
					mCapabilitySet.doubleJump
				);
				mCapabilitySet.speedVault = it->value(
					"speedVault",
					mCapabilitySet.speedVault
				);
				mCapabilitySet.blink = it->value("blink", mCapabilitySet.blink);
				mCapabilitySet.grapple = it->value(
					"grapple",
					mCapabilitySet.grapple
				);
			}
		} else if (const auto it = inState.find("stateName");
			it != inState.end() && it->is_string()) {
			Warning(
				GetComponentName(),
				"Legacy movement replay schema detected. Deterministic compatibility is best-effort."
			);
			const std::string stateName = StrUtil::ToLowerCase(
				it->get<std::string>()
			);
			if (stateName.find("noclip") != std::string::npos) {
				mCurrentModeId = MOVEMENT_MODE_ID::NOCLIP;
			} else if (stateName.find("ground") != std::string::npos) {
				mCurrentModeId = MOVEMENT_MODE_ID::GROUND;
			} else {
				mCurrentModeId = MOVEMENT_MODE_ID::AIR;
			}
		}

		if (mStateMachine) {
			mStateMachine->SetInitialMode(mCurrentModeId);
		}
	}

	uint64_t GameMovementComponent::ComputeReplayStateHash() const {
		uint64_t hash = ReplayHash::Begin();
		ReplayHash::AppendFloating(hash, mVelocity.x);
		ReplayHash::AppendFloating(hash, mVelocity.y);
		ReplayHash::AppendFloating(hash, mVelocity.z);
		ReplayHash::AppendPod(hash, mGrounded);
		ReplayHash::AppendPod(hash, mCollisionEnabled);
		ReplayHash::AppendFloating(hash, mJumpSnapDisableRemaining);

		ReplayHash::AppendPod(hash, mSupportCache.grounded);
		ReplayHash::AppendPod(hash, mSupportCache.supportEntityGuid);
		ReplayHash::AppendFloating(hash, mSupportCache.supportLinearVelocity.x);
		ReplayHash::AppendFloating(hash, mSupportCache.supportLinearVelocity.y);
		ReplayHash::AppendFloating(hash, mSupportCache.supportLinearVelocity.z);
		ReplayHash::AppendFloating(hash, mSupportCache.supportStepDelta.x);
		ReplayHash::AppendFloating(hash, mSupportCache.supportStepDelta.y);
		ReplayHash::AppendFloating(hash, mSupportCache.supportStepDelta.z);
		ReplayHash::AppendFloating(hash, mBoxHalfExtents.x);
		ReplayHash::AppendFloating(hash, mBoxHalfExtents.y);
		ReplayHash::AppendFloating(hash, mBoxHalfExtents.z);

		ReplayHash::AppendPod(
			hash,
			static_cast<uint32_t>(kMovementRuntimeVersion)
		);
		ReplayHash::AppendPod(hash, static_cast<uint32_t>(mCurrentModeId));
		ReplayHash::AppendPod(hash, mActiveAbilityMask);
		ReplayHash::AppendPod(hash, mCapabilitySet.jump);
		ReplayHash::AppendPod(hash, mCapabilitySet.crouch);
		ReplayHash::AppendPod(hash, mCapabilitySet.slide);
		ReplayHash::AppendPod(hash, mCapabilitySet.wallRun);
		ReplayHash::AppendPod(hash, mCapabilitySet.doubleJump);
		ReplayHash::AppendPod(hash, mCapabilitySet.speedVault);
		ReplayHash::AppendPod(hash, mCapabilitySet.blink);
		ReplayHash::AppendPod(hash, mCapabilitySet.grapple);

		if (const TransformComponent* transform = GetTransform()) {
			const Vec3       position = transform->GetPosition();
			const Quaternion rotation = transform->GetRotation();
			ReplayHash::AppendFloating(hash, position.x);
			ReplayHash::AppendFloating(hash, position.y);
			ReplayHash::AppendFloating(hash, position.z);
			ReplayHash::AppendFloating(hash, rotation.x);
			ReplayHash::AppendFloating(hash, rotation.y);
			ReplayHash::AppendFloating(hash, rotation.z);
			ReplayHash::AppendFloating(hash, rotation.w);
		}

		return hash;
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
				if (!transform || !transform->GetParent()) {
					break;
				}
				entity = transform->GetParent()->GetOwner();
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

		Vec3 position = transform->GetPosition();
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
		if (overlapCount > 1) {
			std::sort(
				overlapHits.begin(),
				overlapHits.begin() + overlapCount,
				[](const Physics::Hit& lhs, const Physics::Hit& rhs) {
					if (lhs.hitEntityGuid != rhs.hitEntityGuid) {
						return lhs.hitEntityGuid < rhs.hitEntityGuid;
					}
					if (lhs.triIndex != rhs.triIndex) {
						return lhs.triIndex < rhs.triIndex;
					}
					if (lhs.depth != rhs.depth) {
						return lhs.depth > rhs.depth;
					}
					if (lhs.normal.x != rhs.normal.x) {
						return lhs.normal.x < rhs.normal.x;
					}
					if (lhs.normal.y != rhs.normal.y) {
						return lhs.normal.y < rhs.normal.y;
					}
					if (lhs.normal.z != rhs.normal.z) {
						return lhs.normal.z < rhs.normal.z;
					}
					return false;
				}
			);
		}
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

		Physics::Engine*           physics = mCollisionResolver->GetPhysics();
		std::vector<const Entity*> orderedEntities = {};
		orderedEntities.reserve(scene->GetEntities().size());
		for (const auto& entityPtr : scene->GetEntities()) {
			if (!entityPtr) {
				continue;
			}
			orderedEntities.emplace_back(entityPtr.get());
		}
		if (orderedEntities.size() > 1) {
			std::ranges::sort(
				orderedEntities,
				[](const Entity* lhs, const Entity* rhs) {
					return lhs->GetGuid() < rhs->GetGuid();
				}
			);
		}

		for (const Entity* moverEntity : orderedEntities) {
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

		UpdateCollisionHull(transform);
		bool positionChanged = false;
		if (influenceCount > 0) {
			std::sort(
				influences.begin(),
				influences.begin() + influenceCount,
				[](
				const PassiveMoverInfluence& lhs,
				const PassiveMoverInfluence& rhs
			) {
					const float lhsLenSq = lhs.stepDelta.SqrLength();
					const float rhsLenSq = rhs.stepDelta.SqrLength();
					if (lhsLenSq == rhsLenSq) {
						return lhs.guid < rhs.guid;
					}
					return lhsLenSq > rhsLenSq;
				}
			);

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
						mVelocity =
							BaseKinematicCollisionResolver::ClipVelocity(
								mVelocity,
								blockNormal,
								1.0f
							);
					}
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
			if (depenHitCount > 1) {
				std::sort(
					overlapHits.begin(),
					overlapHits.begin() + depenHitCount,
					[](const Physics::Hit& lhs, const Physics::Hit& rhs) {
						if (lhs.depth != rhs.depth) {
							return lhs.depth > rhs.depth;
						}
						if (lhs.hitEntityGuid != rhs.hitEntityGuid) {
							return lhs.hitEntityGuid < rhs.hitEntityGuid;
						}
						if (lhs.triIndex != rhs.triIndex) {
							return lhs.triIndex < rhs.triIndex;
						}
						return false;
					}
				);
			}

			Vec3  correctionVector = Vec3::zero;
			Vec3  fallbackNormal   = Vec3::zero;
			float deepestDepth     = 0.0f;
			int   validHitCount    = 0;
			for (int i = 0; i < depenHitCount; ++i) {
				const Physics::Hit& hit = overlapHits[i];
				if (hit.hitEntityGuid == 0 || hit.hitEntityGuid == ownerGuid) {
					continue;
				}

				if (hit.normal.SqrLength() <= 1.0e-12f) {
					continue;
				}

				const Vec3  normal     = hit.normal.Normalized();
				const float depenDepth = std::max(
					0.0f,
					hit.depth - contactSkinM
				);
				if (depenDepth <= 1.0e-6f) {
					continue;
				}

				correctionVector += normal * depenDepth;
				++validHitCount;
				if (depenDepth > deepestDepth) {
					deepestDepth   = depenDepth;
					fallbackNormal = normal;
				}
			}

			if (validHitCount <= 0 || deepestDepth <= 1.0e-6f ||
			    fallbackNormal.IsZero()) {
				break;
			}

			Vec3 correctionDir =
				correctionVector.SqrLength() > 1.0e-12f ?
					correctionVector.Normalized() :
					fallbackNormal;

			if (correctionDir.IsZero()) {
				correctionDir = fallbackNormal;
			}
			if (correctionDir.IsZero()) {
				break;
			}

			const float averageDepth = correctionVector.SqrLength() > 1.0e-12f ?
				                           correctionVector.Length() /
				                           static_cast<float>(validHitCount) :
				                           deepestDepth;
			
			const float depenBias  = Math::HtoM(0.05f);
			
			const float correction = std::max(
				                         deepestDepth * 0.5f,
				                         averageDepth
			                         ) +
			                         depenBias;
			position        += correctionDir * correction;
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
		// value/value2 互換を維持しつつ、named payload も同時に供給します。
		if (cue.id == "movement.land") {
			cue.SetFloat("landStrength", value);
			cue.SetFloat("impactSpeedHuPerSec", value2);
		} else if (cue.id == "movement.footstep") {
			cue.SetFloat("footstepScale", value);
			cue.SetFloat("horizontalSpeedHuPerSec", value2);
		} else if (cue.id == "movement.sprint.update") {
			cue.SetFloat("sprintRatio", value);
		}
		world->GetGameplayCueBus().Publish(cue);

#ifdef _DEBUG
		mDebugLastPublishedCueId     = cue.id;
		mDebugLastPublishedCueValue  = cue.value;
		mDebugLastPublishedCueValue2 = cue.value2;
		++mDebugPublishedCueCount;
#endif
	}

	REGISTER_COMPONENT(GameMovementComponent);
}
