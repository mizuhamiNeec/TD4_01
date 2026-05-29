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

	// --- EmitterSpawn 側のランダム位置（発生形状に応じて分布させる） ---
	if (spawn.useRandomPosition) {
		Vec3 shapeOffset{ 0, 0, 0 };
		switch (spawn.emitShape) {
		case EmitShapeType::Sphere:
			shapeOffset = ParticleRandom::PointInSphere(spawn.sphereRadius);
			break;
		case EmitShapeType::Cone:
			shapeOffset = ParticleRandom::PointInCone(
				spawn.coneRadius, spawn.coneHeight);
			break;
		case EmitShapeType::Cylinder:
			shapeOffset = ParticleRandom::PointInCylinder(
				spawn.cylinderRadius, spawn.cylinderHeight);
			break;
		case EmitShapeType::Circle:
			shapeOffset = ParticleRandom::PointInCircle(
				spawn.circleRadius, spawn.circleEmitFromEdge);
			break;
		case EmitShapeType::Box:
		default:
			shapeOffset = ParticleRandom::PointInBox(spawn.boxHalfSize);
			break;
		}
		pos.x += shapeOffset.x;
		pos.y += shapeOffset.y;
		pos.z += shapeOffset.z;
	}

	// --- ParticleSpawn.initialOffset（ランダム対応） ---
	const Vec3 offset =
		ParticleRandom::Evaluate(pSpawn.initialOffsetRandom, pSpawn.initialOffset);
	pos.x += offset.x;
	pos.y += offset.y;
	pos.z += offset.z;

	p.position = pos;
}
