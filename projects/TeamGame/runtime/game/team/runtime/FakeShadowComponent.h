#pragma once

#include "engine/unnamed/framework/components/base/BaseComponent.h"
#include <core/io/json/JsonReader.h>
#include <core/io/json/JsonWriter.h>
#include <core/math/Quaternion.h>
#include <core/math/Vec3.h>
#include <string_view>

namespace Unnamed {
	class TransformComponent;
}

namespace MyGame {

	/// @brief 親エンティティのXZ位置に追従する簡易丸影コンポーネント
	class FakeShadowComponent : public Unnamed::BaseComponent {
	public:
		void OnAttached() override;
		void OnTick(float deltaTime) override;
		void OnDetached() override;

		[[nodiscard]] std::string_view GetStableName() const override;
		[[nodiscard]] std::string_view GetComponentName() const override;

#ifdef _DEBUG
		void DrawInspectorImGui() override;
#endif

		void Deserialize(const Unnamed::JsonReader& reader) override;
		void Serialize(Unnamed::JsonWriter& writer) const override;

	private:
		Unnamed::TransformComponent* _transform = nullptr;
		Unnamed::TransformComponent* _sourceTransform = nullptr;

		float _groundY = 0.0f;
		float _shadowYBias = 0.02f;
		float _maxHeight = 8.0f;
		float _minScale = 0.15f;
		float _maxScale = 1.0f;
		Vec3 _baseScale = Vec3(1.0f, 1.0f, 1.0f);

		void ResolveSourceTransform();
		[[nodiscard]] Vec3 GetWorldPosition(const Unnamed::TransformComponent& transform) const;
	};

}
