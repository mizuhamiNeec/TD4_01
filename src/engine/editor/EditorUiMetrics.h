#pragma once

namespace Unnamed {
	class EditorUiMetrics {
	public:
		void SetScale(const float scale) {
			mScale = scale;
		}

		[[nodiscard]] float Dp(const float value) const {
			return value * mScale;
		}

	private:
		float mScale = 1.0f;
	};
}
