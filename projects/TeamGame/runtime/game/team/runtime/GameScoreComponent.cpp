#include "GameScoreComponent.h"

#ifdef _DEBUG
#include "imgui.h"
#endif

void MyGame::GameScoreComponent::OnAttached() {
	// NOTE: シーン開始時は前回プレイの一度きり加算記録を残さない。
	ResetScore();
}

void MyGame::GameScoreComponent::OnTick(float deltaTime) {}

void MyGame::GameScoreComponent::OnRenderTick(float renderDeltaTime, float interpolationAlpha) {}

void MyGame::GameScoreComponent::OnDetached() {}

std::string_view MyGame::GameScoreComponent::GetStableName() const {
	return "mygame.GameScoreComponent";
}

std::string_view MyGame::GameScoreComponent::GetComponentName() const {
	return "Game Score Component";
}

#ifdef _DEBUG
void MyGame::GameScoreComponent::DrawInspectorImGui() {
	// NOTE: デバッグ中にスコア設定と加算状況を確認できるようにする。
	ImGui::Text("=== Game Score Component ===");
	ImGui::Text("Score: %d", _score);
	ImGui::DragInt("Trash To Sea Score", &_trashToSeaScore, 1, -99999, 99999);
	ImGui::DragInt("Trash Into Hole Score", &_trashIntoHoleScore, 1, -99999, 99999);
	ImGui::DragInt("Ball Catch Score", &_ballCatchScore, 1, -99999, 99999);
	ImGui::DragInt("Hole In One Score", &_holeInOneScore, 1, -99999, 99999);
	ImGui::DragInt("Direct Hole In One Score", &_directHoleInOneScore, 1, -99999, 99999);
	ImGui::DragInt("OB Penalty Score", &_obPenaltyScore, 1, -99999, 99999);
	ImGui::Text("Trash To Sea Count: %zu", _scoredTrashToSeaGuids.size());
	ImGui::Text("Trash Into Hole Count: %zu", _scoredTrashIntoHoleGuids.size());

	if (ImGui::Button("Reset Score")) {
		ResetScore();
	}
}
#endif

void MyGame::GameScoreComponent::Deserialize(const Unnamed::JsonReader& reader) {
	// NOTE: JSONからスコア計算に使う点数設定を読み込む。
	if (auto val = reader.Read<int>("score")) {
		_score = val.value();
	}
	if (auto val = reader.Read<int>("trashToSeaScore")) {
		_trashToSeaScore = val.value();
	}
	if (auto val = reader.Read<int>("trashIntoHoleScore")) {
		_trashIntoHoleScore = val.value();
	}
	if (auto val = reader.Read<int>("ballCatchScore")) {
		_ballCatchScore = val.value();
	}
	if (auto val = reader.Read<int>("holeInOneScore")) {
		_holeInOneScore = val.value();
	}
	if (auto val = reader.Read<int>("directHoleInOneScore")) {
		_directHoleInOneScore = val.value();
	}
	if (auto val = reader.Read<int>("obPenaltyScore")) {
		_obPenaltyScore = val.value();
	}
}

void MyGame::GameScoreComponent::Serialize(Unnamed::JsonWriter& writer) const {
	// NOTE: 点数設定をJSONへ書き込んでエディタ調整値を保存する。
	writer.Key("score");
	writer.Write(_score);
	writer.Key("trashToSeaScore");
	writer.Write(_trashToSeaScore);
	writer.Key("trashIntoHoleScore");
	writer.Write(_trashIntoHoleScore);
	writer.Key("ballCatchScore");
	writer.Write(_ballCatchScore);
	writer.Key("holeInOneScore");
	writer.Write(_holeInOneScore);
	writer.Key("directHoleInOneScore");
	writer.Write(_directHoleInOneScore);
	writer.Key("obPenaltyScore");
	writer.Write(_obPenaltyScore);
}

void MyGame::GameScoreComponent::ResetScore() {
	// NOTE: 合計点と一度だけ加算するための記録をまとめて初期化する。
	_score = 0;
	_hasAddedBallCatchScore = false;
	_hasAddedHoleInOneScore = false;
	_hasAddedDirectHoleInOneScore = false;
	_hasAddedOutOfBoundsPenalty = false;
	_scoredTrashToSeaGuids.clear();
	_scoredTrashIntoHoleGuids.clear();
}

void MyGame::GameScoreComponent::AddScore(int value) {
	// NOTE: 加点・減点を同じ入口で扱い、合計スコアへ反映する。
	_score += value;
}

bool MyGame::GameScoreComponent::AddTrashToSeaScore(uint64_t trashGuid) {
	// NOTE: 同じゴミで海スコアが複数回入らないようGUIDで管理する。
	if (trashGuid == 0 || _scoredTrashToSeaGuids.find(trashGuid) != _scoredTrashToSeaGuids.end()) {
		return false;
	}

	_scoredTrashToSeaGuids.insert(trashGuid);
	AddScore(_trashToSeaScore);
	return true;
}

bool MyGame::GameScoreComponent::AddTrashIntoHoleScore(uint64_t trashGuid) {
	// NOTE: 同じゴミで穴スコアが複数回入らないようGUIDで管理する。
	if (trashGuid == 0 || _scoredTrashIntoHoleGuids.find(trashGuid) != _scoredTrashIntoHoleGuids.end()) {
		return false;
	}

	_scoredTrashIntoHoleGuids.insert(trashGuid);
	AddScore(_trashIntoHoleScore);
	return true;
}

bool MyGame::GameScoreComponent::AddBallCatchScore() {
	// NOTE: ボールキャッチの大型ボーナスは1プレイにつき1回だけ加算する。
	if (_hasAddedBallCatchScore) {
		return false;
	}

	_hasAddedBallCatchScore = true;
	AddScore(_ballCatchScore);
	return true;
}

bool MyGame::GameScoreComponent::AddHoleInOneBonus() {
	// NOTE: ホールインワンボーナスは重複しないようにする。
	if (_hasAddedHoleInOneScore) {
		return false;
	}

	_hasAddedHoleInOneScore = true;
	AddScore(_holeInOneScore);
	return true;
}

bool MyGame::GameScoreComponent::AddDirectHoleInOneBonus() {
	// NOTE: ダイレクトホールインワンボーナスは重複しないようにする。
	if (_hasAddedDirectHoleInOneScore) {
		return false;
	}

	_hasAddedDirectHoleInOneScore = true;
	AddScore(_directHoleInOneScore);
	return true;
}

bool MyGame::GameScoreComponent::AddOutOfBoundsPenalty() {
	// NOTE: OBペナルティは終了判定時に1回だけ加算する。
	if (_hasAddedOutOfBoundsPenalty) {
		return false;
	}

	_hasAddedOutOfBoundsPenalty = true;
	AddScore(_obPenaltyScore);
	return true;
}

int MyGame::GameScoreComponent::GetScore() const {
	return _score;
}
