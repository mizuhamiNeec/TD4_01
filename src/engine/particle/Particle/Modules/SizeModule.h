#pragma once

#include "../ParticleModule.h"

// ===============================================
// SizeModule
// パーティクルのスケールを担当するモジュール。
//   ApplySpawn  : 初期スケールとスケール速度を設定（ともにランダム対応）
//   ApplyUpdate : スケール速度ぶん変化させ、scaleCurve があれば倍率を上書き適用
// ===============================================
class SizeModule final : public ParticleModule {
public:
	const char* GetTypeName() const override { return "Size"; }

	void ApplySpawn(ParticleEmitterInstance& emitter, Particle& p) override;
	void ApplyUpdate(ParticleEmitterInstance& emitter, Particle& p, float dt) override;
};
