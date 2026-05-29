#pragma once

#include "../ParticleModule.h"

// ===============================================
// TrailModule
// 各パーティクルの移動軌跡（過去位置の履歴）を記録するモジュール。
//   - ApplySpawn  : 履歴をクリアして発生位置を 1 点登録
//   - ApplyUpdate : recordInterval ごとに現在位置を履歴へ追加し、
//                   maxPoints を超えた古い点を捨てる
//
// 記録した履歴（Particle::trailPoints）は描画側
// （ParticleEmitterComponent::GatherWorldParticles）が読み取り、
// 履歴点ごとにビルボード板ポリを並べてトレイルとして描画する。
//
// preset->trail.enabled が false のときは何もしない（既存プリセットに無害）。
// ===============================================
class TrailModule final : public ParticleModule {
public:
	const char* GetTypeName() const override { return "Trail"; }

	void ApplySpawn(ParticleEmitterInstance& emitter, Particle& p) override;
	void ApplyUpdate(ParticleEmitterInstance& emitter, Particle& p, float dt) override;
};
