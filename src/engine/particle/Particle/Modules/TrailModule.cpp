#include "TrailModule.h"

#include "../Particle.h"
#include "../ParticleEmitterInstance.h"
#include "../ParticlePreset.h"

#include <algorithm>

void TrailModule::ApplySpawn(ParticleEmitterInstance& emitter, Particle& p)
{
	const ParticlePreset* preset = emitter.GetPreset();
	if (!preset || !preset->trail.enabled) {
		p.trailPoints.clear();
		return;
	}

	// 発生位置を履歴の先頭として登録する。
	const int maxPoints = std::max(2, preset->trail.maxPoints);

	p.trailPoints.clear();
	p.trailPoints.reserve(static_cast<size_t>(maxPoints));
	p.trailPoints.push_back(p.position);
	p.trailTimer = 0.0f;
}

void TrailModule::ApplyUpdate(ParticleEmitterInstance& emitter, Particle& p, float dt)
{
	const ParticlePreset* preset = emitter.GetPreset();
	if (!preset || !preset->trail.enabled) { return; }
	if (!p.active) { return; }

	const TrailModuleSettings& trail = preset->trail;
	const int maxPoints = std::max(2, trail.maxPoints);

	// 履歴へ 1 点追加し、上限を超えた古い点を捨てるヘルパー。
	const auto pushPoint = [&]() {
		p.trailPoints.push_back(p.position);
		if (static_cast<int>(p.trailPoints.size()) > maxPoints) {
			p.trailPoints.erase(p.trailPoints.begin());
		}
	};

	if (trail.recordInterval <= 0.0f) {
		// 間隔 0 以下なら毎フレーム記録する。
		pushPoint();
		return;
	}

	// recordInterval 経過ごとに記録する。
	p.trailTimer += dt;
	// 1 フレームで大量に溜め込まないよう、追加回数は maxPoints までに制限する。
	int guard = maxPoints;
	while (p.trailTimer >= trail.recordInterval && guard-- > 0) {
		p.trailTimer -= trail.recordInterval;
		pushPoint();
	}
}
