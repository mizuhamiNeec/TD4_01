#include "VelocityModule.h"

#include "../Particle.h"
#include "../ParticleEmitterInstance.h"
#include "../ParticlePreset.h"
#include "../ParticleRandom.h"

namespace {
	// 重力加速度（従来の ParticleEmitterInstance の挙動に合わせる）
	constexpr float kGravity = -9.81f;
}

void VelocityModule::ApplySpawn(ParticleEmitterInstance& emitter, Particle& p)
{
	const ParticlePreset* preset = emitter.GetPreset();
	if (!preset) { return; }

	const auto& u = preset->particleUpdate;
	p.velocity = ParticleRandom::Evaluate(u.velocityRandom, u.velocity);
}

void VelocityModule::ApplyUpdate(ParticleEmitterInstance& emitter, Particle& p, float dt)
{
	const ParticlePreset* preset = emitter.GetPreset();
	if (!preset || !p.active) { return; }

	// --- 移動 ---
	p.position.x += p.velocity.x * dt;
	p.position.y += p.velocity.y * dt;
	p.position.z += p.velocity.z * dt;

	// --- 重力 ---
	if (preset->particleUpdate.useGravity) {
		p.velocity.y += kGravity * dt;
	}
}
