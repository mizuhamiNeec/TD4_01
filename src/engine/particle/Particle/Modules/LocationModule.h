#pragma once

#include "../ParticleModule.h"

// ===============================================
// LocationModule
// パーティクルの発生位置を決めるモジュール（発生時のみ）。
//   - World/Local Space の切り替え
//   - EmitterSpawn.useRandomPosition によるランダム発生
//   - ParticleSpawn.initialOffset（＋ランダム）
// ===============================================
class LocationModule final : public ParticleModule {
public:
	const char* GetTypeName() const override { return "Location"; }

	void ApplySpawn(ParticleEmitterInstance& emitter, Particle& p) override;
};
