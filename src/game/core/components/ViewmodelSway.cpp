#include "ViewmodelSway.h"

#include <algorithm>
#include <cmath>

#ifdef _DEBUG
#include <imgui.h>
#endif

#include "core/ComponentRegistry.h"
#include "core/json/JsonReader.h"
#include "core/json/JsonWriter.h"
#include "core/math/Quaternion.h"
#include "core/math/Vec3.h"

#include "engine/ImGui/Icons.h"
#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"

namespace Unnamed {
	namespace {
		float DeltaAngleDegrees(const float currentDeg, const float targetDeg) {
			float delta = std::fmod(targetDeg - currentDeg, 360.0f);
			if (delta > 180.0f) {
				delta -= 360.0f;
			}
			if (delta < -180.0f) {
				delta += 360.0f;
			}
			return delta;
		}
	}

	void ViewmodelSway::OnAttached() {
		BaseComponent::OnAttached();

		Entity* owner = GetOwner();
		if (!owner) {
			return;
		}

		mTransform = owner->GetComponent<TransformComponent>();
		if (!mTransform) {
			return;
		}

		mLookSource = ResolveLookSourceTransform();
		if (!mLookSource) {
			return;
		}

		mBaseLocalRotation = mTransform->Rotation();
		const Vec3 lookDeg = mLookSource->Rotation().ToEulerDegrees();
		mPrevLookDeg       = Vec2(lookDeg.x, lookDeg.y);
		mCurrentSwayDeg    = Vec2::zero;
		mInitialized       = true;
	}

	void ViewmodelSway::OnTick(float deltaTime) {
		if (!mTransform) {
			Entity* owner = GetOwner();
			if (owner) {
				mTransform = owner->GetComponent<TransformComponent>();
			}
		}

		if (!mLookSource) {
			mLookSource = ResolveLookSourceTransform();
		}

		if (!mTransform || !mLookSource) {
			return;
		}

		if (!mInitialized) {
			mBaseLocalRotation = mTransform->Rotation();
			const Vec3 lookDeg = mLookSource->Rotation().ToEulerDegrees();
			mPrevLookDeg       = Vec2(lookDeg.x, lookDeg.y);
			mCurrentSwayDeg    = Vec2::zero;
			mInitialized       = true;
		}

		const Vec3 lookDeg = mLookSource->Rotation().ToEulerDegrees();
		const Vec2 lookNow = Vec2(lookDeg.x, lookDeg.y);

		const float deltaPitch = DeltaAngleDegrees(mPrevLookDeg.x, lookNow.x);
		const float deltaYaw   = DeltaAngleDegrees(mPrevLookDeg.y, lookNow.y);
		mPrevLookDeg           = lookNow;

		Vec2 targetSwayDeg;
		targetSwayDeg.x = std::clamp(
			-deltaPitch * mPitchScale, -mMaxPitchDeg, mMaxPitchDeg
		);
		targetSwayDeg.y = std::clamp(
			deltaYaw * mYawScale, -mMaxYawDeg, mMaxYawDeg
		);

		const float lerpT = std::clamp(deltaTime * mAttenuation, 0.0f, 1.0f);
		mCurrentSwayDeg.x = mCurrentSwayDeg.x +
		                   (targetSwayDeg.x - mCurrentSwayDeg.x) * lerpT;
		mCurrentSwayDeg.y = mCurrentSwayDeg.y +
		                   (targetSwayDeg.y - mCurrentSwayDeg.y) * lerpT;

		const Quaternion swayRotation = Quaternion::EulerDegrees(
			Vec3(mCurrentSwayDeg.x, mCurrentSwayDeg.y, 0.0f)
		);
		mTransform->SetRotation(mBaseLocalRotation * swayRotation);
	}

	std::string_view ViewmodelSway::GetStableName() const {
		return "game.ViewmodelSway";
	}

	std::string_view ViewmodelSway::GetComponentName() const {
		return "ViewmodelSway";
	}

	void ViewmodelSway::DrawInspectorImGui() {
		ImGui::DragFloat("Pitch Scale", &mPitchScale, 0.01f, 0.0f, 4.0f, "%.2f");
		ImGui::DragFloat("Yaw Scale", &mYawScale, 0.01f, 0.0f, 4.0f, "%.2f");
		ImGui::DragFloat("Max Pitch Deg", &mMaxPitchDeg, 0.01f, 0.0f, 20.0f, "%.2f");
		ImGui::DragFloat("Max Yaw Deg", &mMaxYawDeg, 0.01f, 0.0f, 20.0f, "%.2f");
		ImGui::DragFloat(
			"Attenuation", &mAttenuation, 0.1f, 0.0f, 64.0f, "%.2f"
		);
	}

	void ViewmodelSway::Deserialize(const JsonReader& reader) {
		const JsonReader pitchScale = reader["pitchScale"];
		if (pitchScale.Valid()) {
			mPitchScale = pitchScale.GetFloat();
		}

		const JsonReader yawScale = reader["yawScale"];
		if (yawScale.Valid()) {
			mYawScale = yawScale.GetFloat();
		}

		const JsonReader maxPitchDeg = reader["maxPitchDeg"];
		if (maxPitchDeg.Valid()) {
			mMaxPitchDeg = maxPitchDeg.GetFloat();
		}

		const JsonReader maxYawDeg = reader["maxYawDeg"];
		if (maxYawDeg.Valid()) {
			mMaxYawDeg = maxYawDeg.GetFloat();
		}

		const JsonReader attenuation = reader["attenuation"];
		if (attenuation.Valid()) {
			mAttenuation = attenuation.GetFloat();
		}
	}

	void ViewmodelSway::Serialize(JsonWriter& writer) const {
		writer.Key("pitchScale");
		writer.Write(mPitchScale);

		writer.Key("yawScale");
		writer.Write(mYawScale);

		writer.Key("maxPitchDeg");
		writer.Write(mMaxPitchDeg);

		writer.Key("maxYawDeg");
		writer.Write(mMaxYawDeg);

		writer.Key("attenuation");
		writer.Write(mAttenuation);
	}

	uint32_t ViewmodelSway::GetIcon() const {
		return kIcon3DRotation;
	}

	BaseComponent::TICK_GROUP ViewmodelSway::
	GetTickGroup() const {
		return TICK_GROUP::EARLY;
	}

	TransformComponent* ViewmodelSway::ResolveLookSourceTransform() const {
		if (!mTransform) {
			return nullptr;
		}

		return mTransform->Parent();
	}

	REGISTER_COMPONENT(ViewmodelSway);
}
