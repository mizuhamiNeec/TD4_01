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

#include "engine/unnamed/framework/components/CameraComponent.h"
#include "engine/ImGui/Icons.h"
#include "engine/scene/Scene.h"
#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/unnamed/subsystem/input/InputSystem.h"
#include "engine/unnamed/subsystem/input/device/gamepad/GamepadDevice.h"
#include "engine/unnamed/subsystem/input/device/mouse/MouseDevice.h"

#include "VoiceShockWaveComponent.h"

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
		_baseFovCaptured = false;
		_voiceShockWaveComponent = nullptr;
		SetupInputBindings();
	}

	void PlayerFollowCameraComponent::OnDetached() {
		if (_lockMouseCursor) {
			if (auto* inputSystem = GetInputSystem()) {
				inputSystem->SetMouseCursorLocked(false);
				inputSystem->SetMouseCursorVisible(true);
				inputSystem->ClearMouseCursorLockAnchor();
			}
		}
		_voiceShockWaveComponent = nullptr;
	}

	void PlayerFollowCameraComponent::OnTick(const float deltaTime) {
		auto* cameraTransform = GetCameraTransform();
		auto* cameraComponent = GetCameraComponent();
		auto* targetTransform = ResolveTargetTransform();
		if (!cameraTransform || !targetTransform) {
			_initialized = false;
			return;
		}

		const float safeDeltaTime = std::max(deltaTime, kMinDeltaTime);
		UpdateOrbitInput(safeDeltaTime);
		UpdateSmoothedOrbit(safeDeltaTime);
		UpdateVoiceCameraEffect(safeDeltaTime);
		if (cameraComponent) {
			ApplyVoiceFov(*cameraComponent);
		}

		const Mat4 targetWorld = targetTransform->RenderWorldMat();
		const Vec3 targetPosition = GetWorldPosition(*targetTransform);
		if (!_initialized) {
			ResetState(targetPosition, targetWorld);
		}

		if (_bStageIntroMode) {
			ApplyStageIntroCamera(*cameraTransform, targetPosition, safeDeltaTime);
			return;
		}

		UpdateSmoothedBaseYaw(targetWorld, safeDeltaTime);

		const Vec3 targetVelocity =
			(targetPosition - _lastTargetPosition) / safeDeltaTime;
		_lastTargetPosition = targetPosition;
		const float targetAlpha = DampFactor(_targetSharpness, safeDeltaTime);
		_smoothedTargetPosition = Math::Lerp(
			_smoothedTargetPosition,
			targetPosition,
			targetAlpha
		);
		_smoothedTargetVelocity = Math::Lerp(
			_smoothedTargetVelocity,
			targetVelocity,
			targetAlpha
		);

		Vec3 horizontalVelocity = _smoothedTargetVelocity;
		horizontalVelocity.y = 0.0f;
		const Vec3 desiredLookAhead = ClampLength(
			horizontalVelocity * _lookAheadTime,
			_maxLookAheadDistance
		);
		const float lookAheadAlpha =
			DampFactor(_lookAheadSharpness, safeDeltaTime);
		_smoothedLookAhead = Math::Lerp(
			_smoothedLookAhead,
			desiredLookAhead,
			lookAheadAlpha
		);

		const Vec3 cameraOffset = BuildVoiceReactiveOffset(
			BuildOrbitOffset(targetWorld)
		);
		const Vec3 desiredPosition = _smoothedTargetPosition + cameraOffset;

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

		const Vec3 desiredLookAtPosition =
			_smoothedTargetPosition + _lookAtOffset + _smoothedLookAhead;
		_smoothedLookAtPosition = Math::Lerp(
			_smoothedLookAtPosition,
			desiredLookAtPosition,
			DampFactor(_lookAtSharpness, safeDeltaTime)
		);
		ApplyWorldPose(
			*cameraTransform,
			_smoothedPosition,
			_smoothedLookAtPosition,
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
		ImGui::Text("ターゲット");
		ImGui::InputScalar(
			"ターゲットエンティティGUID",
			ImGuiDataType_U64,
			&_targetEntityGuid
		);
		char targetName[128] = {};
		std::snprintf(targetName, sizeof(targetName), "%s", _targetName.c_str());
		if (ImGui::InputText("ターゲット名", targetName, sizeof(targetName))) {
			_targetName = targetName;
		}
		char targetTag[128] = {};
		std::snprintf(targetTag, sizeof(targetTag), "%s", _targetTag.c_str());
		if (ImGui::InputText("ターゲットタグ", targetTag, sizeof(targetTag))) {
			_targetTag = targetTag;
		}

		ImGui::Separator();
		ImGui::DragFloat3("カメラオフセット", &_offset.x, 0.05f);
		ImGui::DragFloat3("注視点オフセット", &_lookAtOffset.x, 0.05f);
		ImGui::Checkbox("ターゲットの向きに追従", &_useTargetYaw);

		ImGui::Separator();
		ImGui::Text("回転入力");
		char lookAxisName[128] = {};
		std::snprintf(
			lookAxisName,
			sizeof(lookAxisName),
			"%s",
			_lookAxisName.c_str()
		);
		if (ImGui::InputText("視点操作軸名", lookAxisName, sizeof(lookAxisName))) {
			_lookAxisName = lookAxisName;
		}
		ImGui::DragFloat("マウス左右感度", &_mouseYawSensitivity, 0.005f, 0.0f, 2.0f);
		ImGui::DragFloat("マウス上下感度", &_mousePitchSensitivity, 0.005f, 0.0f, 2.0f);
		ImGui::DragFloat("ゲームパッド左右感度", &_gamepadYawSensitivity, 1.0f, 0.0f, 720.0f);
		ImGui::DragFloat("ゲームパッド上下感度", &_gamepadPitchSensitivity, 1.0f, 0.0f, 720.0f);
		ImGui::DragFloat("ゲームパッド視点デッドゾーン", &_gamepadLookDeadZone, 0.005f, 0.0f, 0.95f);
		ImGui::DragFloat("最小ピッチ角", &_minPitchDegrees, 0.5f, -89.0f, 89.0f);
		ImGui::DragFloat("最大ピッチ角", &_maxPitchDegrees, 0.5f, -89.0f, 89.0f);
		ImGui::Checkbox("左右操作を反転", &_invertYaw);
		ImGui::Checkbox("上下操作を反転", &_invertPitch);
		ImGui::Checkbox("マウスカーソルをロック", &_lockMouseCursor);
		ImGui::Text("回転量: 左右 %.2f / 上下 %.2f", _orbitYawDegrees, _orbitPitchDegrees);

		ImGui::Separator();
		ImGui::DragFloat(
			"ターゲット追従の速さ",
			&_targetSharpness,
			0.05f,
			0.0f,
			40.0f
		);
		ImGui::DragFloat(
			"カメラ位置追従の速さ",
			&_positionSharpness,
			0.05f,
			0.0f,
			40.0f
		);
		ImGui::DragFloat(
			"カメラ回転追従の速さ",
			&_rotationSharpness,
			0.05f,
			0.0f,
			40.0f
		);
		ImGui::DragFloat(
			"視点回転追従の速さ",
			&_orbitSharpness,
			0.05f,
			0.0f,
			40.0f
		);
		ImGui::DragFloat(
			"ターゲット向き追従の速さ",
			&_yawSharpness,
			0.05f,
			0.0f,
			40.0f
		);
		ImGui::DragFloat(
			"注視点追従の速さ",
			&_lookAtSharpness,
			0.05f,
			0.0f,
			40.0f
		);
		ImGui::DragFloat("先読み時間", &_lookAheadTime, 0.01f, 0.0f, 1.0f);
		ImGui::DragFloat(
			"先読み追従の速さ",
			&_lookAheadSharpness,
			0.05f,
			0.0f,
			40.0f
		);
		ImGui::DragFloat(
			"最大先読み距離",
			&_maxLookAheadDistance,
			0.05f,
			0.0f,
			20.0f
		);
		ImGui::DragFloat("瞬間復帰距離", &_snapDistance, 0.1f, 0.0f, 100.0f);

		ImGui::Separator();
		ImGui::Text("ボイス連動カメラ効果");
		ImGui::Checkbox("有効##voice_camera_enabled", &_voiceCameraEffectEnabled);
		ImGui::DragFloat("音量デッドゾーン", &_voiceDeadZone, 0.005f, 0.0f, 1.0f);
		ImGui::DragFloat("最大音量基準", &_voiceMaxVolume, 0.005f, 0.01f, 1.0f);
		ImGui::DragFloat("最大FOV加算", &_voiceFovAddMax, 0.1f, 0.0f, 30.0f);
		ImGui::DragFloat("最大距離加算", &_voiceDistanceAddMax, 0.05f, 0.0f, 10.0f);
		ImGui::DragFloat("立ち上がり追従の速さ", &_voiceRiseSharpness, 0.05f, 0.0f, 40.0f);
		ImGui::DragFloat("戻り追従の速さ", &_voiceFallSharpness, 0.05f, 0.0f, 40.0f);
		ImGui::Text("ボイス強度: %.3f", _voiceCameraIntensity);

		if (ImGui::Button("追従状態をリセット")) {
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

		if (auto val = reader.Read<std::string>("lookAxisName")) {
			_lookAxisName = val.value();
		}
		if (auto val = reader.Read<float>("mouseYawSensitivity")) {
			_mouseYawSensitivity = std::max(0.0f, val.value());
		}
		if (auto val = reader.Read<float>("mousePitchSensitivity")) {
			_mousePitchSensitivity = std::max(0.0f, val.value());
		}
		if (auto val = reader.Read<float>("gamepadYawSensitivity")) {
			_gamepadYawSensitivity = std::max(0.0f, val.value());
		}
		if (auto val = reader.Read<float>("gamepadPitchSensitivity")) {
			_gamepadPitchSensitivity = std::max(0.0f, val.value());
		}
		if (auto val = reader.Read<float>("gamepadLookDeadZone")) {
			_gamepadLookDeadZone = std::clamp(val.value(), 0.0f, 0.95f);
		}
		if (auto val = reader.Read<float>("minPitchDegrees")) {
			_minPitchDegrees = std::clamp(val.value(), -89.0f, 89.0f);
		}
		if (auto val = reader.Read<float>("maxPitchDegrees")) {
			_maxPitchDegrees = std::clamp(val.value(), -89.0f, 89.0f);
		}
		if (auto val = reader.Read<bool>("invertYaw")) {
			_invertYaw = val.value();
		}
		if (auto val = reader.Read<bool>("invertPitch")) {
			_invertPitch = val.value();
		}
		if (auto val = reader.Read<bool>("lockMouseCursor")) {
			_lockMouseCursor = val.value();
		}
		if (auto val = reader.Read<float>("targetSharpness")) {
			_targetSharpness = std::max(0.0f, val.value());
		}
		if (auto val = reader.Read<float>("positionSharpness")) {
			_positionSharpness = std::max(0.0f, val.value());
		}
		if (auto val = reader.Read<float>("rotationSharpness")) {
			_rotationSharpness = std::max(0.0f, val.value());
		}
		if (auto val = reader.Read<float>("orbitSharpness")) {
			_orbitSharpness = std::max(0.0f, val.value());
		}
		if (auto val = reader.Read<float>("yawSharpness")) {
			_yawSharpness = std::max(0.0f, val.value());
		}
		if (auto val = reader.Read<float>("lookAtSharpness")) {
			_lookAtSharpness = std::max(0.0f, val.value());
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
		if (auto val = reader.Read<bool>("voiceCameraEffectEnabled")) {
			_voiceCameraEffectEnabled = val.value();
		}
		if (reader.Has("stageIntroOffset")) {
			_stageIntroOffset = reader["stageIntroOffset"].GetVec3(_stageIntroOffset);
		}
		if (reader.Has("stageIntroLookAtOffset")) {
			_stageIntroLookAtOffset =
				reader["stageIntroLookAtOffset"].GetVec3(_stageIntroLookAtOffset);
		}
		if (auto val = reader.Read<float>("stageIntroOrbitSpeedDegrees")) {
			_stageIntroOrbitSpeedDegrees = val.value();
		}
		if (auto val = reader.Read<float>("stageIntroPositionSharpness")) {
			_stageIntroPositionSharpness = std::max(0.0f, val.value());
		}
		if (auto val = reader.Read<float>("voiceDeadZone")) {
			_voiceDeadZone = std::clamp(val.value(), 0.0f, 1.0f);
		}
		if (auto val = reader.Read<float>("voiceMaxVolume")) {
			_voiceMaxVolume = std::clamp(val.value(), 0.01f, 1.0f);
		}
		if (auto val = reader.Read<float>("voiceFovAddMax")) {
			_voiceFovAddMax = std::max(0.0f, val.value());
		}
		if (auto val = reader.Read<float>("voiceDistanceAddMax")) {
			_voiceDistanceAddMax = std::max(0.0f, val.value());
		}
		if (auto val = reader.Read<float>("voiceRiseSharpness")) {
			_voiceRiseSharpness = std::max(0.0f, val.value());
		}
		if (auto val = reader.Read<float>("voiceFallSharpness")) {
			_voiceFallSharpness = std::max(0.0f, val.value());
		}

		_initialized = false;
		_baseFovCaptured = false;
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
		writer.Key("lookAxisName");
		writer.Write(_lookAxisName);
		writer.Key("mouseYawSensitivity");
		writer.Write(_mouseYawSensitivity);
		writer.Key("mousePitchSensitivity");
		writer.Write(_mousePitchSensitivity);
		writer.Key("gamepadYawSensitivity");
		writer.Write(_gamepadYawSensitivity);
		writer.Key("gamepadPitchSensitivity");
		writer.Write(_gamepadPitchSensitivity);
		writer.Key("gamepadLookDeadZone");
		writer.Write(_gamepadLookDeadZone);
		writer.Key("minPitchDegrees");
		writer.Write(_minPitchDegrees);
		writer.Key("maxPitchDegrees");
		writer.Write(_maxPitchDegrees);
		writer.Key("invertYaw");
		writer.Write(_invertYaw);
		writer.Key("invertPitch");
		writer.Write(_invertPitch);
		writer.Key("lockMouseCursor");
		writer.Write(_lockMouseCursor);
		writer.Key("targetSharpness");
		writer.Write(_targetSharpness);
		writer.Key("positionSharpness");
		writer.Write(_positionSharpness);
		writer.Key("rotationSharpness");
		writer.Write(_rotationSharpness);
		writer.Key("orbitSharpness");
		writer.Write(_orbitSharpness);
		writer.Key("yawSharpness");
		writer.Write(_yawSharpness);
		writer.Key("lookAtSharpness");
		writer.Write(_lookAtSharpness);
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
		writer.Key("voiceCameraEffectEnabled");
		writer.Write(_voiceCameraEffectEnabled);
		writer.Key("stageIntroOffset");
		writer.BeginArray();
		writer.Write(_stageIntroOffset.x);
		writer.Write(_stageIntroOffset.y);
		writer.Write(_stageIntroOffset.z);
		writer.EndArray();
		writer.Key("stageIntroLookAtOffset");
		writer.BeginArray();
		writer.Write(_stageIntroLookAtOffset.x);
		writer.Write(_stageIntroLookAtOffset.y);
		writer.Write(_stageIntroLookAtOffset.z);
		writer.EndArray();
		writer.Key("stageIntroOrbitSpeedDegrees");
		writer.Write(_stageIntroOrbitSpeedDegrees);
		writer.Key("stageIntroPositionSharpness");
		writer.Write(_stageIntroPositionSharpness);
		writer.Key("voiceDeadZone");
		writer.Write(_voiceDeadZone);
		writer.Key("voiceMaxVolume");
		writer.Write(_voiceMaxVolume);
		writer.Key("voiceFovAddMax");
		writer.Write(_voiceFovAddMax);
		writer.Key("voiceDistanceAddMax");
		writer.Write(_voiceDistanceAddMax);
		writer.Key("voiceRiseSharpness");
		writer.Write(_voiceRiseSharpness);
		writer.Key("voiceFallSharpness");
		writer.Write(_voiceFallSharpness);
	}

	Unnamed::TransformComponent*
	PlayerFollowCameraComponent::GetCameraTransform() const {
		Unnamed::Entity* owner = GetOwner();
		return owner ? owner->GetComponent<Unnamed::TransformComponent>() :
			nullptr;
	}

	Unnamed::CameraComponent*
	PlayerFollowCameraComponent::GetCameraComponent() const {
		Unnamed::Entity* owner = GetOwner();
		return owner ? owner->GetComponent<Unnamed::CameraComponent>() :
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

	VoiceShockWaveComponent*
	PlayerFollowCameraComponent::ResolveVoiceShockWaveComponent() {
		if (_voiceShockWaveComponent) {
			return _voiceShockWaveComponent;
		}

		Unnamed::Scene* scene = GetScene();
		if (!scene) {
			return nullptr;
		}

		const auto& entities = scene->GetEntities();
		for (const auto& entity : entities) {
			if (!entity) {
				continue;
			}
			if (auto* voice = entity->GetComponent<VoiceShockWaveComponent>()) {
				_voiceShockWaveComponent = voice;
				return _voiceShockWaveComponent;
			}
		}
		return nullptr;
	}

	Vec3 PlayerFollowCameraComponent::GetWorldPosition(
		const Unnamed::TransformComponent& transform
	) const {
		return transform.RenderWorldMat().TransformPoint(Vec3::zero);
	}

	Vec3 PlayerFollowCameraComponent::BuildVoiceReactiveOffset(
		const Vec3& baseOffset
	) const {
		if (!_voiceCameraEffectEnabled || _voiceDistanceAddMax <= 0.0f) {
			return baseOffset;
		}

		Vec3 distanceDirection(baseOffset.x, 0.0f, baseOffset.z);
		if (distanceDirection.SqrLength() <= kMinLookDistanceSq) {
			distanceDirection = Vec3(0.0f, 0.0f, -1.0f);
		} else {
			distanceDirection.Normalize();
		}

		return baseOffset +
			distanceDirection * (_voiceDistanceAddMax * _voiceCameraIntensity);
	}

	void PlayerFollowCameraComponent::SetupInputBindings() {
		auto* inputSystem = GetInputSystem();
		if (!inputSystem) {
			return;
		}

		using namespace Unnamed;

		const std::string mouseAxis = _lookAxisName + ".Mouse";
		inputSystem->BindAxis2D(
			mouseAxis,
			{ .device = InputDeviceType::MOUSE, .code = VM_X },
			INPUT_AXIS::X,
			1.0f
		);
		inputSystem->BindAxis2D(
			mouseAxis,
			{ .device = InputDeviceType::MOUSE, .code = VM_Y },
			INPUT_AXIS::Y,
			1.0f
		);

		const std::string gamepadAxis = _lookAxisName + ".Gamepad";
		inputSystem->BindAxis2D(
			gamepadAxis,
			{ .device = InputDeviceType::GAMEPAD, .code = VG_RX },
			INPUT_AXIS::X,
			1.0f
		);
		inputSystem->BindAxis2D(
			gamepadAxis,
			{ .device = InputDeviceType::GAMEPAD, .code = VG_RY },
			INPUT_AXIS::Y,
			1.0f
		);
	}

	void PlayerFollowCameraComponent::UpdateOrbitInput(const float deltaTime) {
		auto* inputSystem = GetInputSystem();
		if (!inputSystem) {
			return;
		}

		if (_lockMouseCursor) {
			inputSystem->SetMouseCursorLocked(true);
			inputSystem->SetMouseCursorVisible(false);
		}

		const Vec2 mouseLook = inputSystem->Axis2D(_lookAxisName + ".Mouse");
		Vec2 gamepadLook = inputSystem->Axis2D(_lookAxisName + ".Gamepad");
		if (gamepadLook.SqrLength() < _gamepadLookDeadZone * _gamepadLookDeadZone) {
			gamepadLook = Vec2::zero;
		}

		const float yawSign = _invertYaw ? 1.0f : -1.0f;
		const float mousePitchSign = _invertPitch ? 1.0f : -1.0f;
		const float gamepadPitchSign = _invertPitch ? -1.0f : 1.0f;
		_orbitYawDegrees +=
			yawSign * (
				mouseLook.x * _mouseYawSensitivity +
				gamepadLook.x * _gamepadYawSensitivity * deltaTime
			);
		_orbitPitchDegrees +=
			mousePitchSign * mouseLook.y * _mousePitchSensitivity +
			gamepadPitchSign *
			gamepadLook.y *
			_gamepadPitchSensitivity *
			deltaTime;

		if (_orbitYawDegrees > 360.0f || _orbitYawDegrees < -360.0f) {
			_orbitYawDegrees = std::fmod(_orbitYawDegrees, 360.0f);
		}

		const float minPitch = std::min(_minPitchDegrees, _maxPitchDegrees);
		const float maxPitch = std::max(_minPitchDegrees, _maxPitchDegrees);
		_orbitPitchDegrees = std::clamp(
			_orbitPitchDegrees,
			minPitch,
			maxPitch
		);
	}

	void PlayerFollowCameraComponent::UpdateSmoothedOrbit(
		const float deltaTime
	) {
		_smoothedOrbitYawDegrees = DampAngleDegrees(
			_smoothedOrbitYawDegrees,
			_orbitYawDegrees,
			_orbitSharpness,
			deltaTime
		);
		_smoothedOrbitPitchDegrees = Math::Lerp(
			_smoothedOrbitPitchDegrees,
			_orbitPitchDegrees,
			DampFactor(_orbitSharpness, deltaTime)
		);
	}

	void PlayerFollowCameraComponent::UpdateSmoothedBaseYaw(
		const Mat4& targetWorld,
		const float deltaTime
	) {
		if (!_useTargetYaw) {
			_smoothedBaseYawDegrees = 0.0f;
			return;
		}

		const float targetYawDegrees = GetTargetYawDegrees(targetWorld);
		_smoothedBaseYawDegrees = DampAngleDegrees(
			_smoothedBaseYawDegrees,
			targetYawDegrees,
			_yawSharpness,
			deltaTime
		);
	}

	Vec3 PlayerFollowCameraComponent::BuildOrbitOffset(
		const Mat4& targetWorld
	) const {
		Vec3 horizontalOffset(_offset.x, 0.0f, _offset.z);
		if (horizontalOffset.SqrLength() <= kMinLookDistanceSq) {
			horizontalOffset = Vec3::backward;
		}

		(void)targetWorld;
		const float baseYawDegrees = _useTargetYaw ? _smoothedBaseYawDegrees : 0.0f;
		const float yawRad =
			(baseYawDegrees + _smoothedOrbitYawDegrees) * Math::deg2Rad;
		const float pitchRad = _smoothedOrbitPitchDegrees * Math::deg2Rad;
		const Quaternion yawRotation = Quaternion::AxisAngle(Vec3::up, yawRad);

		const Vec3 rotatedHorizontal =
			yawRotation.RotateVector(horizontalOffset) * std::cos(pitchRad);
		const float horizontalLength = horizontalOffset.Length();
		const float verticalOffset =
			_offset.y + std::sin(pitchRad) * horizontalLength;

		return rotatedHorizontal + Vec3::up * verticalOffset;
	}

	float PlayerFollowCameraComponent::GetTargetYawDegrees(
		const Mat4& targetWorld
	) const {
		Vec3 forward = targetWorld.GetForward();
		forward.y = 0.0f;
		if (forward.SqrLength() <= kMinLookDistanceSq) {
			return 0.0f;
		}
		forward.Normalize();
		return std::atan2(forward.x, forward.z) * Math::rad2Deg;
	}

	float PlayerFollowCameraComponent::DampAngleDegrees(
		const float current,
		const float target,
		const float sharpness,
		const float deltaTime
	) const {
		const float deltaRad = Math::DeltaAngle(
			current * Math::deg2Rad,
			target * Math::deg2Rad
		);
		const float alpha = DampFactor(sharpness, deltaTime);
		return current + deltaRad * Math::rad2Deg * alpha;
	}

	Vec3 PlayerFollowCameraComponent::GetPlanarForward() const {
		const auto* cameraTransform = GetCameraTransform();
		if (!cameraTransform) {
			return Vec3::forward;
		}

		Vec3 forward = cameraTransform->RenderWorldMat().GetForward();
		forward.y = 0.0f;
		if (forward.SqrLength() <= kMinLookDistanceSq) {
			return Vec3::forward;
		}
		return forward.Normalized();
	}

	Vec3 PlayerFollowCameraComponent::GetPlanarRight() const {
		const auto* cameraTransform = GetCameraTransform();
		if (!cameraTransform) {
			return Vec3::right;
		}

		Vec3 right = cameraTransform->RenderWorldMat().GetRight();
		right.y = 0.0f;
		if (right.SqrLength() <= kMinLookDistanceSq) {
			return Vec3::right;
		}
		return right.Normalized();
	}

	void PlayerFollowCameraComponent::SetStageIntroMode(bool enabled) {
		if (_bStageIntroMode == enabled) {
			return;
		}

		_bStageIntroMode = enabled;
		_stageIntroElapsedTime = 0.0f;
		_initialized = false;
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

	void PlayerFollowCameraComponent::ApplyStageIntroCamera(
		Unnamed::TransformComponent& cameraTransform,
		const Vec3& targetPosition,
		const float deltaTime
	) {
		_stageIntroElapsedTime += deltaTime;

		const float yawRad =
			_stageIntroElapsedTime * _stageIntroOrbitSpeedDegrees * Math::deg2Rad;
		const Quaternion yawRotation = Quaternion::AxisAngle(Vec3::up, yawRad);
		const Vec3 desiredPosition =
			targetPosition + yawRotation.RotateVector(_stageIntroOffset);
		const Vec3 desiredLookAtPosition = targetPosition + _stageIntroLookAtOffset;

		const float positionAlpha =
			DampFactor(_stageIntroPositionSharpness, deltaTime);
		_smoothedPosition = Math::Lerp(
			_smoothedPosition,
			desiredPosition,
			positionAlpha
		);
		_smoothedLookAtPosition = Math::Lerp(
			_smoothedLookAtPosition,
			desiredLookAtPosition,
			positionAlpha
		);

		// NOTE: 紹介中は通常追従の先読みや入力回転を使わず、ステージ全体を見せる固定演出にする。
		ApplyWorldPose(
			cameraTransform,
			_smoothedPosition,
			_smoothedLookAtPosition,
			deltaTime
		);
	}

	void PlayerFollowCameraComponent::UpdateVoiceCameraEffect(
		const float deltaTime
	) {
		float targetIntensity = 0.0f;
		if (_voiceCameraEffectEnabled) {
			if (auto* voice = ResolveVoiceShockWaveComponent()) {
				const float rawVolume = std::clamp(voice->GetLastVolume(), 0.0f, 1.0f);
				const float range = std::max(0.001f, _voiceMaxVolume - _voiceDeadZone);
				targetIntensity = std::clamp((rawVolume - _voiceDeadZone) / range, 0.0f, 1.0f);
				// 大声の瞬間だけ過敏に跳ねないよう、体感を少し寝かせる。
				targetIntensity = targetIntensity * targetIntensity;
			}
		}

		const float sharpness =
			targetIntensity > _voiceCameraIntensity ?
			_voiceRiseSharpness :
			_voiceFallSharpness;
		_voiceCameraIntensity = Math::Lerp(
			_voiceCameraIntensity,
			targetIntensity,
			DampFactor(sharpness, deltaTime)
		);
	}

	void PlayerFollowCameraComponent::ApplyVoiceFov(
		Unnamed::CameraComponent& cameraComponent
	) {
		if (!_baseFovCaptured) {
			_baseFovYDegrees = cameraComponent.GetFovYDegrees();
			_baseFovCaptured = true;
		}

		const float fovAdd =
			_voiceCameraEffectEnabled ?
			_voiceFovAddMax * _voiceCameraIntensity :
			0.0f;
		cameraComponent.SetFovYDegrees(_baseFovYDegrees + fovAdd);
	}

	void PlayerFollowCameraComponent::ResetState(
		const Vec3& targetPosition,
		const Mat4& targetWorld
	) {
		_smoothedTargetPosition = targetPosition;
		_smoothedTargetVelocity = Vec3::zero;
		_smoothedBaseYawDegrees =
			_useTargetYaw ? GetTargetYawDegrees(targetWorld) : 0.0f;
		_smoothedOrbitYawDegrees = _orbitYawDegrees;
		_smoothedOrbitPitchDegrees = _orbitPitchDegrees;
		_smoothedPosition =
			targetPosition + BuildVoiceReactiveOffset(BuildOrbitOffset(targetWorld));
		_smoothedLookAtPosition = targetPosition + _lookAtOffset;
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
