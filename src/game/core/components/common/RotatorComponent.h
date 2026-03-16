#pragma once

#include "engine/unnamed/framework/components/base/BaseComponent.h"

#include "core/math/Vec3.h"

namespace Unnamed {
	class JsonReader;
	class JsonWriter;
	class TransformComponent;

	class RotatorComponent final : public BaseComponent {
	public:
		void PrePhysicsTick(float deltaTime) override;

		[[nodiscard]] std::string_view GetStableName() const override {
			return "game.Rotator";
		}

		[[nodiscard]] std::string_view GetComponentName() const override {
			return "Rotator";
		}

#ifdef _DEBUG
		void DrawInspectorImGui() override;
#endif

		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;

	private:
		[[nodiscard]] TransformComponent* GetTransform() const;

		Vec3 mRotationRate    = Vec3::zero;
		bool mRotationEnabled = true;
	};
}
