#include "OpeningCameraCutsceneComponent.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>

#ifdef _DEBUG
#include <imgui.h>
#endif

#include "core/ComponentRegistry.h"
#include "core/assets/AssetManager.h"
#include "core/assets/AssetType.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"
#include "core/math/Math.h"
#include "core/math/Mat4.h"
#include "core/math/Quaternion.h"
#include "core/math/Vec4.h"

#include "engine/render/frame/RenderFrameInputs.h"
#include "engine/scene/Scene.h"
#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/unnamed/subsystem/input/InputSystem.h"
#include "engine/world/World.h"

#include "game/core/components/CameraRotatorComponent.h"
#include "game/core/components/controller/PlayerCharacterController.h"

namespace Unnamed {
	namespace {
		constexpr float kCountdownAtlasDigitWidthPx = 64.0f;
		constexpr float kCountdownAtlasHeightPx = 64.0f;
		constexpr float kCountdownAtlasDigitCount = 10.0f;

		OpeningCameraCutsceneComponent::MotionType ParseMotionType(
			const JsonReader& reader,
			const OpeningCameraCutsceneComponent::MotionType fallback
		) {
			if (!reader.Valid()) {
				return fallback;
			}

			const std::string motion = reader.GetString();
			if (motion == "pan") {
				return OpeningCameraCutsceneComponent::MotionType::Pan;
			}
			if (motion == "dolly") {
				return OpeningCameraCutsceneComponent::MotionType::Dolly;
			}
			if (motion == "pan_dolly" || motion == "pandolly") {
				return OpeningCameraCutsceneComponent::MotionType::PanDolly;
			}
			return fallback;
		}

		const char* ToMotionTypeString(const OpeningCameraCutsceneComponent::MotionType motion) {
			switch (motion) {
				case OpeningCameraCutsceneComponent::MotionType::Pan:
					return "pan";
				case OpeningCameraCutsceneComponent::MotionType::Dolly:
					return "dolly";
				case OpeningCameraCutsceneComponent::MotionType::PanDolly:
				default:
					return "pan_dolly";
			}
		}
	}

	void OpeningCameraCutsceneComponent::OnAttached() {
		ResolveBindings();
		EnsureUiTextureAssetsLoaded();
		if (mEnabled && mPlayOnAttach) {
			StartCutscene();
		}
	}

	void OpeningCameraCutsceneComponent::OnDetached() {
		FinishCutscene(false);
		mOwnerTransform = nullptr;
		mCameraRootTransform = nullptr;
		mCameraRotator = nullptr;
		mController = nullptr;
		mMovementComponent = nullptr;
	}

	void OpeningCameraCutsceneComponent::OnRenderTick(
		const float renderDeltaTime,
		const float interpolationAlpha
	) {
		(void)interpolationAlpha;
		if (!mEnabled || !mPlaying) {
			return;
		}

		ResolveBindings();
		EnsureUiTextureAssetsLoaded();
		if (!mCameraRootTransform) {
			FinishCutscene(false);
			return;
		}

		if (mAllowSkip) {
			if (InputSystem* input = GetInputSystem()) {
				if (input->IsPressed(mSkipAction.c_str())) {
					FinishCutscene(mRestoreCameraRigOnFinish);
					return;
				}
			}
		}

		UpdateCutscene(std::max(0.0f, renderDeltaTime));
		QueueFadeOverlay();
		QueueCountdownSprites();
	}

	std::string_view OpeningCameraCutsceneComponent::GetStableName() const {
		return "parkour.OpeningCameraCutscene";
	}

	std::string_view OpeningCameraCutsceneComponent::GetComponentName() const {
		return "OpeningCameraCutscene";
	}

	void OpeningCameraCutsceneComponent::Deserialize(const JsonReader& reader) {
		if (const JsonReader enabled = reader["enabled"]; enabled.Valid()) {
			mEnabled = enabled.GetBool(mEnabled);
		}
		if (const JsonReader playOnAttach = reader["playOnAttach"]; playOnAttach.Valid()) {
			mPlayOnAttach = playOnAttach.GetBool(mPlayOnAttach);
		}
		if (const JsonReader allowSkip = reader["allowSkip"]; allowSkip.Valid()) {
			mAllowSkip = allowSkip.GetBool(mAllowSkip);
		}
		if (const JsonReader disableController = reader["disableController"]; disableController.Valid()) {
			mDisableController = disableController.GetBool(mDisableController);
		}
		if (const JsonReader disableMovement = reader["disableMovement"]; disableMovement.Valid()) {
			mDisableMovement = disableMovement.GetBool(mDisableMovement);
		}
		if (const JsonReader restoreRig = reader["restoreCameraRigOnFinish"]; restoreRig.Valid()) {
			mRestoreCameraRigOnFinish = restoreRig.GetBool(mRestoreCameraRigOnFinish);
		}
		if (const JsonReader useCountdown = reader["useCountdown"]; useCountdown.Valid()) {
			mUseCountdown = useCountdown.GetBool(mUseCountdown);
		}
		if (const JsonReader useShotFade = reader["useShotFade"]; useShotFade.Valid()) {
			mUseShotFade = useShotFade.GetBool(mUseShotFade);
		}
		if (const JsonReader shotFadeOutSec = reader["shotFadeOutSec"]; shotFadeOutSec.Valid()) {
			mShotFadeOutSec = shotFadeOutSec.GetFloat(mShotFadeOutSec);
		}
		if (const JsonReader shotFadeInSec = reader["shotFadeInSec"]; shotFadeInSec.Valid()) {
			mShotFadeInSec = shotFadeInSec.GetFloat(mShotFadeInSec);
		}
		if (const JsonReader countdownDigitDurationSec = reader["countdownDigitDurationSec"]; countdownDigitDurationSec.Valid()) {
			mCountdownDigitDurationSec = countdownDigitDurationSec.GetFloat(mCountdownDigitDurationSec);
		}
		if (const JsonReader countdownStartDurationSec = reader["countdownStartDurationSec"]; countdownStartDurationSec.Valid()) {
			mCountdownStartDurationSec = countdownStartDurationSec.GetFloat(mCountdownStartDurationSec);
		}
		if (const JsonReader skipAction = reader["skipAction"]; skipAction.Valid()) {
			mSkipAction = skipAction.GetString(mSkipAction);
		}
		if (const JsonReader digitsTexturePath = reader["digitsTexturePath"]; digitsTexturePath.Valid()) {
			mDigitsTexturePath = digitsTexturePath.GetString(mDigitsTexturePath);
		}
		if (const JsonReader countdownStartTexturePath = reader["countdownStartTexturePath"]; countdownStartTexturePath.Valid()) {
			mCountdownStartTexturePath = countdownStartTexturePath.GetString(mCountdownStartTexturePath);
		}
		if (const JsonReader fadeOverlayTexturePath = reader["fadeOverlayTexturePath"]; fadeOverlayTexturePath.Valid()) {
			mFadeOverlayTexturePath = fadeOverlayTexturePath.GetString(mFadeOverlayTexturePath);
		}

		if (const JsonReader shots = reader["shots"]; shots.Valid() && shots.IsArray()) {
			mShots.clear();
			mShots.reserve(shots.Size());
			for (size_t i = 0; i < shots.Size(); ++i) {
				const JsonReader shotReader = shots[i];
				if (!shotReader.Valid() || !shotReader.IsObject()) {
					continue;
				}

				Shot shot = {};
				shot.startPos = shotReader["startPos"].GetVec3(shot.startPos);
				shot.endPos = shotReader["endPos"].GetVec3(shot.endPos);
				shot.startLook = shotReader["startLook"].GetVec3(shot.startLook);
				shot.endLook = shotReader["endLook"].GetVec3(shot.endLook);
				shot.durationSec = shotReader["durationSec"].GetFloat(shot.durationSec);
				shot.motion = ParseMotionType(shotReader["motion"], shot.motion);
				mShots.emplace_back(shot);
			}
		}

		if (mShots.empty()) {
			mShots = BuildDefaultShots();
		}
		mDigitsTextureAssetId = kInvalidAssetID;
		mCountdownStartTextureAssetId = kInvalidAssetID;
		mFadeOverlayTextureAssetId = kInvalidAssetID;
	}

	void OpeningCameraCutsceneComponent::Serialize(JsonWriter& writer) const {
		writer.Key("enabled");
		writer.Write(mEnabled);
		writer.Key("playOnAttach");
		writer.Write(mPlayOnAttach);
		writer.Key("allowSkip");
		writer.Write(mAllowSkip);
		writer.Key("disableController");
		writer.Write(mDisableController);
		writer.Key("disableMovement");
		writer.Write(mDisableMovement);
		writer.Key("restoreCameraRigOnFinish");
		writer.Write(mRestoreCameraRigOnFinish);
		writer.Key("useCountdown");
		writer.Write(mUseCountdown);
		writer.Key("useShotFade");
		writer.Write(mUseShotFade);
		writer.Key("shotFadeOutSec");
		writer.Write(mShotFadeOutSec);
		writer.Key("shotFadeInSec");
		writer.Write(mShotFadeInSec);
		writer.Key("countdownDigitDurationSec");
		writer.Write(mCountdownDigitDurationSec);
		writer.Key("countdownStartDurationSec");
		writer.Write(mCountdownStartDurationSec);
		writer.Key("skipAction");
		writer.Write(mSkipAction);
		writer.Key("digitsTexturePath");
		writer.Write(mDigitsTexturePath);
		writer.Key("countdownStartTexturePath");
		writer.Write(mCountdownStartTexturePath);
		writer.Key("fadeOverlayTexturePath");
		writer.Write(mFadeOverlayTexturePath);

		writer.Key("shots");
		writer.BeginArray();
		for (const Shot& shot : mShots) {
			writer.BeginObject();
			writer.WriteVec3("startPos", shot.startPos);
			writer.WriteVec3("endPos", shot.endPos);
			writer.WriteVec3("startLook", shot.startLook);
			writer.WriteVec3("endLook", shot.endLook);
			writer.Key("durationSec");
			writer.Write(shot.durationSec);
			writer.Key("motion");
			writer.Write(ToMotionTypeString(shot.motion));
			writer.EndObject();
		}
		writer.EndArray();
	}

	std::vector<OpeningCameraCutsceneComponent::Shot>
	OpeningCameraCutsceneComponent::BuildDefaultShots() {
		return {
			{
				.startPos = Vec3(0.0f, 4.0f, 0.0f),
				.endPos = Vec3(0.0f, 4.0f, 40.0f),
				.startLook = Vec3(0.0f, 3.0f, 50.0f),
				.endLook = Vec3(0.0f, 3.0f, 50.0f),
				.durationSec = 3.0f,
				.motion = MotionType::PanDolly
			},
			{
				.startPos = Vec3(-52.0f, 8.0f, 118.0f),
				.endPos = Vec3(-52.0f, 8.0f, 118.0f),
				.startLook = Vec3(-28.0f, 1.0f, 140.0f),
				.endLook = Vec3(-100.0f, 1.0f, 119.0f),
				.durationSec = 4.0f,
				.motion = MotionType::PanDolly
			},
			{
				.startPos = Vec3(-113.794f, 75.0f, 92.0f),
				.endPos = Vec3(-113.794f, 75.0f, 5.0f),
				.startLook = Vec3(-113.794f, 0.0f, 92.5f),
				.endLook = Vec3(-113.794f, 0.0f, 5.5f),
				.durationSec = 3.0f,
				.motion = MotionType::PanDolly
			},
			{
				.startPos = Vec3(0.0f, 6.0f, 0.0f),
				.endPos = Vec3(0.0f, 3.626f, 0.0f),
				.startLook = Vec3(0.0f, 6.0f, 1.0f),
				.endLook = Vec3(0.0f, 3.626f, 1.0f),
				.durationSec = 0.5f,
				.motion = MotionType::PanDolly
			}
		};
	}

	float OpeningCameraCutsceneComponent::EvaluateEase(const float t) {
		return Math::CubicBezier(std::clamp(t, 0.0f, 1.0f), 0.2f, 0.0f, 0.0f, 1.0f);
	}

	void OpeningCameraCutsceneComponent::StartCutscene() {
		if (mPlaying || mShots.empty()) {
			return;
		}

		ResolveBindings();
		if (!mCameraRootTransform) {
			return;
		}

		if (mCameraRotator) {
			mSavedLookAnglesDeg = mCameraRotator->GetLookAnglesDegrees();
		}
		mSavedCameraLocalPos = mCameraRootTransform->GetPosition();
		mSavedCameraStateValid = true;

		SetGameplayComponentsActive(false);
		mPhase = Phase::Tour;
		mCurrentShotIndex = 0;
		mCurrentShotElapsedSec = 0.0f;
		mCountdownElapsedSec = 0.0f;
		mFadeAlpha = 0.0f;
		mGameplayActivatedDuringCountdown = false;
		mShotFadeActive = false;
		mShotFadeSwapped = false;
		mPlaying = true;
		ApplyShotPose(mShots.front().startPos, mShots.front().startLook);
	}

	void OpeningCameraCutsceneComponent::FinishCutscene(const bool restoreCameraRig) {
		if (mGameplayStateCaptured) {
			if (mController && mDisableController) {
				mController->SetActive(mControllerWasActive);
			}
			if (mMovementComponent && mDisableMovement) {
				mMovementComponent->SetActive(mMovementWasActive);
			}
			mGameplayStateCaptured = false;
		}

		if (restoreCameraRig && mSavedCameraStateValid) {
			ResolveBindings();
			if (mCameraRootTransform) {
				mCameraRootTransform->SetPosition(mSavedCameraLocalPos);
				mCameraRootTransform->RequestInterpolationResync();
			}
			if (mCameraRotator) {
				mCameraRotator->SetLookAnglesDegrees(
					mSavedLookAnglesDeg.x,
					mSavedLookAnglesDeg.y
				);
			}
		}

		mPlaying = false;
		mPhase = Phase::Finished;
		mCurrentShotIndex = 0;
		mCurrentShotElapsedSec = 0.0f;
		mCountdownElapsedSec = 0.0f;
		mFadeAlpha = 0.0f;
		mGameplayActivatedDuringCountdown = false;
		mShotFadeActive = false;
		mShotFadeSwapped = false;
	}

	void OpeningCameraCutsceneComponent::UpdateCutscene(const float deltaTime) {
		switch (mPhase) {
			case Phase::Tour:
				UpdateTour(deltaTime);
				break;
			case Phase::Countdown:
				UpdateCountdown(deltaTime);
				break;
			case Phase::Finished:
			default:
				break;
		}
	}

	void OpeningCameraCutsceneComponent::UpdateTour(const float deltaTime) {
		if (mShots.empty()) {
			EnterCountdown();
			return;
		}

		const Shot& shot = mShots[mCurrentShotIndex];
		const float duration = std::max(0.01f, shot.durationSec);
		mCurrentShotElapsedSec += deltaTime;

		const float rawT = std::clamp(mCurrentShotElapsedSec / duration, 0.0f, 1.0f);
		const float easedT = EvaluateEase(rawT);

		Vec3 cameraPos = shot.startPos;
		Vec3 lookAtPos = shot.startLook;
		switch (shot.motion) {
			case MotionType::Pan:
				lookAtPos = Math::Lerp(shot.startLook, shot.endLook, easedT);
				break;
			case MotionType::Dolly:
				cameraPos = Math::Lerp(shot.startPos, shot.endPos, easedT);
				break;
			case MotionType::PanDolly:
			default:
				cameraPos = Math::Lerp(shot.startPos, shot.endPos, easedT);
				lookAtPos = Math::Lerp(shot.startLook, shot.endLook, easedT);
				break;
		}

		ApplyShotPose(cameraPos, lookAtPos);

		mFadeAlpha = 0.0f;
		const bool hasNextShot = (mCurrentShotIndex + 1 < mShots.size());
		if (mUseShotFade && hasNextShot && !mShotFadeSwapped) {
			const float fadeOutDuration = std::max(0.01f, mShotFadeOutSec);
			const float fadeOutStartSec = std::max(0.0f, duration - fadeOutDuration);
			if (mCurrentShotElapsedSec >= fadeOutStartSec) {
				const float fadeOutT = std::clamp(
					(mCurrentShotElapsedSec - fadeOutStartSec) / fadeOutDuration,
					0.0f,
					1.0f
				);
				mShotFadeActive = true;
				mFadeAlpha = EvaluateEase(fadeOutT);
			}
		}

		if (rawT >= 1.0f) {
			if (hasNextShot) {
				++mCurrentShotIndex;
				mCurrentShotElapsedSec = 0.0f;
				mShotFadeActive = mUseShotFade;
				mShotFadeSwapped = mUseShotFade;
				mFadeAlpha = mUseShotFade ? 1.0f : 0.0f;
				const Shot& nextShot = mShots[mCurrentShotIndex];
				ApplyShotPose(nextShot.startPos, nextShot.startLook);
			} else {
				mFadeAlpha = 0.0f;
				EnterCountdown();
				return;
			}
		}

		if (mUseShotFade && mShotFadeActive && mShotFadeSwapped) {
			const float fadeInDuration = std::max(0.01f, mShotFadeInSec);
			const float fadeInT = std::clamp(
				mCurrentShotElapsedSec / fadeInDuration,
				0.0f,
				1.0f
			);
			mFadeAlpha = 1.0f - EvaluateEase(fadeInT);
			if (fadeInT >= 1.0f) {
				mFadeAlpha = 0.0f;
				mShotFadeActive = false;
				mShotFadeSwapped = false;
			}
		}
	}

	void OpeningCameraCutsceneComponent::EnterCountdown() {
		if (!mUseCountdown) {
			SetGameplayComponentsActive(true);
			FinishCutscene(false);
			return;
		}

		mPhase = Phase::Countdown;
		mCountdownElapsedSec = 0.0f;
		mGameplayActivatedDuringCountdown = false;
		mShotFadeActive = false;
		mShotFadeSwapped = false;
		mFadeAlpha = 0.0f;

		if (mSavedCameraStateValid) {
			if (mCameraRootTransform) {
				mCameraRootTransform->SetPosition(mSavedCameraLocalPos);
				mCameraRootTransform->RequestInterpolationResync();
			}
			if (mCameraRotator) {
				mCameraRotator->SetLookAnglesDegrees(
					mSavedLookAnglesDeg.x,
					mSavedLookAnglesDeg.y
				);
			}
		}
	}

	void OpeningCameraCutsceneComponent::UpdateCountdown(const float deltaTime) {
		mCountdownElapsedSec += deltaTime;

		const float digitTotalDuration = std::max(0.01f, mCountdownDigitDurationSec) * 3.0f;
		const float totalDuration = digitTotalDuration + std::max(0.01f, mCountdownStartDurationSec);
		if (!mGameplayActivatedDuringCountdown && mCountdownElapsedSec >= digitTotalDuration) {
			SetGameplayComponentsActive(true);
			mGameplayActivatedDuringCountdown = true;
		}

		if (mCountdownElapsedSec >= totalDuration) {
			FinishCutscene(false);
		}
	}

	void OpeningCameraCutsceneComponent::QueueCountdownSprites() const {
		if (!mPlaying || mPhase != Phase::Countdown) {
			return;
		}
		if (mDigitsTextureAssetId == kInvalidAssetID ||
		    mCountdownStartTextureAssetId == kInvalidAssetID) {
			return;
		}

		World* world = GetWorld();
		if (!world) {
			return;
		}
		InputSystem* input = GetInputSystem();
		const Vec2 viewport = input ? input->GetMouseClientViewportSize() : Vec2::zero;
		if (viewport.x <= 1.0f || viewport.y <= 1.0f) {
			return;
		}

		const float viewW = viewport.x;
		const float viewH = viewport.y;
		const float centerX = viewW * 0.5f;
		const float centerY = viewH * 0.44f;
		const float digitTarget = std::clamp(std::min(viewW, viewH) * 0.24f, 96.0f, 320.0f);
		const float digitBaseWidth = kCountdownAtlasDigitWidthPx;
		const float digitBaseHeight = kCountdownAtlasHeightPx;
		const float digitScale = digitTarget / std::max(digitBaseWidth, digitBaseHeight);

		const float startBaseWidth = 512.0f;
		const float startBaseHeight = 128.0f;
		float startScale = std::clamp(viewW * 0.42f / std::max(1.0f, startBaseWidth), 0.45f, 1.45f);
		float startWidth = startBaseWidth * startScale;
		float startHeight = startBaseHeight * startScale;
		const float startY = centerY + digitTarget * 0.05f;

		const float digitTotalDuration = std::max(0.01f, mCountdownDigitDurationSec) * 3.0f;
		if (mCountdownElapsedSec < digitTotalDuration) {
			const int digit = 3 - static_cast<int>(mCountdownElapsedSec / std::max(0.01f, mCountdownDigitDurationSec));
			const float normalizedPhase = std::clamp(
				mCountdownElapsedSec / std::max(0.01f, mCountdownDigitDurationSec),
				0.0f,
				3.0f
			);
			const float phase = normalizedPhase - std::floor(normalizedPhase);

			const float pulseScale = 1.0f + (1.0f - phase) * 0.24f;
			const float alpha = std::clamp(1.0f - phase * 0.75f, 0.0f, 1.0f);
			const float digitIndex = std::clamp(static_cast<float>(digit), 0.0f, 9.0f);
			const float u0 = digitIndex / kCountdownAtlasDigitCount;
			const float u1 = (digitIndex + 1.0f) / kCountdownAtlasDigitCount;

			Render::ScreenSpriteInput digitSprite = {};
			digitSprite.texture.source = Render::SPRITE_TEXTURE_SOURCE::ASSET;
			digitSprite.texture.textureAssetId = mDigitsTextureAssetId;
			digitSprite.positionPx = Vec2(centerX, centerY);
			digitSprite.sizePx = Vec2(
				digitBaseWidth * digitScale * pulseScale,
				digitBaseHeight * digitScale * pulseScale
			);
			digitSprite.anchor = Vec2(0.5f, 0.5f);
			digitSprite.color = Vec4(1.0f, 1.0f, 1.0f, alpha);
			digitSprite.sortKey = 300100;
			digitSprite.uvMin = Vec2(u0, 0.0f);
			digitSprite.uvMax = Vec2(u1, 1.0f);
			world->QueueDebugScreenSprite(std::move(digitSprite));

			Render::ScreenSpriteInput startSprite = {};
			startSprite.texture.source = Render::SPRITE_TEXTURE_SOURCE::ASSET;
			startSprite.texture.textureAssetId = mCountdownStartTextureAssetId;
			startSprite.positionPx = Vec2(centerX, startY);
			startSprite.sizePx = Vec2(startWidth, startHeight);
			startSprite.anchor = Vec2(0.5f, 0.5f);
			startSprite.color = Vec4(1.0f, 1.0f, 1.0f, 0.0f);
			startSprite.sortKey = 300101;
			world->QueueDebugScreenSprite(std::move(startSprite));
		} else {
			const float phase = std::clamp(
				(mCountdownElapsedSec - digitTotalDuration) /
				std::max(0.01f, mCountdownStartDurationSec),
				0.0f,
				1.0f
			);
			const float alpha = std::sin(phase * Math::pi);
			const float pulse = 1.0f + 0.08f * std::sin(phase * Math::pi);
			startWidth *= pulse;
			startHeight *= pulse;

			Render::ScreenSpriteInput startSprite = {};
			startSprite.texture.source = Render::SPRITE_TEXTURE_SOURCE::ASSET;
			startSprite.texture.textureAssetId = mCountdownStartTextureAssetId;
			startSprite.positionPx = Vec2(centerX, startY);
			startSprite.sizePx = Vec2(startWidth, startHeight);
			startSprite.anchor = Vec2(0.5f, 0.5f);
			startSprite.color = Vec4(1.0f, 1.0f, 1.0f, alpha);
			startSprite.sortKey = 300101;
			world->QueueDebugScreenSprite(std::move(startSprite));
		}
	}

	void OpeningCameraCutsceneComponent::QueueFadeOverlay() const {
		if (!mPlaying || mFadeAlpha <= 0.0f) {
			return;
		}
		if (mFadeOverlayTextureAssetId == kInvalidAssetID) {
			return;
		}

		World* world = GetWorld();
		if (!world) {
			return;
		}
		InputSystem* input = GetInputSystem();
		const Vec2 viewport = input ? input->GetMouseClientViewportSize() : Vec2::zero;
		if (viewport.x <= 1.0f || viewport.y <= 1.0f) {
			return;
		}

		Render::ScreenSpriteInput fadeSprite = {};
		fadeSprite.texture.source = Render::SPRITE_TEXTURE_SOURCE::ASSET;
		fadeSprite.texture.textureAssetId = mFadeOverlayTextureAssetId;
		fadeSprite.positionPx = Vec2(0.0f, 0.0f);
		fadeSprite.sizePx = viewport;
		fadeSprite.anchor = Vec2(0.0f, 0.0f);
		fadeSprite.color = Vec4(0.0f, 0.0f, 0.0f, std::clamp(mFadeAlpha, 0.0f, 1.0f));
		fadeSprite.sortKey = 300200;
		world->QueueDebugScreenSprite(std::move(fadeSprite));
	}

	void OpeningCameraCutsceneComponent::ResolveBindings() {
		if (!mOwnerTransform) {
			if (Entity* owner = GetOwner()) {
				mOwnerTransform = owner->GetComponent<TransformComponent>();
			}
		}

		if (!mController) {
			if (Entity* owner = GetOwner()) {
				mController = owner->GetComponent<PlayerCharacterController>();
			}
		}

		if (!mMovementComponent) {
			if (Entity* owner = GetOwner()) {
				owner->ForEachComponent(
					[this](BaseComponent& component) {
						if (component.GetStableName() == "parkour.ParkourMovement") {
							mMovementComponent = &component;
							return false;
						}
						return true;
					}
				);
			}
		}

		if (mCameraRootTransform && mCameraRotator) {
			return;
		}

		Scene* scene = GetScene();
		if (!scene) {
			return;
		}

		for (const auto& entityPtr : scene->GetEntities()) {
			if (!entityPtr) {
				continue;
			}
			auto* rotator = entityPtr->GetComponent<CameraRotatorComponent>();
			auto* transform = entityPtr->GetComponent<TransformComponent>();
			if (!rotator || !transform) {
				continue;
			}
			if (mOwnerTransform && transform->GetParent() != mOwnerTransform) {
				continue;
			}

			mCameraRotator = rotator;
			mCameraRootTransform = transform;
			return;
		}
	}

	void OpeningCameraCutsceneComponent::EnsureUiTextureAssetsLoaded() {
		AssetManager* assetManager = GetAssetManager();
		if (!assetManager) {
			return;
		}

		if (mDigitsTextureAssetId == kInvalidAssetID && !mDigitsTexturePath.empty()) {
			mDigitsTextureAssetId = assetManager->LoadFromFile(
				mDigitsTexturePath,
				ASSET_TYPE::TEXTURE
			);
		}
		if (mCountdownStartTextureAssetId == kInvalidAssetID && !mCountdownStartTexturePath.empty()) {
			mCountdownStartTextureAssetId = assetManager->LoadFromFile(
				mCountdownStartTexturePath,
				ASSET_TYPE::TEXTURE
			);
		}
		if (mFadeOverlayTextureAssetId == kInvalidAssetID && !mFadeOverlayTexturePath.empty()) {
			mFadeOverlayTextureAssetId = assetManager->LoadFromFile(
				mFadeOverlayTexturePath,
				ASSET_TYPE::TEXTURE
			);
		}
	}

	void OpeningCameraCutsceneComponent::ApplyShotPose(
		const Vec3& worldCameraPos,
		const Vec3& worldLookAtPos
	) const {
		if (!mCameraRootTransform) {
			return;
		}

		Vec3 localPos = worldCameraPos;
		if (const TransformComponent* parent = mCameraRootTransform->GetParent()) {
			const Mat4 parentInverse = parent->GetWorldMat().Inverse();
			const Vec4 localPos4 = Vec4(worldCameraPos, 1.0f) * parentInverse;
			localPos = Vec3(localPos4.x, localPos4.y, localPos4.z);
		}
		mCameraRootTransform->SetPosition(localPos);

		Vec3 viewDir = worldLookAtPos - worldCameraPos;
		if (viewDir.SqrLength() <= 1.0e-8f) {
			mCameraRootTransform->RequestInterpolationResync();
			return;
		}
		viewDir.Normalize();

		const Quaternion worldLookRot = Quaternion::LookRotation(viewDir, Vec3::up);
		const Vec3 eulerDegrees = worldLookRot.ToEulerDegrees();
		if (mCameraRotator) {
			mCameraRotator->SetLookAnglesDegrees(eulerDegrees.x, eulerDegrees.y);
		}

		Quaternion localRot = worldLookRot;
		if (const TransformComponent* parent = mCameraRootTransform->GetParent()) {
			const Quaternion parentWorldRot = parent->GetWorldMat().ToQuaternion();
			localRot = parentWorldRot.Inverse() * worldLookRot;
		}
		mCameraRootTransform->SetRotation(localRot);
		mCameraRootTransform->RequestInterpolationResync();
	}

	void OpeningCameraCutsceneComponent::SetGameplayComponentsActive(const bool active) {
		ResolveBindings();
		if (!mGameplayStateCaptured) {
			mControllerWasActive = mController ? mController->IsActive() : false;
			mMovementWasActive = mMovementComponent ? mMovementComponent->IsActive() : false;
			mGameplayStateCaptured = true;
		}

		if (mController && mDisableController) {
			mController->SetActive(active);
		}
		if (mMovementComponent && mDisableMovement) {
			mMovementComponent->SetActive(active);
		}
	}

#ifdef _DEBUG
	void OpeningCameraCutsceneComponent::DrawInspectorImGui() {
		ImGui::Checkbox("Enabled", &mEnabled);
		ImGui::Checkbox("Play On Attach", &mPlayOnAttach);
		ImGui::Checkbox("Allow Skip", &mAllowSkip);
		ImGui::Checkbox("Disable Controller", &mDisableController);
		ImGui::Checkbox("Disable Movement", &mDisableMovement);
		ImGui::Checkbox("Restore Camera Rig On Finish", &mRestoreCameraRigOnFinish);
		ImGui::Checkbox("Use Countdown", &mUseCountdown);
		ImGui::Checkbox("Use Shot Fade", &mUseShotFade);
		ImGui::DragFloat("Shot Fade Out (Sec)", &mShotFadeOutSec, 0.01f, 0.01f, 3.0f, "%.2f");
		ImGui::DragFloat("Shot Fade In (Sec)", &mShotFadeInSec, 0.01f, 0.01f, 3.0f, "%.2f");
		ImGui::DragFloat("Countdown Digit Duration", &mCountdownDigitDurationSec, 0.01f, 0.1f, 4.0f, "%.2f");
		ImGui::DragFloat("Countdown Start Duration", &mCountdownStartDurationSec, 0.01f, 0.1f, 4.0f, "%.2f");
		std::array<char, 64> skipActionBuffer{};
		const size_t copyLen = std::min(
			mSkipAction.size(),
			skipActionBuffer.size() - 1
		);
		if (copyLen > 0) {
			std::memcpy(skipActionBuffer.data(), mSkipAction.data(), copyLen);
		}
		if (ImGui::InputText(
			"Skip Action",
			skipActionBuffer.data(),
			skipActionBuffer.size()
		)) {
			mSkipAction = skipActionBuffer.data();
		}
		ImGui::Text("Playing: %s", mPlaying ? "true" : "false");
		ImGui::Text("Phase: %d", static_cast<int>(mPhase));
		ImGui::Text(
			"Shot: %d / %d",
			mPlaying ? static_cast<int>(mCurrentShotIndex + 1) : 0,
			static_cast<int>(mShots.size())
		);
		ImGui::Text("Countdown Elapsed: %.2f", mCountdownElapsedSec);
		ImGui::Text("Fade Alpha: %.2f", mFadeAlpha);

		if (!mPlaying) {
			if (ImGui::Button("Start Preview")) {
				StartCutscene();
			}
		} else {
			if (ImGui::Button("Stop Preview")) {
				FinishCutscene(mRestoreCameraRigOnFinish);
			}
		}

		if (ImGui::Button("Reset To Legacy Default Shots")) {
			mShots = BuildDefaultShots();
		}

		ImGui::SeparatorText("Shots");
		const char* motionItems[] = {"Pan", "Dolly", "PanDolly"};
		for (size_t i = 0; i < mShots.size(); ++i) {
			Shot& shot = mShots[i];
			ImGui::PushID(static_cast<int>(i));
			if (ImGui::TreeNode("Shot")) {
				ImGui::DragFloat3("Start Pos", &shot.startPos.x, 0.05f);
				ImGui::DragFloat3("End Pos", &shot.endPos.x, 0.05f);
				ImGui::DragFloat3("Start Look", &shot.startLook.x, 0.05f);
				ImGui::DragFloat3("End Look", &shot.endLook.x, 0.05f);
				ImGui::DragFloat("Duration", &shot.durationSec, 0.01f, 0.01f, 60.0f, "%.2f s");
				int motionIndex = static_cast<int>(shot.motion);
				if (ImGui::Combo("Motion", &motionIndex, motionItems, IM_ARRAYSIZE(motionItems))) {
					shot.motion = static_cast<MotionType>(std::clamp(motionIndex, 0, 2));
				}
				if (ImGui::Button("Remove Shot") && mShots.size() > 1) {
					mShots.erase(mShots.begin() + static_cast<std::ptrdiff_t>(i));
					ImGui::TreePop();
					ImGui::PopID();
					break;
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
		if (ImGui::Button("Add Shot")) {
			mShots.emplace_back(Shot{});
		}
	}
#endif

	REGISTER_COMPONENT(OpeningCameraCutsceneComponent);
}
