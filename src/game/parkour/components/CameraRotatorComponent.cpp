#include "CameraRotatorComponent.h"

#include <cmath>

#include <imgui.h>

#include "core/ComponentRegistry.h"
#include "core/json/JsonReader.h"
#include "core/json/JsonWriter.h"
#include "core/math/Math.h"
#include "core/math/Quaternion.h"

#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/unnamed/subsystem/console/ConsoleSystem.h"
#include "engine/unnamed/subsystem/console/concommand/ConVar.h"
#include "engine/unnamed/subsystem/input/InputSystem.h"
#include "engine/unnamed/subsystem/input/device/gamepad/GamepadDevice.h"
#include "engine/unnamed/subsystem/input/device/mouse/MouseDevice.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"

namespace Unnamed {
	void CameraRotatorComponent::OnAttached() {
		mInput = ServiceLocator::Get<InputSystem>();
		BindLookAxisOnce();
		if (const TransformComponent* transform = GetTransform()) {
			const Vec3 eulerDegrees = transform->Rotation().ToEulerDegrees();
			mCurrentPitch           = eulerDegrees.x;
			mCurrentYaw             = eulerDegrees.y;
		}

		mConsole = ServiceLocator::Get<ConsoleSystem>();
	}

	void CameraRotatorComponent::PrePhysicsTick(const float deltaTime) {
		auto* transform = GetTransform();
		if (!transform) {
			return;
		}

		Vec2 mouseDelta   = Vec2::zero;
		Vec2 gamepadDelta = Vec2::zero;
		if (mInput) {
			mouseDelta   = mInput->Axis2D("Mouse");
			gamepadDelta = mInput->Axis2D("GamepadLook");
		}

		// コンソールから各値を取得。
		const float sensitivity = mConsole->GetConVarValueOr(
			"sensitivity", 1.0f
		);
		const float pitch = mConsole->GetConVarValueOr(
			"m_pitch", 0.022f
		);
		const float yaw = mConsole->GetConVarValueOr(
			"m_yaw", 0.022f
		);
		const float pitchUpLimit = mConsole->GetConVarValueOr(
			"cl_pitchup", 89.0f
		);
		const float pitchDownLimit = mConsole->GetConVarValueOr(
			"cl_pitchdown", 89.0f
		);

		const float joySensitivity = mConsole->GetConVarValueOr(
			"joy_sensitivity", 360.0f
		);

		// deltaTimeはゲームパッドの入力にのみ適用する。
		mCurrentPitch += mouseDelta.y * sensitivity * pitch;
		mCurrentYaw   += mouseDelta.x * sensitivity * yaw;
		
		mCurrentPitch += gamepadDelta.y * joySensitivity * deltaTime;
		mCurrentYaw   += gamepadDelta.x * joySensitivity * deltaTime;

		mCurrentPitch = std::clamp(
			mCurrentPitch, -pitchUpLimit, pitchDownLimit
		);
		transform->SetRotation(
			Quaternion::AxisAngle(Vec3::up, mCurrentYaw * Math::deg2Rad) *
			Quaternion::AxisAngle(Vec3::right, mCurrentPitch * Math::deg2Rad)
		);
	}

	std::string_view CameraRotatorComponent::GetStableName() const {
		return "parkour.CameraRotator";
	}

	std::string_view CameraRotatorComponent::GetComponentName() const {
		return "CameraRotator";
	}

	void CameraRotatorComponent::Deserialize(const JsonReader& reader) {
		const JsonReader pitch = reader["pitchDegrees"];
		const JsonReader yaw   = reader["yawDegrees"];
		if (pitch.Valid()) {
			mCurrentPitch = pitch.GetFloat();
		}
		if (yaw.Valid()) {
			mCurrentYaw = yaw.GetFloat();
		}
	}

	void CameraRotatorComponent::Serialize(JsonWriter& writer) const {
		writer.Key("pitchDegrees");
		writer.Write(mCurrentPitch);
		writer.Key("yawDegrees");
		writer.Write(mCurrentYaw);
	}

#ifdef _DEBUG
	void CameraRotatorComponent::DrawInspectorImGui() {
		ImGui::DragFloat("Pitch", &mCurrentPitch, 0.1f, -89.0f, 89.0f);
		ImGui::DragFloat("Yaw", &mCurrentYaw, 0.1f, -1080.0f, 1080.0f);
	}
#endif

	void CameraRotatorComponent::SetLookAnglesDegrees(
		const float pitch, const float yaw
	) {
		mCurrentPitch = pitch;
		mCurrentYaw   = yaw;
	}

	Vec2 CameraRotatorComponent::GetLookAnglesDegrees() const {
		return Vec2(mCurrentPitch, mCurrentYaw);
	}

	BaseComponent::TICK_GROUP CameraRotatorComponent::GetTickGroup() const {
		return TICK_GROUP::EARLY;
	}

	TransformComponent* CameraRotatorComponent::GetTransform() const {
		Entity* owner = GetOwner();
		return owner ? owner->GetComponent<TransformComponent>() : nullptr;
	}

	void CameraRotatorComponent::BindLookAxisOnce() const {
		static bool sBound = false;
		if (sBound || !mInput) {
			return;
		}

		constexpr InputKey mouseX = {
			.device = InputDeviceType::MOUSE,
			.code   = VM_X
		};
		constexpr InputKey mouseY = {
			.device = InputDeviceType::MOUSE,
			.code   = VM_Y
		};
		constexpr InputKey gamepadLookX = {
			.device = InputDeviceType::GAMEPAD,
			.code   = VG_RX
		};
		constexpr InputKey gamepadLookY = {
			.device = InputDeviceType::GAMEPAD,
			.code   = VG_RY
		};

		mInput->BindAxis2D("Mouse", mouseX, INPUT_AXIS::X, 1.0f);
		mInput->BindAxis2D("Mouse", mouseY, INPUT_AXIS::Y, 1.0f);
		mInput->BindAxis2D("GamepadLook", gamepadLookX, INPUT_AXIS::X, 1.0f);
		mInput->BindAxis2D("GamepadLook", gamepadLookY, INPUT_AXIS::Y, -1.0f);
		sBound = true;
	}

	REGISTER_COMPONENT(CameraRotatorComponent);
}
