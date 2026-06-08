#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "engine/unnamed/framework/components/base/BaseComponent.h"

#include "core/math/Vec3.h"

namespace Unnamed {
	class Entity;
	class JsonReader;
	class JsonWriter;
	class TransformComponent;
}

namespace MyGame {
	class PlayerFollowCameraComponent : public Unnamed::BaseComponent {
	public:
		void OnAttached() override;
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

	private:
		[[nodiscard]] Unnamed::TransformComponent* GetCameraTransform() const;
		[[nodiscard]] Unnamed::TransformComponent* ResolveTargetTransform();
		[[nodiscard]] Unnamed::Entity* ResolveTargetEntity() const;
		[[nodiscard]] Vec3 GetWorldPosition(
			const Unnamed::TransformComponent& transform
		) const;
		void ApplyWorldPose(
			Unnamed::TransformComponent& cameraTransform,
			const Vec3& worldPosition,
			const Vec3& lookAtPosition,
			float deltaTime
		) const;
		void ResetState(const Vec3& targetPosition);
		[[nodiscard]] float DampFactor(float sharpness, float deltaTime) const;

		uint64_t _targetEntityGuid = 42;
		std::string _targetName = "Player";
		std::string _targetTag;

		Vec3 _offset = Vec3(0.0f, 4.0f, -6.0f);
		Vec3 _lookAtOffset = Vec3(0.0f, 1.2f, 0.0f);

		float _positionSharpness = 7.5f;
		float _rotationSharpness = 10.0f;
		float _lookAheadTime = 0.22f;
		float _lookAheadSharpness = 8.0f;
		float _maxLookAheadDistance = 2.5f;
		float _snapDistance = 18.0f;
		bool _useTargetYaw = true;

		Vec3 _smoothedPosition = Vec3::zero;
		Vec3 _smoothedLookAhead = Vec3::zero;
		Vec3 _lastTargetPosition = Vec3::zero;
		bool _initialized = false;
	};
}
