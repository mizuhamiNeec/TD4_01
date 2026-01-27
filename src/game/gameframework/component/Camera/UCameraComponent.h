#pragma once
#include <game/gameframework/component/base/BaseComponent.h>

#include <runtime/core/math/Math.h>

namespace Unnamed {
	class TransformComponent;

	/// @brief カメラコンポーネント
	class UCameraComponent : public BaseComponent {
	public:
		static Mat4        View(const TransformComponent* transformComponent);
		[[nodiscard]] Mat4 Proj(float aspectRatio) const;

		// BaseComponent
		void Serialize(JsonWriter& writer) const override;
		void Deserialize(const JsonReader& reader) override;

		[[nodiscard]] std::string_view GetComponentName() const override {
			return "Camera";
		}

	private:
		float mFovY  = 90.0f * Math::deg2Rad;
		float mZNear = 0.001f;
		float mZFar  = 10000.0f;
	};
}
