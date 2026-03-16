#include "SequencePlayer.h"

#include <algorithm>
#include <cmath>

#include "engine/tween/TweenEase.h"

namespace Unnamed {
	void SequencePlayer::SetAsset(std::shared_ptr<SequenceAsset> asset) {
		mAsset = std::move(asset);
		mCurrentTime = 0.0f;
		mLoopDirection = 1;
		mState = SEQUENCE_PLAYER_STATE::STOPPED;
	}

	const std::shared_ptr<SequenceAsset>& SequencePlayer::GetAsset() const {
		return mAsset;
	}

	void SequencePlayer::AddBinder(ISequenceBinder* binder) {
		if (!binder) {
			return;
		}
		if (std::find(mBinders.begin(), mBinders.end(), binder) != mBinders.end()) {
			return;
		}
		mBinders.emplace_back(binder);
	}

	void SequencePlayer::ClearBinders() {
		mBinders.clear();
	}

	void SequencePlayer::Play() {
		if (!mAsset) {
			return;
		}
		if (
			mState == SEQUENCE_PLAYER_STATE::COMPLETED &&
			mCurrentTime >= mAsset->GetLengthSeconds()
		) {
			mCurrentTime = 0.0f;
			mLoopDirection = 1;
			ApplyAtCurrentTime();
		}
		mState = SEQUENCE_PLAYER_STATE::PLAYING;
	}

	void SequencePlayer::Pause() {
		if (mState == SEQUENCE_PLAYER_STATE::PLAYING) {
			mState = SEQUENCE_PLAYER_STATE::PAUSED;
		}
	}

	void SequencePlayer::Stop() {
		mCurrentTime = 0.0f;
		mLoopDirection = 1;
		mState = SEQUENCE_PLAYER_STATE::STOPPED;
		ApplyAtCurrentTime();
	}

	void SequencePlayer::Seek(const float seconds) {
		if (!mAsset) {
			mCurrentTime = 0.0f;
			return;
		}
		const float duration = std::max(0.0f, mAsset->GetLengthSeconds());
		mCurrentTime = std::clamp(seconds, 0.0f, duration);
		ApplyAtCurrentTime();
	}

	void SequencePlayer::SetPlayRate(const float playRate) {
		mPlayRate = std::max(0.0f, playRate);
	}

	float SequencePlayer::GetPlayRate() const {
		return mPlayRate;
	}

	void SequencePlayer::SetLoopOverride(
		const bool enabled,
		const LOOP_TYPE loopType
	) {
		mLoopOverridden = true;
		mLoopEnabled    = enabled;
		mLoopType       = loopType;
	}

	void SequencePlayer::ClearLoopOverride() {
		mLoopOverridden = false;
	}

	float SequencePlayer::GetCurrentTime() const {
		return mCurrentTime;
	}

	SEQUENCE_PLAYER_STATE SequencePlayer::GetState() const {
		return mState;
	}

	bool SequencePlayer::IsPlaying() const {
		return mState == SEQUENCE_PLAYER_STATE::PLAYING;
	}

	void SequencePlayer::Tick(const float deltaTime) {
		if (mState != SEQUENCE_PLAYER_STATE::PLAYING || !mAsset) {
			return;
		}

		const float duration = std::max(0.0f, mAsset->GetLengthSeconds());
		if (duration <= 1e-6f) {
			mCurrentTime = 0.0f;
			ApplyAtCurrentTime();
			if (!ResolveLoopEnabled()) {
				mState = SEQUENCE_PLAYER_STATE::COMPLETED;
			}
			return;
		}

		float nextTime = mCurrentTime +
			deltaTime * mPlayRate * static_cast<float>(mLoopDirection);

		const bool loopEnabled = ResolveLoopEnabled();
		const LOOP_TYPE loopType = ResolveLoopType();

		if (!loopEnabled) {
			if (nextTime >= duration) {
				nextTime = duration;
				mState   = SEQUENCE_PLAYER_STATE::COMPLETED;
			}
			if (nextTime <= 0.0f) {
				nextTime = 0.0f;
				mState   = SEQUENCE_PLAYER_STATE::COMPLETED;
			}
		} else {
			for (int i = 0; i < 64; ++i) {
				if (nextTime >= 0.0f && nextTime <= duration) {
					break;
				}

				if (loopType == LOOP_TYPE::RESTART) {
					if (nextTime > duration) {
						nextTime -= duration;
					} else {
						nextTime += duration;
					}
				} else {
					if (nextTime > duration) {
						nextTime = duration - (nextTime - duration);
						mLoopDirection = -mLoopDirection;
					} else {
						nextTime = -nextTime;
						mLoopDirection = -mLoopDirection;
					}
				}
			}
			nextTime = std::clamp(nextTime, 0.0f, duration);
		}

		mCurrentTime = nextTime;
		ApplyAtCurrentTime();
	}

	bool SequencePlayer::EvaluateTrackValue(
		const SequenceTrack& track,
		const float          timeSeconds,
		float&               outValue
	) const {
		if (track.keyframes.empty()) {
			return false;
		}

		const auto& keys = track.keyframes;
		if (keys.size() == 1 || timeSeconds <= keys.front().time) {
			outValue = keys.front().value;
			return true;
		}
		if (timeSeconds >= keys.back().time) {
			outValue = keys.back().value;
			return true;
		}

		for (size_t i = 0; i + 1 < keys.size(); ++i) {
			const SequenceKeyframe& lhs = keys[i];
			const SequenceKeyframe& rhs = keys[i + 1];
			if (timeSeconds < lhs.time || timeSeconds > rhs.time) {
				continue;
			}

			const float duration = rhs.time - lhs.time;
			const float t        = duration > 1e-6f ?
				                       (timeSeconds - lhs.time) / duration :
				                       0.0f;
			const float eased    = TweenEase::Evaluate(lhs.ease, t);
			outValue             = lhs.value + (rhs.value - lhs.value) * eased;
			return true;
		}

		outValue = keys.back().value;
		return true;
	}

	void SequencePlayer::ApplyAtCurrentTime() const {
		if (!mAsset || mBinders.empty()) {
			return;
		}

		for (const SequenceTrack& track : mAsset->GetTracks()) {
			float value = 0.0f;
			if (!EvaluateTrackValue(track, mCurrentTime, value)) {
				continue;
			}

			for (ISequenceBinder* binder : mBinders) {
				if (!binder || !binder->SupportsTarget(track.binding.target)) {
					continue;
				}
				if (binder->ApplyValue(track.binding, value)) {
					break;
				}
			}
		}
	}

	bool SequencePlayer::ResolveLoopEnabled() const {
		return mLoopOverridden ? mLoopEnabled : (mAsset && mAsset->IsLoop());
	}

	LOOP_TYPE SequencePlayer::ResolveLoopType() const {
		if (mLoopOverridden) {
			return mLoopType;
		}
		return mAsset ? mAsset->GetLoopType() : LOOP_TYPE::RESTART;
	}
}
