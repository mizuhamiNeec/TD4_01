#include "ParticleRandom.h"

#include "ParticlePreset.h" // struct RandomRange3

#include <algorithm>
#include <cmath>
#include <random>

namespace {
	// 1つの乱数エンジンを使い回す
	std::mt19937& Rng()
	{
		static std::mt19937 s_rng(std::random_device{}());
		return s_rng;
	}

	constexpr float kTwoPi = 6.28318530718f;

	// XZ 平面・原点中心の円盤内（または外周上）の一様乱数点を返す。
	Vec3 SampleDiscXZ(float radius, bool edgeOnly)
	{
		const float theta = ParticleRandom::Range(0.0f, kTwoPi);
		// 内部一様にするには面積で重み付けが必要なので sqrt を掛ける。
		// 外周のみの場合は半径そのまま。
		const float r = edgeOnly
			? radius
			: radius * std::sqrt(ParticleRandom::Range(0.0f, 1.0f));
		return Vec3{ r * std::cos(theta), 0.0f, r * std::sin(theta) };
	}
}

float ParticleRandom::Range(float minValue, float maxValue)
{
	// ユーザーが min/max を逆に入れても落ちないようにする
	if (maxValue < minValue) {
		std::swap(minValue, maxValue);
	}

	// min == max の場合は、その値をそのまま返す
	if (minValue == maxValue) {
		return minValue;
	}

	std::uniform_real_distribution<float> dist(minValue, maxValue);
	return dist(Rng());
}

Vec3 ParticleRandom::RangeVec3(const Vec3& minValue, const Vec3& maxValue)
{
	return Vec3{
		Range(minValue.x, maxValue.x),
		Range(minValue.y, maxValue.y),
		Range(minValue.z, maxValue.z),
	};
}

Vec3 ParticleRandom::Evaluate(const RandomRange3& range, const Vec3& fallback)
{
	if (!range.useRandom) {
		return fallback;
	}
	return RangeVec3(range.minValue, range.maxValue);
}

Vec3 ParticleRandom::PointInBox(const Vec3& halfExtents)
{
	return Vec3{
		Range(-halfExtents.x, halfExtents.x),
		Range(-halfExtents.y, halfExtents.y),
		Range(-halfExtents.z, halfExtents.z),
	};
}

Vec3 ParticleRandom::PointInSphere(float radius)
{
	// 棄却法: 単位立方体内の点を、単位球内に入るまで引き直す。
	// 球内一様分布になり、表面に偏らない。
	for (int i = 0; i < 32; ++i) {
		const Vec3 p{
			Range(-1.0f, 1.0f),
			Range(-1.0f, 1.0f),
			Range(-1.0f, 1.0f),
		};
		if (p.x * p.x + p.y * p.y + p.z * p.z <= 1.0f) {
			return Vec3{ p.x * radius, p.y * radius, p.z * radius };
		}
	}
	// 念のためのフォールバック（ほぼ到達しない）
	return Vec3{ 0.0f, 0.0f, 0.0f };
}

Vec3 ParticleRandom::PointInCone(float baseRadius, float height)
{
	if (height <= 0.0f) {
		// 高さが無ければ頂点位置の 1 点のみ
		return Vec3{ 0.0f, 0.0f, 0.0f };
	}
	// 円錐は下ほど断面積が大きいので、高さを cbrt で重み付けすると
	// 体積一様分布になる（頂点付近に偏らない）。
	const float t    = std::cbrt(Range(0.0f, 1.0f)); // 0..1
	const float rAtH = baseRadius * t;               // その高さでの半径
	Vec3        p    = SampleDiscXZ(rAtH, false);
	p.y = height * t;                                // 頂点(原点)から +Y へ
	return p;
}

Vec3 ParticleRandom::PointInCylinder(float radius, float height)
{
	Vec3 p = SampleDiscXZ(radius, false);
	p.y = Range(-height * 0.5f, height * 0.5f);      // 原点中心
	return p;
}

Vec3 ParticleRandom::PointInCircle(float radius, bool edgeOnly)
{
	return SampleDiscXZ(radius, edgeOnly);
}
