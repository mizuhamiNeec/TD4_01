#pragma once

#include "../ParticleModule.h"

// ===============================================
// ColorModule
// パーティクルの色を担当するモジュール。
//   ApplySpawn  : Render.color を初期色として設定
//   ApplyUpdate : グラデーション + 時間カーブ + 強さカーブで色を更新
// ===============================================
class ColorModule final : public ParticleModule {
public:
	const char* GetTypeName() const override { return "Color"; }

	void ApplySpawn(ParticleEmitterInstance& emitter, Particle& p) override;
	void ApplyUpdate(ParticleEmitterInstance& emitter, Particle& p, float dt) override;
};
