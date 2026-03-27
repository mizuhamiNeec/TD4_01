#pragma once
#include "engine/unnamed/framework/components/base/BaseComponent.h"

#include "core/math/Quaternion.h"
#include "core/math/Vec2.h"

namespace Unnamed {
	class TransformComponent;

	class ViewmodelSway : public BaseComponent {
	public:
		void OnAttached() override;
		void OnTick(float deltaTime) override;

		[[nodiscard]] std::string_view GetStableName() const override;
		[[nodiscard]] std::string_view GetComponentName() const override;

#ifdef _DEBUG
		void DrawInspectorImGui() override;
#endif

		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;

		[[nodiscard]] uint32_t GetIcon() const override;

		[[nodiscard]] TICK_GROUP GetTickGroup() const override;

	private:
		[[nodiscard]] TransformComponent* ResolveLookSourceTransform() const;

		TransformComponent* mTransform  = nullptr;
		TransformComponent* mLookSource = nullptr;

		Quaternion mBaseLocalRotation = Quaternion::identity;
		Vec2       mPrevLookDeg       = Vec2::zero;
		Vec2       mCurrentSwayDeg    = Vec2::zero;
		bool       mInitialized       = false;

		float mPitchScale  = 80.0f;
		float mYawScale    = 80.0f;
		float mMaxPitchDeg = 8.0f;
		float mMaxYawDeg   = 8.0f;
		float mAttenuation = 16.0f;
	};
}
