#include "ViewmodelSway.h"

#include <algorithm>
#include <cmath>

#ifdef _DEBUG
#include <imgui.h>
#endif

#include "core/ComponentRegistry.h"
#include "core/json/JsonReader.h"
#include "core/json/JsonWriter.h"
#include "core/math/Math.h"
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

		Vec2 ExtractLookPitchYawDegrees(const Quaternion& rotation) {
			const Vec3 forward = rotation.RotateVector(Vec3::forward).
			                              Normalized();

			const float horizontalLen =
				std::sqrt(forward.x * forward.x + forward.z * forward.z);
			const float pitchRad = std::atan2(forward.y, horizontalLen);
			const float yawRad   = std::atan2(forward.x, forward.z);

			return Vec2(pitchRad * Math::rad2Deg, yawRad * Math::rad2Deg);
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

		mLookSource = GetParentTransform();
		if (!mLookSource) {
			return;
		}

		mBaseLocalRotation = mTransform->Rotation();
		mPrevLookDeg = ExtractLookPitchYawDegrees(mLookSource->Rotation());
		mCurrentSwayDeg = Vec2::zero;
		mInitialized = true;
	}

	void ViewmodelSway::OnTick(float deltaTime) {
		if (!mTransform) {
			Entity* owner = GetOwner();
			if (owner) {
				mTransform = owner->GetComponent<TransformComponent>();
			}
		}

		if (!mLookSource) {
			mLookSource = GetParentTransform();
		}

		if (!mTransform || !mLookSource) {
			return;
		}

		if (!mInitialized) {
			mBaseLocalRotation = mTransform->Rotation();
			mPrevLookDeg = ExtractLookPitchYawDegrees(mLookSource->Rotation());
			mCurrentSwayDeg = Vec2::zero;
			mInitialized = true;
		}

		const Vec2 lookNow = ExtractLookPitchYawDegrees(
			mLookSource->Rotation()
		);

		const float deltaPitch = DeltaAngleDegrees(mPrevLookDeg.x, lookNow.x);
		const float deltaYaw   = DeltaAngleDegrees(mPrevLookDeg.y, lookNow.y);
		mPrevLookDeg           = lookNow;

		mPitch += deltaPitch * mSwayAmount * deltaTime;
		mYaw   += deltaYaw * mSwayAmount * deltaTime;

		mPitch = std::lerp(mPitch, 0.0f, mAttenuation * deltaTime); // ピッチの減衰
		mYaw   = std::lerp(mYaw, 0.0f, mAttenuation * deltaTime);   // ヨーの減衰

		const Quaternion pitch = Quaternion::AxisAngle(Vec3::right, mPitch);
		const Quaternion yaw   = Quaternion::AxisAngle(Vec3::up, mYaw);

		const Quaternion finalRotation = yaw * pitch;

		// ちょっとずらす
		mTransform->SetPosition(
			mTransform->Position() + Vec2(deltaYaw, deltaPitch) *
			mLocationAmount
		);

		mTransform->OnTick(0.0f); // ずらした位置を反映させるために先に更新しておく

		// ベースの回転にスウェイの回転を乗算して適用
		mTransform->SetRotation(mBaseLocalRotation * finalRotation);

		mTransform->SetPosition(
			Math::Lerp(
				mTransform->Position(),
				Vec3::zero,
				mAttenuation * deltaTime
			)
		);
	}

	std::string_view ViewmodelSway::GetStableName() const {
		return "game.ViewmodelSway";
	}

	std::string_view ViewmodelSway::GetComponentName() const {
		return "ViewmodelSway";
	}

#ifdef _DEBUG
	void ViewmodelSway::DrawInspectorImGui() {
		ImGui::DragFloat(
			"SwayAmount", &mSwayAmount, 0.01f, 0.0f, 10.0f, "%.2f"
		);
		ImGui::DragFloat(
			"LocationAmount", &mLocationAmount, 0.01f, 0.0f, 10.0f, "%.2f"
		);
		ImGui::DragFloat(
			"Attenuation", &mAttenuation, 0.1f, 0.0f, 64.0f, "%.2f"
		);
	}
#endif

	void ViewmodelSway::Deserialize(const JsonReader& reader) {
		const JsonReader swayAmount = reader["swayAmount"];
		if (swayAmount.Valid()) {
			mSwayAmount = swayAmount.GetFloat();
		}

		const JsonReader locationAmount = reader["locationAmount"];
		if (locationAmount.Valid()) {
			mLocationAmount = locationAmount.GetFloat();
		}

		const JsonReader attenuation = reader["attenuation"];
		if (attenuation.Valid()) {
			mAttenuation = attenuation.GetFloat();
		}
	}

	void ViewmodelSway::Serialize(JsonWriter& writer) const {
		writer.Key("swayAmount");
		writer.Write(mSwayAmount);

		writer.Key("locationAmount");
		writer.Write(mLocationAmount);

		writer.Key("attenuation");
		writer.Write(mAttenuation);
	}

	uint32_t ViewmodelSway::GetIcon() const {
		return kIcon3DRotation;
	}

	BaseComponent::TICK_GROUP ViewmodelSway::GetTickGroup() const {
		return TICK_GROUP::EARLY;
	}

	TransformComponent* ViewmodelSway::GetParentTransform() const {
		if (!mTransform) {
			return nullptr;
		}

		return mTransform->Parent();
	}

	REGISTER_COMPONENT(ViewmodelSway);
}
