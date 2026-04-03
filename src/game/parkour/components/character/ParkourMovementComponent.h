#pragma once

#include <json.hpp>
#include <optional>

#include "GrappleMotor.h"

#include "game/core/components/character/GameMovementComponent.h"
#include "game/core/input/CharacterActionFrameInput.h"

namespace Unnamed {
	struct MovementContext;

	/// @brief パルクール向けの拡張移動コンポーネントです。
	class ParkourMovementComponent final : public GameMovementComponent,
	                                       public
	                                       ICharacterActionInputReceiver {
	public:
		struct WallRunRuntime {
			bool  active            = false;
			Vec3  normal            = Vec3::zero;
			Vec3  direction         = Vec3::zero;
			float time              = 0.0f;
			float timeSinceLast     = 999.0f;
			Vec3  lastWallNormal    = Vec3::zero;
			bool  jumpWasHeldOnInit = false;
		};

		struct SlideRuntime {
			bool  active    = false;
			Vec3  direction = Vec3::zero;
			float time      = 0.0f;
		};

		struct BlinkRuntime {
			float cooldown  = 0.0f;
			bool  active    = false;
			float moveTime  = 0.0f;
			Vec3  startPos  = Vec3::zero;
			Vec3  targetPos = Vec3::zero;
		};

		struct VaultRuntime {
			bool  active      = false;
			float time        = 0.0f;
			float cooldown    = 0.0f;
			Vec3  startPos    = Vec3::zero;
			Vec3  apexPos     = Vec3::zero;
			Vec3  endPos      = Vec3::zero;
			Vec3  preVelocity = Vec3::zero;
		};

		struct ParkourRuntime {
			bool           hasDoubleJump           = true;
			bool           lastJumpHeld            = false;
			bool           duckHullActive          = false;
			bool           pendingDoubleJumpCue    = false;
			float          footstepDistanceAccumHu = 0.0f;
			WallRunRuntime wallRun                 = {};
			SlideRuntime   slide                   = {};
			BlinkRuntime   blink                   = {};
			VaultRuntime   vault                   = {};

			GrappleState grapple = {};
		};

		~ParkourMovementComponent() override;

		void OnAttached() override;
		void SimulateStep(
			TransformComponent*       transform,
			const MovementFrameInput& input,
			float                     stepSeconds
		) override;

		void EnqueueDeterministicActionInput(
			uint64_t                         tick,
			float                            stepSeconds,
			const CharacterActionFrameInput& input
		) override;
		void ClearDeterministicActionInputQueue() override;

		[[nodiscard]] const CharacterActionFrameInput& GetActionFrameInput()
		const;

		[[nodiscard]] std::string_view GetStableName() const override;
		[[nodiscard]] std::string_view GetComponentName() const override;

#ifdef _DEBUG
		void DrawInspectorImGui() override;
#endif

		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;
		void WriteReplayState(nlohmann::json& outState) const override;
		void ReadReplayState(const nlohmann::json& inState) override;
		[[nodiscard]] uint64_t ComputeReplayStateHash() const override;

		[[nodiscard]] uint32_t GetIcon() const override;

		[[nodiscard]] ParkourRuntime&       GetParkourRuntime();
		[[nodiscard]] const ParkourRuntime& GetParkourRuntime() const;

		void TickParkourTimers(float deltaTime);

		bool SetDuckHullEnabled(MovementContext& context, bool enabled);
		[[nodiscard]] bool IsDuckHullEnabled() const;
		[[nodiscard]] bool CanStandAt(const MovementContext& context) const;

		[[nodiscard]] float GetHorizontalSpeedHu(const Vec3& velocity) const;
		[[nodiscard]] bool ShouldSlideFromSpeed(float horizontalSpeedHu) const;
		[[nodiscard]] std::string ResolveGroundStateFromInput(
			const MovementContext& context
		) const;

		bool TryStartBlink(MovementContext& context);
		bool UpdateBlink(MovementContext& context, float deltaTime);

		bool TryStartWallRun(MovementContext& context);
		bool UpdateWallRun(MovementContext& context, float deltaTime);
		void EndWallRun();

		bool TryStartSpeedVault(MovementContext& context);
		bool UpdateSpeedVault(MovementContext& context, float deltaTime);

	protected:
		void RegisterMovementStates(
			GameMovementStateMachine& stateMachine
		) override;
		void OnAfterCoreCueDispatch(
			std::string_view          previousStateName,
			std::string_view          currentStateName,
			const MovementFrameInput& input,
			bool                      wasGrounded,
			bool                      isGrounded,
			float                     stepSeconds
		) override;

	private:
		void RebuildDuckHalfExtents();
		void ResetParkourRuntime();
		bool ApplyDuckHull(MovementContext& context);
		bool ApplyStandHull(MovementContext& context);

		bool                      mAutoSprintActive    = false;
		ParkourRuntime            mRuntime             = {};
		Vec3                      mStandingHalfExtents = Vec3::zero;
		Vec3                      mDuckHalfExtents     = Vec3::zero;
		CharacterActionFrameInput mActionFrameInput    = {};

		struct DeterministicActionInputPacket {
			uint64_t                  tick        = 0;
			float                     stepSeconds = 0.0f;
			CharacterActionFrameInput input       = {};
		};

		std::optional<DeterministicActionInputPacket>
		mDeterministicActionInputPacket;
	};
}
