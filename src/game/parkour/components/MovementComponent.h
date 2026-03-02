#pragma once

#include "engine/physics/core/UPhysics.h"
#include "engine/unnamed/framework/components/base/UBaseComponent.h"

#include "core/math/Vec2.h"

#include "game/parkour/ParkourReplay.h"

namespace Unnamed {
	class CameraRotatorComponent;
	class JsonReader;
	class JsonWriter;
	class TransformComponent;
	class UInputSystem;

	struct PlayerAabb {
		Vec3 center   = Vec3::zero;
		Vec3 halfSize = Vec3::zero;
	};

	class MovementComponent final : public UBaseComponent {
	public:
		[[nodiscard]] std::string_view GetStableName() const override {
			return "parkour.Movement";
		}

		[[nodiscard]] std::string_view GetComponentName() const override {
			return "Movement";
		}

		void OnAttached() override;
		void PrePhysicsTick(float deltaTime) override;
		void OnTick(float deltaTime) override;
		void PostPhysicsTick(float deltaTime) override;
		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;

#ifdef _DEBUG
		void DrawInspectorImGui() override;
#endif

		void SetReplayFrame(const ReplayUserCmdFrame& frame);
		void ClearReplayFrame();
		void SetLiveInputOverride(
			const Vec2& moveInput,
			bool        wishJump,
			bool        wishCrouch,
			bool        wishBlink
		);
		void ClearLiveInputOverride();
		void SetRespawnPosition(const Vec3& respawnPosition);
		[[nodiscard]] Vec3 GetRespawnPosition() const noexcept;
		void ApplyJumpPadBoost(float velocityHu);
		void ApplySpeedBoost(float multiplier, float durationSec);
		void TeleportTo(const Vec3& feetPosition);
		void SetReplayVaultState(
			bool isSpeedVaulting,
			float vaultProgress,
			const Vec3& vaultStartPos,
			const Vec3& vaultApexPos,
			const Vec3& vaultEndPos
		);

		[[nodiscard]] Vec3 GetVelocity() const noexcept;
		void               SetVelocity(const Vec3& velocity) noexcept;
		[[nodiscard]] Vec3 GetFeetPosition() const noexcept;
		[[nodiscard]] Vec3 GetHeadPosition() const noexcept;
		[[nodiscard]] bool IsGrounded() const noexcept;
		[[nodiscard]] bool IsSliding() const noexcept;
		[[nodiscard]] bool IsWallRunning() const noexcept;
		[[nodiscard]] bool IsSpeedVaulting() const noexcept;
		[[nodiscard]] float GetVaultProgress() const noexcept;
		[[nodiscard]] Vec3 GetVaultStartPos() const noexcept;
		[[nodiscard]] Vec3 GetVaultApexPos() const noexcept;
		[[nodiscard]] Vec3 GetVaultEndPos() const noexcept;
		[[nodiscard]] PlayerAabb BuildWorldAabb() const noexcept;

	private:
		struct InputFrame {
			Vec2 moveInput   = Vec2::zero;
			bool wishJump    = false;
			bool wishCrouch  = false;
			bool wishBlink   = false;
		};

		[[nodiscard]] TransformComponent* GetTransform() const;
		[[nodiscard]] UPhysics::Engine* ResolvePhysics() const;
		[[nodiscard]] CameraRotatorComponent* ResolveCameraRotator() const;
		[[nodiscard]] InputFrame SampleInput() const;
		[[nodiscard]] Vec3 BuildWishDirection(const Vec2& moveInput) const;
		[[nodiscard]] Unnamed::Box BuildPhysicsBoxAt(
			const Vec3& feetPosition
		) const noexcept;
		void Accelerate(
			Vec3 wishDir, float wishSpeedHu, float accelHu, float deltaTime
		);
		void ApplyFriction(float amount, float deltaTime);
		void ResolveMovement(const Vec3& delta);
		void UpdateGrounding();
		void TryBlink(const InputFrame& input);
		void TryStartWallrun();
		void UpdateWallrun(float deltaTime, const InputFrame& input);
		void TryStartSlide();
		void UpdateSlide(float deltaTime, const InputFrame& input);
		void EndSlide();
		void EndWallrun();

		UInputSystem*           mInput         = nullptr;
		UPhysics::Engine*       mPhysics       = nullptr;
		CameraRotatorComponent* mCameraRotator = nullptr;
		uint64_t                mLookSourceEntityGuid = 0;

		Vec3 mVelocity = Vec3::zero;
		Vec3 mRespawnPosition = Vec3::zero;

		float mWidthHu          = 32.0f;
		float mHeightHu         = 72.0f;
		float mCrouchHeightHu   = 48.0f;
		float mMoveSpeedHu      = 320.0f;
		float mCrouchSpeedHu    = 63.3f;
		float mJumpVelocityHu   = 400.0f;
		float mGravityHu        = 800.0f;
		float mGroundAccelHu    = 18.0f;
		float mAirAccelHu       = 7.5f;
		float mFriction         = 6.0f;
		float mSlideMinSpeedHu  = 200.0f;
		float mSlideBoostHu     = 50.0f;
		float mSlideStopHu      = 50.0f;
		float mBlinkDistanceHu  = 512.0f;
		float mBlinkCooldownSec = 1.0f;
		float mGroundSnapHu     = 10.0f;
		float mWallrunMinSpeedHu = 200.0f;
		float mWallrunJumpHu     = 350.0f;
		float mWallrunGravityScale = 0.1f;
		float mGroundNormalY       = 0.7f;

		bool  mGrounded        = false;
		bool  mSliding         = false;
		bool  mWallRunning     = false;
		bool  mReplayInputActive = false;
		bool  mLiveInputOverrideActive = false;
		InputFrame mLiveInputOverride = {};
		ReplayUserCmdFrame mReplayFrame = {};
		float mBlinkCooldownRemaining = 0.0f;
		float mSpeedBoostMultiplier   = 1.0f;
		float mSpeedBoostRemaining    = 0.0f;
		float mSlideTime              = 0.0f;
		Vec3  mSlideDirection         = Vec3::zero;
		Vec3  mWallNormal             = Vec3::zero;
		Vec3  mWallDirection          = Vec3::zero;
		float mWallrunTime            = 0.0f;
		bool  mWasJumpHeldLastFrame   = false;

		bool  mSpeedVaulting = false;
		float mVaultProgress = 0.0f;
		Vec3  mVaultStartPos = Vec3::zero;
		Vec3  mVaultApexPos  = Vec3::zero;
		Vec3  mVaultEndPos   = Vec3::zero;
	};
}
