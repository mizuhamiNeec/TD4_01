#include "TweenLerp.h"

#include "core/math/Math.h"
#include "core/math/Quaternion.h"

namespace Unnamed {
	float TweenLerp<float>::Evaluate(
		const float& startValue, const float& endValue, const float t
	) { return std::lerp(startValue, endValue, t); }

	double TweenLerp<double>::Evaluate(
		const double& startValue, const double& endValue, const float t
	) { return std::lerp(startValue, endValue, t); }

	Vec2 TweenLerp<Vec2>::Evaluate(
		const Vec2& startValue, const Vec2& endValue, const float t
	) { return Math::Lerp(startValue, endValue, t); }

	Vec3 TweenLerp<Vec3>::Evaluate(
		const Vec3& startValue, const Vec3& endValue, const float t
	) { return Math::Lerp(startValue, endValue, t); }

	Vec4 TweenLerp<Vec4>::Evaluate(
		const Vec4& startValue, const Vec4& endValue, const float t
	) { return Math::Lerp(startValue, endValue, t); }

	Quaternion TweenLerp<Quaternion>::Evaluate(
		const Quaternion& startValue, const Quaternion& endValue, const float t
	) { return Quaternion::Slerp(startValue, endValue, t); }
}
