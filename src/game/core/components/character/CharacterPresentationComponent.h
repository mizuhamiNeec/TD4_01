#pragma once

#include <cfloat>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/assets/AssetID.h"
#include "engine/unnamed/framework/components/base/BaseComponent.h"
#include "engine/world/GameplayCueBus.h"

namespace Unnamed {
	class JsonReader;
	class JsonWriter;
	class SkeletalAnimationComponent;
	class CameraFxControllerComponent;
	class AudioFxControllerComponent;

	class CharacterPresentationComponent final : public BaseComponent {
	public:
		void OnAttached() override;
		void OnDetached() override;
		void OnRenderTick(float renderDeltaTime, float interpolationAlpha) override;

		[[nodiscard]] std::string_view GetStableName() const override;
		[[nodiscard]] std::string_view GetComponentName() const override;
		[[nodiscard]] uint32_t         GetIcon() const override;

#ifdef _DEBUG
		void DrawInspectorImGui() override;
#endif

		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;

	private:
		enum class OutputType : uint8_t {
			Unknown = 0,
			AnimationState,
			AnimationSpeed,
			CameraShake,
			CameraFov,
			CameraRotation,
			AudioPlay,
		};

		struct RuntimeOutput {
			OutputType   type = OutputType::Unknown;
			std::string  typeName;
			std::string  id;
			float        scale = 1.0f;
			std::string  stateCueSuffix;
			float        enterFovInSec = 0.0f;
		};

		struct RuntimeRule {
			std::string                cueId;
			float                      minValue      = -FLT_MAX;
			float                      maxValue      = FLT_MAX;
			float                      cooldownSec   = 0.0f;
			float                      lastTriggerAt = -FLT_MAX;
			std::vector<RuntimeOutput> outputs;
		};

		void SubscribeAll();
		void UnsubscribeAll();
		void HandleCue(const GameplayCue& cue);
		void SetProfilePath(const std::string& path);
		bool LoadProfile();
		void RefreshProfileIfNeeded();
		void RebuildUpdateFovWaitMap();
		void ValidateResolvedRules() const;

		[[nodiscard]] uint64_t ResolveCueSourceEntityGuid() const;
		[[nodiscard]] SkeletalAnimationComponent* ResolveAnimation();
		[[nodiscard]] CameraFxControllerComponent* ResolveCameraFx();
		[[nodiscard]] AudioFxControllerComponent* ResolveAudioFx();

		std::string                 mProfilePath;
		AssetID                     mProfileAssetId       = kInvalidAssetID;
		uint64_t                    mLoadedProfileVersion = 0;
		std::vector<RuntimeRule>    mRules;
		std::vector<GameplayCueBus::Handle> mCueHandles;

		uint64_t mCueSourceEntityGuid = 0;
		uint64_t mAnimationEntityGuid = 0;
		uint64_t mCameraFxEntityGuid  = 0;

		SkeletalAnimationComponent*  mAnimation = nullptr;
		CameraFxControllerComponent* mCameraFx  = nullptr;
		AudioFxControllerComponent*  mAudioFx   = nullptr;
		float                        mElapsedSeconds = 0.0f;
		std::unordered_map<std::string, float> mStateEnterTimeSeconds;

#ifdef _DEBUG
		std::string mDebugLastCueId;
		float       mDebugLastCueValue      = 0.0f;
		float       mDebugLastCueValue2     = 0.0f;
		uint64_t    mDebugLastCueSourceGuid = 0;
		uint64_t    mDebugHandledCueCount   = 0;
		float       mDebugLastCueAt         = -FLT_MAX;
		std::string mDebugLastPlayStateId;
		bool        mDebugLastPlayStateOk = true;
		std::string mDebugLastAudioPresetId;
		bool        mDebugLastAudioTriggerOk = true;

		std::string mDebugPublishCueId = "movement.land";
		float       mDebugPublishValue = 1.0f;
		float       mDebugPublishValue2 = 0.0f;
#endif
	};
}
