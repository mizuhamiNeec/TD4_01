#include "GameCountDownComponent.h"
#include <algorithm>

#ifdef _DEBUG
#include "imgui.h"
#endif

void MyGame::GameCountDownComponent::OnAttached() {
	// NOTE: エディタ上で保存された残り時間を初期値の範囲に丸めておく。
	_countDownTime = std::clamp(_countDownTime, 0.0f, _initialTime);
}

void MyGame::GameCountDownComponent::OnTick(float deltaTime) {
	// NOTE: カウントダウン停止中は残り時間を変化させない。
	if (!_isActive) {
		return;
	}

	// NOTE: 経過時間分だけ残り時間を減らし、0秒で停止状態にする。
	_countDownTime = std::max(0.0f, _countDownTime - deltaTime);
	if (_countDownTime <= 0.0f) {
		_isActive = false;
	}
}

void MyGame::GameCountDownComponent::OnRenderTick(float renderDeltaTime, float interpolationAlpha) {

}

void MyGame::GameCountDownComponent::OnDetached() {

}

std::string_view MyGame::GameCountDownComponent::GetStableName() const {
	return "mygame.GameCountDownComponent";
}
std::string_view MyGame::GameCountDownComponent::GetComponentName() const {
	return "Game Count Down Component";
}

#ifdef _DEBUG
void MyGame::GameCountDownComponent::DrawInspectorImGui() {
	// NOTE: デバッグ中にカウント秒数と状態を確認・調整できるようにする。
	ImGui::Text("=== Game Count Down Component ===");
	ImGui::DragFloat("Initial Time", &_initialTime, 0.1f, 0.0f, 999.0f, "%.2f sec");
	ImGui::DragFloat("Remaining Time", &_countDownTime, 0.1f, 0.0f, 999.0f, "%.2f sec");
	ImGui::Checkbox("Active", &_isActive);
	ImGui::Text("Finished: %s", IsFinished() ? "true" : "false");

	if (ImGui::Button("Start")) {
		Start(_initialTime);
	}
	ImGui::SameLine();
	if (ImGui::Button("Stop")) {
		Stop();
	}
	ImGui::SameLine();
	if (ImGui::Button("Reset")) {
		Reset();
	}
}
#endif

void MyGame::GameCountDownComponent::Deserialize(const Unnamed::JsonReader& reader) {
	// NOTE: JSONからカウントダウン設定と現在状態を復元する。
	if (auto val = reader.Read<float>("initialTime")) {
		_initialTime = std::max(0.0f, val.value());
	}
	if (auto val = reader.Read<float>("countDownTime")) {
		_countDownTime = std::clamp(val.value(), 0.0f, _initialTime);
	}
	if (auto val = reader.Read<bool>("isActive")) {
		_isActive = val.value();
	}
}

void MyGame::GameCountDownComponent::Serialize(Unnamed::JsonWriter& writer) const {
	// NOTE: エディタ保存時にカウントダウン設定をJSONへ書き込む。
	writer.Key("initialTime");
	writer.Write(_initialTime);
	writer.Key("countDownTime");
	writer.Write(_countDownTime);
	writer.Key("isActive");
	writer.Write(_isActive);
}

void MyGame::GameCountDownComponent::Start(float seconds) {
	// NOTE: 指定秒数でカウントダウンを開始する。
	_initialTime = std::max(0.0f, seconds);
	_countDownTime = _initialTime;
	_isActive = _countDownTime > 0.0f;
}

void MyGame::GameCountDownComponent::Stop() {
	// NOTE: 残り時間は保持したままカウントだけ停止する。
	_isActive = false;
}

void MyGame::GameCountDownComponent::Reset() {
	// NOTE: 初期時間へ戻して停止状態にする。
	_countDownTime = _initialTime;
	_isActive = false;
}

bool MyGame::GameCountDownComponent::IsActive() const {
	return _isActive;
}

bool MyGame::GameCountDownComponent::IsFinished() const {
	return !_isActive && _countDownTime <= 0.0f;
}

float MyGame::GameCountDownComponent::GetRemainingTime() const {
	return _countDownTime;
}

float MyGame::GameCountDownComponent::GetProgress01() const {
	// NOTE: 初期時間が0の場合は即終了扱いにして0を返す。
	if (_initialTime <= 0.0f) {
		return 0.0f;
	}

	return std::clamp(_countDownTime / _initialTime, 0.0f, 1.0f);
}
