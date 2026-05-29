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

	// 原点中心の直方体（各軸 ±halfExtents）内の一様乱数点を返す。
	Vec3 PointInBox(const Vec3& halfExtents);

	// 原点中心の球（半径 radius）内の一様乱数点を返す。
	Vec3 PointInSphere(float radius);

	// 円錐（頂点が原点、+Y 方向へ開く）内の一様乱数点を返す。
	Vec3 PointInCone(float baseRadius, float height);

	// 円柱（Y 軸が中心軸、原点中心）内の一様乱数点を返す。
	Vec3 PointInCylinder(float radius, float height);

	// 円/円盤（XZ 平面、原点中心）上の一様乱数点を返す。
	// edgeOnly が true なら外周上、false なら円盤内部に一様分布する。
	Vec3 PointInCircle(float radius, bool edgeOnly);

}
