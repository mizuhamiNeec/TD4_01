#pragma once

#include <limits>
#include <string>
#include <string_view>
#include <unordered_set>

#include "DemoTypes.h"
#include "ReplaySerializerRegistry.h"

namespace Unnamed {
	class Entity;

	class DemoManager {
	public:
		/// @brief デモのモード
		enum class MODE : uint8_t {
			IDLE      = 0, // 待機中
			RECORDING = 1, // 録画中
			PLAYBACK  = 2, // 再生中
		};

		/// @brief スナップショットのズレが発生した際のポリシー
		enum class MISMATCH_POLICY : uint8_t {
			CONTINUE = 0, // ズレようがお構いなしに続行する
			STOP     = 1, // ズレた時点で再生を停止する
		};

		/// @brief デモの録画を開始する
		/// @param path 録画ファイルの保存先パス
		/// @return 録画開始に成功したか
		bool StartRecording(std::string path);

		/// @brief デモの再生を開始する
		/// @param path 再生するデモファイルのパス
		/// @return 再生開始に成功したか
		bool StartPlayback(std::string path);

		/// @brief 録画または再生を停止する
		/// @return 停止に成功したか
		bool Stop();

		/// @brief 現在のモードを取得する
		/// @return 現在のモード
		[[nodiscard]] MODE GetMode() const;

		/// @brief 録画中かどうか
		/// @return 録画中であればtrue、そうでなければfalse
		[[nodiscard]] bool IsRecording() const;

		/// @brief 再生中かどうか
		/// @return 再生中であればtrue、そうでなければfalse
		[[nodiscard]] bool IsPlayback() const;

		/// @brief 再生が終了しているかどうか
		/// @return 再生が終了していればtrue、そうでなければfalse
		[[nodiscard]] bool IsPlaybackFinished() const;

		/// @brief 再生開始からの経過ティック数を取得する
		/// @return 再生開始からの経過ティック数
		[[nodiscard]] uint64_t GetPlaybackStartTick() const;

		/// @brief 再生開始からの経過時間（秒）を取得する
		/// @return 再生開始からの経過時間（秒）
		[[nodiscard]] std::string_view GetCurrentPath() const;

		/// @brief シミュレーションのティックレートを取得する
		/// @return シミュレーションのティックレート
		[[nodiscard]] uint32_t GetSimulationTickRate() const;

		/// @brief シミュレーションのティックステップ時間（秒）を取得する
		/// @return シミュレーションのティックステップ時間（秒）
		[[nodiscard]] float GetSimulationStepSeconds() const;

		/// @brief 録画のティックレートを取得する
		/// @return 録画のティックレート
		[[nodiscard]] static uint32_t ResolveConfiguredTickRate();

		/// @brief ティックレートからティックステップ時間（秒）を計算する
		/// @param tickRate ティックレート
		/// @return ティックステップ時間（秒）
		[[nodiscard]] static float TickStepSecondsFromRate(uint32_t tickRate);

		/// @brief ライブコマンドを録画に追加する（録画中のみ有効）
		/// @param command 追加するコマンド
		void SubmitLiveCommand(const DemoTickCommand& command);

		/// @brief 再生コマンドを消費する（再生中のみ有効）
		/// @param tick コマンドのティック
		/// @param subjectEntityGuid コマンドの対象エンティティのGUID
		/// @param outCommand コマンドの出力先
		/// @return コマンドが存在していて出力先に書き込めた場合はtrue、そうでなければfalse
		[[nodiscard]] bool ConsumePlaybackCommand(
			uint64_t         tick,
			uint64_t         subjectEntityGuid,
			DemoTickCommand& outCommand
		);

		/// @brief 再生中のエンティティに対して、必要に応じて初期スナップショットをキャプチャする
		/// @param subjectEntity 対象のエンティティ
		void CaptureInitialSnapshotIfNeeded(const Entity& subjectEntity);

		/// @brief 再生中のエンティティに対して、必要に応じて初期スナップショットを適用する
		/// @param subjectEntity 対象のエンティティ
		/// @return 初期スナップショットが適用された場合はtrue、そうでなければfalse
		[[nodiscard]] bool ApplyInitialSnapshotIfNeeded(Entity& subjectEntity);

		/// @brief 再生中のエンティティに対して、スナップショットの記録または検証を行う
		/// @param tick 現在のティック
		/// @param subjectEntity 対象のエンティティ
		void RecordOrVerifySnapshot(uint64_t tick, Entity& subjectEntity);

		/// @brief 録画または再生をリセットして待機状態に戻す
		void Reset();

		/// @brief 現在の状態を表す文字列を構築する
		/// @return 現在の状態を表す文字列
		[[nodiscard]] std::string BuildStatusString() const;

	private:
		/// @brief スナップショットのズレが発生した際のポリシーを解決する
		/// @return スナップショットのズレが発生した際のポリシー
		[[nodiscard]] static MISMATCH_POLICY ResolveMismatchPolicy();

		/// @brief スナップショットのズレが発生した際のログ出力間隔を解決する
		/// @return スナップショットのズレが発生した際のログ出力間隔（ティック数）
		[[nodiscard]] static uint64_t ResolveMismatchLogInterval();

		/// @brief ティックレートの値をサニタイズする
		/// @param tickRate サニタイズするティックレートの値
		/// @return サニタイズされたティックレートの値
		[[nodiscard]] static uint32_t SanitizeTickRate(uint32_t tickRate);

		/// @brief スナップショットのズレが発生した際のポリシーを文字列に変換する
		/// @param policy スナップショットのズレが発生した際のポリシー
		/// @return スナップショットのズレが発生した際のポリシーを表す文字列
		[[nodiscard]] static std::string_view MismatchPolicyToString(
			MISMATCH_POLICY policy
		);

		/// @brief 録画ファイルを保存する
		/// @return 保存に成功した場合はtrue、そうでなければfalse
		[[nodiscard]] bool SaveRecordingToFile() const;

		/// @brief 再生ファイルを読み込む
		/// @param path 読み込むファイルのパス
		/// @return 読み込みに成功した場合はtrue、そうでなければfalse
		[[nodiscard]] bool LoadPlaybackFile(const std::string& path);

		MODE                     mMode               = MODE::IDLE;
		std::string              mCurrentPath        = {};
		DemoFileV2               mFile               = {};
		ReplaySerializerRegistry mSerializerRegistry =
			ReplaySerializerRegistry::BuildDefault();
		size_t mPlaybackCommandCursor = 0;
		size_t mPlaybackSnapshotCursor = 0;
		uint64_t mSnapshotMismatchCount = 0;
		uint64_t mFirstMismatchTick = std::numeric_limits<uint64_t>::max();
		std::unordered_set<uint64_t> mPlaybackInitialAppliedEntities = {};
		bool mPlaybackInitialSetApplied = false;
		uint32_t mActiveTickRate = kDefaultDemoTickRate;
	};
}
