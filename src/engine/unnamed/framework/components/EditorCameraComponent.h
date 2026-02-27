#pragma once
#include "base/UBaseComponent.h"

#include "engine/render/frame/RenderFrameInputs.h"

namespace Unnamed {
	class TransformComponent;
	class JsonReader;
	class JsonWriter;

	class EditorCameraComponent final : public UBaseComponent {
	public:
		[[nodiscard]] std::string_view GetStableName() const override {
			return "engine.EditorCamera";
		}

		[[nodiscard]] std::string_view GetComponentName() const override {
			return "EditorCamera";
		}

		void OnAttached() override;
		void OnTick(float deltaTime) override;

		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;

		bool BuildCameraInput(
			float                      aspect,
			Render::RenderCameraInput& outCamera
		) const;

	private:
		[[nodiscard]] TransformComponent* GetTransform() const;

		float mFovYDegrees = 90.0f;
		float mNearZ       = 0.001f;
		float mFarZ        = 10000.0f;
		float mMoveSpeed   = 5.0f;
		float mRotateSpeed = 1.5f;
	};
}
