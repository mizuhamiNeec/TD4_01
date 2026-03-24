#pragma once

#include "engine/unnamed/framework/components/base/BaseComponent.h"

#include "core/math/Vec2.h"

#include "engine/unnamed/subsystem/console/concommand/ConVar.h"

namespace Unnamed {
	class World;
	class TransformComponent;
	class InputSystem;
	class JsonReader;
	class JsonWriter;

	class CameraRotatorComponent final : public BaseComponent {
	public:
		void OnAttached() override;

		void PrePhysicsTick(float deltaTime) override;

		[[nodiscard]] std::string_view GetStableName() const override;
		[[nodiscard]] std::string_view GetComponentName() const override;

#ifdef _DEBUG
		void DrawInspectorImGui() override;
#endif

		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;

		void               SetLookAnglesDegrees(float pitch, float yaw);
		[[nodiscard]] Vec2 GetLookAnglesDegrees() const;

		[[nodiscard]] TICK_GROUP GetTickGroup() const override;

	private:
		[[nodiscard]] TransformComponent* GetTransform() const;
		void                              BindLookAxisOnce() const;

		ConsoleSystem* mConsole = nullptr;
		InputSystem*   mInput         = nullptr;

		float mCurrentPitch = 0.0f;
		float mCurrentYaw   = 0.0f;

		ConVar<float>* mSensitivity = nullptr;
		ConVar<float>* mPitch       = nullptr;
		ConVar<float>* mYaw         = nullptr;

		ConVar<float>* mJoySensitivity = nullptr;
	};
}
