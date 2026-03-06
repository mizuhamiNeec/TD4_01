#pragma once

#include "engine/unnamed/framework/components/base/UBaseComponent.h"

#include "core/math/Vec3.h"

namespace Unnamed {
	class JsonReader;
	class JsonWriter;
	class TransformComponent;

	class RotateComponent final : public UBaseComponent {
	public:
		[[nodiscard]] std::string_view GetStableName() const override {
			return "parkour.Rotate";
		}

		[[nodiscard]] std::string_view GetComponentName() const override {
			return "Rotate";
		}

		void PrePhysicsTick(float deltaTime) override;
		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;

#ifdef _DEBUG
		void DrawInspectorImGui() override;
#endif

	private:
		[[nodiscard]] TransformComponent* GetTransform() const;

		Vec3 mRotationRate    = Vec3::zero;
		bool mRotationEnabled = true;
	};
}
