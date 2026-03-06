#include "TriggerComponents.h"

#include "core/ComponentRegistry.h"
#include "core/json/JsonReader.h"
#include "core/json/JsonWriter.h"
#include "core/math/Math.h"

#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/entity/UEntity.h"

namespace Unnamed {
	namespace {
		Vec3 ReadVec3Or(
			const JsonReader& reader, const char* key, const Vec3& fallback
		) {
			const JsonReader value = reader[key];
			if (!value.Valid() || value.Size() < 3) {
				return fallback;
			}
			return Vec3(
				value[0].GetFloat(),
				value[1].GetFloat(),
				value[2].GetFloat()
			);
		}

		void WriteVec3(JsonWriter& writer, const char* key, const Vec3& value) {
			writer.Key(key);
			writer.BeginArray();
			writer.Write(value.x);
			writer.Write(value.y);
			writer.Write(value.z);
			writer.EndArray();
		}
	}

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
		mLocalCenter = ReadVec3Or(reader, "localCenter", mLocalCenter);
		mExtentsHu   = ReadVec3Or(reader, "extentsHu", mExtentsHu);
	}

	void TriggerVolumeComponentBase::SerializeVolume(JsonWriter& writer) const {
		WriteVec3(writer, "localCenter", mLocalCenter);
		WriteVec3(writer, "extentsHu", mExtentsHu);
	}

	TransformComponent* TriggerVolumeComponentBase::GetTransform() const {
		UEntity* owner = GetOwner();
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
		mRespawnPosition = ReadVec3Or(
			reader, "respawnPosition", mRespawnPosition
		);
	}

	void CheckpointComponent::Serialize(JsonWriter& writer) const {
		SerializeVolume(writer);
		writer.Key("index");
		writer.Write(mIndex);
		WriteVec3(writer, "respawnPosition", mRespawnPosition);
	}

	void GoalComponent::Deserialize(const JsonReader& reader) {
		DeserializeVolume(reader);
	}

	void GoalComponent::Serialize(JsonWriter& writer) const {
		SerializeVolume(writer);
	}

	void StaticMeshColliderComponent::Deserialize(const JsonReader& reader) {
		const JsonReader enabled = reader["enabled"];
		if (enabled.Valid()) {
			mEnabled = enabled.GetBool();
		}
	}

	void StaticMeshColliderComponent::Serialize(JsonWriter& writer) const {
		writer.Key("enabled");
		writer.Write(mEnabled);
	}

	REGISTER_COMPONENT(JumpPadComponent);

	REGISTER_COMPONENT(SpeedBoostAreaComponent);

	REGISTER_COMPONENT(CheckpointComponent);

	REGISTER_COMPONENT(GoalComponent);

	REGISTER_COMPONENT(StaticMeshColliderComponent);
}
