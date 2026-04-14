#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "SequenceAsset.h"

namespace Unnamed {
	enum class SEQUENCE_PLAYER_STATE : uint8_t {
		STOPPED   = 0,
		PLAYING   = 1,
		PAUSED    = 2,
		COMPLETED = 3,
	};

	class SequencePlayer {
	public:
		void SetAsset(std::shared_ptr<SequenceAsset> asset);
		[[nodiscard]] const std::shared_ptr<SequenceAsset>& GetAsset() const;

		void AddBinder(ISequenceBinder* binder);
		void ClearBinders();

		void Play();
		void Pause();
		void Stop();
		void Seek(float seconds);

		void SetPlayRate(float playRate);
		[[nodiscard]] float GetPlayRate() const;

		void SetLoopOverride(bool enabled, LOOP_TYPE loopType);
		void ClearLoopOverride();

		[[nodiscard]] float GetCurrentTime() const;
		[[nodiscard]] SEQUENCE_PLAYER_STATE GetState() const;
		[[nodiscard]] bool IsPlaying() const;

		void Tick(float deltaTime);

	private:
		[[nodiscard]] bool EvaluateTrackValue(
			const SequenceTrack& track, float timeSeconds, float& outValue
		) const;
		void ApplyAtCurrentTime() const;
		[[nodiscard]] bool ResolveLoopEnabled() const;
		[[nodiscard]] LOOP_TYPE ResolveLoopType() const;

		std::shared_ptr<SequenceAsset> mAsset;
		std::vector<ISequenceBinder*>  mBinders;

		SEQUENCE_PLAYER_STATE mState          = SEQUENCE_PLAYER_STATE::STOPPED;
		float                 mCurrentTime    = 0.0f;
		float                 mPlayRate       = 1.0f;
		int32_t               mLoopDirection  = 1;
		bool                  mLoopOverridden = false;
		bool                  mLoopEnabled    = false;
		LOOP_TYPE             mLoopType       = LOOP_TYPE::RESTART;
	};
}
