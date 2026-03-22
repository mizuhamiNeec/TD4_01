#pragma once
#include "../character/base/BaseCharacterComponent.h"

#include "base/BaseCharacterController.h"

namespace Unnamed {
	/// @brief プレイヤーがキャラクターを制御するためのコンポーネントです。
	class PlayerCharacterController : public BaseCharacterController {
	public:
		~PlayerCharacterController() override;

		// ---- BaseComponent ------------------------------------------------
		void OnAttached() override;

		void                           PrePhysicsTick(float deltaTime) override;
		[[nodiscard]] TickGroup        GetTickGroup() const override {
			return TickGroup::Early;
		}
		[[nodiscard]] std::string_view GetStableName() const override;
		[[nodiscard]] std::string_view GetComponentName() const override;

#ifdef _DEBUG
		void DrawInspectorImGui() override;
#endif

	protected:
		InputSystem*       mInput = nullptr;
		MovementFrameInput mDebugMoveFrameInput;
	};
}
