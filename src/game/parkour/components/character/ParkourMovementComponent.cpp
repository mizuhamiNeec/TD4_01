#include "ParkourMovementComponent.h"

#include <algorithm>
#include <array>
#include <imgui.h>

#include "core/ComponentRegistry.h"

// ReSharper disable once CppUnusedIncludeDirective
#include "core/string/StrUtil.h"

#include "engine/ImGui/Icons.h"
#include "engine/physics/core/Physics.h"
#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/unnamed/primitive/Primitives.h"
#include "engine/world/World.h"

#include "game/core/collision/kinematic/base/BaseKinematicCollisionResolver.h"
#include "game/core/components/character/state/GameMovementStateMachine.h"
#include "game/core/components/character/state/interface/IMovementState.h"
#include "game/core/components/character/state/MovementStateIds.h"
#include "game/core/replay/ReplayHash.h"

#include "state/ParkourMovementStates.h"

namespace Unnamed {
	namespace {
		constexpr std::string_view kStateParkourGroundMoveLower =
			"parkourgroundmove";
		constexpr float kForwardSprintDotThreshold = 0.7f;
		constexpr float kMinSprintMoveSpeedHu      = 5.0f;

		Vec3 HorizontalNormalized(Vec3 value) {
			value.y = 0.0f;
			if (value.IsZero()) {
				return Vec3::zero;
			}
			value.Normalize();
			return value;
		}

		Vec3 SafeNormalized(Vec3 value) {
			if (value.IsZero()) {
				return Vec3::zero;
			}
			value.Normalize();
			return value;
		}

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

		GameMovementComponent::SimulateStep(transform, input, stepSeconds);
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

	std::string ParkourMovementComponent::ResolveGroundStateFromInput(
		const MovementContext& context
	) const {
		if (context.input.crouchPressed) {
			const float speedHu = GetHorizontalSpeedHu(context.velocity);
			if (ShouldSlideFromSpeed(speedHu)) {
				return std::string(MovementStateIds::ParkourSlide);
			}
			return std::string(MovementStateIds::ParkourDuck);
		}

		if (!CanStandAt(context)) {
			return std::string(MovementStateIds::ParkourDuck);
		}

		return std::string(MovementStateIds::ParkourGround);
	}

	bool ParkourMovementComponent::TryStartBlink(MovementContext& context) {
		if (!context.transform || !context.resolver) {
			return false;
		}

		if (!context.input.sprintPressed || mRuntime.blink.active ||
		    mRuntime.vault.active || mRuntime.blink.cooldown > 0.0f) {
			return false;
		}

		const Vec3 dir = SafeNormalized(context.input.forward);
		if (dir.IsZero()) {
			return false;
		}

		const float blinkDistanceHu = mConsole ?
			                              mConsole->GetConVarValueOr(
				                              "park_blink_distance", 512.0f
			                              ) :
			                              512.0f;
		const Vec3 startPos       = context.transform->Position();
		Vec3       targetPos      = startPos;
		Vec3       pseudoVelocity = dir * Math::HtoM(blinkDistanceHu);
		context.resolver->SlideMove(targetPos, pseudoVelocity, 1.0f);

		const Vec3 blinkDelta = targetPos - startPos;
		if (blinkDelta.SqrLength() <= 1.0e-10f) {
			return false;
		}

		const float verticalSpeed      = context.velocity.y;
		Vec3        horizontalVelocity = context.velocity;
		horizontalVelocity.y           = 0.0f;
		const float horizontalSpeed    = horizontalVelocity.Length();

		Vec3 horizontalDelta = blinkDelta;
		horizontalDelta.y    = 0.0f;
		if (!horizontalDelta.IsZero() && horizontalSpeed > 0.0f) {
			horizontalDelta.Normalize();
			context.velocity   = horizontalDelta * horizontalSpeed;
			context.velocity.y = verticalSpeed;
		}

		mRuntime.blink.active    = true;
		mRuntime.blink.moveTime  = 0.0f;
		mRuntime.blink.startPos  = startPos;
		mRuntime.blink.targetPos = targetPos;
		mRuntime.blink.cooldown  = mConsole ?
			                           mConsole->GetConVarValueOr(
				                           "park_blink_cooldown", 1.0f
			                           ) :
			                           1.0f;

		context.isGrounded               = false;
		context.supportEntityGuid        = 0;
		context.supportLinearVelocity    = Vec3::zero;
		context.supportStepDelta         = Vec3::zero;
		context.jumpSnapDisableRemaining = std::max(
			context.jumpSnapDisableRemaining,
			mConsole ?
				mConsole->GetConVarValueOr("sv_jumpsnapdisabletime", 0.1f) :
				0.1f
		);
		context.requestedState = MovementStateIds::ParkourBlink;
		return true;
	}

	bool ParkourMovementComponent::UpdateBlink(
		MovementContext& context,
		const float      deltaTime
	) {
		if (!mRuntime.blink.active || !context.transform) {
			return false;
		}

		const float duration = std::max(
			1.0e-4f,
			mConsole ?
				mConsole->GetConVarValueOr("park_blink_moveduration", 0.08f) :
				0.08f
		);
		mRuntime.blink.moveTime += std::max(0.0f, deltaTime);
		const float t           = std::clamp(
			mRuntime.blink.moveTime / duration, 0.0f, 1.0f
		);
		const Vec3 position = Math::Lerp(
			mRuntime.blink.startPos,
			mRuntime.blink.targetPos,
			t
		);
		context.transform->SetPosition(position);
		UpdateCollisionHull(context.transform);

		context.isGrounded            = false;
		context.supportEntityGuid     = 0;
		context.supportLinearVelocity = Vec3::zero;
		context.supportStepDelta      = Vec3::zero;

		if (t >= 1.0f) {
			mRuntime.blink.active  = false;
			context.requestedState = MovementStateIds::ParkourAir;
		}
		return true;
	}

	bool ParkourMovementComponent::TryStartWallRun(MovementContext& context) {
		if (!context.transform || !context.resolver) {
			return false;
		}
		const auto* physics = context.resolver->GetPhysics();
		if (!physics) {
			return false;
		}

		if (context.isGrounded || context.input.crouchPressed ||
		    mRuntime.wallRun.active) {
			return false;
		}

		Vec3 velHorz          = context.velocity;
		velHorz.y             = 0.0f;
		const float minSpeedM = Math::HtoM(
			mConsole ?
				mConsole->GetConVarValueOr("park_wallrun_minspeed", 200.0f) :
				200.0f
		);
		if (velHorz.SqrLength() < minSpeedM * minSpeedM) {
			return false;
		}

		const float cooldown = mConsole ?
			                       mConsole->GetConVarValueOr(
				                       "park_wallrun_cooldown", 0.1f
			                       ) :
			                       0.1f;
		if (mRuntime.wallRun.timeSinceLast < cooldown) {
			return false;
		}

		Vec3 forward = HorizontalNormalized(context.input.forward);
		if (forward.IsZero()) {
			return false;
		}

		Vec3 right = HorizontalNormalized(context.input.right);
		if (right.IsZero()) {
			right = SafeNormalized(Vec3::up.Cross(forward));
			if (right.IsZero()) {
				return false;
			}
		}

		const float sideCheckDistance = context.halfExtents.x + Math::HtoM(
			                                mConsole ?
				                                mConsole->GetConVarValueOr(
					                                "park_wallrun_checkdistance",
					                                10.0f
				                                ) :
				                                10.0f
		                                );

		const std::array<Vec3, 2> checkDirections = {
			right,
			-right
		};

		for (const Vec3& checkDir : checkDirections) {
			Physics::Hit hit{};
			Box          probe = {
				.center   = context.transform->Position(),
				.halfSize = context.halfExtents
			};
			if (!physics->BoxCast(probe, checkDir, sideCheckDistance, &hit)) {
				continue;
			}

			Vec3 wallNormal = SafeNormalized(hit.normal);
			if (wallNormal.IsZero() || std::abs(wallNormal.y) > 0.2f) {
				continue;
			}

			if (!velHorz.IsZero()) {
				const Vec3  velDir = velHorz.Normalized();
				const float dot    = std::abs(velDir.Dot(wallNormal));
				const float maxDot = mConsole ?
					                     mConsole->GetConVarValueOr(
						                     "park_wallrun_maxnormaldot", 0.5f
					                     ) :
					                     0.5f;
				if (dot > maxDot) {
					continue;
				}
			}

			const float sameWallCooldown = mConsole ?
				                               mConsole->GetConVarValueOr(
					                               "park_wallrun_samewallcooldown",
					                               1.0f
				                               ) :
				                               1.0f;
			if (
				mRuntime.wallRun.timeSinceLast < sameWallCooldown &&
				wallNormal.Dot(mRuntime.wallRun.lastWallNormal) > 0.9f
			) {
				continue;
			}

			mRuntime.wallRun.active            = true;
			mRuntime.wallRun.normal            = wallNormal;
			mRuntime.wallRun.time              = 0.0f;
			mRuntime.wallRun.jumpWasHeldOnInit = context.input.jumpPressed;
			mRuntime.hasDoubleJump             = true;
			mRuntime.slide.active              = false;
			const bool isRightWall             = checkDir.Dot(right) > 0.0f;
			PublishCue(
				isRightWall ?
					"movement.wallrun.start.right" :
					"movement.wallrun.start.left"
			);

			Vec3 along = SafeNormalized(Vec3::up.Cross(wallNormal));
			if (along.Dot(forward) < 0.0f) {
				along = -along;
			}
			mRuntime.wallRun.direction = along;

			const float originalY    = context.velocity.y;
			const float currentSpeed = velHorz.Length();
			const float alongSpeed   = velHorz.Dot(along);
			if (std::abs(alongSpeed) > 1.0e-3f) {
				context.velocity = along * std::abs(alongSpeed);
			} else {
				context.velocity = along * currentSpeed;
			}

			const float verticalDamping = mConsole ?
				                              mConsole->GetConVarValueOr(
					                              "park_wallrun_verticaldamping",
					                              0.3f
				                              ) :
				                              0.3f;
			if (originalY > 0.0f) {
				context.velocity.y = originalY * verticalDamping;
			} else if (originalY < 0.0f) {
				context.velocity.y = originalY * 0.3f;
			}

			const float startBoost = mConsole ?
				                         mConsole->GetConVarValueOr(
					                         "park_wallrun_startboost", 50.0f
				                         ) :
				                         50.0f;
			context.velocity       += along * Math::HtoM(startBoost);
			context.requestedState = MovementStateIds::ParkourWallRun;
			return true;
		}

		return false;
	}

	bool ParkourMovementComponent::UpdateWallRun(
		MovementContext& context,
		const float      deltaTime
	) {
		if (!mRuntime.wallRun.active || !context.transform || !context.
		    resolver) {
			return false;
		}
		const auto* physics = context.resolver->GetPhysics();
		if (!physics) {
			EndWallRun();
			context.requestedState = MovementStateIds::ParkourAir;
			return false;
		}

		mRuntime.wallRun.time += std::max(0.0f, deltaTime);
		const float maxTime   = mConsole ?
			                        mConsole->GetConVarValueOr(
				                        "park_wallrun_maxtime", 2.5f
			                        ) :
			                        2.5f;
		if (mRuntime.wallRun.time >= maxTime) {
			EndWallRun();
			context.requestedState = MovementStateIds::ParkourAir;
			return false;
		}

		const float maintainDistance = context.halfExtents.x + Math::HtoM(
			                               mConsole ?
				                               mConsole->GetConVarValueOr(
					                               "park_wallrun_maintaindistance",
					                               20.0f
				                               ) :
				                               20.0f
		                               );
		Physics::Hit wallHit{};
		Box          probe = {
			.center   = context.transform->Position(),
			.halfSize = context.halfExtents
		};
		if (!physics->BoxCast(
			probe,
			-mRuntime.wallRun.normal,
			maintainDistance,
			&wallHit
		)) {
			EndWallRun();
			context.requestedState = MovementStateIds::ParkourAir;
			return false;
		}

		const Vec3 newNormal = SafeNormalized(wallHit.normal);
		if (newNormal.IsZero() || newNormal.Dot(mRuntime.wallRun.normal) <
		    0.5f) {
			EndWallRun();
			context.requestedState = MovementStateIds::ParkourAir;
			return false;
		}

		mRuntime.wallRun.normal = SafeNormalized(
			mRuntime.wallRun.normal * 0.8f + newNormal * 0.2f
		);

		Vec3 projectedDir = Math::ProjectOnPlane(
			HorizontalNormalized(context.input.forward),
			mRuntime.wallRun.normal
		);
		if (!projectedDir.IsZero()) {
			projectedDir.Normalize();
			mRuntime.wallRun.direction = projectedDir;
		}

		if (context.input.crouchPressed || context.isGrounded) {
			EndWallRun();
			context.requestedState = MovementStateIds::ParkourAir;
			return false;
		}

		Vec3 velHorz           = context.velocity;
		velHorz.y              = 0.0f;
		const float minSpeedHu = mConsole ?
			                         mConsole->GetConVarValueOr(
				                         "park_wallrun_minspeed", 200.0f
			                         ) :
			                         200.0f;
		if (Math::MtoH(velHorz.Length()) < minSpeedHu * 0.5f) {
			EndWallRun();
			context.requestedState = MovementStateIds::ParkourAir;
			return false;
		}

		const bool detachOnSideInput = mConsole ?
			                               mConsole->GetConVarValueOr(
				                               "park_wallrun_detachonsideinput",
				                               true
			                               ) :
			                               true;
		if (detachOnSideInput && std::abs(context.input.moveAxis.x) > 0.5f) {
			Vec3 camRight = HorizontalNormalized(context.input.right);
			if (!camRight.IsZero()) {
				const float wallSide = camRight.Dot(mRuntime.wallRun.normal);
				if (
					(wallSide > 0.0f && context.input.moveAxis.x > 0.5f) ||
					(wallSide < 0.0f && context.input.moveAxis.x < -0.5f)
				) {
					EndWallRun();
					context.requestedState = MovementStateIds::ParkourAir;
					return false;
				}
			}
		}

		if (context.input.jumpPressed) {
			if (!mRuntime.wallRun.jumpWasHeldOnInit) {
				const float jumpForceHu = mConsole ?
					                          mConsole->GetConVarValueOr(
						                          "park_wallrun_jumpforce",
						                          350.0f
					                          ) :
					                          350.0f;
				const Vec3 forwardVel =
					mRuntime.wallRun.direction *
					context.velocity.Dot(mRuntime.wallRun.direction);
				Vec3 awayDir = mRuntime.wallRun.normal * 0.7f + Vec3::up;
				awayDir.Normalize();
				context.velocity = forwardVel + awayDir * Math::HtoM(
					                   jumpForceHu
				                   );
				mRuntime.hasDoubleJump = true;
				PublishCue("movement.wallrun.jump");
				context.jumpSnapDisableRemaining = std::max(
					context.jumpSnapDisableRemaining,
					mConsole ?
						mConsole->GetConVarValueOr(
							"sv_jumpsnapdisabletime", 0.1f
						) :
						0.1f
				);
				EndWallRun();
				context.requestedState = MovementStateIds::ParkourAir;
				return false;
			}
		} else {
			mRuntime.wallRun.jumpWasHeldOnInit = false;
		}

		return true;
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

	bool ParkourMovementComponent::TryStartSpeedVault(
		MovementContext& context
	) {
		if (!context.transform || !context.resolver) {
			return false;
		}
		auto* physics = context.resolver->GetPhysics();
		if (!physics) {
			return false;
		}

		if (context.isGrounded || context.input.crouchPressed ||
		    mRuntime.vault.active || mRuntime.vault.cooldown > 0.0f) {
			return false;
		}
		if (context.input.moveAxis.z < 0.5f) {
			return false;
		}

		const float minSpeedHu = mConsole ?
			                         mConsole->GetConVarValueOr(
				                         "park_vault_minspeed", 150.0f
			                         ) :
			                         150.0f;
		Vec3 velocityH = context.velocity;
		velocityH.y    = 0.0f;
		if (Math::MtoH(velocityH.Length()) < minSpeedHu) {
			return false;
		}

		if (!ApplyStandHull(context)) {
			return false;
		}

		Vec3 forward = HorizontalNormalized(context.input.forward);
		if (forward.IsZero()) {
			return false;
		}

		const Vec3  centerPos     = context.transform->Position();
		const float halfWidthM    = context.halfExtents.x;
		const float halfHeightM   = context.halfExtents.y;
		const float playerHeightM = halfHeightM * 2.0f;
		const Vec3  feetPos       = centerPos - Vec3::up * halfHeightM;

		const float checkDistM = Math::HtoM(
			mConsole ?
				mConsole->GetConVarValueOr("park_vault_forwardcheck", 32.0f) :
				32.0f
		);
		const float maxVaultHeightM = Math::HtoM(
			mConsole ?
				mConsole->GetConVarValueOr("park_vault_maxheight", 72.0f) :
				72.0f
		);

		const Box forwardProbe = {
			.center   = feetPos + Vec3::up * (playerHeightM * 0.25f),
			.halfSize = {
				halfWidthM,
				playerHeightM * 0.25f,
				halfWidthM
			}
		};

		Physics::Hit wallHit{};
		float        distToWall = 0.0f;
		bool         wallFound  = false;
		if (physics->BoxCast(
			forwardProbe, forward, checkDistM + halfWidthM, &wallHit
		)) {
			const Vec3 wallNormal = SafeNormalized(wallHit.normal);
			if (std::abs(wallNormal.y) > 0.3f) {
				return false;
			}
			distToWall = std::max(0.0f, wallHit.t);
			wallFound  = true;
		}

		if (!wallFound) {
			Physics::Hit overlapHit{};
			if (physics->BoxOverlap(forwardProbe, &overlapHit)) {
				const Vec3 wallNormal = SafeNormalized(overlapHit.normal);
				if (std::abs(wallNormal.y) > 0.3f) {
					return false;
				}
				distToWall = 0.0f;
				wallFound  = true;
			}
		}

		if (!wallFound) {
			return false;
		}

		float       wallTopHeightM = 0.0f;
		bool        foundTop       = false;
		const float probeStepM     = Math::HtoM(8.0f);
		const float probeSizeM     = Math::HtoM(4.0f);
		const float startCheckM    = -Math::HtoM(16.0f);
		for (
			float testHeight = startCheckM;
			testHeight <= maxVaultHeightM + probeStepM;
			testHeight += probeStepM
		) {
			const Box topProbe = {
				.center   = feetPos + Vec3::up * testHeight,
				.halfSize = {
					probeSizeM,
					probeSizeM,
					probeSizeM
				}
			};
			Physics::Hit topHit{};
			if (!physics->BoxCast(
				topProbe, forward, checkDistM + halfWidthM * 2.0f, &topHit
			)) {
				wallTopHeightM = testHeight;
				foundTop       = true;
				break;
			}
		}

		if (!foundTop || wallTopHeightM > maxVaultHeightM) {
			return false;
		}

		{
			const Vec3 surfaceCheckPos =
				feetPos + forward * (distToWall + halfWidthM) +
				Vec3::up * (wallTopHeightM + Math::HtoM(8.0f));
			const Box surfaceProbe = {
				.center   = surfaceCheckPos,
				.halfSize = {
					probeSizeM,
					probeSizeM,
					probeSizeM
				}
			};
			Physics::Hit surfaceHit{};
			if (physics->BoxCast(
				surfaceProbe, Vec3::down, Math::HtoM(16.0f), &surfaceHit
			)) {
				if (surfaceHit.normal.y < 0.7f) {
					return false;
				}
			}
		}

		float wallThicknessM = 0.0f;
		{
			const Vec3 thicknessCheckPos =
				feetPos + Vec3::up * (wallTopHeightM - probeStepM);
			const Box thicknessProbe = {
				.center   = thicknessCheckPos,
				.halfSize = {
					probeSizeM,
					probeSizeM,
					probeSizeM
				}
			};
			Physics::Hit thicknessHit{};
			if (physics->BoxCast(
				thicknessProbe, forward, Math::HtoM(256.0f), &thicknessHit
			)) {
				wallThicknessM = thicknessHit.t + probeSizeM * 2.0f;
			} else {
				wallThicknessM = halfWidthM * 2.0f;
			}
		}

		const float overWallOffsetM = std::max(
			distToWall + wallThicknessM + halfWidthM + Math::HtoM(8.0f),
			halfWidthM * 3.0f + Math::HtoM(8.0f)
		);
		const Vec3 overWallPos =
			feetPos + forward * overWallOffsetM +
			Vec3::up * (wallTopHeightM + Math::HtoM(4.0f));

		const Box landingProbe = {
			.center   = overWallPos,
			.halfSize = {
				halfWidthM,
				Math::HtoM(2.0f),
				halfWidthM
			}
		};

		Physics::Hit landHit{};
		const float  dropCheckDistM = std::max(
			maxVaultHeightM + Math::HtoM(32.0f),
			Math::HtoM(
				mConsole ?
					mConsole->GetConVarValueOr(
						"park_vault_landingcheck", 72.0f
					) :
					72.0f
			)
		);
		if (!physics->BoxCast(
			landingProbe, Vec3::down, dropCheckDistM, &landHit
		)) {
			return false;
		}
		if (landHit.normal.y < 0.7f) {
			return false;
		}

		Vec3 landingFeetPos = overWallPos + Vec3::down * landHit.t;
		if (landingFeetPos.y < feetPos.y) {
			landingFeetPos.y = feetPos.y;
		}

		const Box landingHull = {
			.center   = landingFeetPos + Vec3::up * halfHeightM,
			.halfSize = context.halfExtents
		};
		Physics::Hit overlapHit{};
		if (physics->BoxOverlap(landingHull, &overlapHit)) {
			return false;
		}

		{
			const Box backCheckProbe = {
				.center   = landingFeetPos + Vec3::up * halfHeightM,
				.halfSize = {
					probeSizeM,
					halfHeightM,
					probeSizeM
				}
			};
			Physics::Hit backHit{};
			if (physics->BoxCast(
				backCheckProbe, -forward, overWallOffsetM, &backHit
			)) {
				if (backHit.normal.Dot(forward) > 0.3f) {
					return false;
				}
			}
		}

		const float apexForwardM = std::max(halfWidthM, distToWall * 0.5f);
		const Vec3  apexFeetPos  =
			feetPos + forward * apexForwardM +
			Vec3::up * (wallTopHeightM + Math::HtoM(8.0f));

		mRuntime.vault.active      = true;
		mRuntime.vault.time        = 0.0f;
		mRuntime.vault.startPos    = centerPos;
		mRuntime.vault.apexPos     = apexFeetPos + Vec3::up * halfHeightM;
		mRuntime.vault.endPos      = landingFeetPos + Vec3::up * halfHeightM;
		mRuntime.vault.preVelocity = context.velocity;

		context.velocity              = Vec3::zero;
		context.isGrounded            = false;
		context.supportEntityGuid     = 0;
		context.supportLinearVelocity = Vec3::zero;
		context.supportStepDelta      = Vec3::zero;
		context.requestedState        = MovementStateIds::ParkourSpeedVault;
		return true;
	}

	bool ParkourMovementComponent::UpdateSpeedVault(
		MovementContext& context,
		const float      deltaTime
	) {
		if (!mRuntime.vault.active || !context.transform) {
			return false;
		}

		const float duration = std::max(
			1.0e-4f,
			mConsole ?
				mConsole->GetConVarValueOr("park_vault_duration", 0.25f) :
				0.25f
		);
		mRuntime.vault.time += std::max(0.0f, deltaTime);
		const float t = std::clamp(mRuntime.vault.time / duration, 0.0f, 1.0f);
		const float u = 1.0f - t;

		const Vec3 position =
			mRuntime.vault.startPos * (u * u) +
			mRuntime.vault.apexPos * (2.0f * u * t) +
			mRuntime.vault.endPos * (t * t);
		context.transform->SetPosition(position);
		UpdateCollisionHull(context.transform);

		context.velocity              = Vec3::zero;
		context.isGrounded            = false;
		context.supportEntityGuid     = 0;
		context.supportLinearVelocity = Vec3::zero;
		context.supportStepDelta      = Vec3::zero;

		if (t < 1.0f) {
			return true;
		}

		mRuntime.vault.active   = false;
		mRuntime.vault.cooldown = mConsole ?
			                          mConsole->GetConVarValueOr(
				                          "park_vault_cooldown", 0.3f
			                          ) :
			                          0.3f;

		Vec3 horizontalVel          = mRuntime.vault.preVelocity;
		horizontalVel.y             = 0.0f;
		float       horizontalSpeed = horizontalVel.Length();
		const float minExitSpeed    = Math::HtoM(
			mConsole ?
				mConsole->GetConVarValueOr("park_vault_minspeed", 150.0f) :
				150.0f
		);
		if (horizontalSpeed < minExitSpeed) {
			horizontalSpeed = minExitSpeed;
		}

		Vec3 vaultDir = mRuntime.vault.endPos - mRuntime.vault.startPos;
		vaultDir.y    = 0.0f;
		if (!vaultDir.IsZero()) {
			vaultDir.Normalize();
		} else {
			vaultDir = HorizontalNormalized(context.input.forward);
			if (vaultDir.IsZero()) {
				vaultDir = Vec3::forward;
			}
		}

		context.velocity       = vaultDir * horizontalSpeed;
		context.velocity.y     = 0.0f;
		context.isGrounded     = true;
		mRuntime.hasDoubleJump = true;
		context.requestedState = ResolveGroundStateFromInput(context);
		return true;
	}

	void ParkourMovementComponent::RegisterMovementStates(
		GameMovementStateMachine& stateMachine
	) {
		RegisterParkourMovementStates(stateMachine);
	}

	std::string ParkourMovementComponent::GetInitialStateName() const {
		return std::string(MovementStateIds::ParkourAir);
	}

	std::string ParkourMovementComponent::GetAirStateNameForTransitions() const {
		return std::string(MovementStateIds::ParkourAir);
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
			PublishCue(
				shouldSprint ? "movement.sprint.start" : "movement.sprint.end"
			);
			mAutoSprintActive = shouldSprint;
		}
		if (shouldSprint) {
			const float sprintRatio = std::clamp(
				horizontalSpeedHu / sprintSpeedSafeHu, 0.0f, 2.0f
			);
			PublishCue("movement.sprint.update", sprintRatio);
		}

		if (!loweredState.empty()) {
			PublishCue("movement.state.update." + loweredState, 1.0f);
		}

		if (mRuntime.pendingDoubleJumpCue) {
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
