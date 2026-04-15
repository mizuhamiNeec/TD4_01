#pragma once

#include <memory>
#include <string>
#include <vector>

#include "engine/unnamed/framework/components/base/BaseComponent.h"

namespace Unnamed {
	class BaseComponent;
	class EntitySequenceBinder;
	class JsonReader;
	class JsonWriter;
	class SequenceAsset;
	class SequencePlayer;
	class UiSequenceBinder;

	/// @brief SequenceAsset を再生し、再生中のロック/カメラ切り替えを管理するコンポーネントです。
	class SequenceCutsceneComponent final : public BaseComponent {
	public:
		/// @brief コンポーネントが追加された直後に呼び出されます。
		void OnAttached() override;
		/// @brief コンポーネントが破棄される直前に呼び出されます。
		void OnDetached() override;
		/// @brief プレイ中のカットシーン更新を行います。
		void OnTick(float deltaTime) override;
		/// @brief エディタ中のカットシーン更新を行います。
		void OnEditorTick(float deltaTime) override;

		/// @brief コンポーネントの stableName を返します。
		[[nodiscard]] std::string_view GetStableName() const override;
		/// @brief コンポーネント名を返します。
		[[nodiscard]] std::string_view GetComponentName() const override;
		[[nodiscard]] uint32_t GetIcon() const override;

#ifdef _DEBUG
		/// @brief インスペクタ UI を描画します。
		void DrawInspectorImGui() override;
#endif

		/// @brief JSON から設定を読み込みます。
		void Deserialize(const JsonReader& reader) override;
		/// @brief 設定を JSON に書き出します。
		void Serialize(JsonWriter& writer) const override;

	private:
		/// @brief ロック対象を表す設定です。
		struct LockTargetSpec {
			uint64_t    componentGuid       = 0;
			uint64_t    entityGuid          = 0;
			std::string componentStableName = "";
		};

		/// @brief 実行中に保持するロック復帰情報です。
		struct ActiveLockState {
			BaseComponent* component     = nullptr;
			bool           previousActive = true;
		};

		/// @brief 再生を開始できる状態に初期化します。
		bool StartPlayback();
		/// @brief 再生を停止し、必要ならロック状態を復帰します。
		void StopPlayback(bool restoreState);
		/// @brief SequencePlayer をティックします。
		void TickPlayback(float deltaTime);
		/// @brief SequenceAsset をロードします。
		[[nodiscard]] bool EnsureSequenceLoaded();
		/// @brief シーケンスバインダーを現在のシーンへ接続し直します。
		void RefreshBinders();
		/// @brief ロック対象を無効化します。
		void ApplyLockTargets();
		/// @brief ロック対象の有効状態を復帰します。
		void RestoreLockTargets();
		/// @brief lock spec から対象コンポーネントを解決します。
		[[nodiscard]] BaseComponent* ResolveLockTarget(const LockTargetSpec& spec)
		const;
		/// @brief カットシーンカメラを現在カメラへ設定します。
		void ApplyCutsceneCamera() const;

		static void SerializeLockTarget(
			JsonWriter& writer,
			const LockTargetSpec& spec
		);

		std::string mSequencePath = "";
		bool        mPlayOnAttach = true;
		bool        mAutoStopWhenCompleted = true;
		float       mPlayRate = 1.0f;
		bool        mForceCutsceneCamera = true;
		uint64_t    mCutsceneCameraEntityGuid = 0;
		bool        mRestorePreviousCamera = true;
		bool        mApplyComponentLocks = true;
		std::vector<LockTargetSpec> mLockTargets = {};

		bool        mPlayRequested = false;
		bool        mPlaying = false;
		std::string mLoadedSequencePath = "";
		bool        mLoggedLoadFailure = false;
		bool        mSavedCameraValid = false;
		uint64_t    mSavedCameraEntityGuid = 0;
		std::vector<ActiveLockState> mActiveLocks = {};

		std::shared_ptr<SequenceAsset>   mSequenceAsset = nullptr;
		std::unique_ptr<SequencePlayer>  mSequencePlayer = nullptr;
		std::unique_ptr<EntitySequenceBinder> mEntityBinder = nullptr;
		std::unique_ptr<UiSequenceBinder> mUiBinder = nullptr;
	};
}
