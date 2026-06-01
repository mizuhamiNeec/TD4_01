#pragma once
#include "base/BaseComponent.h"

#include "core/math/Vec3.h"

namespace Unnamed {
	class TransformComponent;

	class RotatorComponent : public BaseComponent {
	public:
		~RotatorComponent() override;
		
		//---------------------------------------------------------------------
		// BaseComponent↓ 
		//---------------------------------------------------------------------
		void                           OnAttached() override;
		void                           OnTick(float deltaTime) override;
		
		[[nodiscard]] TICK_GROUP       GetTickGroup() const override;
		
		[[nodiscard]] std::string_view GetStableName() const override;
		[[nodiscard]] std::string_view GetComponentName() const override;
		
		void                           Deserialize(const JsonReader& reader) override;
		void                           Serialize(JsonWriter& writer) const override;
		
		[[nodiscard]] uint32_t         GetIcon() const override;
		
#ifdef _DEBUG
		void                           DrawInspectorImGui() override;
#endif
		
	private:
		TransformComponent* mTransform = nullptr;
		
		Vec3 mRotationSpeed = Vec3::zero; // [度/秒]
	};
}
