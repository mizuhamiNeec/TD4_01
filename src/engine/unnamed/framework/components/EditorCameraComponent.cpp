#include "EditorCameraComponent.h"

#include "TransformComponent.h"

#include "core/ComponentRegistry.h"
#include "core/json/JsonReader.h"
#include "core/json/JsonWriter.h"
#include "core/math/Math.h"

#include "engine/unnamed/framework/entity/UEntity.h"
#include "engine/unnamed/subsystem/console/ConsoleSystem.h"
#include "engine/unnamed/subsystem/input/UInputSystem.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"

namespace Unnamed {
	namespace {
		void InstallEditorCameraBindingsOnce() {
			static bool sInstalled = false;
			if (sInstalled) { return; }

			auto* console = ServiceLocator::Get<ConsoleSystem>();
			if (!console) { return; }

			static constexpr const char* kBindings[] = {
				"bind w +forward",
				"bind s +backward",
				"bind a +left",
				"bind d +right",
				"bind e +up",
				"bind q +down",
				"bind uparrow +pitchup",
				"bind downarrow +pitchdown",
				"bind leftarrow +yawleft",
				"bind rightarrow +yawright",
			};

			for (const char* command : kBindings) {
				console->ExecuteCommand(command);
			}
			sInstalled = true;
		}

		float ReadFloatOr(
			const JsonReader& reader, const char* key, const float fallback
		) {
			const JsonReader value = reader[key];
			return value.Valid() ? value.GetFloat() : fallback;
		}
	}

	void EditorCameraComponent::OnAttached() {
		InstallEditorCameraBindingsOnce();
	}

	void EditorCameraComponent::OnTick(const float deltaTime) {
		auto* transform = GetTransform();
		if (!transform) { return; }

		const auto* input = ServiceLocator::Get<UInputSystem>();
		if (!input) { return; }

		Vec3       position  = transform->Position();
		Quaternion rotation  = transform->Rotation();
		const Mat4 rotMatrix = Mat4::FromQuaternion(rotation);

		const Vec3 right   = rotMatrix.GetRight();
		const Vec3 up      = rotMatrix.GetUp();
		const Vec3 forward = rotMatrix.GetForward();

		Vec3 move = Vec3::zero;
		if (input->IsHeld("forward")) { move += forward; }
		if (input->IsHeld("backward")) { move -= forward; }
		if (input->IsHeld("left")) { move -= right; }
		if (input->IsHeld("right")) { move += right; }
		if (input->IsHeld("up")) { move += up; }
		if (input->IsHeld("down")) { move -= up; }

		position += move * (mMoveSpeed * deltaTime);

		const float deltaRot = mRotateSpeed * deltaTime;
		if (input->IsHeld("pitchup")) {
			rotation = rotation * Quaternion::AxisAngle(Vec3::right, -deltaRot);
		}
		if (input->IsHeld("pitchdown")) {
			rotation = rotation * Quaternion::AxisAngle(Vec3::right, deltaRot);
		}
		if (input->IsHeld("yawleft")) {
			rotation = rotation * Quaternion::AxisAngle(Vec3::up, -deltaRot);
		}
		if (input->IsHeld("yawright")) {
			rotation = rotation * Quaternion::AxisAngle(Vec3::up, deltaRot);
		}

		transform->SetPosition(position);
		transform->SetRotation(rotation);
	}

	void EditorCameraComponent::Deserialize(const JsonReader& reader) {
		mFovYDegrees = ReadFloatOr(reader, "fovYDegrees", mFovYDegrees);
		mNearZ       = ReadFloatOr(reader, "nearZ", mNearZ);
		mFarZ        = ReadFloatOr(reader, "farZ", mFarZ);
		mMoveSpeed   = ReadFloatOr(reader, "moveSpeed", mMoveSpeed);
		mRotateSpeed = ReadFloatOr(reader, "rotateSpeed", mRotateSpeed);
	}

	void EditorCameraComponent::Serialize(JsonWriter& writer) const {
		writer.Key("fovYDegrees");
		writer.Write(mFovYDegrees);
		writer.Key("nearZ");
		writer.Write(mNearZ);
		writer.Key("farZ");
		writer.Write(mFarZ);
		writer.Key("moveSpeed");
		writer.Write(mMoveSpeed);
		writer.Key("rotateSpeed");
		writer.Write(mRotateSpeed);
	}

	bool EditorCameraComponent::BuildCameraInput(
		const float aspect, Render::RenderCameraInput& outCamera
	) const {
		const auto* transform = GetTransform();
		if (!transform) { return false; }

		const Vec3       position = transform->Position();
		const Quaternion rotation = transform->Rotation();
		const Mat4       world    =
			Mat4::FromQuaternion(rotation) *
			Mat4::Scale(Vec3::one) *
			Mat4::Translate(position);

		const Mat4 view = world.Inverse();
		const Mat4 proj = Mat4::PerspectiveFovMat(
			mFovYDegrees * Math::deg2Rad,
			aspect,
			mNearZ,
			mFarZ
		);

		outCamera.viewProj  = view * proj;
		outCamera.cameraPos = position;
		outCamera.valid     = true;
		return true;
	}

	TransformComponent* EditorCameraComponent::GetTransform() const {
		UEntity* owner = GetOwner();
		return owner ? owner->GetComponent<TransformComponent>() : nullptr;
	}

	REGISTER_COMPONENT(EditorCameraComponent);
}
