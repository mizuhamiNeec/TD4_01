#pragma once

#include <string>
#include <vector>

#include "core/assets/AssetID.h"
#include "engine/unnamed/framework/components/base/BaseComponent.h"

#include "core/math/Vec2.h"
#include "core/math/Vec3.h"

namespace Unnamed {
	class JsonReader;
	class JsonWriter;
	class Scene;
	class TransformComponent;
	class CameraRotatorComponent;
	class PlayerCharacterController;

	class OpeningCameraCutsceneComponent final : public BaseComponent {
	public:
		enum class MotionType {
			Pan = 0,
			Dolly = 1,
			PanDolly = 2,
		};

		struct Shot {
			Vec3       startPos = Vec3::zero;
			Vec3       endPos = Vec3::zero;
			Vec3       startLook = Vec3::forward;
			Vec3       endLook = Vec3::forward;
			float      durationSec = 0.0f;
			MotionType motion = MotionType::PanDolly;
		};

		void OnAttached() override;
		void OnDetached() override;
		void OnRenderTick(float renderDeltaTime, float interpolationAlpha) override;

		[[nodiscard]] std::string_view GetStableName() const override;
		[[nodiscard]] std::string_view GetComponentName() const override;

#ifdef _DEBUG
		void DrawInspectorImGui() override;
#endif

		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;

	private:
		enum class Phase {
			Tour = 0,
			Countdown = 1,
			Finished = 2,
		};

		[[nodiscard]] static std::vector<Shot> BuildDefaultShots();
		[[nodiscard]] static float EvaluateEase(float t);

		void StartCutscene();
		void FinishCutscene(bool restoreCameraRig);
		void UpdateCutscene(float deltaTime);
		void UpdateTour(float deltaTime);
		void EnterCountdown();
		void UpdateCountdown(float deltaTime);
		void QueueCountdownSprites() const;
		void QueueFadeOverlay() const;
		void ResolveBindings();
		void EnsureUiTextureAssetsLoaded();
		void ApplyShotPose(const Vec3& worldCameraPos, const Vec3& worldLookAtPos) const;
		void SetGameplayComponentsActive(bool active);

		bool mEnabled = true;
		bool mPlayOnAttach = true;
		bool mAllowSkip = true;
		bool mDisableController = true;
		bool mDisableMovement = true;
		bool mRestoreCameraRigOnFinish = true;
		bool mUseCountdown = true;
		bool mUseShotFade = true;
		std::string mSkipAction = "jump";
		std::string mDigitsTexturePath = "content/parkour/textures/digits.png";
		std::string mCountdownStartTexturePath = "content/parkour/textures/press_space_start.png";
		std::string mFadeOverlayTexturePath = "content/parkour/textures/title_overlay.png";
		float mShotFadeOutSec = 0.25f;
		float mShotFadeInSec = 0.25f;
		float mCountdownDigitDurationSec = 1.0f;
		float mCountdownStartDurationSec = 0.8f;
		std::vector<Shot> mShots = BuildDefaultShots();

		bool mPlaying = false;
		Phase mPhase = Phase::Finished;
		size_t mCurrentShotIndex = 0;
		float mCurrentShotElapsedSec = 0.0f;
		float mCountdownElapsedSec = 0.0f;
		float mFadeAlpha = 0.0f;
		bool mGameplayActivatedDuringCountdown = false;
		bool mShotFadeActive = false;
		bool mShotFadeSwapped = false;

		AssetID mDigitsTextureAssetId = kInvalidAssetID;
		AssetID mCountdownStartTextureAssetId = kInvalidAssetID;
		AssetID mFadeOverlayTextureAssetId = kInvalidAssetID;

		bool mControllerWasActive = false;
		bool mMovementWasActive = false;
		bool mGameplayStateCaptured = false;

		Vec3 mSavedCameraLocalPos = Vec3::zero;
		Vec2 mSavedLookAnglesDeg = Vec2::zero;
		bool mSavedCameraStateValid = false;

		TransformComponent* mOwnerTransform = nullptr;
		TransformComponent* mCameraRootTransform = nullptr;
		CameraRotatorComponent* mCameraRotator = nullptr;
		PlayerCharacterController* mController = nullptr;
		BaseComponent* mMovementComponent = nullptr;
	};
}
