#include "ParkourMovementComponent.h"

#include <algorithm>
#include <imgui.h>

#include "core/ComponentRegistry.h"

// ReSharper disable once CppUnusedIncludeDirective
#include "core/string/StrUtil.h"

#include "engine/ImGui/Icons.h"
#include "engine/physics/core/Physics.h"
#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/unnamed/primitive/Primitives.h"
#include "engine/unnamed/subsystem/console/ConsoleSystem.h"
#include "engine/unnamed/subsystem/console/concommand/ConVar.h"
#include "engine/world/World.h"

#include "game/core/collision/kinematic/base/BaseKinematicCollisionResolver.h"
#include "game/core/components/character/state/GameMovementStateMachine.h"
#include "game/core/replay/ReplayHash.h"

#include "ability/ParkourMovementAbilities.h"

namespace Unnamed {
	namespace {
		constexpr std::string_view kStateParkourGroundMoveLower =
			"parkourgroundmove";
		constexpr float kForwardSprintDotThreshold = 0.7f;
		constexpr float kMinSprintMoveSpeedHu      = 5.0f;

		bool TryReadVec3FromObject(
			const nlohmann::json& object,
			const char*           key,
			Vec3&                 outValue
		) {
			if (!object.is_object()) {
				return false;
			}
			const auto it = object.find(key);
			if (it == object.end() || !it->is_array() || it->size() != 3) {
				return false;
			}
			outValue = Vec3(
				(*it)[0].get<float>(),
				(*it)[1].get<float>(),
				(*it)[2].get<float>()
			);
			return true;
		}
	}

	ParkourMovementComponent::~ParkourMovementComponent() = default;

	void ParkourMovementComponent::OnAttached() {
		GameMovementComponent::OnAttached();
		mAutoSprintActive = false;
		ResetParkourRuntime();
		mStandingHalfExtents = mBoxHalfExtents;
		RebuildDuckHalfExtents();
		ClearDeterministicActionInputQueue();
		mActionFrameInput = {};
		auto& [jump, crouch, slide, wallRun, doubleJump, speedVault, blink, grapple] = GetCapabilitySet();
		jump = true;
		crouch = true;
		slide = true;
		wallRun = true;
		doubleJump = true;
		speedVault = true;
		blink = true;
		grapple = true;
	}

	void ParkourMovementComponent::SimulateStep(
		TransformComponent*       transform,
		const MovementFrameInput& input,
		const float               stepSeconds
	) {
		DeterministicActionInputPacket actionPacket = {};
		if (mDeterministicActionInputQueue.Pop(actionPacket)) {
			mActionFrameInput = actionPacket.input;
		} else {
			mActionFrameInput = {};
		}

		TickParkourTimers(stepSeconds);
		GameMovementComponent::SimulateStep(transform, input, stepSeconds);
		// 旧State実装と同じ入力エッジ判定に合わせるため、毎tick最後に同期します。
		mRuntime.lastJumpHeld = input.jumpPressed;
		if (mGrounded) {
			mRuntime.hasDoubleJump = true;
		}
	}

	void ParkourMovementComponent::EnqueueDeterministicActionInput(
		const uint64_t                   tick,
		const float                      stepSeconds,
		const CharacterActionFrameInput& input
	) {
		mDeterministicActionInputQueue.Push(DeterministicActionInputPacket{
			.tick        = tick,
			.stepSeconds = stepSeconds,
			.input       = input
		});
	}

	void ParkourMovementComponent::ClearDeterministicActionInputQueue() {
		mDeterministicActionInputQueue.Clear();
	}

	const CharacterActionFrameInput&
	ParkourMovementComponent::GetActionFrameInput() const {
		return mActionFrameInput;
	}

	std::string_view ParkourMovementComponent::GetStableName() const {
		return "parkour.ParkourMovement";
	}

	std::string_view ParkourMovementComponent::GetComponentName() const {
		return "ParkourMovement";
	}

#ifdef _DEBUG
	void ParkourMovementComponent::DrawInspectorImGui() {
		GameMovementComponent::DrawInspectorImGui();

		ImGui::SeparatorText("Parkour Runtime");
		ImGui::Text(
			"DoubleJump: %s", mRuntime.hasDoubleJump ? "true" : "false"
		);
		ImGui::Text("DuckHull: %s", mRuntime.duckHullActive ? "true" : "false");
		ImGui::Text(
			"WallRun: %s (%.2f s)", mRuntime.wallRun.active ? "true" : "false",
			mRuntime.wallRun.time
		);
		ImGui::Text(
			"Slide: %s (%.2f s)", mRuntime.slide.active ? "true" : "false",
			mRuntime.slide.time
		);
		ImGui::Text(
			"Blink: %s (cool %.2f s)", mRuntime.blink.active ? "true" : "false",
			mRuntime.blink.cooldown
		);
		ImGui::Text(
			"Vault: %s (cool %.2f s)", mRuntime.vault.active ? "true" : "false",
			mRuntime.vault.cooldown
		);
	}
#endif

	void ParkourMovementComponent::Deserialize(const JsonReader& reader) {
		GameMovementComponent::Deserialize(reader);
		mStandingHalfExtents = mBoxHalfExtents;
		RebuildDuckHalfExtents();
	}

	void ParkourMovementComponent::Serialize(JsonWriter& writer) const {
		GameMovementComponent::Serialize(writer);
	}

	void ParkourMovementComponent::WriteReplayState(
		nlohmann::json& outState
	) const {
		nlohmann::json baseState = nlohmann::json::object();
		GameMovementComponent::WriteReplayState(baseState);
		outState["base"] = std::move(baseState);

		outState["standingHalfExtents"] = nlohmann::json::array(
			{
				mStandingHalfExtents.x, mStandingHalfExtents.y,
				mStandingHalfExtents.z
			}
		);
		outState["duckHalfExtents"] = nlohmann::json::array(
			{mDuckHalfExtents.x, mDuckHalfExtents.y, mDuckHalfExtents.z}
		);

		nlohmann::json runtime    = nlohmann::json::object();
		runtime["hasDoubleJump"]  = mRuntime.hasDoubleJump;
		runtime["lastJumpHeld"]   = mRuntime.lastJumpHeld;
		runtime["duckHullActive"] = mRuntime.duckHullActive;
		runtime["wallRun"]        = nlohmann::json::object(
			{
				{"active", mRuntime.wallRun.active},
				{
					"normal", nlohmann::json::array(
						{
							mRuntime.wallRun.normal.x,
							mRuntime.wallRun.normal.y, mRuntime.wallRun.normal.z
						}
					)
				},
				{
					"direction", nlohmann::json::array(
						{
							mRuntime.wallRun.direction.x,
							mRuntime.wallRun.direction.y,
							mRuntime.wallRun.direction.z
						}
					)
				},
				{"time", mRuntime.wallRun.time},
				{"timeSinceLast", mRuntime.wallRun.timeSinceLast},
				{
					"lastWallNormal", nlohmann::json::array(
						{
							mRuntime.wallRun.lastWallNormal.x,
							mRuntime.wallRun.lastWallNormal.y,
							mRuntime.wallRun.lastWallNormal.z
						}
					)
				},
				{"jumpWasHeldOnInit", mRuntime.wallRun.jumpWasHeldOnInit}
			}
		);
		runtime["slide"] = nlohmann::json::object(
			{
				{"active", mRuntime.slide.active},
				{
					"direction", nlohmann::json::array(
						{
							mRuntime.slide.direction.x,
							mRuntime.slide.direction.y,
							mRuntime.slide.direction.z
						}
					)
				},
				{"time", mRuntime.slide.time}
			}
		);
		runtime["blink"] = nlohmann::json::object(
			{
				{"cooldown", mRuntime.blink.cooldown},
				{"active", mRuntime.blink.active},
				{"moveTime", mRuntime.blink.moveTime},
				{
					"startPos", nlohmann::json::array(
						{
							mRuntime.blink.startPos.x,
							mRuntime.blink.startPos.y,
							mRuntime.blink.startPos.z
						}
					)
				},
				{
					"targetPos", nlohmann::json::array(
						{
							mRuntime.blink.targetPos.x,
							mRuntime.blink.targetPos.y,
							mRuntime.blink.targetPos.z
						}
					)
				}
			}
		);
		runtime["vault"] = nlohmann::json::object(
			{
				{"active", mRuntime.vault.active},
				{"time", mRuntime.vault.time},
				{"cooldown", mRuntime.vault.cooldown},
				{
					"startPos", nlohmann::json::array(
						{
							mRuntime.vault.startPos.x,
							mRuntime.vault.startPos.y,
							mRuntime.vault.startPos.z
						}
					)
				},
				{
					"apexPos", nlohmann::json::array(
						{
							mRuntime.vault.apexPos.x,
							mRuntime.vault.apexPos.y,
							mRuntime.vault.apexPos.z
						}
					)
				},
				{
					"endPos", nlohmann::json::array(
						{
							mRuntime.vault.endPos.x,
							mRuntime.vault.endPos.y,
							mRuntime.vault.endPos.z
						}
					)
				},
				{
					"preVelocity", nlohmann::json::array(
						{
							mRuntime.vault.preVelocity.x,
							mRuntime.vault.preVelocity.y,
							mRuntime.vault.preVelocity.z
						}
					)
				}
			}
		);
		runtime["grapple"] = nlohmann::json::object(
			{
				{"active", mRuntime.grapple.isActive},
				{"latched", mRuntime.grapple.isLatched},
				{
					"anchorPoint", nlohmann::json::array(
						{
							mRuntime.grapple.anchorPoint.x,
							mRuntime.grapple.anchorPoint.y,
							mRuntime.grapple.anchorPoint.z
						}
					)
				},
				{"ropeLength", mRuntime.grapple.ropeLength},
				{"minRopeLength", mRuntime.grapple.minRopeLength},
				{"maxRopeLength", mRuntime.grapple.maxRopeLength}
			}
		);

		outState["runtime"] = std::move(runtime);
	}

	void ParkourMovementComponent::ReadReplayState(
		const nlohmann::json& inState
	) {
		if (!inState.is_object()) {
			return;
		}

		if (const auto itBase = inState.find("base");
			itBase != inState.end() && itBase->is_object()) {
			GameMovementComponent::ReadReplayState(*itBase);
		} else {
			// backward compatibility for older/flat dump styles
			GameMovementComponent::ReadReplayState(inState);
		}
		(void)TryReadVec3FromObject(
			inState,
			"standingHalfExtents",
			mStandingHalfExtents
		);
		(void)TryReadVec3FromObject(
			inState,
			"duckHalfExtents",
			mDuckHalfExtents
		);

		if (const auto itRuntime = inState.find("runtime");
			itRuntime != inState.end() && itRuntime->is_object()) {
			const nlohmann::json& runtime = *itRuntime;
			mRuntime.hasDoubleJump        = runtime.value(
				"hasDoubleJump",
				mRuntime.hasDoubleJump
			);
			mRuntime.lastJumpHeld = runtime.value(
				"lastJumpHeld",
				mRuntime.lastJumpHeld
			);
			mRuntime.duckHullActive = runtime.value(
				"duckHullActive",
				mRuntime.duckHullActive
			);
			if (const auto itWallRun = runtime.find("wallRun");
				itWallRun != runtime.end() && itWallRun->is_object()) {
				const nlohmann::json& wallRun = *itWallRun;
				mRuntime.wallRun.active       = wallRun.value(
					"active",
					mRuntime.wallRun.active
				);
				(void)TryReadVec3FromObject(
					wallRun,
					"normal",
					mRuntime.wallRun.normal
				);
				(void)TryReadVec3FromObject(
					wallRun,
					"direction",
					mRuntime.wallRun.direction
				);
				mRuntime.wallRun.time = wallRun.value(
					"time",
					mRuntime.wallRun.time
				);
				mRuntime.wallRun.timeSinceLast = wallRun.value(
					"timeSinceLast",
					mRuntime.wallRun.timeSinceLast
				);
				(void)TryReadVec3FromObject(
					wallRun,
					"lastWallNormal",
					mRuntime.wallRun.lastWallNormal
				);
				mRuntime.wallRun.jumpWasHeldOnInit = wallRun.value(
					"jumpWasHeldOnInit",
					mRuntime.wallRun.jumpWasHeldOnInit
				);
			}

			if (const auto itSlide = runtime.find("slide");
				itSlide != runtime.end() && itSlide->is_object()) {
				const nlohmann::json& slide = *itSlide;
				mRuntime.slide.active       = slide.value(
					"active",
					mRuntime.slide.active
				);
				(void)TryReadVec3FromObject(
					slide,
					"direction",
					mRuntime.slide.direction
				);
				mRuntime.slide.time = slide.value(
					"time",
					mRuntime.slide.time
				);
			}

			if (const auto itBlink = runtime.find("blink");
				itBlink != runtime.end() && itBlink->is_object()) {
				const nlohmann::json& blink = *itBlink;
				mRuntime.blink.cooldown     = blink.value(
					"cooldown",
					mRuntime.blink.cooldown
				);
				mRuntime.blink.active = blink.value(
					"active",
					mRuntime.blink.active
				);
				mRuntime.blink.moveTime = blink.value(
					"moveTime",
					mRuntime.blink.moveTime
				);
				(void)TryReadVec3FromObject(
					blink,
					"startPos",
					mRuntime.blink.startPos
				);
				(void)TryReadVec3FromObject(
					blink,
					"targetPos",
					mRuntime.blink.targetPos
				);
			}

			if (const auto itVault = runtime.find("vault");
				itVault != runtime.end() && itVault->is_object()) {
				const nlohmann::json& vault = *itVault;
				mRuntime.vault.active       = vault.value(
					"active",
					mRuntime.vault.active
				);
				mRuntime.vault.time = vault.value(
					"time",
					mRuntime.vault.time
				);
				mRuntime.vault.cooldown = vault.value(
					"cooldown",
					mRuntime.vault.cooldown
				);
				(void)TryReadVec3FromObject(
					vault,
					"startPos",
					mRuntime.vault.startPos
				);
				(void)TryReadVec3FromObject(
					vault,
					"apexPos",
					mRuntime.vault.apexPos
				);
				(void)TryReadVec3FromObject(
					vault,
					"endPos",
					mRuntime.vault.endPos
				);
				(void)TryReadVec3FromObject(
					vault,
					"preVelocity",
					mRuntime.vault.preVelocity
				);
			}

			if (const auto itGrapple = runtime.find("grapple");
				itGrapple != runtime.end() && itGrapple->is_object()) {
				const nlohmann::json& grapple = *itGrapple;
				mRuntime.grapple.isActive = grapple.value(
					"active",
					mRuntime.grapple.isActive
				);
				mRuntime.grapple.isLatched = grapple.value(
					"latched",
					mRuntime.grapple.isLatched
				);
				(void)TryReadVec3FromObject(
					grapple,
					"anchorPoint",
					mRuntime.grapple.anchorPoint
				);
				mRuntime.grapple.ropeLength = grapple.value(
					"ropeLength",
					mRuntime.grapple.ropeLength
				);
				mRuntime.grapple.minRopeLength = grapple.value(
					"minRopeLength",
					mRuntime.grapple.minRopeLength
				);
				mRuntime.grapple.maxRopeLength = grapple.value(
					"maxRopeLength",
					mRuntime.grapple.maxRopeLength
				);
			}
		}

		if (mStandingHalfExtents.IsZero()) {
			mStandingHalfExtents = mBoxHalfExtents;
		}
		if (mDuckHalfExtents.IsZero()) {
			RebuildDuckHalfExtents();
		}
		mBoxHalfExtents = mRuntime.duckHullActive ?
			                  mDuckHalfExtents :
			                  mStandingHalfExtents;

		if (TransformComponent* transform = GetTransform()) {
			UpdateCollisionHull(transform);
		}
	}

	uint64_t ParkourMovementComponent::ComputeReplayStateHash() const {
		uint64_t       hash     = ReplayHash::Begin();
		const uint64_t baseHash =
			GameMovementComponent::ComputeReplayStateHash();
		ReplayHash::AppendPod(hash, baseHash);
		ReplayHash::AppendFloating(hash, mStandingHalfExtents.x);
		ReplayHash::AppendFloating(hash, mStandingHalfExtents.y);
		ReplayHash::AppendFloating(hash, mStandingHalfExtents.z);
		ReplayHash::AppendFloating(hash, mDuckHalfExtents.x);
		ReplayHash::AppendFloating(hash, mDuckHalfExtents.y);
		ReplayHash::AppendFloating(hash, mDuckHalfExtents.z);

		ReplayHash::AppendPod(hash, mRuntime.hasDoubleJump);
		ReplayHash::AppendPod(hash, mRuntime.lastJumpHeld);
		ReplayHash::AppendPod(hash, mRuntime.duckHullActive);

		ReplayHash::AppendPod(hash, mRuntime.wallRun.active);
		ReplayHash::AppendFloating(hash, mRuntime.wallRun.normal.x);
		ReplayHash::AppendFloating(hash, mRuntime.wallRun.normal.y);
		ReplayHash::AppendFloating(hash, mRuntime.wallRun.normal.z);
		ReplayHash::AppendFloating(hash, mRuntime.wallRun.direction.x);
		ReplayHash::AppendFloating(hash, mRuntime.wallRun.direction.y);
		ReplayHash::AppendFloating(hash, mRuntime.wallRun.direction.z);
		ReplayHash::AppendFloating(hash, mRuntime.wallRun.time);
		ReplayHash::AppendFloating(hash, mRuntime.wallRun.timeSinceLast);
		ReplayHash::AppendFloating(hash, mRuntime.wallRun.lastWallNormal.x);
		ReplayHash::AppendFloating(hash, mRuntime.wallRun.lastWallNormal.y);
		ReplayHash::AppendFloating(hash, mRuntime.wallRun.lastWallNormal.z);
		ReplayHash::AppendPod(hash, mRuntime.wallRun.jumpWasHeldOnInit);

		ReplayHash::AppendPod(hash, mRuntime.slide.active);
		ReplayHash::AppendFloating(hash, mRuntime.slide.direction.x);
		ReplayHash::AppendFloating(hash, mRuntime.slide.direction.y);
		ReplayHash::AppendFloating(hash, mRuntime.slide.direction.z);
		ReplayHash::AppendFloating(hash, mRuntime.slide.time);

		ReplayHash::AppendPod(hash, mRuntime.blink.active);
		ReplayHash::AppendFloating(hash, mRuntime.blink.cooldown);
		ReplayHash::AppendFloating(hash, mRuntime.blink.moveTime);
		ReplayHash::AppendFloating(hash, mRuntime.blink.startPos.x);
		ReplayHash::AppendFloating(hash, mRuntime.blink.startPos.y);
		ReplayHash::AppendFloating(hash, mRuntime.blink.startPos.z);
		ReplayHash::AppendFloating(hash, mRuntime.blink.targetPos.x);
		ReplayHash::AppendFloating(hash, mRuntime.blink.targetPos.y);
		ReplayHash::AppendFloating(hash, mRuntime.blink.targetPos.z);

		ReplayHash::AppendPod(hash, mRuntime.vault.active);
		ReplayHash::AppendFloating(hash, mRuntime.vault.time);
		ReplayHash::AppendFloating(hash, mRuntime.vault.cooldown);
		ReplayHash::AppendFloating(hash, mRuntime.vault.startPos.x);
		ReplayHash::AppendFloating(hash, mRuntime.vault.startPos.y);
		ReplayHash::AppendFloating(hash, mRuntime.vault.startPos.z);
		ReplayHash::AppendFloating(hash, mRuntime.vault.apexPos.x);
		ReplayHash::AppendFloating(hash, mRuntime.vault.apexPos.y);
		ReplayHash::AppendFloating(hash, mRuntime.vault.apexPos.z);
		ReplayHash::AppendFloating(hash, mRuntime.vault.endPos.x);
		ReplayHash::AppendFloating(hash, mRuntime.vault.endPos.y);
		ReplayHash::AppendFloating(hash, mRuntime.vault.endPos.z);
		ReplayHash::AppendFloating(hash, mRuntime.vault.preVelocity.x);
		ReplayHash::AppendFloating(hash, mRuntime.vault.preVelocity.y);
		ReplayHash::AppendFloating(hash, mRuntime.vault.preVelocity.z);
		ReplayHash::AppendPod(hash, mRuntime.grapple.isActive);
		ReplayHash::AppendPod(hash, mRuntime.grapple.isLatched);
		ReplayHash::AppendFloating(hash, mRuntime.grapple.anchorPoint.x);
		ReplayHash::AppendFloating(hash, mRuntime.grapple.anchorPoint.y);
		ReplayHash::AppendFloating(hash, mRuntime.grapple.anchorPoint.z);
		ReplayHash::AppendFloating(hash, mRuntime.grapple.ropeLength);
		ReplayHash::AppendFloating(hash, mRuntime.grapple.minRopeLength);
		ReplayHash::AppendFloating(hash, mRuntime.grapple.maxRopeLength);
		return hash;
	}

	uint32_t ParkourMovementComponent::GetIcon() const {
		return kIconDirectionsRun;
	}

	ParkourMovementComponent::ParkourRuntime&
	ParkourMovementComponent::GetParkourRuntime() {
		return mRuntime;
	}

	const ParkourMovementComponent::ParkourRuntime&
	ParkourMovementComponent::GetParkourRuntime() const {
		return mRuntime;
	}

	void ParkourMovementComponent::TickParkourTimers(const float deltaTime) {
		const float clampedDt          = std::max(0.0f, deltaTime);
		mRuntime.wallRun.timeSinceLast += clampedDt;
		mRuntime.blink.cooldown        = std::max(
			0.0f, mRuntime.blink.cooldown - clampedDt
		);
		mRuntime.vault.cooldown = std::max(
			0.0f, mRuntime.vault.cooldown - clampedDt
		);
	}

	void ParkourMovementComponent::PublishParkourMovementCue(
		const std::string_view id,
		const float            value,
		const float            value2
	) {
		PublishCue(std::string(id), value, value2);
	}

	bool ParkourMovementComponent::SetDuckHullEnabled(
		MovementContext& context,
		const bool       enabled
	) {
		return enabled ? ApplyDuckHull(context) : ApplyStandHull(context);
	}

	bool ParkourMovementComponent::IsDuckHullEnabled() const {
		return mRuntime.duckHullActive;
	}

	bool ParkourMovementComponent::CanStandAt(
		const MovementContext& context
	) const {
		if (!context.transform || !context.resolver) {
			return true;
		}

		const auto* physics = context.resolver->GetPhysics();
		if (!physics) {
			return true;
		}

		const float deltaHalfY = std::max(
			0.0f,
			mStandingHalfExtents.y - mDuckHalfExtents.y
		);

		const Box testBox = {
			.center   = context.transform->GetPosition() + Vec3::up * deltaHalfY,
			.halfSize = mStandingHalfExtents
		};

		Physics::Hit hit{};
		return !physics->BoxOverlap(testBox, &hit);
	}

	float ParkourMovementComponent::GetHorizontalSpeedHu(
		const Vec3& velocity
	) const {
		Vec3 horizontal = velocity;
		horizontal.y    = 0.0f;
		return Math::MtoH(horizontal.Length());
	}

	bool ParkourMovementComponent::ShouldSlideFromSpeed(
		const float horizontalSpeedHu
	) const {
		const float minSlideSpeedHu = mConsole ?
			                              mConsole->GetConVarValueOr(
				                              "park_slide_minspeed", 200.0f
			                              ) :
			                              200.0f;
		return horizontalSpeedHu >= minSlideSpeedHu;
	}

	bool ParkourMovementComponent::WantsDuckStance(
		const MovementContext& context
	) const {
		if (context.input.crouchPressed) {
			return true;
		}
		if (!CanStandAt(context)) {
			return true;
		}
		return (mActiveAbilityMask & AbilitySlotToMask(MOVEMENT_ABILITY_SLOT::SLIDE)
		       ) != 0;
	}

	void ParkourMovementComponent::SyncCollisionHull(
		TransformComponent* transform
	) {
		UpdateCollisionHull(transform);
	}

	void ParkourMovementComponent::EndWallRun() {
		if (!mRuntime.wallRun.active) {
			return;
		}

		mRuntime.wallRun.active            = false;
		mRuntime.wallRun.time              = 0.0f;
		mRuntime.wallRun.lastWallNormal    = mRuntime.wallRun.normal;
		mRuntime.wallRun.timeSinceLast     = 0.0f;
		mRuntime.wallRun.jumpWasHeldOnInit = false;
	}

	void ParkourMovementComponent::RegisterMovementModes(
		GameMovementStateMachine& stateMachine
	) {
		GameMovementComponent::RegisterMovementModes(stateMachine);
	}

	void ParkourMovementComponent::RegisterMovementAbilities(
		GameMovementStateMachine& stateMachine
	) {
		RegisterParkourMovementAbilities(stateMachine);
	}

	MOVEMENT_MODE_ID ParkourMovementComponent::GetInitialMode() const {
		return MOVEMENT_MODE_ID::AIR;
	}

	MOVEMENT_MODE_ID ParkourMovementComponent::GetAirModeForTransitions() const {
		return MOVEMENT_MODE_ID::AIR;
	}

	std::string ParkourMovementComponent::ResolvePresentationStateName() const {
		if (
			(mActiveAbilityMask & AbilitySlotToMask(MOVEMENT_ABILITY_SLOT::BLINK)) !=
			0
		) {
			return "ParkourBlinkMove";
		}
		if (
			(mActiveAbilityMask &
			 AbilitySlotToMask(MOVEMENT_ABILITY_SLOT::SPEED_VAULT)) != 0
		) {
			return "ParkourSpeedVaultMove";
		}
		if (
			(mActiveAbilityMask &
			 AbilitySlotToMask(MOVEMENT_ABILITY_SLOT::WALL_RUN)) != 0
		) {
			return "ParkourWallRunMove";
		}
		if (
			(mActiveAbilityMask & AbilitySlotToMask(MOVEMENT_ABILITY_SLOT::SLIDE)) !=
			0
		) {
			return "ParkourSlideMove";
		}

		if (mCurrentModeId == MOVEMENT_MODE_ID::GROUND) {
			return mRuntime.duckHullActive ?
				       std::string("ParkourDuckMove") :
				       std::string("ParkourGroundMove");
		}
		if (mCurrentModeId == MOVEMENT_MODE_ID::NOCLIP) {
			return "NoclipMove";
		}
		return "ParkourAirMove";
	}

	bool ParkourMovementComponent::UseSprintSpeedAsDefaultGroundSpeed() const {
		return true;
	}

	void ParkourMovementComponent::OnAfterCoreCueDispatch(
		const std::string_view,
		const std::string_view    currentStateName,
		const MovementFrameInput& input,
		const bool,
		const bool  isGrounded,
		const float stepSeconds
	) {
		const std::string loweredState         = StrUtil::ToLowerCase(currentStateName.data());
		const bool        isParkourGroundState = loweredState ==
		                                         kStateParkourGroundMoveLower;
		const bool isWallRunState = loweredState == "parkourwallrunmove";
		const bool isSlideState   = loweredState == "parkourslidemove";

		const float sprintSpeedHu = mConsole ?
			                            mConsole->GetConVarValueOr(
				                            "sv_sprintspeed",
				                            320.0f
			                            ) :
			                            320.0f;
		const float sprintSpeedSafeHu = std::max(1.0f, sprintSpeedHu);

		Vec3 horizontalVelocity       = mVelocity;
		horizontalVelocity.y          = 0.0f;
		const float horizontalSpeedHu = Math::MtoH(horizontalVelocity.Length());

		Vec3 horizontalForward   = input.forward;
		horizontalForward.y      = 0.0f;
		const bool hasForwardDir = !horizontalForward.IsZero();
		if (hasForwardDir) {
			horizontalForward.Normalize();
		}
		const bool  hasHorizontalVelocity = !horizontalVelocity.IsZero();
		const float forwardDot = hasForwardDir && hasHorizontalVelocity ?
			                         horizontalForward.Dot(
				                         horizontalVelocity.Normalized()
			                         ) :
			                         -1.0f;

		const bool shouldSprint = isGrounded &&
		                          isParkourGroundState &&
		                          hasForwardDir &&
		                          hasHorizontalVelocity &&
		                          horizontalSpeedHu >= kMinSprintMoveSpeedHu &&
		                          forwardDot >= kForwardSprintDotThreshold;
		if (shouldSprint != mAutoSprintActive) {
			// movement.sprint.start / movement.sprint.end payload contract:
			// value  = unused (0)
			// value2 = unused (0)
			PublishCue(
				shouldSprint ? "movement.sprint.start" : "movement.sprint.end"
			);
			mAutoSprintActive = shouldSprint;
		}
		if (shouldSprint) {
			const float sprintRatio = std::clamp(
				horizontalSpeedHu / sprintSpeedSafeHu, 0.0f, 2.0f
			);
			// movement.sprint.update payload contract:
			// value  = sprint ratio [0..2] (horizontalSpeedHu / sv_sprintspeed)
			// value2 = unused (0)
			// named:
			//   payload.sprintRatio
			PublishCue("movement.sprint.update", sprintRatio);
		}

		if (!loweredState.empty()) {
			// movement.state.update.* payload contract:
			// value  = state update intensity (currently 1.0 while active)
			// value2 = unused (0)
			PublishCue("movement.state.update." + loweredState, 1.0f);
		}

		if (mRuntime.pendingDoubleJumpCue) {
			// movement.doublejump: 現状は payload 未使用（value/value2 は 0）。
			PublishCue("movement.doublejump");
			mRuntime.pendingDoubleJumpCue = false;
		}

		const bool allowFootstep = isGrounded || isWallRunState;
		if (!allowFootstep || isSlideState) {
			mRuntime.footstepDistanceAccumHu = 0.0f;
			return;
		}

		const float footstepMinSpeedHu = mConsole ?
			                                 mConsole->GetConVarValueOr(
				                                 "park_footstep_minspeed", 80.0f
			                                 ) :
			                                 80.0f;
		if (horizontalSpeedHu < footstepMinSpeedHu || stepSeconds <= 0.0f) {
			mRuntime.footstepDistanceAccumHu = 0.0f;
			return;
		}

		const float footstepStrideHu = mConsole ?
			                               mConsole->GetConVarValueOr(
				                               "park_footstep_stride", 100.0f
			                               ) :
			                               170.0f;
		const float footstepMaxSpeedHu = mConsole ?
			                                 mConsole->GetConVarValueOr(
				                                 "park_footstep_maxspeed",
				                                 600.0f
			                                 ) :
			                                 360.0f;
		const float cappedSpeedHu = std::clamp(
			horizontalSpeedHu,
			0.0f,
			std::max(footstepMinSpeedHu, footstepMaxSpeedHu)
		);
		const float safeStrideHu         = std::max(1.0f, footstepStrideHu);
		mRuntime.footstepDistanceAccumHu += cappedSpeedHu * stepSeconds;
		const float footstepScale        = std::clamp(
			cappedSpeedHu / sprintSpeedSafeHu, 0.0f, 2.0f
		);
		while (mRuntime.footstepDistanceAccumHu >= safeStrideHu) {
			mRuntime.footstepDistanceAccumHu -= safeStrideHu;
			// movement.footstep payload contract:
			// value  = footstepScale (speed-normalized intensity, [0..2])
			// value2 = capped horizontal speed in HU/s
			// named:
			//   payload.footstepScale
			//   payload.horizontalSpeedHuPerSec
			PublishCue("movement.footstep", footstepScale, cappedSpeedHu);
		}
	}

	void ParkourMovementComponent::RebuildDuckHalfExtents() {
		const float duckScale = std::clamp(
			mConsole ?
				mConsole->GetConVarValueOr("park_duck_heightscale", 0.75f) :
				0.75f,
			0.2f,
			1.0f
		);
		mDuckHalfExtents   = mStandingHalfExtents;
		mDuckHalfExtents.y = mStandingHalfExtents.y * duckScale;
	}

	void ParkourMovementComponent::ResetParkourRuntime() {
		mRuntime                       = {};
		mRuntime.wallRun.timeSinceLast = 999.0f;
	}

	bool ParkourMovementComponent::ApplyDuckHull(MovementContext& context) {
		if (mRuntime.duckHullActive) {
			context.halfExtents = mBoxHalfExtents;
			return true;
		}
		if (!context.transform) {
			return false;
		}

		const float deltaHalfY = std::max(
			0.0f,
			mStandingHalfExtents.y - mDuckHalfExtents.y
		);
		context.transform->SetPosition(
			context.transform->GetPosition() - Vec3::up * deltaHalfY
		);

		mBoxHalfExtents         = mDuckHalfExtents;
		context.halfExtents     = mDuckHalfExtents;
		mRuntime.duckHullActive = true;
		UpdateCollisionHull(context.transform);
		return true;
	}

	bool ParkourMovementComponent::ApplyStandHull(MovementContext& context) {
		if (!mRuntime.duckHullActive) {
			context.halfExtents = mBoxHalfExtents;
			return true;
		}
		if (!context.transform) {
			return false;
		}
		if (!CanStandAt(context)) {
			return false;
		}

		const float deltaHalfY = std::max(
			0.0f,
			mStandingHalfExtents.y - mDuckHalfExtents.y
		);
		context.transform->SetPosition(
			context.transform->GetPosition() + Vec3::up * deltaHalfY
		);

		mBoxHalfExtents         = mStandingHalfExtents;
		context.halfExtents     = mStandingHalfExtents;
		mRuntime.duckHullActive = false;
		UpdateCollisionHull(context.transform);
		return true;
	}

	REGISTER_COMPONENT(ParkourMovementComponent);
}
