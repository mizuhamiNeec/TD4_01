#pragma once

#include <string>
#include <vector>

#include "engine/render/frame/RenderFrameInputs.h"
#include "engine/unnamed/framework/components/base/UBaseComponent.h"

#include "core/assets/AssetID.h"
#include "core/math/Vec2.h"
#include "core/math/Vec3.h"
#include "core/math/Vec4.h"

namespace Unnamed {
	class AssetManager;
	class JsonReader;
	class JsonWriter;
	class MovementComponent;
	class TransformComponent;

	class ParticleEmitterComponent final : public UBaseComponent {
	public:
		enum class EmissionMode : uint8_t {
			Continuous = 0,
			Burst = 1,
		};

		struct BurstRequest {
			Vec3     worldPosition = Vec3::zero;
			uint32_t count = 0;
		};

		[[nodiscard]] std::string_view GetStableName() const override {
			return "parkour.ParticleEmitter";
		}

		[[nodiscard]] std::string_view GetComponentName() const override {
			return "ParticleEmitter";
		}

		void OnTick(float deltaTime) override;
		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;

		void SetEmissionScale(float scale);
		void SetEmissionEnabled(bool enabled);
		void EmitBurstAt(const Vec3& worldPosition, uint32_t count = 0);
		void AppendWorldBillboards(
			std::vector<Render::WorldBillboardInput>& out,
			AssetManager&                             assetManager
		);

		[[nodiscard]] EmissionMode GetEmissionMode() const noexcept {
			return mEmissionMode;
		}

	private:
		struct Particle {
			Vec3  position = Vec3::zero;
			Vec3  velocity = Vec3::zero;
			float age = 0.0f;
			float lifetime = 1.0f;
			float rotation = 0.0f;
		};

		[[nodiscard]] TransformComponent* GetTransform() const;
		[[nodiscard]] Vec3 LocalToWorldDirection(const Vec3& local) const;
		[[nodiscard]] Vec3 BuildSpawnPosition() const;
		void SpawnOne(const Vec3& worldPosition);
		void SpawnBurst(const Vec3& worldPosition, uint32_t count);

		std::vector<Particle>     mParticles;
		std::vector<BurstRequest> mPendingBursts;
		AssetID                   mTextureAssetId = kInvalidAssetID;
		float                     mEmissionAccumulator = 0.0f;
		bool                      mPlayedStartBurst = false;
		bool                      mEmissionEnabled = true;
		float                     mEmissionScale = 1.0f;

		std::string mTexturePath = "./content/core/textures/circle.png";
		uint32_t    mMaxParticles = 128;
		EmissionMode mEmissionMode = EmissionMode::Continuous;
		float mSpawnRate = 48.0f;
		uint32_t mBurstCount = 32;
		float mLifetimeMinSec = 0.2f;
		float mLifetimeMaxSec = 0.5f;
		Vec2  mStartSize = Vec2(0.18f, 0.18f);
		Vec2  mEndSize = Vec2(0.02f, 0.02f);
		Vec4  mStartColor = Vec4::one;
		Vec4  mEndColor = Vec4(1.0f, 1.0f, 1.0f, 0.0f);
		Vec3  mVelocityMinLocal = Vec3(-0.1f, -0.1f, 0.3f);
		Vec3  mVelocityMaxLocal = Vec3(0.1f, 0.1f, 0.6f);
		Vec3  mAccelerationWorld = Vec3::zero;
		Vec3  mSpawnBoxExtents = Vec3::zero;
		bool  mLoop = true;
		bool  mPlayOnStart = true;
	};

	class MovementWindEmitterComponent final : public UBaseComponent {
	public:
		[[nodiscard]] std::string_view GetStableName() const override {
			return "parkour.MovementWindEmitter";
		}

		[[nodiscard]] std::string_view GetComponentName() const override {
			return "MovementWindEmitter";
		}

		void OnTick(float deltaTime) override;
		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;

	private:
		[[nodiscard]] MovementComponent* ResolveMovement() const;
		[[nodiscard]] ParticleEmitterComponent* ResolveEmitter() const;

		uint64_t mMovementEntityGuid = 0;
		float    mSpeedThresholdHu = 80.0f;
		float    mFullEmissionSpeedHu = 400.0f;
		float    mSpawnRateScaleMin = 0.2f;
		float    mSpawnRateScaleMax = 1.0f;
	};
}
