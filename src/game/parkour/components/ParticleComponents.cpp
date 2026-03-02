#include "ParticleComponents.h"

#include <algorithm>

#include "MovementComponent.h"

#include "core/ComponentRegistry.h"
#include "core/assets/AssetManager.h"
#include "core/json/JsonReader.h"
#include "core/json/JsonWriter.h"
#include "core/math/Math.h"
#include "core/math/random/Random.h"

#include "engine/scene/UScene.h"
#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/entity/UEntity.h"
#include "engine/world/UWorld.h"

namespace Unnamed {
	namespace {
		Vec3 ReadVec3Or(
			const JsonReader& reader, const char* key, const Vec3& fallback
		) {
			const JsonReader value = reader[key];
			if (!value.Valid() || value.Size() < 3) { return fallback; }
			return Vec3(
				value[0].GetFloat(),
				value[1].GetFloat(),
				value[2].GetFloat()
			);
		}

		Vec2 ReadVec2Or(
			const JsonReader& reader, const char* key, const Vec2& fallback
		) {
			const JsonReader value = reader[key];
			if (!value.Valid() || value.Size() < 2) { return fallback; }
			return Vec2(value[0].GetFloat(), value[1].GetFloat());
		}

		float ReadFloatOr(
			const JsonReader& reader, const char* key, const float fallback
		) {
			const JsonReader value = reader[key];
			return value.Valid() ? value.GetFloat() : fallback;
		}

		bool ReadBoolOr(
			const JsonReader& reader, const char* key, const bool fallback
		) {
			const JsonReader value = reader[key];
			return value.Valid() ? value.GetBool() : fallback;
		}

		std::string ReadStringOr(
			const JsonReader& reader,
			const char*       key,
			const std::string& fallback
		) {
			const JsonReader value = reader[key];
			return value.Valid() ? value.GetString() : fallback;
		}

		void WriteVec2(JsonWriter& writer, const char* key, const Vec2& value) {
			writer.Key(key);
			writer.BeginArray();
			writer.Write(value.x);
			writer.Write(value.y);
			writer.EndArray();
		}

		void WriteVec3(JsonWriter& writer, const char* key, const Vec3& value) {
			writer.Key(key);
			writer.BeginArray();
			writer.Write(value.x);
			writer.Write(value.y);
			writer.Write(value.z);
			writer.EndArray();
		}

		Vec4 ReadVec4Or(
			const JsonReader& reader, const char* key, const Vec4& fallback
		) {
			const JsonReader value = reader[key];
			if (!value.Valid() || value.Size() < 4) { return fallback; }
			return Vec4(
				value[0].GetFloat(),
				value[1].GetFloat(),
				value[2].GetFloat(),
				value[3].GetFloat()
			);
		}
	}

	void ParticleEmitterComponent::OnTick(const float deltaTime) {
		for (Particle& particle : mParticles) {
			particle.age += deltaTime;
			particle.velocity += mAccelerationWorld * deltaTime;
			particle.position += particle.velocity * deltaTime;
		}
		std::erase_if(
			mParticles,
			[](const Particle& particle) { return particle.age >= particle.lifetime; }
		);

		for (const BurstRequest& request : mPendingBursts) {
			SpawnBurst(
				request.worldPosition,
				request.count != 0 ? request.count : mBurstCount
			);
		}
		mPendingBursts.clear();

		if (
			mEmissionMode == EmissionMode::Burst &&
			mPlayOnStart &&
			!mPlayedStartBurst
		) {
			Vec3 burstOrigin = Vec3::zero;
			if (TransformComponent* transform = GetTransform()) {
				Mat4 world = transform->WorldMat();
				burstOrigin = world.GetTranslate();
			}
			SpawnBurst(
				burstOrigin,
				mBurstCount
			);
			mPlayedStartBurst = true;
		}

		if (
			mEmissionMode != EmissionMode::Continuous ||
			!mEmissionEnabled ||
			!mLoop ||
			mSpawnRate <= 0.0f
		) {
			return;
		}

		mEmissionAccumulator += mSpawnRate * std::max(0.0f, mEmissionScale) *
		                        deltaTime;
		while (mEmissionAccumulator >= 1.0f) {
			mEmissionAccumulator -= 1.0f;
			SpawnOne(BuildSpawnPosition());
		}
	}

	void ParticleEmitterComponent::Deserialize(const JsonReader& reader) {
		mTexturePath = ReadStringOr(reader, "texturePath", mTexturePath);
		mMaxParticles = static_cast<uint32_t>(std::max(
			1,
			reader["maxParticles"].Valid() ?
				reader["maxParticles"].GetInt() :
				static_cast<int>(mMaxParticles)
		));
		const int modeValue = reader["emissionMode"].Valid() ?
			                      reader["emissionMode"].GetInt() :
			                      static_cast<int>(mEmissionMode);
		mEmissionMode = modeValue == 1 ?
			                EmissionMode::Burst :
			                EmissionMode::Continuous;
		mSpawnRate = ReadFloatOr(reader, "spawnRate", mSpawnRate);
		mBurstCount = static_cast<uint32_t>(std::max(
			1,
			reader["burstCount"].Valid() ?
				reader["burstCount"].GetInt() :
				static_cast<int>(mBurstCount)
		));
		mLifetimeMinSec = ReadFloatOr(reader, "lifetimeMinSec", mLifetimeMinSec);
		mLifetimeMaxSec = ReadFloatOr(reader, "lifetimeMaxSec", mLifetimeMaxSec);
		mStartSize = ReadVec2Or(reader, "startSize", mStartSize);
		mEndSize = ReadVec2Or(reader, "endSize", mEndSize);
		mStartColor = ReadVec4Or(reader, "startColor", mStartColor);
		mEndColor = ReadVec4Or(reader, "endColor", mEndColor);
		mVelocityMinLocal = ReadVec3Or(
			reader, "velocityMinLocal", mVelocityMinLocal
		);
		mVelocityMaxLocal = ReadVec3Or(
			reader, "velocityMaxLocal", mVelocityMaxLocal
		);
		mAccelerationWorld = ReadVec3Or(
			reader, "accelerationWorld", mAccelerationWorld
		);
		mSpawnBoxExtents = ReadVec3Or(reader, "spawnBoxExtents", mSpawnBoxExtents);
		mLoop = ReadBoolOr(reader, "loop", mLoop);
		mPlayOnStart = ReadBoolOr(reader, "playOnStart", mPlayOnStart);
	}

	void ParticleEmitterComponent::Serialize(JsonWriter& writer) const {
		writer.Key("texturePath");
		writer.Write(mTexturePath);
		writer.Key("maxParticles");
		writer.Write(static_cast<int>(mMaxParticles));
		writer.Key("emissionMode");
		writer.Write(static_cast<int>(mEmissionMode));
		writer.Key("spawnRate");
		writer.Write(mSpawnRate);
		writer.Key("burstCount");
		writer.Write(static_cast<int>(mBurstCount));
		writer.Key("lifetimeMinSec");
		writer.Write(mLifetimeMinSec);
		writer.Key("lifetimeMaxSec");
		writer.Write(mLifetimeMaxSec);
		WriteVec2(writer, "startSize", mStartSize);
		WriteVec2(writer, "endSize", mEndSize);
		writer.Key("startColor");
		writer.BeginArray();
		writer.Write(mStartColor.x);
		writer.Write(mStartColor.y);
		writer.Write(mStartColor.z);
		writer.Write(mStartColor.w);
		writer.EndArray();
		writer.Key("endColor");
		writer.BeginArray();
		writer.Write(mEndColor.x);
		writer.Write(mEndColor.y);
		writer.Write(mEndColor.z);
		writer.Write(mEndColor.w);
		writer.EndArray();
		WriteVec3(writer, "velocityMinLocal", mVelocityMinLocal);
		WriteVec3(writer, "velocityMaxLocal", mVelocityMaxLocal);
		WriteVec3(writer, "accelerationWorld", mAccelerationWorld);
		WriteVec3(writer, "spawnBoxExtents", mSpawnBoxExtents);
		writer.Key("loop");
		writer.Write(mLoop);
		writer.Key("playOnStart");
		writer.Write(mPlayOnStart);
	}

	void ParticleEmitterComponent::SetEmissionScale(const float scale) {
		mEmissionScale = scale;
	}

	void ParticleEmitterComponent::SetEmissionEnabled(const bool enabled) {
		mEmissionEnabled = enabled;
	}

	void ParticleEmitterComponent::EmitBurstAt(
		const Vec3& worldPosition,
		const uint32_t count
	) {
		BurstRequest request = {};
		request.worldPosition = worldPosition;
		request.count = count;
		mPendingBursts.emplace_back(request);
	}

	void ParticleEmitterComponent::AppendWorldBillboards(
		std::vector<Render::WorldBillboardInput>& out,
		AssetManager&                             assetManager
	) {
		if (mTextureAssetId == kInvalidAssetID) {
			mTextureAssetId = assetManager.LoadFromFile(
				mTexturePath,
				ASSET_TYPE::TEXTURE
			);
		}

		for (const Particle& particle : mParticles) {
			const float t = std::clamp(
				particle.age / std::max(0.001f, particle.lifetime),
				0.0f,
				1.0f
			);
			Render::WorldBillboardInput billboard = {};
			billboard.textureAssetId = mTextureAssetId;
			billboard.worldPosition = particle.position;
			billboard.sizeWorld = Math::Lerp(mStartSize, mEndSize, t);
			billboard.color = Math::Lerp(mStartColor, mEndColor, t);
			billboard.rotationRad = particle.rotation;
			out.emplace_back(billboard);
		}
	}

	TransformComponent* ParticleEmitterComponent::GetTransform() const {
		UEntity* owner = GetOwner();
		return owner ? owner->GetComponent<TransformComponent>() : nullptr;
	}

	Vec3 ParticleEmitterComponent::LocalToWorldDirection(const Vec3& local) const {
		if (const TransformComponent* transform = GetTransform()) {
			const Mat4 world = transform->WorldMat();
			return world.GetRight() * local.x +
			       world.GetUp() * local.y +
			       world.GetForward() * local.z;
		}
		return local;
	}

	Vec3 ParticleEmitterComponent::BuildSpawnPosition() const {
		Vec3 position = Vec3::zero;
		if (const TransformComponent* transform = GetTransform()) {
			Mat4 world = transform->WorldMat();
			position = world.GetTranslate();
		}
		const Vec3 localOffset = Random::Vec3Range(
			-mSpawnBoxExtents,
			mSpawnBoxExtents
		);
		return position + LocalToWorldDirection(localOffset);
	}

	void ParticleEmitterComponent::SpawnOne(const Vec3& worldPosition) {
		if (mParticles.size() >= mMaxParticles && !mParticles.empty()) {
			mParticles.erase(mParticles.begin());
		}
		Particle particle = {};
		particle.position = worldPosition;
		particle.velocity = LocalToWorldDirection(
			Random::Vec3Range(mVelocityMinLocal, mVelocityMaxLocal)
		);
		particle.lifetime = Random::FloatRange(
			std::min(mLifetimeMinSec, mLifetimeMaxSec),
			std::max(mLifetimeMinSec, mLifetimeMaxSec)
		);
		particle.rotation = Random::FloatRange(0.0f, Math::pi * 2.0f);
		mParticles.emplace_back(particle);
	}

	void ParticleEmitterComponent::SpawnBurst(
		const Vec3& worldPosition,
		const uint32_t count
	) {
		for (uint32_t i = 0; i < count; ++i) { SpawnOne(worldPosition); }
	}

	void MovementWindEmitterComponent::OnTick(const float) {
		MovementComponent* movement = ResolveMovement();
		ParticleEmitterComponent* emitter = ResolveEmitter();
		if (!movement || !emitter) { return; }

		const float speedHu = Math::MtoH(movement->GetVelocity().Length());
		if (speedHu < mSpeedThresholdHu) {
			emitter->SetEmissionEnabled(false);
			emitter->SetEmissionScale(0.0f);
			return;
		}

		const float speedRange = std::max(
			1.0f,
			mFullEmissionSpeedHu - mSpeedThresholdHu
		);
		const float normalized = std::clamp(
			(speedHu - mSpeedThresholdHu) / speedRange,
			0.0f,
			1.0f
		);
		emitter->SetEmissionEnabled(true);
		emitter->SetEmissionScale(std::lerp(
			mSpawnRateScaleMin,
			mSpawnRateScaleMax,
			normalized
		));
	}

	void MovementWindEmitterComponent::Deserialize(const JsonReader& reader) {
		mMovementEntityGuid = reader.ReadUint64("movementEntityGuid").value_or(
			mMovementEntityGuid
		);
		mSpeedThresholdHu = ReadFloatOr(
			reader, "speedThresholdHu", mSpeedThresholdHu
		);
		mFullEmissionSpeedHu = ReadFloatOr(
			reader, "fullEmissionSpeedHu", mFullEmissionSpeedHu
		);
		mSpawnRateScaleMin = ReadFloatOr(
			reader, "spawnRateScaleMin", mSpawnRateScaleMin
		);
		mSpawnRateScaleMax = ReadFloatOr(
			reader, "spawnRateScaleMax", mSpawnRateScaleMax
		);
	}

	void MovementWindEmitterComponent::Serialize(JsonWriter& writer) const {
		writer.Key("movementEntityGuid");
		writer.Write(mMovementEntityGuid);
		writer.Key("speedThresholdHu");
		writer.Write(mSpeedThresholdHu);
		writer.Key("fullEmissionSpeedHu");
		writer.Write(mFullEmissionSpeedHu);
		writer.Key("spawnRateScaleMin");
		writer.Write(mSpawnRateScaleMin);
		writer.Key("spawnRateScaleMax");
		writer.Write(mSpawnRateScaleMax);
	}

	MovementComponent* MovementWindEmitterComponent::ResolveMovement() const {
		UEntity* owner = GetOwner();
		if (!owner) { return nullptr; }
		UWorld* world = UWorld::GetTickingWorld();
		if (world && mMovementEntityGuid != 0) {
			if (UEntity* entity = world->GetScene().FindEntity(mMovementEntityGuid)) {
				return entity->GetComponent<MovementComponent>();
			}
		}
		return owner->GetComponent<MovementComponent>();
	}

	ParticleEmitterComponent* MovementWindEmitterComponent::ResolveEmitter() const {
		UEntity* owner = GetOwner();
		return owner ? owner->GetComponent<ParticleEmitterComponent>() : nullptr;
	}

	REGISTER_COMPONENT(ParticleEmitterComponent);
	REGISTER_COMPONENT(MovementWindEmitterComponent);
}
