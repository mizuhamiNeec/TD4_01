#pragma once
#include "../character/base/BaseCharacterComponent.h"

#include "base/BaseCharacterController.h"

namespace Unnamed {
	class CameraRotatorComponent;

	/// @brief プレイヤーがキャラクターを制御するためのコンポーネントです。
	class PlayerCharacterController : public BaseCharacterController {
	public:
		~PlayerCharacterController() override;

		// ---- BaseComponent ------------------------------------------------
		void OnAttached() override;
		void OnDetached() override;

		void                           PrePhysicsTick(float deltaTime) override;
		[[nodiscard]] TICK_GROUP        GetTickGroup() const override {
			return TICK_GROUP::EARLY;
		}
		[[nodiscard]] std::string_view GetStableName() const override;
		[[nodiscard]] std::string_view GetComponentName() const override;

#ifdef _DEBUG
		void DrawInspectorImGui() override;
#endif

	protected:
		void TryBindCameraRotator();

		InputSystem*       mInput = nullptr;
		CameraRotatorComponent* mCameraRotator = nullptr;
		MovementFrameInput mDebugMoveFrameInput;
	};
}
