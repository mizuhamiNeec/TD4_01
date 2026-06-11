#include "VelocityModule.h"

#include <cmath>

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

	// --- 中心への引き寄せ（吸い込み）---
	// inwardAccel > 0 のとき、発生中心へ向かう加速度を速度に加える。
	// 中心は LocationModule と同じ規約: ローカル空間なら原点、ワールド空間ならエミッタ位置。
	const float inwardAccel = preset->particleUpdate.inwardAccel;
	if (inwardAccel != 0.0f) {
		Mat4 emitterTransform = emitter.GetTransform();
		const Vec3 center = preset->emitterSpawn.useLocalSpace
			? Vec3{ 0.0f, 0.0f, 0.0f }
			: emitterTransform.GetTranslate();

		const float dx = center.x - p.position.x;
		const float dy = center.y - p.position.y;
		const float dz = center.z - p.position.z;
		const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
		if (dist > 1e-4f) {
			const float inv = 1.0f / dist;
			p.velocity.x += dx * inv * inwardAccel * dt;
			p.velocity.y += dy * inv * inwardAccel * dt;
			p.velocity.z += dz * inv * inwardAccel * dt;
		}
	}
}
