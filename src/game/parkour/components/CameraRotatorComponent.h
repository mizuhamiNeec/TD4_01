#pragma once

#include "engine/unnamed/framework/components/base/UBaseComponent.h"

#include "core/math/Vec2.h"

namespace Unnamed {
	class TransformComponent;
	class UInputSystem;
	class JsonReader;
	class JsonWriter;

	class CameraRotatorComponent final : public UBaseComponent {
	public:
		[[nodiscard]] std::string_view GetStableName() const override {
			return "parkour.CameraRotator";
		}

		[[nodiscard]] std::string_view GetComponentName() const override {
			return "CameraRotator";
		}

		void OnAttached() override;
		void PrePhysicsTick(float deltaTime) override;
		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;

#ifdef _DEBUG
		void DrawInspectorImGui() override;
#endif

		void SetLookAnglesDegrees(float pitch, float yaw);
		[[nodiscard]] Vec2 GetLookAnglesDegrees() const;
		void SetReplayLookOverride(float pitch, float yaw);
		void ClearReplayLookOverride();

	private:
		[[nodiscard]] TransformComponent* GetTransform() const;

		UInputSystem* mInput = nullptr;
		float         mPitch = 0.0f;
		float         mYaw   = 0.0f;
		float         mSensitivity = 0.15f;
		bool          mReplayLookPending = false;
		float         mReplayPitchDeg    = 0.0f;
		float         mReplayYawDeg      = 0.0f;
	};
}
