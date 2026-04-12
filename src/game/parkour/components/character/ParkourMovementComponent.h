#pragma once

#include <json.hpp>
#include <core/containers/RingBuffer.h>

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
		/// @brief deterministicアクション入力キューの最大保持件数です。
		static constexpr size_t kDeterministicActionInputQueueCapacity = 128;

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
		/// @brief Parkour移動Abilityから共通Cueを発火します。
		void PublishParkourMovementCue(
			std::string_view id,
			float            value = 0.0f,
			float            value2 = 0.0f
		);

		bool SetDuckHullEnabled(MovementContext& context, bool enabled);
		[[nodiscard]] bool IsDuckHullEnabled() const;
		[[nodiscard]] bool CanStandAt(const MovementContext& context) const;

		[[nodiscard]] float GetHorizontalSpeedHu(const Vec3& velocity) const;
		[[nodiscard]] bool ShouldSlideFromSpeed(float horizontalSpeedHu) const;
		[[nodiscard]] bool WantsDuckStance(const MovementContext& context) const;
		void SyncCollisionHull(TransformComponent* transform);
		void EndWallRun();

	protected:
		void RegisterMovementModes(
			GameMovementStateMachine& stateMachine
		) override;
		void RegisterMovementAbilities(
			GameMovementStateMachine& stateMachine
		) override;

		/// @brief Parkour移動の初期Modeを返します。
		[[nodiscard]] MOVEMENT_MODE_ID GetInitialMode() const override;

		/// @brief ノークリップ解除後に戻るParkour空中状態IDを返します。
		[[nodiscard]] MOVEMENT_MODE_ID GetAirModeForTransitions() const override;
		[[nodiscard]] std::string ResolvePresentationStateName() const override;
		/// @brief Parkour地上移動は常時スプリント基準にするためtrueを返します。
		[[nodiscard]] bool UseSprintSpeedAsDefaultGroundSpeed() const override;

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

		RingBuffer<
			DeterministicActionInputPacket,
			kDeterministicActionInputQueueCapacity
		>
		mDeterministicActionInputQueue;
	};
}
