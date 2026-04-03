#include "KinematicMoverComponent.h"

#include "../TransformComponent.h"

#include "../../entity/Entity.h"

#include "core/ComponentRegistry.h"

#include "game/core/replay/ReplayHash.h"

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

	void KinematicMoverComponent::WriteReplayState(nlohmann::json& outState) const {
		outState = nlohmann::json::object();
		outState["positionPrev"] = nlohmann::json::array(
			{mState.positionPrev.x, mState.positionPrev.y, mState.positionPrev.z}
		);
		outState["positionCurr"] = nlohmann::json::array(
			{mState.positionCurr.x, mState.positionCurr.y, mState.positionCurr.z}
		);
		outState["rotationPrev"] = nlohmann::json::array(
			{
				mState.rotationPrev.x,
				mState.rotationPrev.y,
				mState.rotationPrev.z,
				mState.rotationPrev.w
			}
		);
		outState["rotationCurr"] = nlohmann::json::array(
			{
				mState.rotationCurr.x,
				mState.rotationCurr.y,
				mState.rotationCurr.z,
				mState.rotationCurr.w
			}
		);
		outState["linearVelocity"] = nlohmann::json::array(
			{mState.linearVelocity.x, mState.linearVelocity.y, mState.linearVelocity.z}
		);
		outState["deltaPosition"] = nlohmann::json::array(
			{mState.deltaPosition.x, mState.deltaPosition.y, mState.deltaPosition.z}
		);
		outState["frameDeltaTime"] = mState.frameDeltaTime;
		outState["wasTeleported"]  = mState.wasTeleported;
	}

	void KinematicMoverComponent::ReadReplayState(const nlohmann::json& inState) {
		if (!inState.is_object()) {
			return;
		}

		if (const auto it = inState.find("positionPrev");
			it != inState.end() && it->is_array() && it->size() == 3) {
			mState.positionPrev = Vec3(
				(*it)[0].get<float>(),
				(*it)[1].get<float>(),
				(*it)[2].get<float>()
			);
		}
		if (const auto it = inState.find("positionCurr");
			it != inState.end() && it->is_array() && it->size() == 3) {
			mState.positionCurr = Vec3(
				(*it)[0].get<float>(),
				(*it)[1].get<float>(),
				(*it)[2].get<float>()
			);
		}
		if (const auto it = inState.find("rotationPrev");
			it != inState.end() && it->is_array() && it->size() == 4) {
			mState.rotationPrev = Quaternion(
				(*it)[0].get<float>(),
				(*it)[1].get<float>(),
				(*it)[2].get<float>(),
				(*it)[3].get<float>()
			);
		}
		if (const auto it = inState.find("rotationCurr");
			it != inState.end() && it->is_array() && it->size() == 4) {
			mState.rotationCurr = Quaternion(
				(*it)[0].get<float>(),
				(*it)[1].get<float>(),
				(*it)[2].get<float>(),
				(*it)[3].get<float>()
			);
		}
		if (const auto it = inState.find("linearVelocity");
			it != inState.end() && it->is_array() && it->size() == 3) {
			mState.linearVelocity = Vec3(
				(*it)[0].get<float>(),
				(*it)[1].get<float>(),
				(*it)[2].get<float>()
			);
		}
		if (const auto it = inState.find("deltaPosition");
			it != inState.end() && it->is_array() && it->size() == 3) {
			mState.deltaPosition = Vec3(
				(*it)[0].get<float>(),
				(*it)[1].get<float>(),
				(*it)[2].get<float>()
			);
		}
		mState.frameDeltaTime = inState.value("frameDeltaTime", mState.frameDeltaTime);
		mState.wasTeleported  = inState.value("wasTeleported", mState.wasTeleported);

		auto* transform = mOwner ? mOwner->GetComponent<TransformComponent>() : nullptr;
		if (transform) {
			transform->SetPosition(mState.positionCurr);
			transform->SetRotation(mState.rotationCurr);
		}
	}

	uint64_t KinematicMoverComponent::ComputeReplayStateHash() const {
		uint64_t hash = ReplayHash::Begin();
		ReplayHash::AppendFloating(hash, mState.positionPrev.x);
		ReplayHash::AppendFloating(hash, mState.positionPrev.y);
		ReplayHash::AppendFloating(hash, mState.positionPrev.z);
		ReplayHash::AppendFloating(hash, mState.positionCurr.x);
		ReplayHash::AppendFloating(hash, mState.positionCurr.y);
		ReplayHash::AppendFloating(hash, mState.positionCurr.z);
		ReplayHash::AppendFloating(hash, mState.rotationPrev.x);
		ReplayHash::AppendFloating(hash, mState.rotationPrev.y);
		ReplayHash::AppendFloating(hash, mState.rotationPrev.z);
		ReplayHash::AppendFloating(hash, mState.rotationPrev.w);
		ReplayHash::AppendFloating(hash, mState.rotationCurr.x);
		ReplayHash::AppendFloating(hash, mState.rotationCurr.y);
		ReplayHash::AppendFloating(hash, mState.rotationCurr.z);
		ReplayHash::AppendFloating(hash, mState.rotationCurr.w);
		ReplayHash::AppendFloating(hash, mState.linearVelocity.x);
		ReplayHash::AppendFloating(hash, mState.linearVelocity.y);
		ReplayHash::AppendFloating(hash, mState.linearVelocity.z);
		ReplayHash::AppendFloating(hash, mState.deltaPosition.x);
		ReplayHash::AppendFloating(hash, mState.deltaPosition.y);
		ReplayHash::AppendFloating(hash, mState.deltaPosition.z);
		ReplayHash::AppendFloating(hash, mState.frameDeltaTime);
		ReplayHash::AppendPod(hash, mState.wasTeleported);
		return hash;
	}

	REGISTER_COMPONENT(KinematicMoverComponent);
}