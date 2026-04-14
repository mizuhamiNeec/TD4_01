#include "TriggerVolumeComponentBase.h"

#ifdef _DEBUG
#include <imgui.h>
#endif

#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"
#include "core/math/Math.h"

#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"

namespace Unnamed {
	Vec3 TriggerVolumeComponentBase::GetWorldCenter() const noexcept {
		if (const auto* transform = GetTransform()) {
			return transform->GetWorldMat().TransformPoint(mLocalCenter);
		}
		return mLocalCenter;
	}

	Vec3
	TriggerVolumeComponentBase::GetWorldHalfExtentsMeters() const noexcept {
		return Math::HtoM(mExtentsHu * 0.5f);
	}

	void TriggerVolumeComponentBase::DeserializeVolume(
		const JsonReader& reader
	) {
		mLocalCenter = reader["localCenter"].GetVec3(mLocalCenter);
		mExtentsHu   = reader["extentsHu"].GetVec3(mExtentsHu);
	}

	void TriggerVolumeComponentBase::SerializeVolume(JsonWriter& writer) const {
		writer.WriteVec3("localCenter", mLocalCenter);
		writer.WriteVec3("extentsHu", mExtentsHu);
	}

	TransformComponent* TriggerVolumeComponentBase::GetTransform() const {
		Entity* owner = GetOwner();
		return owner ? owner->GetComponent<TransformComponent>() : nullptr;
	}

#ifdef _DEBUG
	void TriggerVolumeComponentBase::DrawVolumeInspectorImGui() {
		ImGui::DragFloat3("Local Center", &mLocalCenter.x, 1.0f);
		ImGui::DragFloat3("Extents HU", &mExtentsHu.x, 1.0f, 0.0f, 100000.0f);
	}
#endif
}
