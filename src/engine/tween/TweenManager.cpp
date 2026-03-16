#include "TweenManager.h"

namespace Unnamed {
	void TweenManager::Update(const float deltaTime) {
		for (const auto& tween : mTweens) {
			if (!tween) {
				continue;
			}

			tween->Update(deltaTime);
		}

		std::erase_if(
			mTweens,
			[](const std::shared_ptr<ITweenPlayable>& tween) {
				return !tween || !tween->IsAlive();
			}
		);
	}

	void TweenManager::KillAll(const bool complete) {
		for (const auto& tween : mTweens) {
			if (!tween) {
				continue;
			}

			tween->Kill(complete);
		}

		std::erase_if(
			mTweens,
			[](const std::shared_ptr<ITweenPlayable>& tween) {
				return !tween || !tween->IsAlive();
			}
		);
	}
}
