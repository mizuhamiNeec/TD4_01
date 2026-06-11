#pragma once

#include <algorithm>
#include <cstddef>

#include "core/assets/types/SequenceAssetData.h"

namespace Unnamed {
	/// @brief RichCurveを指定フレームで評価します。
	[[nodiscard]] inline float EvaluateSequenceRichCurve(
		const SequenceRichCurveAssetData& curve,
		const float                       frame,
		const float                       fallback = 0.0f
	) {
		if (curve.keys.empty()) {
			return fallback;
		}
		if (curve.keys.size() == 1) {
			return curve.keys.front().value;
		}

		if (frame <= static_cast<float>(curve.keys.front().frame)) {
			return curve.keys.front().value;
		}
		if (frame >= static_cast<float>(curve.keys.back().frame)) {
			return curve.keys.back().value;
		}

		for (size_t i = 0; i + 1 < curve.keys.size(); ++i) {
			const SequenceFloatKeyAssetData& lhs = curve.keys[i];
			const SequenceFloatKeyAssetData& rhs = curve.keys[i + 1];
			const float lhsFrame = static_cast<float>(lhs.frame);
			const float rhsFrame = static_cast<float>(rhs.frame);
			if (frame < lhsFrame || frame > rhsFrame) {
				continue;
			}

			const float segmentFrames = std::max(1.0f, rhsFrame - lhsFrame);
			const float t = std::clamp(
				(frame - lhsFrame) / segmentFrames,
				0.0f,
				1.0f
			);

			switch (lhs.interpolation) {
				case SEQUENCE_INTERPOLATION_MODE::MODE_STEP:
					return lhs.value;
				case SEQUENCE_INTERPOLATION_MODE::MODE_LINEAR:
					return lhs.value + (rhs.value - lhs.value) * t;
				case SEQUENCE_INTERPOLATION_MODE::MODE_CUBIC: {
					const float t2 = t * t;
					const float t3 = t2 * t;
					const float h00 = 2.0f * t3 - 3.0f * t2 + 1.0f;
					const float h10 = t3 - 2.0f * t2 + t;
					const float h01 = -2.0f * t3 + 3.0f * t2;
					const float h11 = t3 - t2;
					return h00 * lhs.value +
					       h10 * lhs.leaveTangent * segmentFrames +
					       h01 * rhs.value +
					       h11 * rhs.arriveTangent * segmentFrames;
				}
				case SEQUENCE_INTERPOLATION_MODE::MODE_SPLINE: {
					const float p0 = i > 0 ? curve.keys[i - 1].value : lhs.value;
					const float p1 = lhs.value;
					const float p2 = rhs.value;
					const float p3 = i + 2 < curve.keys.size() ?
						                 curve.keys[i + 2].value :
						                 rhs.value;
					const float t2 = t * t;
					const float t3 = t2 * t;
					return 0.5f * (
						(2.0f * p1) +
						(-p0 + p2) * t +
						(2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
						(-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3
					);
				}
			}
		}

		return curve.keys.back().value;
	}
}
