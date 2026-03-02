#pragma once

#include "base/UBaseComponent.h"

#include "engine/render/frame/RenderFrameInputs.h"

namespace Unnamed {
	class TransformComponent;
	class JsonReader;
	class JsonWriter;

	class GameplayCameraComponent final : public UBaseComponent {
	public:
		[[nodiscard]] std::string_view GetStableName() const override {
			return "gameplay.GameplayCamera";
		}

		[[nodiscard]] std::string_view GetComponentName() const override {
			return "GameplayCamera";
		}

		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;

#ifdef _DEBUG
		void DrawInspectorImGui() override;
#endif

		void SetAspectRatio(float aspectRatio);
		void SetCameraActive(bool active) noexcept { mCameraActive = active; }

		[[nodiscard]] float GetAspectRatio() const noexcept {
			return mAspectRatio;
		}
		[[nodiscard]] bool IsCameraActive() const noexcept {
			return mCameraActive;
		}

		bool BuildCameraInput(Render::RenderCameraInput& outCamera) const;

	private:
		[[nodiscard]] TransformComponent* GetTransform() const;

		float mFovYDegrees = 90.0f;
		float mNearZ       = 0.01f;
		float mFarZ        = 10000.0f;
		float mAspectRatio = 16.0f / 9.0f;
		bool  mCameraActive = true;
	};
}
