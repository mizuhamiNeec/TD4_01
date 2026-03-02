#pragma once

#include <string>

#include "TriggerComponents.h"
#include "engine/render/frame/RenderFrameInputs.h"
#include "engine/unnamed/framework/components/base/UBaseComponent.h"

#include "core/assets/AssetID.h"
#include "core/math/Vec2.h"
#include "core/math/Vec3.h"

#include "game/parkour/ParkourReplay.h"

namespace Unnamed {
	class AssetManager;
	class JsonReader;
	class JsonWriter;
	class TransformComponent;
	class UInputSystem;

	class TitleFlowComponent final : public UBaseComponent {
	public:
		[[nodiscard]] std::string_view GetStableName() const override {
			return "parkour.TitleFlow";
		}

		[[nodiscard]] std::string_view GetComponentName() const override {
			return "TitleFlow";
		}

		void OnAttached() override;
		void OnTick(float deltaTime) override;
		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;

		void AppendOverlaySprites(
			Render::RenderFrameInputs& inputs,
			AssetManager&              assetManager
		);

	private:
		void EnsureReplayLoaded();

		UInputSystem*                mInput = nullptr;
		ParkourReplayManager         mReplayManager = {};
		ParkourReplayManager::ReplayClip mReplayClip = {};
		size_t                       mReplayCursor = 0;
		float                        mReplayAccumulator = 0.0f;
		bool                         mReplayLoaded = false;
		AssetID                      mLogoTexture = kInvalidAssetID;
		AssetID                      mPromptTexture = kInvalidAssetID;

		uint64_t    mPlayerEntityGuid = 0;
		uint64_t    mCameraRootEntityGuid = 0;
		std::string mReplayPath = std::string(
			ParkourReplayManager::kDefaultTitleDemoPath
		);
		bool        mLoop = true;
		std::string mStartAction = "jump";
		std::string mTargetScene = "Game";
		std::string mLogoTexturePath = "./content/parkour/textures/title_logo.png";
		std::string mPromptTexturePath =
			"./content/parkour/textures/press_space_start.png";
		Vec2  mLogoAnchorNormalized = Vec2(0.5f, 0.32f);
		Vec2  mPromptAnchorNormalized = Vec2(0.5f, 0.82f);
		Vec2  mLogoSizePx = Vec2(1400.0f, 320.0f);
		Vec2  mPromptSizePx = Vec2(1200.0f, 140.0f);
		float mPromptBlinkSpeed = 4.0f;
	};

	class CourseComponent final : public UBaseComponent {
	public:
		[[nodiscard]] std::string_view GetStableName() const override {
			return "parkour.Course";
		}

		[[nodiscard]] std::string_view GetComponentName() const override {
			return "Course";
		}

		void OnTick(float deltaTime) override;
		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;

		void AppendOverlaySprites(
			Render::RenderFrameInputs& inputs,
			AssetManager&              assetManager
		);

	private:
		uint64_t    mPlayerEntityGuid = 0;
		std::string mReturnScene = "Title";
		std::string mPingTexturePath = "./content/parkour/textures/ping.png";
		std::string mArrowTexturePath = "./content/parkour/textures/arrow.png";
		Vec2        mPingSizePx = Vec2(96.0f, 96.0f);
		Vec2        mArrowSizePx = Vec2(96.0f, 96.0f);

		int32_t mCurrentCheckpointIndex = 0;
		AssetID mPingTexture = kInvalidAssetID;
		AssetID mArrowTexture = kInvalidAssetID;
	};

	class TeleportTriggerComponent final : public TriggerVolumeComponentBase {
	public:
		[[nodiscard]] std::string_view GetStableName() const override {
			return "parkour.TeleportTrigger";
		}

		[[nodiscard]] std::string_view GetComponentName() const override {
			return "TeleportTrigger";
		}

		void OnTick(float deltaTime) override;
		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;

	private:
		Vec3  mDestination = Vec3::zero;
		bool  mResetVelocity = true;
		float mRearmBuffer = 1.0f;
		bool  mArmed = true;
	};

	class SceneChangeOnActionComponent final : public UBaseComponent {
	public:
		[[nodiscard]] std::string_view GetStableName() const override {
			return "parkour.SceneChangeOnAction";
		}

		[[nodiscard]] std::string_view GetComponentName() const override {
			return "SceneChangeOnAction";
		}

		void OnAttached() override;
		void OnTick(float deltaTime) override;
		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;

	private:
		UInputSystem* mInput = nullptr;
		std::string   mActionName = "returntotitle";
		std::string   mTargetScene = "Title";
	};

	class CursorModeComponent final : public UBaseComponent {
	public:
		[[nodiscard]] std::string_view GetStableName() const override {
			return "parkour.CursorMode";
		}

		[[nodiscard]] std::string_view GetComponentName() const override {
			return "CursorMode";
		}

		void OnAttached() override;
		void OnTick(float deltaTime) override;
		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;

	private:
		UInputSystem* mInput = nullptr;
		bool          mLockCursor = true;
		bool          mShowCursor = false;
	};

	class OscillateTransformComponent final : public UBaseComponent {
	public:
		[[nodiscard]] std::string_view GetStableName() const override {
			return "parkour.OscillateTransform";
		}

		[[nodiscard]] std::string_view GetComponentName() const override {
			return "OscillateTransform";
		}

		void OnAttached() override;
		void PrePhysicsTick(float deltaTime) override;
		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;

	private:
		[[nodiscard]] TransformComponent* GetTransform() const;

		Vec3  mAxis = Vec3::right;
		float mAmplitude = 20.0f;
		float mFrequency = 1.0f;
		float mPhaseOffset = 0.0f;
		Vec3  mBasePosition = Vec3::zero;
		float mTime = 0.0f;
		bool  mCapturedBase = false;
	};
}
