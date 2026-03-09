#include "CameraComponent.h"

#include <imgui.h>

#include "TransformComponent.h"

#include "core/ComponentRegistry.h"
#include "core/json/JsonReader.h"
#include "core/json/JsonWriter.h"
#include "core/math/Math.h"

#include "engine/unnamed/framework/entity/Entity.h"

namespace Unnamed {
	namespace {
		float ReadFloatOr(
			const JsonReader& reader, const char* key, const float fallback
		) {
			const JsonReader value = reader[key];
			return value.Valid() ? value.GetFloat() : fallback;
		}

		bool ReadBoolOr(
			const JsonReader& reader, const char* key, const bool fallback
		) {
			const JsonReader value = reader[key];
			return value.Valid() ? value.GetBool() : fallback;
		}
	}

	void CameraComponent::Deserialize(const JsonReader& reader) {
		mFovYDegrees  = ReadFloatOr(reader, "fovYDegrees", mFovYDegrees);
		mNearZ        = ReadFloatOr(reader, "nearZ", mNearZ);
		mFarZ         = ReadFloatOr(reader, "farZ", mFarZ);
		mCameraActive = ReadBoolOr(reader, "cameraActive", mCameraActive);
	}

	void CameraComponent::Serialize(JsonWriter& writer) const {
		writer.Key("fovYDegrees");
		writer.Write(mFovYDegrees);
		writer.Key("nearZ");
		writer.Write(mNearZ);
		writer.Key("farZ");
		writer.Write(mFarZ);
		writer.Key("cameraActive");
		writer.Write(mCameraActive);
	}

#ifdef _DEBUG
	void CameraComponent::DrawInspectorImGui() {
		ImGui::Checkbox("Camera Active", &mCameraActive);
		ImGui::DragFloat("FovYDegrees", &mFovYDegrees, 0.1f, 1.0f, 179.0f);
		ImGui::DragFloat("NearZ", &mNearZ, 0.001f, 0.001f, mFarZ - 0.01f);
		ImGui::DragFloat("FarZ", &mFarZ, 1.0f, mNearZ + 0.01f, 100000.0f);
		ImGui::Text("AspectRatio: %.3f", mAspectRatio);
	}
#endif

	void CameraComponent::SetAspectRatio(const float aspectRatio) {
		mAspectRatio = aspectRatio > 0.0f ? aspectRatio : 16.0f / 9.0f;
	}

	void CameraComponent::SetCameraActive(bool active) noexcept {
		mCameraActive = active;
	}

	float CameraComponent::GetAspectRatio() const noexcept {
		return mAspectRatio;
	}

	bool CameraComponent::IsCameraActive() const noexcept {
		return mCameraActive;
	}

	bool CameraComponent::BuildCameraInput(
		Render::RenderCameraInput& outCamera
	) const {
		if (!mCameraActive) {
			return false;
		}

		const auto* transform = GetTransform();
		if (!transform) {
			return false;
		}

		const Mat4 world = transform->WorldMat();
		outCamera.view   = world.Inverse();
		outCamera.proj   = Mat4::PerspectiveFovD3D(
			mFovYDegrees * Math::deg2Rad,
			mAspectRatio,
			mNearZ,
			mFarZ,
			ProjectionDepthMode::ReverseZ
		);
		outCamera.viewProj  = outCamera.view * outCamera.proj;
		outCamera.cameraPos = world.TransformPoint(Vec3::zero);
		outCamera.nearZ     = mNearZ;
		outCamera.farZ      = mFarZ;
		outCamera.depthMode = Render::PROJECTION_DEPTH_MODE::ReverseZ;
		outCamera.valid     = true;
		return true;
	}

	std::string_view CameraComponent::GetStableName() const {
		return "game.Camera";
	}

	std::string_view CameraComponent::GetComponentName() const {
		return "Camera";
	}

	TransformComponent* CameraComponent::GetTransform() const {
		Entity* owner = GetOwner();
		return owner ? owner->GetComponent<TransformComponent>() : nullptr;
	}

	REGISTER_COMPONENT(CameraComponent);
}
