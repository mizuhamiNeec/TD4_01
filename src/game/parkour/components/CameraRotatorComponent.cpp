#include "CameraRotatorComponent.h"

#include <imgui.h>

#include "core/ComponentRegistry.h"
#include "core/json/JsonReader.h"
#include "core/json/JsonWriter.h"
#include "core/math/Math.h"
#include "core/math/Quaternion.h"

#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/entity/UEntity.h"
#include "engine/unnamed/subsystem/input/UInputSystem.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"
#include "engine/world/UWorld.h"

namespace Unnamed {
	void CameraRotatorComponent::OnAttached() {
		mInput = ServiceLocator::Get<UInputSystem>();
	}

	void CameraRotatorComponent::PrePhysicsTick(const float) {
		auto* transform = GetTransform();
		if (!transform) { return; }
		const UWorld* world = UWorld::GetTickingWorld();
		if (!world || !world->IsGameSimulationEnabled()) { return; }

		if (mReplayLookPending) {
			mPitch             = mReplayPitchDeg;
			mYaw               = mReplayYawDeg;
			mReplayLookPending = false;
		} else if (mInput) {
			const Vec2 delta = mInput->Axis2D("Mouse");
			mPitch += delta.y * mSensitivity;
			mYaw   += delta.x * mSensitivity;
		}

		mPitch = std::clamp(mPitch, -89.0f, 89.0f);
		transform->SetRotation(
			Quaternion::AxisAngle(Vec3::up, mYaw * Math::deg2Rad) *
			Quaternion::AxisAngle(Vec3::right, mPitch * Math::deg2Rad)
		);
	}

	void CameraRotatorComponent::Deserialize(const JsonReader& reader) {
		const JsonReader pitch = reader["pitchDegrees"];
		const JsonReader yaw   = reader["yawDegrees"];
		const JsonReader sensitivity = reader["sensitivity"];
		if (pitch.Valid()) { mPitch = pitch.GetFloat(); }
		if (yaw.Valid()) { mYaw = yaw.GetFloat(); }
		if (sensitivity.Valid()) { mSensitivity = sensitivity.GetFloat(); }
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

	TransformComponent* CameraRotatorComponent::GetTransform() const {
		UEntity* owner = GetOwner();
		return owner ? owner->GetComponent<TransformComponent>() : nullptr;
	}

	REGISTER_COMPONENT(CameraRotatorComponent);
}
