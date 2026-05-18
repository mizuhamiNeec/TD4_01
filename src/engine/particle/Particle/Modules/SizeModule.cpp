#include "SizeModule.h"

#include "../Particle.h"
#include "../ParticleEmitterInstance.h"
#include "../ParticlePreset.h"
#include "../ParticleRandom.h"

void SizeModule::ApplySpawn(ParticleEmitterInstance& emitter, Particle& p)
{
	const ParticlePreset* preset = emitter.GetPreset();
	if (!preset) { return; }

	const auto& pSpawn = preset->particleSpawn;
	const auto& u = preset->particleUpdate;

	// --- 初期スケール（ランダム対応） ---
	p.scale = ParticleRandom::Evaluate(pSpawn.initialScaleRandom, pSpawn.initialScale);

	// カーブ適用前の基準スケールを控えておく
	p.initialScale = p.scale;

	// --- スケール速度（ランダム対応） ---
	p.scaleSpeed = ParticleRandom::Evaluate(u.scaleRandom, u.scaleSpeed);
}

void SizeModule::ApplyUpdate(ParticleEmitterInstance& emitter, Particle& p, float dt)
{
	const ParticlePreset* preset = emitter.GetPreset();
	if (!preset || !p.active) { return; }

	const auto& u = preset->particleUpdate;

	// --- スケール速度ぶん変化 ---
	p.scale.x += p.scaleSpeed.x * dt;
	p.scale.y += p.scaleSpeed.y * dt;
	p.scale.z += p.scaleSpeed.z * dt;

	// --- カーブスケール（倍率）。有効なら基準スケールへ倍率を適用して上書き ---
	if (u.scaleCurve.enabled && !u.scaleCurve.keys.empty()) {
		const float age = (p.maxLife > 0.0f) ? (p.life / p.maxLife) : 0.0f;
		const float s = u.scaleCurve.Evaluate(age);
		p.scale.x = p.initialScale.x * s;
		p.scale.y = p.initialScale.y * s;
		p.scale.z = p.initialScale.z * s;
	}
}
