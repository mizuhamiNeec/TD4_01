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
#include "engine/unnamed/subsystem/input/device/mouse/MouseDevice.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"

namespace Unnamed {
	namespace {
		void BindMouseAxisOnce(InputSystem* input) {
			static bool sBound = false;
			if (sBound || !input) {
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

			input->BindAxis2D("Mouse", mouseX, INPUT_AXIS::X, 1.0f);
			input->BindAxis2D("Mouse", mouseY, INPUT_AXIS::Y, 1.0f);
			sBound = true;
		}
	}

	void CameraRotatorComponent::OnAttached() {
		mInput = ServiceLocator::Get<InputSystem>();
		BindMouseAxisOnce(mInput);
		if (const TransformComponent* transform = GetTransform()) {
			const Vec3 eulerDegrees = transform->Rotation().ToEulerDegrees();
			mPitch                  = eulerDegrees.x;
			mYaw                    = eulerDegrees.y;
		}
	}

	void CameraRotatorComponent::PrePhysicsTick(const float) {
		auto* transform = GetTransform();
		if (!transform) {
			return;
		}

		float sensi          = 1.0f;
		float m_pitch        = 0.022f;
		float m_yaw          = 0.022f;
		float pitchUpLimit   = 89.0f;
		float pitchDownLimit = 89.0f;
		if (auto* console = ServiceLocator::Get<ConsoleSystem>()) {
			if (const auto* sensitivity =
				console->GetConVarAs<ConVar<float>>("sensitivity")) {
				sensi = sensitivity->GetValue();
			}
			if (const auto* pitch = console->GetConVarAs<ConVar<float>>(
				"m_pitch"
			)) {
				m_pitch = pitch->GetValue();
			}
			if (const auto* yaw = console->GetConVarAs<ConVar<float>>(
				"m_yaw"
			)) {
				m_yaw = yaw->GetValue();
			}

			if (const auto* pitchUp =
				console->GetConVarAs<ConVar<float>>("cl_pitchup")) {
				pitchUpLimit = std::abs(pitchUp->GetValue());
			}
			if (const auto* pitchDown = console->GetConVarAs<ConVar<
				float>>(
				"cl_pitchdown"
			)) {
				pitchDownLimit = std::abs(pitchDown->GetValue());
			}
		}

		if (mReplayLookPending) {
			mPitch             = mReplayPitchDeg;
			mYaw               = mReplayYawDeg;
			mReplayLookPending = false;
		} else {
			Vec2 delta = Vec2::zero;
			if (mLiveLookPending) {
				delta            = mLiveLookDelta;
				mLiveLookPending = false;
			} else if (mInput) {
				delta = mInput->Axis2D("Mouse");
			}
			mPitch += delta.y * sensi * m_pitch;
			mYaw   += delta.x * sensi * m_yaw;
		}

		mPitch = std::clamp(mPitch, -pitchUpLimit, pitchDownLimit);
		transform->SetRotation(
			Quaternion::AxisAngle(Vec3::up, mYaw * Math::deg2Rad) *
			Quaternion::AxisAngle(Vec3::right, mPitch * Math::deg2Rad)
		);
	}

	void CameraRotatorComponent::Deserialize(const JsonReader& reader) {
		const JsonReader pitch       = reader["pitchDegrees"];
		const JsonReader yaw         = reader["yawDegrees"];
		const JsonReader sensitivity = reader["sensitivity"];
		if (pitch.Valid()) {
			mPitch = pitch.GetFloat();
		}
		if (yaw.Valid()) {
			mYaw = yaw.GetFloat();
		}
		if (sensitivity.Valid()) {
			mSensitivity = sensitivity.GetFloat();
		}
	}

	void CameraRotatorComponent::Serialize(JsonWriter& writer) const {
		writer.Key("pitchDegrees");
		writer.Write(mPitch);
		writer.Key("yawDegrees");
		writer.Write(mYaw);
		writer.Key("sensitivity");
		writer.Write(mSensitivity);
	}

#ifdef _DEBUG
	void CameraRotatorComponent::DrawInspectorImGui() {
		ImGui::DragFloat("Pitch", &mPitch, 0.1f, -89.0f, 89.0f);
		ImGui::DragFloat("Yaw", &mYaw, 0.1f, -1080.0f, 1080.0f);
		ImGui::DragFloat("Sensitivity", &mSensitivity, 0.001f, 0.01f, 1.0f);
	}
#endif

	void CameraRotatorComponent::SetLookAnglesDegrees(
		const float pitch, const float yaw
	) {
		mPitch = pitch;
		mYaw   = yaw;
	}

	Vec2 CameraRotatorComponent::GetLookAnglesDegrees() const {
		return Vec2(mPitch, mYaw);
	}

	void CameraRotatorComponent::SetReplayLookOverride(
		const float pitch, const float yaw
	) {
		mReplayPitchDeg    = pitch;
		mReplayYawDeg      = yaw;
		mReplayLookPending = true;
	}

	void CameraRotatorComponent::ClearReplayLookOverride() {
		mReplayLookPending = false;
	}

	void CameraRotatorComponent::SetLiveLookInput(const Vec2& delta) {
		mLiveLookDelta   = delta;
		mLiveLookPending = true;
	}

	void CameraRotatorComponent::ClearLiveLookInput() {
		mLiveLookPending = false;
		mLiveLookDelta   = Vec2::zero;
	}

	TransformComponent* CameraRotatorComponent::GetTransform() const {
		Entity* owner = GetOwner();
		return owner ? owner->GetComponent<TransformComponent>() : nullptr;
	}

	REGISTER_COMPONENT(CameraRotatorComponent);
}
