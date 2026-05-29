#pragma once

#include "../ParticleModule.h"

// ===============================================
// LifetimeModule
// パーティクルの寿命を担当するモジュール。
//   ApplySpawn  : preset の lifeTime を寿命に設定し、粒子を有効化する
//   ApplyUpdate : 経過時間を進め、寿命を超えたら粒子を無効化する
//
// ※ 後続モジュールが「死んだ粒子」をスキップできるよう、
//   ParticleEmitterInstance はこのモジュールをモジュール列の先頭に置く。
// ===============================================
class LifetimeModule final : public ParticleModule {
public:
	const char* GetTypeName() const override { return "Lifetime"; }

	void ApplySpawn(ParticleEmitterInstance& emitter, Particle& p) override;
	void ApplyUpdate(ParticleEmitterInstance& emitter, Particle& p, float dt) override;
};
