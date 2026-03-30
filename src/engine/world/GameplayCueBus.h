#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace Unnamed {
	struct GameplayCue {
		std::string id;
		uint64_t    sourceEntityGuid = 0;
		float       value            = 0.0f;
		float       value2           = 0.0f;
	};

	struct GameplayCueFilter {
		std::string cueId;
		uint64_t    sourceEntityGuid = 0;
	};

	class GameplayCueBus final {
	public:
		using Handle   = uint64_t;
		using Callback = std::function<void(const GameplayCue&)>;

		[[nodiscard]] Handle Subscribe(
			const GameplayCueFilter& filter, Callback callback
		) {
			if (
				filter.cueId.empty() ||
				filter.sourceEntityGuid == 0 ||
				!callback
			) {
				return 0;
			}

			const Handle handle = mNextHandle++;
			mListeners.emplace_back(
				Listener{
					.handle   = handle,
					.filter   = filter,
					.callback = std::move(callback),
					.active   = true
				}
			);
			return handle;
		}

		bool Unsubscribe(const Handle handle) {
			if (handle == 0) {
				return false;
			}

			for (Listener& listener : mListeners) {
				if (listener.handle != handle || !listener.active) {
					continue;
				}

				listener.active = false;
				listener.callback = nullptr;
				PruneInactiveListeners();
				return true;
			}
			return false;
		}

		void Publish(const GameplayCue& cue) {
			if (cue.id.empty() || cue.sourceEntityGuid == 0) {
				return;
			}

			std::vector<Listener> snapshot;
			snapshot.reserve(mListeners.size());
			for (const Listener& listener : mListeners) {
				if (!listener.active || !listener.callback) {
					continue;
				}
				snapshot.emplace_back(listener);
			}

			mIsPublishing = true;
			for (const Listener& listener : snapshot) {
				if (!Matches(listener.filter, cue)) {
					continue;
				}
				listener.callback(cue);
			}
			mIsPublishing = false;
			PruneInactiveListeners();
		}

		void Clear() {
			mListeners.clear();
		}

	private:
		struct Listener {
			Handle            handle   = 0;
			GameplayCueFilter filter   = {};
			Callback          callback = nullptr;
			bool              active   = false;
		};

		[[nodiscard]] static bool Matches(
			const GameplayCueFilter& filter, const GameplayCue& cue
		) {
			return filter.sourceEntityGuid == cue.sourceEntityGuid &&
			       filter.cueId == cue.id;
		}

		void PruneInactiveListeners() {
			if (mIsPublishing) {
				return;
			}
			std::erase_if(
				mListeners,
				[](const Listener& listener) {
					return !listener.active || !listener.callback;
				}
			);
		}

		std::vector<Listener> mListeners;
		Handle                mNextHandle   = 1;
		bool                  mIsPublishing = false;
	};
}
