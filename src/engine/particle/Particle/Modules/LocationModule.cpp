#include "LocationModule.h"

#include "../Particle.h"
#include "../ParticleEmitterInstance.h"
#include "../ParticlePreset.h"
#include "../ParticleRandom.h"

void LocationModule::ApplySpawn(ParticleEmitterInstance& emitter, Particle& p)
{
	const ParticlePreset* preset = emitter.GetPreset();
	if (!preset) { return; }

	const auto& spawn = preset->emitterSpawn;
	const auto& pSpawn = preset->particleSpawn;

	// Local Space の場合: 原点(0,0,0)からの相対座標
	// World Space の場合: エミッタのワールド座標が基準
	// ※ Mat4::GetTranslate() は非 const のためローカルコピーを介して取得する
	Mat4 emitterTransform = emitter.GetTransform();
	Vec3 pos = spawn.useLocalSpace
		? Vec3{ 0, 0, 0 }
		: emitterTransform.GetTranslate();

	// --- EmitterSpawn 側のランダム位置 ---
	if (spawn.useRandomPosition) {
		pos.x += ParticleRandom::Range(-1.0f, 1.0f);
		pos.y += ParticleRandom::Range(-1.0f, 1.0f);
		pos.z += ParticleRandom::Range(-1.0f, 1.0f);
	}

	// --- ParticleSpawn.initialOffset（ランダム対応） ---
	const Vec3 offset =
		ParticleRandom::Evaluate(pSpawn.initialOffsetRandom, pSpawn.initialOffset);
	pos.x += offset.x;
	pos.y += offset.y;
	pos.z += offset.z;

	p.position = pos;
}
