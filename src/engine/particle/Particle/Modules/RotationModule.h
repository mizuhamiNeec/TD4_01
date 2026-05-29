#pragma once

#include "../ParticleModule.h"

// ===============================================
// RotationModule
// パーティクルの回転を担当するモジュール。
//   ApplySpawn  : 初期回転と回転速度を設定（ともにランダム対応）
//   ApplyUpdate : 回転速度ぶん回転を進める
// ===============================================
class RotationModule final : public ParticleModule {
public:
	const char* GetTypeName() const override { return "Rotation"; }

	void ApplySpawn(ParticleEmitterInstance& emitter, Particle& p) override;
	void ApplyUpdate(ParticleEmitterInstance& emitter, Particle& p, float dt) override;
};
