#include "TriggerComponents.h"

#include "core/ComponentRegistry.h"
#include "core/assets/AssetManager.h"
#include "core/assets/types/MeshAssetData.h"
#include "core/json/JsonReader.h"
#include "core/json/JsonWriter.h"
#include "core/math/Math.h"

#include "engine/Engine.h"
#include "engine/EngineServices.h"
#include "engine/physics/core/Physics.h"
#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/components/mesh/StaticMeshRendererComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"
#include "engine/world/World.h"

namespace Unnamed {
	Vec3 TriggerVolumeComponentBase::GetWorldCenter() const noexcept {
		if (const auto* transform = GetTransform()) {
			return transform->WorldMat().TransformPoint(mLocalCenter);
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

	void JumpPadComponent::Deserialize(const JsonReader& reader) {
		DeserializeVolume(reader);
		const JsonReader velocity = reader["boostVelocityHu"];
		if (velocity.Valid()) {
			mBoostVelocityHu = velocity.GetFloat();
		}
	}

	void JumpPadComponent::Serialize(JsonWriter& writer) const {
		SerializeVolume(writer);
		writer.Key("boostVelocityHu");
		writer.Write(mBoostVelocityHu);
	}

	void SpeedBoostAreaComponent::Deserialize(const JsonReader& reader) {
		DeserializeVolume(reader);
		const JsonReader multiplier = reader["multiplier"];
		const JsonReader duration   = reader["durationSec"];
		if (multiplier.Valid()) {
			mMultiplier = multiplier.GetFloat();
		}
		if (duration.Valid()) {
			mDurationSec = duration.GetFloat();
		}
	}

	void SpeedBoostAreaComponent::Serialize(JsonWriter& writer) const {
		SerializeVolume(writer);
		writer.Key("multiplier");
		writer.Write(mMultiplier);
		writer.Key("durationSec");
		writer.Write(mDurationSec);
	}

	void CheckpointComponent::Deserialize(const JsonReader& reader) {
		DeserializeVolume(reader);
		const JsonReader index = reader["index"];
		if (index.Valid()) {
			mIndex = index.GetInt();
		}

		mRespawnPosition = reader["respawnPosition"].GetVec3(mRespawnPosition);
	}

	void CheckpointComponent::Serialize(JsonWriter& writer) const {
		SerializeVolume(writer);
		writer.Key("index");
		writer.Write(mIndex);
		writer.WriteVec3("respawnPosition", mRespawnPosition);
	}

	void GoalComponent::Deserialize(const JsonReader& reader) {
		DeserializeVolume(reader);
	}

	void GoalComponent::Serialize(JsonWriter& writer) const {
		SerializeVolume(writer);
	}

	

	REGISTER_COMPONENT(JumpPadComponent);

	REGISTER_COMPONENT(SpeedBoostAreaComponent);

	REGISTER_COMPONENT(CheckpointComponent);

	REGISTER_COMPONENT(GoalComponent);
}
