#pragma once

#include "../ParticleModule.h"

// ===============================================
// VelocityModule
// パーティクルの速度と移動を担当するモジュール。
//   ApplySpawn  : ParticleUpdate.velocity（＋ランダム）で初速度を設定
//   ApplyUpdate : 速度ぶん位置を進め、useGravity なら重力を加算
// ===============================================
class VelocityModule final : public ParticleModule {
public:
	const char* GetTypeName() const override { return "Velocity"; }

	void ApplySpawn(ParticleEmitterInstance& emitter, Particle& p) override;
	void ApplyUpdate(ParticleEmitterInstance& emitter, Particle& p, float dt) override;
};
