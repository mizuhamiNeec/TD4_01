#include "ParticleRandom.h"

#include "ParticlePreset.h" // struct RandomRange3

#include <algorithm>
#include <random>

namespace {
	// 1つの乱数エンジンを使い回す
	std::mt19937& Rng()
	{
		static std::mt19937 s_rng(std::random_device{}());
		return s_rng;
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
