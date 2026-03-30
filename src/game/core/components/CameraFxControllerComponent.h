#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "engine/unnamed/framework/components/base/BaseComponent.h"
#include "engine/tween/TweenTypes.h"

#include "core/math/Quaternion.h"
#include "core/math/Vec2.h"

namespace Unnamed {
	class CameraComponent;
	class TransformComponent;
	class JsonReader;
	class JsonWriter;

	class CameraFxControllerComponent final : public BaseComponent {
	public:
		struct ShakePreset {
			std::string id;
			Vec2        ampPitchYawDeg = Vec2(0.25f, 0.25f);
			float       freqHz         = 18.0f;
			float       durationSec    = 0.2f;
			float       decay          = 1.0f;
		};

		struct FovPreset {
			std::string id;
			float       deltaDeg = 8.0f;
			float       inSec    = 0.08f;
			float       outSec   = 0.12f;
			EASE_TYPE   ease     = EASE_TYPE::OUT_SINE;
		};

		void OnAttached() override;
		void OnDetached() override;
		void OnTick(float deltaTime) override;
		[[nodiscard]] TICK_GROUP GetTickGroup() const override;

		void TriggerShake(std::string_view presetId, float intensityScale = 1.0f);
		void TriggerFov(std::string_view presetId, float intensityScale = 1.0f);
		[[nodiscard]] bool HasShakePreset(std::string_view presetId) const;
		[[nodiscard]] bool HasFovPreset(std::string_view presetId) const;
		[[nodiscard]] bool GetFovPresetInSec(
			std::string_view presetId, float& outInSec
		) const;

		[[nodiscard]] std::string_view GetStableName() const override;
		[[nodiscard]] std::string_view GetComponentName() const override;
		[[nodiscard]] uint32_t         GetIcon() const override;

#ifdef _DEBUG
		void DrawInspectorImGui() override;
#endif

		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;

	private:
		struct ActiveShake {
			Vec2  amplitudeDeg = Vec2::zero;
			float frequencyHz  = 18.0f;
			float durationSec  = 0.2f;
			float decay        = 1.0f;
			float elapsedSec   = 0.0f;
			float phasePitch   = 0.0f;
			float phaseYaw     = 0.0f;
		};

		struct ActiveFovAnim {
			float     targetDeltaDeg = 0.0f;
			float     inSec          = 0.08f;
			float     outSec         = 0.12f;
			EASE_TYPE ease           = EASE_TYPE::OUT_SINE;
			float     elapsedSec     = 0.0f;
			bool      active         = false;
		};

		[[nodiscard]] CameraComponent* ResolveCamera();
		[[nodiscard]] TransformComponent* ResolveShakeTransform();
		[[nodiscard]] const ShakePreset* FindShakePreset(std::string_view id) const;
		[[nodiscard]] const FovPreset* FindFovPreset(std::string_view id) const;
		[[nodiscard]] static EASE_TYPE ParseEase(const JsonReader& reader);
		[[nodiscard]] static std::string_view EaseToString(EASE_TYPE ease);
		void               ApplyOutputs(const Vec2& lookOffsetDeg, float fovOffsetDeg);
		void               ResetOutputs();

		std::vector<ShakePreset> mShakePresets;
		std::vector<FovPreset>   mFovPresets;

		std::vector<ActiveShake> mActiveShakes;
		ActiveFovAnim            mActiveFovAnim = {};

		uint64_t mCameraEntityGuid = 0;
		uint64_t mShakeEntityGuid = 0;

		CameraComponent*        mCamera  = nullptr;
		TransformComponent*     mShakeTransform = nullptr;

		float mBaseFovYDegrees   = 90.0f;
		float mCurrentFovOffset  = 0.0f;
		Vec2  mCurrentLookOffset = Vec2::zero;
		Quaternion mLastAppliedShakeRotation = Quaternion::identity;

#ifdef _DEBUG
		float mDebugTriggerIntensity = 1.0f;
#endif
	};
}
