#pragma once

#include "../ParticleModule.h"

// ===============================================
// CurlNoiseModule
// カールノイズ（発散ゼロの流れ場）で粒子位置を移流させ、
// 煙・塵のような滑らかな揺らぎを与えるモジュール。
//   ApplyUpdate : curlNoise.enabled かつ strength!=0 のときだけ位置を流す
// パラメータは ParticlePreset::curlNoise (CurlNoiseModuleSettings) を参照。
// ===============================================
class CurlNoiseModule final : public ParticleModule {
public:
	const char* GetTypeName() const override { return "CurlNoise"; }

	void ApplyUpdate(ParticleEmitterInstance& emitter, Particle& p, float dt) override;
};
