#include "ColorModule.h"

#include "../Particle.h"
#include "../ParticleEmitterInstance.h"
#include "../ParticlePreset.h"
#include "../ParticleRandom.h"

#include <algorithm>

void ColorModule::ApplySpawn(ParticleEmitterInstance& emitter, Particle& p)
{
	const ParticlePreset* preset = emitter.GetPreset();
	if (!preset) { return; }

	// カラー初期値（カーブ適用前の基準色も控える）
	// colorPalette が指定されていればパーティクルごとにランダムで1色選ぶ。
	const auto& palette = preset->render.colorPalette;
	if (!palette.empty()) {
		const int count = static_cast<int>(palette.size());
		int index = static_cast<int>(ParticleRandom::Range(0.0f, static_cast<float>(count)));
		index = std::clamp(index, 0, count - 1);
		p.color = palette[static_cast<size_t>(index)];
	} else {
		p.color = preset->render.color;
	}
	p.initialColor = p.color;
}

void ColorModule::ApplyUpdate(ParticleEmitterInstance& emitter, Particle& p, float dt)
{
	(void)dt;
	const ParticlePreset* preset = emitter.GetPreset();
	if (!preset || !p.active) { return; }

	const auto& renderM = preset->render;
	const float age = (p.maxLife > 0.0f) ? (p.life / p.maxLife) : 0.0f;

	// 1) グラデーション用の t を決める（時間カーブ）
	float gradT = age;
	if (renderM.gradientTimeCurve.enabled && !renderM.gradientTimeCurve.keys.empty()) {
		gradT = renderM.gradientTimeCurve.Evaluate(age);
	}
	gradT = std::clamp(gradT, 0.0f, 1.0f);

	// 2) グラデーションから「ベース色」を取得
	Vec4 col = p.initialColor;
	if (renderM.colorGradient.enabled && !renderM.colorGradient.keys.empty()) {
		col = renderM.colorGradient.Evaluate(gradT);
	}

	// 3) colorCurve で色の強さ/αを時間でコントロール
	if (renderM.colorCurve.enabled && !renderM.colorCurve.keys.empty()) {
		const float c = renderM.colorCurve.Evaluate(age);
		col.x *= c;
		col.y *= c;
		col.z *= c;
		col.w *= c;
	}

	p.color = col;
}
