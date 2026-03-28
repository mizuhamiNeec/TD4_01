#include "KinematicMoverComponent.h"

#include "../TransformComponent.h"

#include "../../entity/Entity.h"

namespace Unnamed {
	class TransformComponent;

	const KinematicMoverState& KinematicMoverComponent::GetState() const {
		return mState;
	}

	Vec3 KinematicMoverComponent::GetDeltaPosition() const {
		return mState.deltaPosition;
	}

	Vec3 KinematicMoverComponent::GetLinearVelocity() const {
		return mState.linearVelocity;
	}

	Vec3 KinematicMoverComponent::GetStepDelta(const float stepSeconds) const {
		if (mState.wasTeleported || stepSeconds <= 0.0f ||
		    mState.frameDeltaTime <= 0.0f) {
			return Vec3::zero;
		}

		return mState.linearVelocity * stepSeconds;
	}

	bool KinematicMoverComponent::WasTeleported() const {
		return mState.wasTeleported;
	}

	void KinematicMoverComponent::MarkTeleported() {
		mState.wasTeleported = true;
	}

	void KinematicMoverComponent::OnAttached() {
		BaseComponent::OnAttached();

		auto* transform = mOwner ?
			                  mOwner->GetComponent<TransformComponent>() :
			                  nullptr;
		if (!transform) {
			return;
		}

		mState.positionPrev   = transform->Position();
		mState.positionCurr   = transform->Position();
		mState.rotationPrev   = transform->Rotation();
		mState.rotationCurr   = transform->Rotation();
		mState.linearVelocity = Vec3::zero;
		mState.deltaPosition  = Vec3::zero;
		mState.frameDeltaTime = 0.0f;
		mState.wasTeleported  = false;
	}

	void KinematicMoverComponent::PrePhysicsTick(float) {
		auto* transform = mOwner->GetComponent<TransformComponent>();
		if (!transform) {
			return;
		}

		mState.positionPrev = mState.positionCurr;
		mState.rotationPrev = mState.rotationCurr;

		mState.positionCurr = transform->Position();
		mState.rotationCurr = transform->Rotation();

		mState.wasTeleported = false;
	}

	void KinematicMoverComponent::OnTick(float deltaTime) {
		BaseComponent::OnTick(deltaTime);

		auto* transform = mOwner->GetComponent<TransformComponent>();
		if (!transform) {
			return;
		}

		mState.positionCurr = transform->Position();
		mState.rotationCurr = transform->Rotation();

		mState.deltaPosition  = mState.positionCurr - mState.positionPrev;
		mState.frameDeltaTime = deltaTime;
		if (deltaTime > 0.0f) {
			mState.linearVelocity = mState.deltaPosition / deltaTime;
		} else {
			mState.linearVelocity = Vec3::zero;
		}
	}

	void KinematicMoverComponent::PostPhysicsTick(float) {}

	std::string_view KinematicMoverComponent::GetStableName() const {
		return "engine.KinematicMover";
	}

	std::string_view KinematicMoverComponent::GetComponentName() const {
		return "KinematicMover";
	}

	uint32_t KinematicMoverComponent::GetIcon() const {
		return BaseComponent::GetIcon();
	}

#ifdef _DEBUG
	void KinematicMoverComponent::DrawInspectorImGui() {
		BaseComponent::DrawInspectorImGui();
	}
#endif

	void KinematicMoverComponent::Deserialize(const JsonReader& reader) {
		reader;
	}

	void KinematicMoverComponent::Serialize(JsonWriter& writer) const {
		writer;
	}

	REGISTER_COMPONENT(KinematicMoverComponent);
}
