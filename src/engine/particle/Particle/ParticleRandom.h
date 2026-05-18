#pragma once

#include "core/math/Vec3.h"

struct RandomRange3;

// ===============================================
// ParticleRandom
// パーティクルモジュールが共通で使う乱数ヘルパー。
// 以前は ParticleEmitterInstance.cpp 内の static 関数だったものを、
// 各モジュールから使えるよう独立させた。
// ===============================================
namespace ParticleRandom {

	// [minValue, maxValue] の一様乱数を返す。
	// min/max が逆でも落ちないように内部でスワップする。
	float Range(float minValue, float maxValue);

	// 各成分ごとに Range() を適用した Vec3 を返す。
	Vec3 RangeVec3(const Vec3& minValue, const Vec3& maxValue);

	// RandomRange3 を評価する。
	// useRandom が true なら範囲乱数、false なら fallback をそのまま返す。
	Vec3 Evaluate(const RandomRange3& range, const Vec3& fallback);

}
