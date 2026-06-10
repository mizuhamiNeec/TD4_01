#pragma once
#include "engine/unnamed/framework/components/base/BaseComponent.h"
#include <core/io/json/JsonReader.h>
#include <core/io/json/JsonWriter.h>
#include <./core/math/Vec2.h>
#include <./core/math/Vec3.h>
#include <./core/math/Vec4.h>

#include <cstdint>
#include <string>

namespace Unnamed {
	class Entity;
}

namespace MyGame {

	// ゴルフボール発射カウントダウンコンポーネント
	class GolfBallLaunchCountdownComponent;
	// ゴルフボールコンポーネント
	class GolfBallComponent;
	// プレイヤーの穴コンポーネント
	class PlayerHoleComponent;
	// スコア管理コンポーネント
	class GameScoreComponent;
	// ゴミ移動コンポーネント
	class TrashObjMoverComponent;
	// ゴミ自動生成コンポーネント
	class TrashObjSpawnerComponent;

	// ゲームルール管理コンポーネント
	class GameRuleSystemComponent : public Unnamed::BaseComponent {
	public:
		// -----------------------------------------------------------------------
		// ライフサイクル
		// -----------------------------------------------------------------------

		/// コンポーネントがアタッチされたときに呼び出される
		void OnAttached() override;

		/// 毎フレーム更新
		void OnTick(float deltaTime) override;

		/// 描画フレーム更新
		void OnRenderTick(float renderDeltaTime, float interpolationAlpha) override;

		/// コンポーネントがデタッチされたときに呼び出される
		void OnDetached() override;

		// -----------------------------------------------------------------------
		// BaseComponent override
		// -----------------------------------------------------------------------

		[[nodiscard]] std::string_view GetStableName() const override;
		[[nodiscard]] std::string_view GetComponentName() const override;

#ifdef _DEBUG
		void DrawInspectorImGui() override;
#endif

		/// コンポーネントの値を読み込む際に使用されます
		void Deserialize(const Unnamed::JsonReader& reader) override;

		/// コンポーネントの値を書き込む際に使用されます
		void Serialize(Unnamed::JsonWriter& writer) const override;

		// -----------------------------------------------------------------------
		// ゲーム進行操作
		// -----------------------------------------------------------------------

		/// カウントダウンからゲームを開始
		void StartCountdown();

		/// プレイ中へ移行
		void StartPlaying();

		/// リザルトへ移行
		void FinishGame();

		/// ルール状態とスコアを初期状態へ戻す
		void ResetGame();

		/// 現在ゲーム中かどうかを取得
		[[nodiscard]] bool IsPlaying() const;

		/// 現在リザルト状態かどうかを取得
		[[nodiscard]] bool IsResult() const;

	private:
		// ゲームフェーズ
		enum class GamePhase {
			Ready,
			Countdown,
			Playing,
			Result
		};

		// 現在のゲームフェーズ
		GamePhase _phase = GamePhase::Ready;
		// ゲーム開始フラグ
		bool _isGameStarted = false;
		// ゲーム終了フラグ
		bool _isGameEnded = false;

		// 場外か　
		bool _isOutOfBounds = false;
		// ホールインワンかどうか
		bool _isHoleInOne = false;
		// ダイレクトホールインワンかどうか
		bool _isDirectHoleInOne = false;
		// 起動時に自動でカウントダウンを始めるかどうか
		bool _autoStartCountdown = true;
		// カウントダウン秒数
		float _countdownSeconds = 10.0f;
		// 旧設定。現在はホールインワンのルールに合わせ、停止したら即失敗にする。
		float _minBallFlightSeconds = 5.0f;
		// ボールが打たれてから強制終了する時間
		float _maxBallFlightSeconds = 20.0f;
		// ボールやゴミがこの高さ以下なら海へ落ちた扱い
		float _seaOutHeight = -10.0f;
		// プレイ中の経過時間
		float _playingElapsedTime = 0.0f;
		// ボールが打たれてからの経過時間
		float _ballFlightElapsedTime = 0.0f;
		// ボールが一度でも飛行状態になったかどうか
		bool _hasBallLaunched = false;
		// プレイ開始時にボールをLaunchするかどうか
		bool _launchBallOnPlayingStart = true;
		// PDF上のゴミ出現タイミング1を発火済みかどうか
		bool _hasTriggeredCountdownTrashWave = false;
		// PDF上のゴミ出現タイミング2を発火済みかどうか
		bool _hasTriggeredBallHitTrashWave = false;
		// PDF上のゴミ出現タイミング3を発火済みかどうか
		bool _hasTriggeredAfterHitTrashWave = false;
		// カウントダウン開始後に落とす想定のゴミ数
		int _countdownTrashWaveCount = 10;
		// ボールが打たれてから落とす想定のゴミ数
		int _ballHitTrashWaveCount = 15;
		// ボールが打たれて数秒後に落とす想定のゴミ数
		int _afterHitTrashWaveCount = 15;
		// 3回目のゴミ出現までの待ち時間
		float _afterHitTrashWaveDelay = 6.5f;
		// ゴルフボール発射カウントダウンコンポーネントのキャッシュ
		GolfBallLaunchCountdownComponent* _launchCountdownComponent = nullptr;
		// スコアコンポーネントのキャッシュ
		GameScoreComponent* _scoreComponent = nullptr;
		// プレイヤー穴コンポーネントのキャッシュ
		PlayerHoleComponent* _playerHoleComponent = nullptr;
		// ゴルフボールコンポーネントのキャッシュ
		GolfBallComponent* _golfBallComponent = nullptr;
		// ゴミ自動生成コンポーネントのキャッシュ
		TrashObjSpawnerComponent* _trashObjSpawnerComponent = nullptr;

		/// シーン内の必要なコンポーネント参照を検索
		void ResolveRuntimeReferences();

		/// 現在フェーズに応じたゲーム進行を更新
		void UpdateGamePhase(float deltaTime);

		/// カウントダウン中の処理を更新
		void UpdateCountdownPhase();

		/// プレイ中の処理を更新
		void UpdatePlayingPhase(float deltaTime);

		/// 穴や海に入ったゴミをスコアへ反映
		void UpdateTrashScore();

		/// ボールのキャッチ・停止・OBを判定
		void UpdateBallResult(float deltaTime);

		/// PDFに書かれたゴミ出現タイミングを状態として発火
		void UpdateTrashWaveTiming();

		/// ゴミ出現タイミングを発火したときの入口
		void TriggerTrashWave(int trashCount);

		/// 対象エンティティのGUIDを取得
		[[nodiscard]] uint64_t GetEntityGuid(Unnamed::Entity* entity) const;
	};
}
