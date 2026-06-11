#pragma once
#include "engine/unnamed/framework/components/base/BaseComponent.h"
#include <core/io/json/JsonReader.h>
#include <core/io/json/JsonWriter.h>
#include <./core/math/Vec2.h>
#include <./core/math/Vec3.h>
#include <./core/math/Vec4.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace Unnamed {
	class Entity;
	class UiCanvasComponent;
	class ParticleEmitterComponent;
}

namespace MyGame {

	// ゴルフボール発射カウントダウンコンポーネント
	class GolfBallLaunchCountdownComponent;
	// ゴルフボールコンポーネント
	class GolfBallComponent;
	// プレイヤーの穴コンポーネント
	class PlayerHoleComponent;
	class PlayerMoveComponent;
	class PlayerFollowCameraComponent;
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
			StageIntro,
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
		// ゲーム開始前にステージ紹介を挟むかどうか
		bool _bUseStageIntro = true;
		// ステージ紹介に使う秒数
		float _stageIntroSeconds = 5.0f;
		// ステージ紹介の経過時間
		float _stageIntroElapsedTime = 0.0f;
		// カウントダウン秒数
		float _countdownSeconds = 10.0f;
		// 旧設定。現在はホールインワンのルールに合わせ、停止したら即失敗にする。
		float _minBallFlightSeconds = 5.0f;
		// ボールが打たれてから強制終了する時間
		float _maxBallFlightSeconds = 20.0f;
		// ボールやゴミがこの高さ以下なら海へ落ちた扱い
		float _seaOutHeight = -10.0f;
		// ボールがこの高さ以下ならホールインワン失敗としてリザルトへ進める
		float _ballSeaOutHeight = -100.0f;
		// ボール停止失敗を判定する速度閾値
		float _ballStopResultVelocityThreshold = 0.08f;
		// プレイ中の経過時間
		float _playingElapsedTime = 0.0f;
		// ボールが打たれてからの経過時間
		float _ballFlightElapsedTime = 0.0f;
		// ボールが一度でも飛行状態になったかどうか
		bool _hasBallLaunched = false;
		// 発射後に一度でもバウンドしたかどうか
		bool _hasBallBounced = false;
		// ボール発射Cueを発火済みかどうか
		bool _hasPublishedBallShootCue = false;
		// ホールインワン失敗Cueを発火済みかどうか
		bool _hasPublishedNotHoleInOneCue = false;
		// プレイ開始時にボールをLaunchするかどうか
		bool _launchBallOnPlayingStart = true;
		// PDF上のゴミ出現タイミング1を発火済みかどうか
		bool _hasTriggeredCountdownTrashWave = false;
		// PDF上のゴミ出現タイミング2を発火済みかどうか
		bool _hasTriggeredBallHitTrashWave = false;
		// PDF上のゴミ出現タイミング3を発火済みかどうか
		bool _hasTriggeredAfterHitTrashWave = false;
		// ゴール（ホールインワン成功）時の紙吹雪を発火済みかどうか
		bool _hasTriggeredGoalConfetti = false;
		// カウントダウン開始後に落とす想定のゴミ数
		int _countdownTrashWaveCount = 10;
		// ボールが打たれてから落とす想定のゴミ数
		int _ballHitTrashWaveCount = 15;
		// ボールが打たれて数秒後に落とす想定のゴミ数
		int _afterHitTrashWaveCount = 15;
		// 3回目のゴミ出現までの待ち時間
		float _afterHitTrashWaveDelay = 6.5f;
		// 通常ホールインワン時に表示するUI
		std::string _holeInOneUiAssetPath = "projects/TeamGame/content/ui/holeinone.ui.json";
		// ダイレクトホールインワン時に表示するUI
		std::string _directHoleInOneUiAssetPath = "projects/TeamGame/content/ui/directHoleinone.ui.json";
		// ボール停止・海落下など、ホールインワン失敗時に表示するUI
		std::string _notHoleInOneUiAssetPath = "projects/TeamGame/content/ui/notHoleInone.ui.json";
		// Notホールインワン確定後に遷移するリザルトシーンコマンド
		std::string _resultSceneCommand = "map projects/TeamGame/content/scenes/result.json";
		// 終了表示を書き換える対象UIエンティティ名
		std::string _clearUiEntityName = "Clear_UI";
		// ステージ紹介中に表示する目標UIエンティティ名
		std::string _stageIntroUiEntityName = "StageIntro_UI";
		// ステージ紹介中に表示する目標UI
		//std::string _stageIntroUiAssetPath = "projects/TeamGame/content/ui/stageIntro.ui.json";
		// ゴール（ホールインワン成功）時に再生する紙吹雪エミッターのエンティティ名に含まれる文字列
		std::string _goalConfettiEntityNamePrefix = "ParticleEmitter_GoalConfetti";
		std::string _stageIntroUiAssetPath = "projects/TeamGame/content/ui/gameshow.ui.json";
		// ステージ紹介中だけ隠す通常UIの復元用キャッシュ
		std::unordered_map<uint64_t, bool> _stageIntroUiVisibility;
		// NotホールインワンUIを見せてからリザルトへ遷移するまでの猶予
		float _notHoleInOneResultTransitionDelay = 2.0f;
		// Notホールインワン後のリザルト遷移予約
		bool _pendingNotHoleInOneResultTransition = false;
		// リザルト遷移予約後の経過時間
		float _notHoleInOneResultTransitionElapsed = 0.0f;
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
		// プレイヤー移動コンポーネントのキャッシュ
		PlayerMoveComponent* _playerMoveComponent = nullptr;
		// 追従カメラコンポーネントのキャッシュ
		PlayerFollowCameraComponent* _playerFollowCameraComponent = nullptr;

		/// シーン内の必要なコンポーネント参照を検索
		void ResolveRuntimeReferences();

		/// 現在フェーズに応じたゲーム進行を更新
		void UpdateGamePhase(float deltaTime);

		/// ステージ紹介を開始
		void StartStageIntro();

		/// ステージ紹介中の処理を更新
		void UpdateStageIntroPhase(float deltaTime);

		/// カウントダウン中の処理を更新
		void UpdateCountdownPhase();

		/// 紹介演出中のプレイヤー操作とカメラ状態を切り替える
		void ApplyStageIntroControlState(bool isStageIntro);

		/// ステージ紹介UIの表示を切り替える
		void SetStageIntroUiVisible(bool visible);

		/// ステージ紹介中は通常HUDだけを隠す
		void SetStageIntroHudHidden(bool hidden);

		/// プレイ中の処理を更新
		void UpdatePlayingPhase(float deltaTime);

		/// 穴や海に入ったゴミをスコアへ反映
		void UpdateTrashScore();

		/// @brief Presentation Cue を GameRuleSystemComponent の Owner から発火
		void PublishPresentationCue(std::string_view cueId, float value = 1.0f, float value2 = 1.0f) const;

		/// @brief ボール発射 Presentation Cue を一度だけ発火
		void PublishBallShootPresentationCue();

		/// @brief ゴミが穴へ入ったときの Presentation Cue を発火
		void PublishTrashIntoHolePresentationCue(Unnamed::Entity& trashEntity) const;

		/// @brief ゴミが海へ落ちたときの Presentation Cue を発火
		void PublishTrashToSeaPresentationCue(Unnamed::Entity& trashEntity) const;

		/// @brief ホールインワン失敗 Presentation Cue を一度だけ発火
		void PublishNotHoleInOnePresentationCue();

		/// ボールのキャッチ・停止・OBを判定
		void UpdateBallResult(float deltaTime);

		/// ボール結果監視を開始できる状態かを判定
		[[nodiscard]] bool ShouldMonitorBallResult() const;

		/// ボールが海落下としてNot結果へ進める状態かを判定
		/// @reason 穴入り成功フラグを立てずに、海落下だけを強制失敗として扱うため
		[[nodiscard]] bool IsGolfBallSeaFallResult() const;

		/// ボールをNotホールインワンとして確定
		/// @reason 海落下と地面範囲外落下の終了処理を同じ副作用にそろえる
		void FinishBallAsNotHoleInOne();

		/// NotホールインワンUI表示後のリザルト遷移を予約
		/// @reason UIボタン入力が拾えない状態でも、失敗結果を必ずResultシーンへ進めるため
		void QueueNotHoleInOneResultTransition();

		/// 予約済みのリザルト遷移を更新
		/// @reason Not表示を短時間見せてからmapコマンドを実行するため
		[[nodiscard]] bool UpdatePendingResultSceneTransition(float deltaTime);

#ifdef _DEBUG
		/// InspectorからDirectホールインワンを手動発火
		/// @reason 実際の入球条件に依存せず、Cue・UI・リザルト遷移を単体確認するため
		void DebugTriggerDirectHoleInOne();

		/// Inspectorから通常ホールインワンを手動発火
		/// @reason バウンド後成功のCue・UI・リザルト遷移を単体確認するため
		void DebugTriggerNormalHoleInOne();

		/// InspectorからNotホールインワンを手動発火
		/// @reason 海落下や場外失敗のCue・UI・リザルト遷移を単体確認するため
		void DebugTriggerNotHoleInOne();

		/// Debug発火前にリザルト早期returnを解除
		/// @reason Result済み状態からでも各クリア条件を繰り返し検証できるようにするため
		void PrepareDebugClearTrigger();
#endif

		/// PDFに書かれたゴミ出現タイミングを状態として発火
		void UpdateTrashWaveTiming();

		/// ゴミ出現タイミングを発火したときの入口
		void TriggerTrashWave(int trashCount);

		/// ボールキャッチ成立時のスコアとリザルト状態を確定
		bool TryScoreBallCatch(GolfBallComponent& golfBall);

		/// ゴール紙吹雪エミッターをシーンから収集
		[[nodiscard]] std::vector<Unnamed::ParticleEmitterComponent*>
			CollectGoalConfettiEmitters() const;

		/// ゴール（ホールインワン成功）時の紙吹雪を一度だけ再生
		void StartGoalConfetti();

		/// ゴール紙吹雪の再生を止め、発火フラグを戻す
		void StopGoalConfetti();

		/// 現在の結果に合わせてクリア表示UIを差し替える
		[[nodiscard]] bool ApplyClearUiForResult();

		/// Notホールインワン表示UIを直接表示
		/// @reason 音Cueとは独立して、海落下時に必ず失敗UIを出すため
		[[nodiscard]] bool ApplyNotHoleInOneUiForResult();

		/// ボールが停止失敗として扱えるかを判定
		[[nodiscard]] bool IsBallStoppedForResult() const;

		/// クリア表示用のUIキャンバスを検索
		[[nodiscard]] Unnamed::UiCanvasComponent* ResolveClearUiCanvas() const;

		/// ステージ紹介表示用のUIキャンバスを検索
		[[nodiscard]] Unnamed::UiCanvasComponent* ResolveStageIntroUiCanvas() const;

		/// 対象エンティティのGUIDを取得
		[[nodiscard]] uint64_t GetEntityGuid(Unnamed::Entity* entity) const;
	};
}
