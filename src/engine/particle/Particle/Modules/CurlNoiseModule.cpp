#include "CurlNoiseModule.h"

#include "CurlNoise.h"

#include "../Particle.h"
#include "../ParticleEmitterInstance.h"
#include "../ParticlePreset.h"

void CurlNoiseModule::ApplyUpdate(ParticleEmitterInstance& emitter, Particle& p, float dt)
{
	const ParticlePreset* preset = emitter.GetPreset();
	if (!preset || !p.active) { return; }

	const auto& n = preset->curlNoise;
	// enabled が false、または強さ 0 なら流れ場の計算ごとスキップ（コストゼロ）。
	if (!n.enabled || n.strength == 0.0f) { return; }

	// サンプル位置：空間スケールを frequency で調整し、
	// z を寿命で進めることで場を時間アニメーションさせる（流れて見える）。
	const Vec3 samplePos{
		p.position.x * n.frequency,
		p.position.y * n.frequency,
		p.position.z * n.frequency + p.life * n.speed
	};

	const Vec3 flow = curlnoise::Curl(samplePos);

	// 発散ゼロの流れ場で位置を直接移流させる。
	// 速度に足し込まない（慣性を乗せない）ことで、重力や inwardAccel と
	// 干渉して発散するのを防ぎ、安定して滑らかに揺れる。
	p.position.x += flow.x * n.strength * dt;
	p.position.y += flow.y * n.strength * dt;
	p.position.z += flow.z * n.strength * dt;
}
