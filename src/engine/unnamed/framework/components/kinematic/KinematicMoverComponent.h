#pragma once
#include "../base/BaseComponent.h"

#include "core/ComponentRegistry.h"
#include "core/math/Quaternion.h"
#include "core/math/Vec3.h"

namespace Unnamed {
	struct KinematicMoverState {
		Vec3 positionPrev = Vec3::zero;
		Vec3 positionCurr = Vec3::zero;

		Quaternion rotationPrev = Quaternion::identity;
		Quaternion rotationCurr = Quaternion::identity;

		Vec3 linearVelocity = Vec3::zero;
		Vec3 deltaPosition  = Vec3::zero;

		float frameDeltaTime = 0.0f;
		bool  wasTeleported  = false;
	};

	class KinematicMoverComponent : public BaseComponent {
	public:
		// ---- KinematicMoverComponent ---------------------------------------
		[[nodiscard]] const KinematicMoverState& GetState() const;

		[[nodiscard]] Vec3 GetDeltaPosition() const;

		[[nodiscard]] Vec3 GetLinearVelocity() const;

		[[nodiscard]] Vec3 GetStepDelta(float stepSeconds) const;

		[[nodiscard]] bool WasTeleported() const;

		void MarkTeleported();

		// ---- BaseComponent -------------------------------------------------
		void OnAttached() override;

		void PrePhysicsTick(float deltaTime) override;
		void OnTick(float deltaTime) override;
		void PostPhysicsTick(float deltaTime) override;

		[[nodiscard]] TICK_GROUP GetTickGroup() const override {
			return TICK_GROUP::KINEMATIC_SOURCE;
		}

		[[nodiscard]] std::string_view GetStableName() const override;
		[[nodiscard]] std::string_view GetComponentName() const override;

		[[nodiscard]] uint32_t GetIcon() const override;

#ifdef _DEBUG
		void DrawInspectorImGui() override;
#endif

		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;

	private:
		KinematicMoverState mState;
	};
}
