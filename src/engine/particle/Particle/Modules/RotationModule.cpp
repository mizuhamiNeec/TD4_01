#include "RotationModule.h"

#include "../Particle.h"
#include "../ParticleEmitterInstance.h"
#include "../ParticlePreset.h"
#include "../ParticleRandom.h"

void RotationModule::ApplySpawn(ParticleEmitterInstance& emitter, Particle& p)
{
	const ParticlePreset* preset = emitter.GetPreset();
	if (!preset) { return; }

	const auto& pSpawn = preset->particleSpawn;
	const auto& u = preset->particleUpdate;

	// --- 初期回転（ランダム対応） ---
	p.rotation = ParticleRandom::Evaluate(pSpawn.initialRotateRandom, pSpawn.initialRotate);

	// --- 回転速度（ランダム対応） ---
	p.rotationSpeed = ParticleRandom::Evaluate(u.rotationRandom, u.rotationSpeed);
}

void RotationModule::ApplyUpdate(ParticleEmitterInstance& emitter, Particle& p, float dt)
{
	(void)emitter;
	if (!p.active) { return; }

	p.rotation.x += p.rotationSpeed.x * dt;
	p.rotation.y += p.rotationSpeed.y * dt;
	p.rotation.z += p.rotationSpeed.z * dt;
}
