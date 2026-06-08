#include "PlayerFollowCameraComponent.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "core/ComponentRegistry.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"
#include "core/math/Mat4.h"
#include "core/math/Math.h"
#include "core/math/Quaternion.h"

#include "engine/ImGui/Icons.h"
#include "engine/scene/Scene.h"
#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"

#ifdef _DEBUG
#include <imgui.h>
#endif

namespace MyGame {
	namespace {
		constexpr float kMinDeltaTime = 0.000001f;
		constexpr float kMinLookDistanceSq = 0.0001f;

		Vec3 ClampLength(Vec3 value, const float maxLength) {
			if (maxLength <= 0.0f) {
				return Vec3::zero;
			}

			const float lengthSq = value.SqrLength();
			const float maxLengthSq = maxLength * maxLength;
			if (lengthSq <= maxLengthSq) {
				return value;
			}

			const float length = std::sqrt(lengthSq);
			if (length <= kMinDeltaTime) {
				return Vec3::zero;
			}
			return value * (maxLength / length);
		}
	}

	void PlayerFollowCameraComponent::OnAttached() {
		_initialized = false;
	}

	void PlayerFollowCameraComponent::OnTick(const float deltaTime) {
		auto* cameraTransform = GetCameraTransform();
		auto* targetTransform = ResolveTargetTransform();
		if (!cameraTransform || !targetTransform) {
			_initialized = false;
			return;
		}

		const Vec3 targetPosition = GetWorldPosition(*targetTransform);
		if (!_initialized) {
			ResetState(targetPosition);
		}

		const float safeDeltaTime = std::max(deltaTime, kMinDeltaTime);
		const Vec3 targetVelocity =
			(targetPosition - _lastTargetPosition) / safeDeltaTime;
		_lastTargetPosition = targetPosition;

		const Vec3 desiredLookAhead = ClampLength(
			targetVelocity * _lookAheadTime,
			_maxLookAheadDistance
		);
		const float lookAheadAlpha =
			DampFactor(_lookAheadSharpness, safeDeltaTime);
		_smoothedLookAhead = Math::Lerp(
			_smoothedLookAhead,
			desiredLookAhead,
			lookAheadAlpha
		);

		const Mat4 targetWorld = targetTransform->RenderWorldMat();
		Vec3 desiredPosition = targetPosition + _offset;
		if (_useTargetYaw) {
			const Vec3 targetForward = targetWorld.GetForward();
			const Vec3 targetRight = targetWorld.GetRight();
			desiredPosition =
				targetPosition +
				targetRight.Normalized() * _offset.x +
				Vec3::up * _offset.y +
				targetForward.Normalized() * _offset.z;
		}

		const float distanceSq =
			(desiredPosition - _smoothedPosition).SqrLength();
		if (_snapDistance > 0.0f && distanceSq > _snapDistance * _snapDistance) {
			// 大きく離れた状態で補間を続けると操作感が重くなるため、復帰時は即座に寄せる。
			_smoothedPosition = desiredPosition;
		} else {
			const float positionAlpha =
				DampFactor(_positionSharpness, safeDeltaTime);
			_smoothedPosition = Math::Lerp(
				_smoothedPosition,
				desiredPosition,
				positionAlpha
			);
		}

		const Vec3 lookAtPosition =
			targetPosition + _lookAtOffset + _smoothedLookAhead;
		ApplyWorldPose(
			*cameraTransform,
			_smoothedPosition,
			lookAtPosition,
			safeDeltaTime
		);
	}

	Unnamed::BaseComponent::TICK_GROUP
	PlayerFollowCameraComponent::GetTickGroup() const {
		return TICK_GROUP::LATE;
	}

	std::string_view PlayerFollowCameraComponent::GetStableName() const {
		return "mygame.PlayerFollowCameraComponent";
	}

	std::string_view PlayerFollowCameraComponent::GetComponentName() const {
		return "Player Follow Camera Component";
	}

	uint32_t PlayerFollowCameraComponent::GetIcon() const {
		return kIconVideoCam;
	}

#ifdef _DEBUG
	void PlayerFollowCameraComponent::DrawInspectorImGui() {
		ImGui::Text("Target");
		ImGui::InputScalar(
			"Target Entity Guid",
			ImGuiDataType_U64,
			&_targetEntityGuid
		);
		char targetName[128] = {};
		std::snprintf(targetName, sizeof(targetName), "%s", _targetName.c_str());
		if (ImGui::InputText("Target Name", targetName, sizeof(targetName))) {
			_targetName = targetName;
		}
		char targetTag[128] = {};
		std::snprintf(targetTag, sizeof(targetTag), "%s", _targetTag.c_str());
		if (ImGui::InputText("Target Tag", targetTag, sizeof(targetTag))) {
			_targetTag = targetTag;
		}

		ImGui::Separator();
		ImGui::DragFloat3("Offset", &_offset.x, 0.05f);
		ImGui::DragFloat3("Look At Offset", &_lookAtOffset.x, 0.05f);
		ImGui::Checkbox("Use Target Yaw", &_useTargetYaw);

		ImGui::Separator();
		ImGui::DragFloat(
			"Position Sharpness",
			&_positionSharpness,
			0.05f,
			0.0f,
			40.0f
		);
		ImGui::DragFloat(
			"Rotation Sharpness",
			&_rotationSharpness,
			0.05f,
			0.0f,
			40.0f
		);
		ImGui::DragFloat("Look Ahead Time", &_lookAheadTime, 0.01f, 0.0f, 1.0f);
		ImGui::DragFloat(
			"Look Ahead Sharpness",
			&_lookAheadSharpness,
			0.05f,
			0.0f,
			40.0f
		);
		ImGui::DragFloat(
			"Max Look Ahead Distance",
			&_maxLookAheadDistance,
			0.05f,
			0.0f,
			20.0f
		);
		ImGui::DragFloat("Snap Distance", &_snapDistance, 0.1f, 0.0f, 100.0f);

		if (ImGui::Button("Reset Follow State")) {
			_initialized = false;
		}
	}
#endif

	void PlayerFollowCameraComponent::Deserialize(
		const Unnamed::JsonReader& reader
	) {
		_targetEntityGuid =
			reader.ReadUint64("targetEntityGuid").value_or(_targetEntityGuid);

		if (reader.Has("targetName")) {
			_targetName = reader["targetName"].GetString(_targetName);
		}
		if (reader.Has("targetTag")) {
			_targetTag = reader["targetTag"].GetString(_targetTag);
		}

		_offset = reader["offset"].GetVec3(_offset);
		_lookAtOffset = reader["lookAtOffset"].GetVec3(_lookAtOffset);

		if (auto val = reader.Read<float>("positionSharpness")) {
			_positionSharpness = std::max(0.0f, val.value());
		}
		if (auto val = reader.Read<float>("rotationSharpness")) {
			_rotationSharpness = std::max(0.0f, val.value());
		}
		if (auto val = reader.Read<float>("lookAheadTime")) {
			_lookAheadTime = std::max(0.0f, val.value());
		}
		if (auto val = reader.Read<float>("lookAheadSharpness")) {
			_lookAheadSharpness = std::max(0.0f, val.value());
		}
		if (auto val = reader.Read<float>("maxLookAheadDistance")) {
			_maxLookAheadDistance = std::max(0.0f, val.value());
		}
		if (auto val = reader.Read<float>("snapDistance")) {
			_snapDistance = std::max(0.0f, val.value());
		}
		if (auto val = reader.Read<bool>("useTargetYaw")) {
			_useTargetYaw = val.value();
		}

		_initialized = false;
	}

	void PlayerFollowCameraComponent::Serialize(
		Unnamed::JsonWriter& writer
	) const {
		writer.Key("targetEntityGuid");
		writer.Write(_targetEntityGuid);
		writer.Key("targetName");
		writer.Write(_targetName);
		writer.Key("targetTag");
		writer.Write(_targetTag);
		writer.Key("offset");
		writer.BeginArray();
		writer.Write(_offset.x);
		writer.Write(_offset.y);
		writer.Write(_offset.z);
		writer.EndArray();
		writer.Key("lookAtOffset");
		writer.BeginArray();
		writer.Write(_lookAtOffset.x);
		writer.Write(_lookAtOffset.y);
		writer.Write(_lookAtOffset.z);
		writer.EndArray();
		writer.Key("positionSharpness");
		writer.Write(_positionSharpness);
		writer.Key("rotationSharpness");
		writer.Write(_rotationSharpness);
		writer.Key("lookAheadTime");
		writer.Write(_lookAheadTime);
		writer.Key("lookAheadSharpness");
		writer.Write(_lookAheadSharpness);
		writer.Key("maxLookAheadDistance");
		writer.Write(_maxLookAheadDistance);
		writer.Key("snapDistance");
		writer.Write(_snapDistance);
		writer.Key("useTargetYaw");
		writer.Write(_useTargetYaw);
	}

	Unnamed::TransformComponent*
	PlayerFollowCameraComponent::GetCameraTransform() const {
		Unnamed::Entity* owner = GetOwner();
		return owner ? owner->GetComponent<Unnamed::TransformComponent>() :
			nullptr;
	}

	Unnamed::TransformComponent*
	PlayerFollowCameraComponent::ResolveTargetTransform() {
		Unnamed::Entity* target = ResolveTargetEntity();
		if (!target) {
			return nullptr;
		}
		return target->GetComponent<Unnamed::TransformComponent>();
	}

	Unnamed::Entity* PlayerFollowCameraComponent::ResolveTargetEntity() const {
		Unnamed::Scene* scene = GetScene();
		if (!scene) {
			return nullptr;
		}

		if (_targetEntityGuid != 0) {
			if (auto* entity = scene->FindEntity(_targetEntityGuid)) {
				return entity;
			}
		}

		if (!_targetTag.empty()) {
			if (auto* entity = scene->FindFirstEntityByTag(_targetTag)) {
				return entity;
			}
		}

		if (!_targetName.empty()) {
			const auto& entities = scene->GetEntities();
			for (const auto& entity : entities) {
				if (entity && entity->GetName() == _targetName) {
					return entity.get();
				}
			}
		}
		return nullptr;
	}

	Vec3 PlayerFollowCameraComponent::GetWorldPosition(
		const Unnamed::TransformComponent& transform
	) const {
		return transform.RenderWorldMat().TransformPoint(Vec3::zero);
	}

	void PlayerFollowCameraComponent::ApplyWorldPose(
		Unnamed::TransformComponent& cameraTransform,
		const Vec3& worldPosition,
		const Vec3& lookAtPosition,
		const float deltaTime
	) const {
		Vec3 forward = lookAtPosition - worldPosition;
		if (forward.SqrLength() <= kMinLookDistanceSq) {
			forward = Vec3::forward;
		} else {
			forward.Normalize();
		}

		const Quaternion desiredWorldRotation =
			Quaternion::LookRotation(forward, Vec3::up);
		const Quaternion currentWorldRotation =
			cameraTransform.RenderWorldMat().ToQuaternion();
		const Quaternion worldRotation = Quaternion::Slerp(
			currentWorldRotation,
			desiredWorldRotation,
			DampFactor(_rotationSharpness, deltaTime)
		).Normalized();

		Mat4 world = Mat4::Affine(Vec3::one, worldRotation, worldPosition);
		if (const auto* parent = cameraTransform.GetParent()) {
			world = world * parent->RenderWorldMat().Inverse();
		}

		cameraTransform.SetPosition(world.GetTranslate());
		cameraTransform.SetRotation(world.ToQuaternion().Normalized());
	}

	void PlayerFollowCameraComponent::ResetState(const Vec3& targetPosition) {
		_smoothedPosition = targetPosition + _offset;
		_smoothedLookAhead = Vec3::zero;
		_lastTargetPosition = targetPosition;
		_initialized = true;
	}

	float PlayerFollowCameraComponent::DampFactor(
		const float sharpness,
		const float deltaTime
	) const {
		if (sharpness <= 0.0f) {
			return 1.0f;
		}
		return 1.0f - std::exp(-sharpness * std::max(0.0f, deltaTime));
	}

	REGISTER_COMPONENT(PlayerFollowCameraComponent);
}
