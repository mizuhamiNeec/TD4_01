#include "GameRuleSystemComponent.h"
#include "GameScoreComponent.h"
#include "GolfBallComponent.h"
#include "GolfBallLaunchCountdownComponent.h"
#include "PlayerHoleComponent.h"
#include "PlayerMoveComponent.h"
#include "PlayerFollowCameraComponent.h"
#include "TeamGameResultScore.h"
#include "TrashObjMoverComponent.h"
#include "TrashObjSpawnerComponent.h"
#include "engine/scene/Scene.h"
#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/components/ui/UiCanvasComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include <algorithm>
#include <array>
#include <cmath>

#ifdef _DEBUG
#include "imgui.h"
#endif

void MyGame::GameRuleSystemComponent::OnAttached()
{
	// NOTE: インゲーム開始時に必要な参照を探して、PDFの流れどおりカウントダウンへ入る。
	ResolveRuntimeReferences();
	ResetGame();
	if (_autoStartCountdown) {
		if (_bUseStageIntro && _stageIntroSeconds > 0.0f) {
			StartStageIntro();
		} else {
			StartCountdown();
		}
	}
}

void MyGame::GameRuleSystemComponent::OnTick(float deltaTime)
{
	// NOTE: シーン編集や遅延生成に対応するため、参照は毎フレーム補完する。
	ResolveRuntimeReferences();
	UpdateGamePhase(deltaTime);
}

void MyGame::GameRuleSystemComponent::OnRenderTick(float renderDeltaTime, float interpolationAlpha)
{}

void MyGame::GameRuleSystemComponent::OnDetached()
{
	// NOTE: シーン終了時に他コンポーネントへのキャッシュを破棄する。
	_launchCountdownComponent = nullptr;
	_scoreComponent = nullptr;
	_playerHoleComponent = nullptr;
	_golfBallComponent = nullptr;
	_trashObjSpawnerComponent = nullptr;
	_playerMoveComponent = nullptr;
	_playerFollowCameraComponent = nullptr;
}

std::string_view MyGame::GameRuleSystemComponent::GetStableName() const {
	return "mygame.GameRuleSystemComponent";
}

std::string_view MyGame::GameRuleSystemComponent::GetComponentName() const {
	return "Game Rule System Component";
}

#ifdef _DEBUG
void MyGame::GameRuleSystemComponent::DrawInspectorImGui()
{
	// NOTE: デバッグ中にゲーム進行とPDF上のタイミング設定を確認できるようにする。
	const char* phaseName = "Ready";
	if (_phase == GamePhase::StageIntro) {
		phaseName = "StageIntro";
	} else if (_phase == GamePhase::Countdown) {
		phaseName = "Countdown";
	} else if (_phase == GamePhase::Playing) {
		phaseName = "Playing";
	} else if (_phase == GamePhase::Result) {
		phaseName = "Result";
	}

	ImGui::Text("=== Game Rule System Component ===");
	ImGui::Text("Phase: %s", phaseName);
	ImGui::Text("Started: %s", _isGameStarted ? "true" : "false");
	ImGui::Text("Ended: %s", _isGameEnded ? "true" : "false");
	ImGui::Text("Playing Time: %.2f", _playingElapsedTime);
	ImGui::Text("Ball Flight Time: %.2f", _ballFlightElapsedTime);
	ImGui::Checkbox("Auto Start Countdown", &_autoStartCountdown);
	ImGui::Checkbox("Use Stage Intro", &_bUseStageIntro);
	ImGui::DragFloat("Stage Intro Seconds", &_stageIntroSeconds, 0.1f, 0.0f, 60.0f, "%.2f sec");
	ImGui::DragFloat("Countdown Seconds", &_countdownSeconds, 0.1f, 0.0f, 999.0f, "%.2f sec");
	ImGui::DragFloat("Min Ball Flight Seconds", &_minBallFlightSeconds, 0.1f, 0.0f, 999.0f, "%.2f sec");
	ImGui::DragFloat("Max Ball Flight Seconds", &_maxBallFlightSeconds, 0.1f, 0.0f, 999.0f, "%.2f sec");
	ImGui::DragFloat("Sea Out Height", &_seaOutHeight, 0.1f, -999.0f, 999.0f, "%.2f");
	ImGui::DragFloat("Ball Sea Out Height", &_ballSeaOutHeight, 0.1f, -999.0f, 999.0f, "%.2f");
	ImGui::DragFloat(
		"Ball Stop Result Velocity", &_ballStopResultVelocityThreshold, 0.01f, 0.0f, 10.0f, "%.3f"
	);
	ImGui::DragInt("Countdown Trash Wave Count", &_countdownTrashWaveCount, 1, 0, 999);
	ImGui::DragInt("Ball Hit Trash Wave Count", &_ballHitTrashWaveCount, 1, 0, 999);
	ImGui::DragInt("After Hit Trash Wave Count", &_afterHitTrashWaveCount, 1, 0, 999);
	ImGui::DragFloat("After Hit Trash Wave Delay", &_afterHitTrashWaveDelay, 0.1f, 0.0f, 999.0f, "%.2f sec");
	auto inputString = [](const char* label, std::string& value) {
		std::array<char, 256> buffer{};
		const size_t copyLength = std::min(value.size(), buffer.size() - 1);
		std::copy_n(value.data(), copyLength, buffer.data());
		if (ImGui::InputText(label, buffer.data(), buffer.size())) {
			value = buffer.data();
		}
	};
	inputString("Clear UI Entity Name", _clearUiEntityName);
	inputString("Stage Intro UI Entity Name", _stageIntroUiEntityName);
	inputString("Stage Intro UI", _stageIntroUiAssetPath);
	inputString("Hole In One UI", _holeInOneUiAssetPath);
	inputString("Direct Hole In One UI", _directHoleInOneUiAssetPath);
	inputString("Not Hole In One UI", _notHoleInOneUiAssetPath);
	ImGui::Text("Score: %d", _scoreComponent ? _scoreComponent->GetScore() : 0);
	if (_scoreComponent) {
		ImGui::Separator();
		ImGui::Text("Score Breakdown");
		ImGui::Text("Trash Into Hole: %d", _scoreComponent->GetTrashIntoHoleTotal());
		ImGui::Text("Trash To Sea: %d", _scoreComponent->GetTrashToSeaTotal());
		ImGui::Text("Ball Catch: %d", _scoreComponent->GetBallCatchTotal());
		ImGui::Text("Hole In One: %d", _scoreComponent->GetHoleInOneTotal());
		ImGui::Text("Ace: %d", _scoreComponent->GetDirectHoleInOneTotal());
		ImGui::Text("OB Penalty: %d", _scoreComponent->GetOutOfBoundsPenaltyTotal());
	}

	if (ImGui::Button("Start Countdown")) {
		StartCountdown();
	}
	ImGui::SameLine();
	if (ImGui::Button("Start Playing")) {
		StartPlaying();
	}
	ImGui::SameLine();
	if (ImGui::Button("Finish")) {
		FinishGame();
	}
	ImGui::SameLine();
	if (ImGui::Button("Reset")) {
		ResetGame();
	}
}
#endif

void MyGame::GameRuleSystemComponent::Deserialize(const Unnamed::JsonReader & reader)
{
	// NOTE: PDFのインゲーム進行に関わる調整値をJSONから読み込む。
	if (auto val = reader.Read<bool>("autoStartCountdown")) {
		_autoStartCountdown = val.value();
	}
	if (auto val = reader.Read<bool>("useStageIntro")) {
		_bUseStageIntro = val.value();
	}
	if (auto val = reader.Read<float>("stageIntroSeconds")) {
		_stageIntroSeconds = std::max(0.0f, val.value());
	}
	if (auto val = reader.Read<float>("countdownSeconds")) {
		_countdownSeconds = std::max(0.0f, val.value());
	}
	if (auto val = reader.Read<float>("minBallFlightSeconds")) {
		_minBallFlightSeconds = std::max(0.0f, val.value());
	}
	if (auto val = reader.Read<float>("maxBallFlightSeconds")) {
		_maxBallFlightSeconds = std::max(0.0f, val.value());
	}
	if (auto val = reader.Read<float>("seaOutHeight")) {
		_seaOutHeight = val.value();
	}
	if (auto val = reader.Read<float>("ballSeaOutHeight")) {
		_ballSeaOutHeight = val.value();
	}
	if (auto val = reader.Read<float>("ballStopResultVelocityThreshold")) {
		_ballStopResultVelocityThreshold = std::max(0.0f, val.value());
	}
	if (auto val = reader.Read<bool>("launchBallOnPlayingStart")) {
		_launchBallOnPlayingStart = val.value();
	}
	if (auto val = reader.Read<int>("countdownTrashWaveCount")) {
		_countdownTrashWaveCount = std::max(0, val.value());
	}
	if (auto val = reader.Read<int>("ballHitTrashWaveCount")) {
		_ballHitTrashWaveCount = std::max(0, val.value());
	}
	if (auto val = reader.Read<int>("afterHitTrashWaveCount")) {
		_afterHitTrashWaveCount = std::max(0, val.value());
	}
	if (auto val = reader.Read<float>("afterHitTrashWaveDelay")) {
		_afterHitTrashWaveDelay = std::max(0.0f, val.value());
	}
	if (reader.Has("clearUiEntityName")) {
		_clearUiEntityName = reader["clearUiEntityName"].GetString(_clearUiEntityName);
	}
	if (reader.Has("stageIntroUiEntityName")) {
		_stageIntroUiEntityName =
			reader["stageIntroUiEntityName"].GetString(_stageIntroUiEntityName);
	}
	if (reader.Has("stageIntroUiAssetPath")) {
		_stageIntroUiAssetPath =
			reader["stageIntroUiAssetPath"].GetString(_stageIntroUiAssetPath);
	}
	if (reader.Has("holeInOneUiAssetPath")) {
		_holeInOneUiAssetPath = reader["holeInOneUiAssetPath"].GetString(_holeInOneUiAssetPath);
	}
	if (reader.Has("directHoleInOneUiAssetPath")) {
		_directHoleInOneUiAssetPath =
			reader["directHoleInOneUiAssetPath"].GetString(_directHoleInOneUiAssetPath);
	}
	if (reader.Has("notHoleInOneUiAssetPath")) {
		_notHoleInOneUiAssetPath =
			reader["notHoleInOneUiAssetPath"].GetString(_notHoleInOneUiAssetPath);
	}
}

void MyGame::GameRuleSystemComponent::Serialize(Unnamed::JsonWriter & writer) const
{
	// NOTE: インゲーム進行の調整値をJSONへ保存する。
	writer.Key("autoStartCountdown");
	writer.Write(_autoStartCountdown);
	writer.Key("useStageIntro");
	writer.Write(_bUseStageIntro);
	writer.Key("stageIntroSeconds");
	writer.Write(_stageIntroSeconds);
	writer.Key("countdownSeconds");
	writer.Write(_countdownSeconds);
	writer.Key("minBallFlightSeconds");
	writer.Write(_minBallFlightSeconds);
	writer.Key("maxBallFlightSeconds");
	writer.Write(_maxBallFlightSeconds);
	writer.Key("seaOutHeight");
	writer.Write(_seaOutHeight);
	writer.Key("ballSeaOutHeight");
	writer.Write(_ballSeaOutHeight);
	writer.Key("ballStopResultVelocityThreshold");
	writer.Write(_ballStopResultVelocityThreshold);
	writer.Key("launchBallOnPlayingStart");
	writer.Write(_launchBallOnPlayingStart);
	writer.Key("countdownTrashWaveCount");
	writer.Write(_countdownTrashWaveCount);
	writer.Key("ballHitTrashWaveCount");
	writer.Write(_ballHitTrashWaveCount);
	writer.Key("afterHitTrashWaveCount");
	writer.Write(_afterHitTrashWaveCount);
	writer.Key("afterHitTrashWaveDelay");
	writer.Write(_afterHitTrashWaveDelay);
	writer.Key("clearUiEntityName");
	writer.Write(_clearUiEntityName);
	writer.Key("stageIntroUiEntityName");
	writer.Write(_stageIntroUiEntityName);
	writer.Key("stageIntroUiAssetPath");
	writer.Write(_stageIntroUiAssetPath);
	writer.Key("holeInOneUiAssetPath");
	writer.Write(_holeInOneUiAssetPath);
	writer.Key("directHoleInOneUiAssetPath");
	writer.Write(_directHoleInOneUiAssetPath);
	writer.Key("notHoleInOneUiAssetPath");
	writer.Write(_notHoleInOneUiAssetPath);
}

void MyGame::GameRuleSystemComponent::StartStageIntro()
{
	// NOTE: 開始前にステージと目標を見せるため、通常カウントダウンの前に専用フェーズへ入る。
	ResolveRuntimeReferences();
	_phase = GamePhase::StageIntro;
	_isGameStarted = false;
	_isGameEnded = false;
	_isOutOfBounds = false;
	_isHoleInOne = false;
	_isDirectHoleInOne = false;
	_stageIntroElapsedTime = 0.0f;
	_playingElapsedTime = 0.0f;
	_ballFlightElapsedTime = 0.0f;
	_hasBallLaunched = false;
	_hasTriggeredCountdownTrashWave = false;
	_hasTriggeredBallHitTrashWave = false;
	_hasTriggeredAfterHitTrashWave = false;

	if (_scoreComponent) {
		_scoreComponent->ResetScore();
	}
	if (_launchCountdownComponent) {
		_launchCountdownComponent->ResetCountdown();
	}
	ApplyStageIntroControlState(true);
	SetStageIntroHudHidden(true);
	SetStageIntroUiVisible(true);
}

void MyGame::GameRuleSystemComponent::StartCountdown()
{
	// NOTE: スコアと進行状態をリセットし、ステージ紹介後の10秒カウントへ入る。
	ResolveRuntimeReferences();
	_phase = GamePhase::Countdown;
	_isGameStarted = false;
	_isGameEnded = false;
	_isOutOfBounds = false;
	_isHoleInOne = false;
	_isDirectHoleInOne = false;
	_playingElapsedTime = 0.0f;
	_ballFlightElapsedTime = 0.0f;
	_hasBallLaunched = false;
	_hasTriggeredCountdownTrashWave = false;
	_hasTriggeredBallHitTrashWave = false;
	_hasTriggeredAfterHitTrashWave = false;

	if (_scoreComponent) {
		_scoreComponent->ResetScore();
	}
	SetStageIntroHudHidden(false);
	ApplyStageIntroControlState(false);
	SetStageIntroUiVisible(false);
	if (_launchCountdownComponent) {
		_launchCountdownComponent->StartCountdown(_countdownSeconds);
	}
}

void MyGame::GameRuleSystemComponent::StartPlaying()
{
	// NOTE: カウントダウン終了後にボールを打ち、インゲーム本編へ移行する。
	ResolveRuntimeReferences();
	_phase = GamePhase::Playing;
	_isGameStarted = true;
	_isGameEnded = false;
	_playingElapsedTime = 0.0f;
	_ballFlightElapsedTime = 0.0f;
	_hasBallLaunched = false;
	ApplyStageIntroControlState(false);

	if (_launchCountdownComponent) {
		_launchCountdownComponent->StopCountdown();
	}
	if (!_launchCountdownComponent && _golfBallComponent && _launchBallOnPlayingStart) {
		// NOTE: 発射カウントダウンが無いシーンだけ、ルール管理側から直接発射する。
		_golfBallComponent->Launch();
		_hasBallLaunched = _golfBallComponent->IsInFlight();
	} else if (_golfBallComponent) {
		// NOTE: 発射カウントダウン側がLaunch済みのボール状態を引き継ぐ。
		_hasBallLaunched = _golfBallComponent->IsInFlight();
	}
}

void MyGame::GameRuleSystemComponent::FinishGame()
{
	// NOTE: ボール停止・海落下・キャッチ後はリザルトへ移行する。
	ResolveRuntimeReferences();
	if (_phase == GamePhase::Result && _isGameEnded) {
		return;
	}
	SetStageIntroHudHidden(false);
	if (_scoreComponent) {
		StoreTeamGameResultScore(
			{
				.total = _scoreComponent->GetScore(),
				.trashToSea = _scoreComponent->GetTrashToSeaTotal(),
				.trashIntoHole = _scoreComponent->GetTrashIntoHoleTotal(),
				.ballCatch = _scoreComponent->GetBallCatchTotal(),
				.holeInOne = _scoreComponent->GetHoleInOneTotal(),
				.directHoleInOne = _scoreComponent->GetDirectHoleInOneTotal(),
				.outOfBoundsPenalty =
					_scoreComponent->GetOutOfBoundsPenaltyTotal(),
			}
		);
	}
	(void)ApplyClearUiForResult();
	ApplyStageIntroControlState(false);
	SetStageIntroUiVisible(false);
	_phase = GamePhase::Result;
	_isGameEnded = true;
	if (_launchCountdownComponent) {
		_launchCountdownComponent->StopCountdown();
	}
}

void MyGame::GameRuleSystemComponent::ResetGame()
{
	// NOTE: ルール状態を初期化し、必要なら後でStartCountdownから再開する。
	_phase = GamePhase::Ready;
	_isGameStarted = false;
	_isGameEnded = false;
	_isOutOfBounds = false;
	_isHoleInOne = false;
	_isDirectHoleInOne = false;
	_playingElapsedTime = 0.0f;
	_ballFlightElapsedTime = 0.0f;
	_hasBallLaunched = false;
	_hasTriggeredCountdownTrashWave = false;
	_hasTriggeredBallHitTrashWave = false;
	_hasTriggeredAfterHitTrashWave = false;
	_stageIntroElapsedTime = 0.0f;

	if (_launchCountdownComponent) {
		_launchCountdownComponent->ResetCountdown();
	}
	if (_scoreComponent) {
		_scoreComponent->ResetScore();
	}
	SetStageIntroHudHidden(false);
	ApplyStageIntroControlState(false);
	SetStageIntroUiVisible(false);
	if (auto* clearUiCanvas = ResolveClearUiCanvas()) {
		if (auto* clearUiEntity = clearUiCanvas->GetOwner()) {
			// NOTE: 結果が確定するまでクリア文言を表示しないため、再開始時に非表示へ戻す。
			clearUiEntity->SetVisible(false);
		}
	}
}

bool MyGame::GameRuleSystemComponent::IsPlaying() const
{
	return _phase == GamePhase::Playing;
}

bool MyGame::GameRuleSystemComponent::IsResult() const
{
	return _phase == GamePhase::Result;
}

void MyGame::GameRuleSystemComponent::ResolveRuntimeReferences()
{
	// NOTE: 既に必要な参照が揃っている場合は再検索を省略する。
	if (_launchCountdownComponent && _scoreComponent && _playerHoleComponent &&
		_golfBallComponent && _trashObjSpawnerComponent && _playerMoveComponent &&
		_playerFollowCameraComponent) {
		return;
	}

	auto* scene = GetScene();
	if (!scene && GetOwner()) {
		scene = GetOwner()->GetScene();
	}
	if (!scene) {
		return;
	}

	// NOTE: シーン内のエンティティを走査して、最初に見つかった対象コンポーネントを使う。
	for (const auto& entityPtr : scene->GetEntities()) {
		if (!entityPtr) {
			continue;
		}

		auto* entity = entityPtr.get();
		if (!_launchCountdownComponent) {
			_launchCountdownComponent = entity->GetComponent<GolfBallLaunchCountdownComponent>();
		}
		if (!_scoreComponent) {
			_scoreComponent = entity->GetComponent<GameScoreComponent>();
		}
		if (!_playerHoleComponent) {
			_playerHoleComponent = entity->GetComponent<PlayerHoleComponent>();
		}
		if (!_golfBallComponent) {
			_golfBallComponent = entity->GetComponent<GolfBallComponent>();
		}
		if (!_trashObjSpawnerComponent) {
			_trashObjSpawnerComponent = entity->GetComponent<TrashObjSpawnerComponent>();
		}
		if (!_playerMoveComponent) {
			_playerMoveComponent = entity->GetComponent<PlayerMoveComponent>();
		}
		if (!_playerFollowCameraComponent) {
			_playerFollowCameraComponent = entity->GetComponent<PlayerFollowCameraComponent>();
		}
	}
}

void MyGame::GameRuleSystemComponent::UpdateGamePhase(float deltaTime)
{
	// NOTE: フェーズごとにPDFのインゲーム進行を分岐する。
	if (_phase == GamePhase::StageIntro) {
		UpdateStageIntroPhase(deltaTime);
		return;
	}
	if (_phase == GamePhase::Countdown) {
		UpdateCountdownPhase();
		return;
	}
	if (_phase == GamePhase::Playing) {
		UpdatePlayingPhase(deltaTime);
	}
}

void MyGame::GameRuleSystemComponent::UpdateStageIntroPhase(float deltaTime)
{
	ApplyStageIntroControlState(true);
	_stageIntroElapsedTime += std::max(0.0f, deltaTime);
	if (_stageIntroElapsedTime >= _stageIntroSeconds) {
		StartCountdown();
	}
}

void MyGame::GameRuleSystemComponent::UpdateCountdownPhase()
{
	// NOTE: カウント開始時点のゴミ出現タイミングを1回だけ発火する。
	if (!_hasTriggeredCountdownTrashWave) {
		TriggerTrashWave(_countdownTrashWaveCount);
		_hasTriggeredCountdownTrashWave = true;
	}

	// NOTE: カウントダウンが終わったらボールを打つフェーズへ進む。
	if ((_launchCountdownComponent && _launchCountdownComponent->HasLaunched()) ||
		(!_launchCountdownComponent && _golfBallComponent && _golfBallComponent->IsInFlight())) {
		StartPlaying();
	}
}

void MyGame::GameRuleSystemComponent::ApplyStageIntroControlState(bool isStageIntro)
{
	if (_playerMoveComponent) {
		_playerMoveComponent->SetMovementEnabled(!isStageIntro);
		if (isStageIntro) {
			_playerMoveComponent->StopMovement();
		}
	}
	if (_playerFollowCameraComponent) {
		_playerFollowCameraComponent->SetStageIntroMode(isStageIntro);
	}
}

void MyGame::GameRuleSystemComponent::SetStageIntroUiVisible(bool visible)
{
	auto* canvas = ResolveStageIntroUiCanvas();
	if (!canvas) {
		return;
	}

	if (auto* entity = canvas->GetOwner()) {
		// NOTE: 紹介フェーズ以外では目標表示が通常HUDやリザルト表示と重ならないよう隠す。
		entity->SetVisible(visible);
	}
	if (visible && !_stageIntroUiAssetPath.empty()) {
		canvas->SetUiAssetPath(_stageIntroUiAssetPath);
		canvas->EnsureRuntimeLoaded();
	}
}

void MyGame::GameRuleSystemComponent::SetStageIntroHudHidden(bool hidden)
{
	auto* scene = GetScene();
	if (!scene && GetOwner()) {
		scene = GetOwner()->GetScene();
	}
	if (!scene) {
		return;
	}

	if (hidden) {
		if (!_stageIntroUiVisibility.empty()) {
			return;
		}

		for (const auto& entityPtr : scene->GetEntities()) {
			if (!entityPtr) {
				continue;
			}

			auto* entity = entityPtr.get();
			if (!_stageIntroUiEntityName.empty() &&
				entity->GetName() == _stageIntroUiEntityName) {
				continue;
			}
			if (!entity->GetComponent<Unnamed::UiCanvasComponent>()) {
				continue;
			}

			// NOTE: 紹介演出中は通常HUDを隠すが、個別UIの初期表示状態は後で復元する。
			_stageIntroUiVisibility.emplace(GetEntityGuid(entity), entity->IsVisible());
			entity->SetVisible(false);
		}
		return;
	}

	for (const auto& entityPtr : scene->GetEntities()) {
		if (!entityPtr) {
			continue;
		}

		auto* entity = entityPtr.get();
		const auto visibilityIt = _stageIntroUiVisibility.find(GetEntityGuid(entity));
		if (visibilityIt == _stageIntroUiVisibility.end()) {
			continue;
		}

		// NOTE: 紹介前の状態へ戻すことで、元から非表示のUIを誤って表示しない。
		entity->SetVisible(visibilityIt->second);
	}
	_stageIntroUiVisibility.clear();
}

void MyGame::GameRuleSystemComponent::UpdatePlayingPhase(float deltaTime)
{
	// NOTE: プレイ中はゴミスコア・ボール結果・ゴミ出現タイミングをまとめて監視する。
	_playingElapsedTime += deltaTime;
	UpdateTrashWaveTiming();
	UpdateTrashScore();
	UpdateBallResult(deltaTime);
}

void MyGame::GameRuleSystemComponent::UpdateTrashScore()
{
	// NOTE: 穴に入ったゴミはPlayerHoleComponentの検出結果から一度だけ加点する。
	if (_scoreComponent && _playerHoleComponent) {
		for (auto* entity : _playerHoleComponent->GetTrashInHole()) {
			if (!entity) {
				continue;
			}

			if (entity->GetComponent<TrashObjMoverComponent>()) {
				_scoreComponent->AddTrashIntoHoleScore(GetEntityGuid(entity));
			}

			if (auto* golfBall = entity->GetComponent<GolfBallComponent>()) {
				if (TryScoreBallCatch(*golfBall)) {
					FinishGame();
				}
			}
		}
	}

	auto* scene = GetScene();
	if (!scene && GetOwner()) {
		scene = GetOwner()->GetScene();
	}
	if (!scene || !_scoreComponent) {
		return;
	}

	// NOTE: 海の高さより下へ落ちたゴミは「海へ飛ばした」扱いで一度だけ加点する。
	for (const auto& entityPtr : scene->GetEntities()) {
		if (!entityPtr) {
			continue;
		}

		auto* entity = entityPtr.get();
		if (!entity->GetComponent<TrashObjMoverComponent>()) {
			continue;
		}

		auto* transform = entity->GetComponent<Unnamed::TransformComponent>();
		if (!transform || transform->GetPosition().y > _seaOutHeight) {
			continue;
		}

		_scoreComponent->AddTrashToSeaScore(GetEntityGuid(entity));
	}
}

void MyGame::GameRuleSystemComponent::UpdateBallResult(float deltaTime)
{
	// NOTE: ボールが無いシーンでは最大プレイ時間だけでリザルトへ進める。
	if (!_golfBallComponent) {
		if (_playingElapsedTime >= _maxBallFlightSeconds) {
			FinishGame();
		}
		return;
	}

	if (_golfBallComponent->IsInFlight()) {
		_hasBallLaunched = true;
	}
	if (_hasBallLaunched) {
		_ballFlightElapsedTime += deltaTime;
	}

	if (_playerHoleComponent && _playerHoleComponent->TryEnterGolfBall(*_golfBallComponent)) {
		// NOTE: PlayerHoleComponentのTick順に依存せず、動く穴に入った事実を最優先で成功扱いにする。
		(void)TryScoreBallCatch(*_golfBallComponent);
		FinishGame();
		return;
	}

	if (_golfBallComponent->HasEnteredHole()) {
		// NOTE: 穴へ入ったボールは落下演出で海高さを下回るため、OB/停止より先に成功扱いで確定する。
		(void)TryScoreBallCatch(*_golfBallComponent);
		FinishGame();
		return;
	}

	auto* ballEntity = _golfBallComponent->GetOwner();
	auto* transform = ballEntity ? ballEntity->GetComponent<Unnamed::TransformComponent>() : nullptr;
	if (transform && transform->GetPosition().y <= _ballSeaOutHeight) {
		// NOTE: 海面付近の一時的な沈み込みではなく、十分に落下した時点でNotホールインワンを確定する。
		_isOutOfBounds = true;
		if (_scoreComponent) {
			_scoreComponent->AddOutOfBoundsPenalty();
		}
		FinishGame();
		return;
	}

	if (_hasBallLaunched && IsBallStoppedForResult()) {
		// NOTE: ホールインワンのルールなので、止まった時点で失敗として終了する。
		FinishGame();
		return;
	}

	if (_hasBallLaunched && _ballFlightElapsedTime >= _maxBallFlightSeconds) {
		// NOTE: 15〜20秒想定を超えたら、止まりきらない場合でもリザルトへ進める。
		FinishGame();
	}
}

void MyGame::GameRuleSystemComponent::UpdateTrashWaveTiming()
{
	// NOTE: ボールが打たれたタイミングのゴミ出現を1回だけ発火する。
	if (!_hasTriggeredBallHitTrashWave) {
		TriggerTrashWave(_ballHitTrashWaveCount);
		_hasTriggeredBallHitTrashWave = true;
	}

	// NOTE: ボールが打たれて5〜8秒後に追加ゴミを出す想定のタイミングを発火する。
	if (!_hasTriggeredAfterHitTrashWave && _playingElapsedTime >= _afterHitTrashWaveDelay) {
		TriggerTrashWave(_afterHitTrashWaveCount);
		_hasTriggeredAfterHitTrashWave = true;
	}
}

void MyGame::GameRuleSystemComponent::TriggerTrashWave(int trashCount)
{
	// NOTE: 進行タイミングはGameRule、配置と種類選択はSpawnerへ分けて調整箇所を明確にする。
	if (!_trashObjSpawnerComponent) {
		ResolveRuntimeReferences();
	}
	if (_trashObjSpawnerComponent) {
		_trashObjSpawnerComponent->SpawnWave(trashCount);
	}
}

bool MyGame::GameRuleSystemComponent::TryScoreBallCatch(GolfBallComponent& golfBall)
{
	if (!golfBall.HasEnteredHole()) {
		return false;
	}
	if (_isHoleInOne) {
		return true;
	}

	// NOTE: 穴に重なっただけでなく、落下状態が確定した後だけホールインワンとして扱う。
	_hasBallLaunched = true;
	// NOTE: ボールキャッチ自体には加点せず、直接/バウンド後のホールインワンボーナスだけを採点対象にする。
	_isHoleInOne = true;
	if (!golfBall.HasBounced()) {
		_isDirectHoleInOne = true;
		if (_scoreComponent) {
			_scoreComponent->AddDirectHoleInOneBonus();
		}
	} else {
		if (_scoreComponent) {
			_scoreComponent->AddHoleInOneBonus();
		}
	}
	return true;
}

bool MyGame::GameRuleSystemComponent::ApplyClearUiForResult()
{
	auto* canvas = ResolveClearUiCanvas();
	if (!canvas) {
		return false;
	}
	const std::string* uiAssetPath = &_notHoleInOneUiAssetPath;
	if (_isDirectHoleInOne) {
		uiAssetPath = &_directHoleInOneUiAssetPath;
	} else if (_isHoleInOne) {
		uiAssetPath = &_holeInOneUiAssetPath;
	}

	// NOTE: UI自体はシーン上のClear_UIを使い回し、結果テキストだけをJSON差し替えで選ぶ。
	canvas->SetUiAssetPath(*uiAssetPath);
	canvas->EnsureRuntimeLoaded();
	if (auto* clearUiEntity = canvas->GetOwner()) {
		// NOTE: 差し替え後に表示し、古い結果UIが一瞬見える可能性を避ける。
		clearUiEntity->SetVisible(true);
	} else {
		return false;
	}
	return true;
}

bool MyGame::GameRuleSystemComponent::IsBallStoppedForResult() const
{
	if (!_golfBallComponent) {
		return false;
	}
	if (_golfBallComponent->HasEnteredHole()) {
		return false;
	}
	if (_golfBallComponent->IsBeingSucked()) {
		return false;
	}
	if (!_golfBallComponent->IsInFlight()) {
		return true;
	}

	const Vec3 velocity = _golfBallComponent->GetCurrentVelocity();
	const Vec3 position = _golfBallComponent->GetCurrentPosition();
	const float horizontalSpeedSq = velocity.x * velocity.x + velocity.z * velocity.z;
	const float stopSpeedSq =
		_ballStopResultVelocityThreshold * _ballStopResultVelocityThreshold;
	const bool isNearlyResting =
		horizontalSpeedSq <= stopSpeedSq &&
		std::abs(velocity.y) <= _ballStopResultVelocityThreshold &&
		position.y > _ballSeaOutHeight;

	// NOTE: 吸い込み中は穴への移行処理なので、速度が低くても停止失敗として扱わない。
	return isNearlyResting;
}

Unnamed::UiCanvasComponent* MyGame::GameRuleSystemComponent::ResolveClearUiCanvas() const
{
	auto* scene = GetScene();
	if (!scene && GetOwner()) {
		scene = GetOwner()->GetScene();
	}
	if (!scene) {
		return nullptr;
	}

	for (const auto& entityPtr : scene->GetEntities()) {
		if (!entityPtr) {
			continue;
		}

		auto* entity = entityPtr.get();
		auto* canvas = entity->GetComponent<Unnamed::UiCanvasComponent>();
		if (!canvas) {
			continue;
		}

		if (!_clearUiEntityName.empty() && entity->GetName() != _clearUiEntityName) {
			continue;
		}

		return canvas;
	}

	return nullptr;
}

Unnamed::UiCanvasComponent* MyGame::GameRuleSystemComponent::ResolveStageIntroUiCanvas() const
{
	auto* scene = GetScene();
	if (!scene && GetOwner()) {
		scene = GetOwner()->GetScene();
	}
	if (!scene) {
		return nullptr;
	}

	for (const auto& entityPtr : scene->GetEntities()) {
		if (!entityPtr) {
			continue;
		}

		auto* entity = entityPtr.get();
		auto* canvas = entity->GetComponent<Unnamed::UiCanvasComponent>();
		if (!canvas) {
			continue;
		}

		if (!_stageIntroUiEntityName.empty() &&
			entity->GetName() != _stageIntroUiEntityName) {
			continue;
		}

		return canvas;
	}

	return nullptr;
}

uint64_t MyGame::GameRuleSystemComponent::GetEntityGuid(Unnamed::Entity* entity) const
{
	// NOTE: スコアの二重加算防止に使うため、エンティティGUIDを安全に取り出す。
	return entity ? static_cast<uint64_t>(entity->GetGuid()) : 0;
}
