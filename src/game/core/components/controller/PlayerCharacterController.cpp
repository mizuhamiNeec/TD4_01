#include "PlayerCharacterController.h"

#include "../character/base/BaseCharacterComponent.h"
#include "../../../parkour/components/CameraRotatorComponent.h"

#include "core/ComponentRegistry.h"

#include "engine/ImGui/ImGuiWidgets.h"
#include "engine/scene/Scene.h"
#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/unnamed/subsystem/console/Log.h"
#include "engine/unnamed/subsystem/input/InputSystem.h"
#include "engine/unnamed/subsystem/input/device/gamepad/GamepadDevice.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"
#include "engine/world/World.h"

namespace Unnamed {
	namespace {
		void BindGamepadMoveAxisOnce(InputSystem* input) {
			static bool sBound = false;
			if (sBound || !input) {
				return;
			}

			constexpr InputKey gamepadMoveX = {
				.device = InputDeviceType::GAMEPAD,
				.code   = VG_LX
			};
			constexpr InputKey gamepadMoveY = {
				.device = InputDeviceType::GAMEPAD,
				.code   = VG_LY
			};

			input->BindAxis2D("GamepadMove", gamepadMoveX, INPUT_AXIS::X, 1.0f);
			input->BindAxis2D("GamepadMove", gamepadMoveY, INPUT_AXIS::Y, 1.0f);
			sBound = true;
		}
	}

	PlayerCharacterController::~PlayerCharacterController() {}

	void PlayerCharacterController::OnAttached() {
		BaseCharacterController::OnAttached();

		if (auto* inputSystem = ServiceLocator::Get<InputSystem>()) {
			mInput = inputSystem;
			BindGamepadMoveAxisOnce(mInput);
		} else {
			Error(
				GetComponentName(),
				"UInputSystemを取得できませんでした。BaseCharacterControllerは入力を処理できません。"
			);
		}

		TryBindCameraRotator();
	}

	void PlayerCharacterController::OnDetached() {
		if (mCameraRotator) {
			mCameraRotator->SetDirectInputEnabled(true);
			mCameraRotator = nullptr;
		}
		mInput = nullptr;

		BaseCharacterController::OnDetached();
	}

	void PlayerCharacterController::PrePhysicsTick(const float deltaTime) {
		// 各入力を詰め込む
		MovementFrameInput input;
		if (!mInput) {
			return;
		}

		if (mInput->IsHeld("moveright")) {
			input.moveAxis.x += 1.0f;
		}
		if (mInput->IsHeld("moveleft")) {
			input.moveAxis.x -= 1.0f;
		}
		if (mInput->IsHeld("forward")) {
			input.moveAxis.z += 1.0f;
		}
		if (mInput->IsHeld("back")) {
			input.moveAxis.z -= 1.0f;
		}
		if (mInput->IsHeld("moveup")) {
			input.moveAxis.y += 1.0f;
		}
		if (mInput->IsHeld("movedown")) {
			input.moveAxis.y -= 1.0f;
		}

		if (mInput->IsHeld("jump")) {
			input.jumpPressed = true;
		}

		if (mInput->IsHeld("duck")) {
			input.crouchPressed = true;
		}

		if (mInput->IsHeld("sprint")) {
			input.sprintPressed = true;
		}

		const Vec2 gamepadMoveAxis = mInput->Axis2D("GamepadMove");
		input.moveAxis.x           += gamepadMoveAxis.x;
		input.moveAxis.z           += gamepadMoveAxis.y;

		if (!mCameraRotator) {
			TryBindCameraRotator();
		}

		if (mCameraRotator) {
			const Vec2 mouseLookDelta   = mInput->Axis2D("Mouse");
			const Vec2 gamepadLookDelta = mInput->Axis2D("GamepadLook");
			mCameraRotator->AddLookInput(
				mouseLookDelta, gamepadLookDelta, deltaTime
			);
		}

		if (input.moveAxis.Length() > 1.0f) {
			input.moveAxis.Normalize();
		}

		if (auto* world = GetWorld()) {
			const auto& cameraMgr = world->GetCameraManager();

			const auto info = cameraMgr.GetCurrentCameraInfo();
			if (info.valid) {
				input.right   = info.camera.view.Inverse().GetRight();
				input.up      = info.camera.view.Inverse().GetUp();
				input.forward = info.camera.view.Inverse().GetForward();
			}
		}

		mDebugMoveFrameInput = input;

		if (mTarget) {
			mTarget->AddMovementFrameInput(input);
		}
	}

	std::string_view PlayerCharacterController::GetStableName() const {
		return "game.PlayerCharacterController";
	}

	void PlayerCharacterController::TryBindCameraRotator() {
		if (mCameraRotator) {
			return;
		}

		TransformComponent* controlledTransform = nullptr;
		if (Entity* owner = GetOwner()) {
			controlledTransform = owner->GetComponent<TransformComponent>();
		}

		if (auto* scene = GetScene()) {
			for (const auto& entityPtr : scene->GetEntities()) {
				if (!entityPtr) {
					continue;
				}

				auto* rotator = entityPtr->GetComponent<CameraRotatorComponent>();
				if (!rotator) {
					continue;
				}

				auto* rotatorTransform = entityPtr->GetComponent<TransformComponent>();
				if (!rotatorTransform) {
					continue;
				}
				if (controlledTransform &&
				    rotatorTransform->Parent() != controlledTransform) {
					continue;
				}

				mCameraRotator = rotator;
				mCameraRotator->SetDirectInputEnabled(false);
				return;
			}
		}
	}

	std::string_view PlayerCharacterController::GetComponentName() const {
		return "PlayerCharacterController";
	}

#ifdef _DEBUG
	void PlayerCharacterController::DrawInspectorImGui() {
		ImGui::BeginDisabled();
		ImGui::TextUnformatted("PlayerInput");
		ImGuiWidgets::DragVec3(
			"Move Axis", mDebugMoveFrameInput.moveAxis, Vec3::zero, 0.1f, "%.3f"
		);
		ImGuiWidgets::DragVec3(
			"Right", mDebugMoveFrameInput.right, Vec3::zero, 0.1f, "%.3f"
		);
		ImGuiWidgets::DragVec3(
			"Up", mDebugMoveFrameInput.up, Vec3::zero, 0.1f, "%.3f"
		);
		ImGuiWidgets::DragVec3(
			"Forward", mDebugMoveFrameInput.forward, Vec3::zero, 0.1f, "%.3f"
		);
		ImGui::Checkbox("Jump", &mDebugMoveFrameInput.jumpPressed);
		ImGui::Checkbox("Sprint", &mDebugMoveFrameInput.sprintPressed);
		ImGui::Checkbox("Crouch", &mDebugMoveFrameInput.crouchPressed);
		ImGui::EndDisabled();
	}
#endif

	REGISTER_COMPONENT(PlayerCharacterController);
}
