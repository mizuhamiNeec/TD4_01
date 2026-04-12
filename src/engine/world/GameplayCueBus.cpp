#include "GameplayCueBus.h"

#include "core/string/StrUtil.h"

namespace Unnamed {
	void GameplayCuePayloadBag::SetBool(
		const std::string_view name, const bool value
	) {
		// キーを正規化して、同一名の表記ゆれを吸収します。
		const std::string key = NormalizeKey(name);
		if (key.empty()) {
			return;
		}
		mBoolValues[key] = value;
	}

	void GameplayCuePayloadBag::SetFloat(
		const std::string_view name, const float value
	) {
		// キーを正規化して、同一名の表記ゆれを吸収します。
		const std::string key = NormalizeKey(name);
		if (key.empty()) {
			return;
		}
		mFloatValues[key] = value;
	}

	void GameplayCuePayloadBag::SetVec2(
		const std::string_view name, const Vec2& value
	) {
		// キーを正規化して、同一名の表記ゆれを吸収します。
		const std::string key = NormalizeKey(name);
		if (key.empty()) {
			return;
		}
		mVec2Values[key] = value;
	}

	void GameplayCuePayloadBag::SetVec3(
		const std::string_view name, const Vec3& value
	) {
		// キーを正規化して、同一名の表記ゆれを吸収します。
		const std::string key = NormalizeKey(name);
		if (key.empty()) {
			return;
		}
		mVec3Values[key] = value;
	}

	void GameplayCuePayloadBag::SetEntityId(
		const std::string_view name, const uint64_t value
	) {
		// キーを正規化して、同一名の表記ゆれを吸収します。
		const std::string key = NormalizeKey(name);
		if (key.empty()) {
			return;
		}
		mEntityIdValues[key] = value;
	}

	bool GameplayCuePayloadBag::TryGetBool(
		const std::string_view name, bool& outValue
	) const {
		return TryGetValue(mBoolValues, name, outValue);
	}

	bool GameplayCuePayloadBag::TryGetFloat(
		const std::string_view name, float& outValue
	) const {
		return TryGetValue(mFloatValues, name, outValue);
	}

	bool GameplayCuePayloadBag::TryGetVec2(
		const std::string_view name, Vec2& outValue
	) const {
		return TryGetValue(mVec2Values, name, outValue);
	}

	bool GameplayCuePayloadBag::TryGetVec3(
		const std::string_view name, Vec3& outValue
	) const {
		return TryGetValue(mVec3Values, name, outValue);
	}

	bool GameplayCuePayloadBag::TryGetEntityId(
		const std::string_view name, uint64_t& outValue
	) const {
		return TryGetValue(mEntityIdValues, name, outValue);
	}

	void GameplayCuePayloadBag::Clear() {
		// すべての型別ストアを同時に空にして、再利用しやすくします。
		mBoolValues.clear();
		mFloatValues.clear();
		mVec2Values.clear();
		mVec3Values.clear();
		mEntityIdValues.clear();
	}

	std::string GameplayCuePayloadBag::NormalizeKey(
		const std::string_view name
	) {
		const std::string trimmed = StrUtil::TrimSpaces(std::string(name));
		if (trimmed.empty()) {
			return {};
		}
		return StrUtil::ToLowerCase(trimmed);
	}

	void GameplayCue::SetBool(
		const std::string_view name, const bool payloadValue
	) {
		payload.SetBool(name, payloadValue);
	}

	void GameplayCue::SetFloat(
		const std::string_view name, const float payloadValue
	) {
		payload.SetFloat(name, payloadValue);
	}

	void GameplayCue::SetVec2(
		const std::string_view name, const Vec2& payloadValue
	) {
		payload.SetVec2(name, payloadValue);
	}

	void GameplayCue::SetVec3(
		const std::string_view name, const Vec3& payloadValue
	) {
		payload.SetVec3(name, payloadValue);
	}

	void GameplayCue::SetEntityId(
		const std::string_view name, const uint64_t payloadValue
	) {
		payload.SetEntityId(name, payloadValue);
	}

	bool GameplayCue::TryGetBool(
		const std::string_view name, bool& outValue
	) const {
		return payload.TryGetBool(name, outValue);
	}

	bool GameplayCue::TryGetFloat(
		const std::string_view name, float& outValue
	) const {
		return payload.TryGetFloat(name, outValue);
	}

	bool GameplayCue::TryGetVec2(
		const std::string_view name, Vec2& outValue
	) const {
		return payload.TryGetVec2(name, outValue);
	}

	bool GameplayCue::TryGetVec3(
		const std::string_view name, Vec3& outValue
	) const {
		return payload.TryGetVec3(name, outValue);
	}

	bool GameplayCue::TryGetEntityId(
		const std::string_view name, uint64_t& outValue
	) const {
		return payload.TryGetEntityId(name, outValue);
	}

	GameplayCueBus::Handle GameplayCueBus::Subscribe(
		const GameplayCueFilter& filter, Callback callback
	) {
		// 無効な条件は購読として保持せず、早期に弾きます。
		if (
			filter.cueId.empty() ||
			filter.sourceEntityGuid == 0 ||
			!callback
		) {
			return 0;
		}

		const Handle handle = mNextHandle++;
		// 後で解除しやすいように、ハンドル付きで登録します。
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

	bool GameplayCueBus::Unsubscribe(const Handle handle) {
		// 対象の購読を無効化し、配信中でなければ即座に掃除します。
		if (handle == 0) {
			return false;
		}

		for (Listener& listener : mListeners) {
			if (listener.handle != handle || !listener.active) {
				continue;
			}

			listener.active   = false;
			listener.callback = nullptr;
			PruneInactiveListeners();
			return true;
		}
		return false;
	}

	void GameplayCueBus::Publish(const GameplayCue& cue) {
		// 発行に必要な識別子が欠けている場合は、何もしません。
		if (cue.id.empty() || cue.sourceEntityGuid == 0) {
			return;
		}

		// 配信中の変更で反復が壊れないよう、まず有効なリスナーを複製します。
		std::vector<Listener> snapshot;
		snapshot.reserve(mListeners.size());
		for (const Listener& listener : mListeners) {
			if (!listener.active || !listener.callback) {
				continue;
			}
			snapshot.emplace_back(listener);
		}

		mIsPublishing = true;
		// スナップショット順に評価し、条件に合うものだけを呼び出します。
		for (const Listener& listener : snapshot) {
			if (!Matches(listener.filter, cue)) {
				continue;
			}
			listener.callback(cue);
		}
		mIsPublishing = false;
		// 配信後に無効化済みの購読をまとめて削除します。
		PruneInactiveListeners();
	}

	void GameplayCueBus::Clear() {
		// すべての購読状態を初期化します。
		mListeners.clear();
	}

	bool GameplayCueBus::Matches(
		const GameplayCueFilter& filter, const GameplayCue& cue
	) {
		return filter.sourceEntityGuid == cue.sourceEntityGuid &&
		       filter.cueId == cue.id;
	}

	void GameplayCueBus::PruneInactiveListeners() {
		// 配信中はコンテナを再配置したくないため、後でまとめて掃除します。
		if (mIsPublishing) {
			return;
		}
		// 無効化済み、またはコールバックが消えた購読だけを除去します。
		std::erase_if(
			mListeners,
			[](const Listener& listener) {
				return !listener.active || !listener.callback;
			}
		);
	}
}
