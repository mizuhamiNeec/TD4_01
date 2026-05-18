#include "LifetimeModule.h"

#include "../Particle.h"
#include "../ParticleEmitterInstance.h"
#include "../ParticlePreset.h"

void LifetimeModule::ApplySpawn(ParticleEmitterInstance& emitter, Particle& p)
{
	const ParticlePreset* preset = emitter.GetPreset();
	if (!preset) { return; }

	p.life = 0.0f;
	p.maxLife = preset->particleUpdate.lifeTime;
	p.active = true;
}

void LifetimeModule::ApplyUpdate(ParticleEmitterInstance& emitter, Particle& p, float dt)
{
	(void)emitter;
	if (!p.active) { return; }

	p.life += dt;
	if (p.life >= p.maxLife) {
		p.active = false;
	}
}
