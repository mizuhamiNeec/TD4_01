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

	void StaticMeshColliderComponent::OnAttached() {
		BaseComponent::OnAttached();
		RefreshPhysicsRegistration();
	}

	void StaticMeshColliderComponent::OnDetached() {
		UnregisterPhysicsMesh();
		BaseComponent::OnDetached();
	}

	void StaticMeshColliderComponent::OnTick(float deltaTime) {
		BaseComponent::OnTick(deltaTime);
		RefreshPhysicsRegistration();
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

	void StaticMeshColliderComponent::RefreshPhysicsRegistration() {
		if (!mEnabled) {
			UnregisterPhysicsMesh();
			return;
		}

		Entity* owner     = GetOwner();
		auto*   transform = owner ?
			                    owner->GetComponent<TransformComponent>() :
			                    nullptr;
		auto* meshRenderer =
			owner ?
				owner->GetComponent<StaticMeshRendererComponent>() :
				nullptr;
		auto*            assetManager = ServiceLocator::Get<AssetManager>();
		const auto*      engine       = EngineServices::Get();
		Physics::Engine* physics      =
			engine ? engine->GetPhysicsEngine() : nullptr;

		if (!transform || !meshRenderer || !assetManager || !physics) {
			UnregisterPhysicsMesh();
			return;
		}

		transform->OnTick(0.0f);
		const AssetID meshAssetId = meshRenderer->ResolveMeshAsset(
			*assetManager
		);
		const MeshAssetData* mesh = assetManager->Get<MeshAssetData>(
			meshAssetId
		);
		if (!mesh || mesh->vertices.empty() || mesh->indices.empty()) {
			UnregisterPhysicsMesh();
			return;
		}

		const Mat4 world = transform->WorldMat();
		if (
			mRegistered &&
			mRegisteredMeshAssetId == meshAssetId &&
			mRegisteredWorld == world
		) {
			return;
		}

		UnregisterPhysicsMesh();
		if (!owner) {
			return;
		}

		if (physics->RegisterStaticMesh(
			owner->GetGuid(),
			std::span<const MeshVertex>(
				mesh->vertices.data(), mesh->vertices.size()
			),
			std::span<const uint32_t>(
				mesh->indices.data(), mesh->indices.size()
			),
			world
		)) {
			mRegistered            = true;
			mRegisteredMeshAssetId = meshAssetId;
			mRegisteredWorld       = world;
		}
	}

	void StaticMeshColliderComponent::UnregisterPhysicsMesh() {
		if (!mRegistered) {
			return;
		}

		if (const Entity* owner = GetOwner()) {
			if (const Engine* engine = EngineServices::Get()) {
				if (Physics::Engine* physics = engine->GetPhysicsEngine()) {
					physics->UnregisterStaticMesh(owner->GetGuid());
				}
			}
		}

		mRegistered            = false;
		mRegisteredMeshAssetId = kInvalidAssetID;
		mRegisteredWorld       = Mat4::zero;
	}

	REGISTER_COMPONENT(JumpPadComponent);

	REGISTER_COMPONENT(SpeedBoostAreaComponent);

	REGISTER_COMPONENT(CheckpointComponent);

	REGISTER_COMPONENT(GoalComponent);

	REGISTER_COMPONENT(StaticMeshColliderComponent);
}
