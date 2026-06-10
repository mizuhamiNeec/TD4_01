#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "engine/unnamed/framework/components/base/BaseComponent.h"

#include "core/math/Vec2.h"
#include "core/math/Vec3.h"

struct Mat4;

namespace Unnamed {
	class CameraComponent;
	class Entity;
	class JsonReader;
	class JsonWriter;
	class TransformComponent;
}

namespace MyGame {
	class VoiceShockWaveComponent;

	class PlayerFollowCameraComponent : public Unnamed::BaseComponent {
	public:
		void OnAttached() override;
		void OnDetached() override;
		void OnTick(float deltaTime) override;

		[[nodiscard]] Unnamed::BaseComponent::TICK_GROUP GetTickGroup()
			const override;
		[[nodiscard]] std::string_view GetStableName() const override;
		[[nodiscard]] std::string_view GetComponentName() const override;
		[[nodiscard]] uint32_t GetIcon() const override;

#ifdef _DEBUG
		void DrawInspectorImGui() override;
#endif

		void Deserialize(const Unnamed::JsonReader& reader) override;
		void Serialize(Unnamed::JsonWriter& writer) const override;

		[[nodiscard]] Vec3 GetPlanarForward() const;
		[[nodiscard]] Vec3 GetPlanarRight() const;

	private:
		[[nodiscard]] Unnamed::TransformComponent* GetCameraTransform() const;
		[[nodiscard]] Unnamed::CameraComponent* GetCameraComponent() const;
		[[nodiscard]] Unnamed::TransformComponent* ResolveTargetTransform();
		[[nodiscard]] Unnamed::Entity* ResolveTargetEntity() const;
		[[nodiscard]] VoiceShockWaveComponent* ResolveVoiceShockWaveComponent();
		[[nodiscard]] Vec3 GetWorldPosition(
			const Unnamed::TransformComponent& transform
		) const;
		[[nodiscard]] Vec3 BuildVoiceReactiveOffset(const Vec3& baseOffset)
			const;
		void SetupInputBindings();
		void UpdateOrbitInput(float deltaTime);
		void UpdateSmoothedOrbit(float deltaTime);
		void UpdateSmoothedBaseYaw(const Mat4& targetWorld, float deltaTime);
		[[nodiscard]] Vec3 BuildOrbitOffset(const Mat4& targetWorld) const;
		[[nodiscard]] float GetTargetYawDegrees(const Mat4& targetWorld) const;
		[[nodiscard]] float DampAngleDegrees(
			float current,
			float target,
			float sharpness,
			float deltaTime
		) const;
		void ApplyWorldPose(
			Unnamed::TransformComponent& cameraTransform,
			const Vec3& worldPosition,
			const Vec3& lookAtPosition,
			float deltaTime
		) const;
		void UpdateVoiceCameraEffect(float deltaTime);
		void ApplyVoiceFov(Unnamed::CameraComponent& cameraComponent);
		void ResetState(const Vec3& targetPosition, const Mat4& targetWorld);
		[[nodiscard]] float DampFactor(float sharpness, float deltaTime) const;

		uint64_t _targetEntityGuid = 42;
		std::string _targetName = "Player";
		std::string _targetTag;

		Vec3 _offset = Vec3(0.0f, 4.0f, -6.0f);
		Vec3 _lookAtOffset = Vec3(0.0f, 1.2f, 0.0f);

		std::string _lookAxisName = "CameraLook";
		float _mouseYawSensitivity = 0.08f;
		float _mousePitchSensitivity = 0.08f;
		float _gamepadYawSensitivity = 180.0f;
		float _gamepadPitchSensitivity = 135.0f;
		float _gamepadLookDeadZone = 0.12f;
		float _minPitchDegrees = -35.0f;
		float _maxPitchDegrees = 60.0f;
		bool _invertYaw = false;
		bool _invertPitch = false;
		bool _lockMouseCursor = true;

		float _targetSharpness = 8.0f;
		float _positionSharpness = 4.5f;
		float _rotationSharpness = 6.0f;
		float _orbitSharpness = 12.0f;
		float _yawSharpness = 7.0f;
		float _lookAtSharpness = 7.0f;
		float _lookAheadTime = 0.08f;
		float _lookAheadSharpness = 4.0f;
		float _maxLookAheadDistance = 0.7f;
		float _snapDistance = 18.0f;
		bool _useTargetYaw = true;

		bool _voiceCameraEffectEnabled = true;
		float _voiceDeadZone = 0.04f;
		float _voiceMaxVolume = 0.55f;
		float _voiceFovAddMax = 4.0f;
		float _voiceDistanceAddMax = 0.8f;
		float _voiceRiseSharpness = 4.5f;
		float _voiceFallSharpness = 2.0f;

		Vec3 _smoothedPosition = Vec3::zero;
		Vec3 _smoothedTargetPosition = Vec3::zero;
		Vec3 _smoothedLookAtPosition = Vec3::zero;
		Vec3 _smoothedLookAhead = Vec3::zero;
		Vec3 _smoothedTargetVelocity = Vec3::zero;
		Vec3 _lastTargetPosition = Vec3::zero;
		VoiceShockWaveComponent* _voiceShockWaveComponent = nullptr;
		float _voiceCameraIntensity = 0.0f;
		float _baseFovYDegrees = 90.0f;
		float _smoothedBaseYawDegrees = 0.0f;
		float _orbitYawDegrees = 0.0f;
		float _orbitPitchDegrees = 0.0f;
		float _smoothedOrbitYawDegrees = 0.0f;
		float _smoothedOrbitPitchDegrees = 0.0f;
		bool _baseFovCaptured = false;
		bool _initialized = false;
	};
}
