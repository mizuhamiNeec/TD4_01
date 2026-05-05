#include "PlayerControlComponent.h"
#include "PlayerMoveComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/unnamed/subsystem/input/InputSystem.h"
#include "engine/unnamed/subsystem/input/KeyNameTable.h"
#include "engine/unnamed/subsystem/input/device/base/BaseInputDevice.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"
#include "./core/ComponentRegistry.h"
#include <core/math/Vec2.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#ifdef _DEBUG
#include "imgui.h"
#endif

namespace MyGame {

	// -----------------------------------------------------------------------
	// ライフサイクル
	// -----------------------------------------------------------------------

	void PlayerControlComponent::OnAttached() {
		// NOTE: コンポーネントがアタッチされたときに初期化
		_playerMoveComponent = nullptr;
		_currentMoveInput = Vec2::zero;

		// NOTE: PlayerMoveComponent をキャッシュ
		GetOrCachePlayerMoveComponent();

		// NOTE: InputSystem からキーバインディングを設定
		SetupInputBindings();
	}

	void PlayerControlComponent::SetupInputBindings() {
		// NOTE: ServiceLocator から InputSystem を取得
		auto* inputSystem = ServiceLocator::Get<Unnamed::InputSystem>();
		if (!inputSystem) {
			return;
		}

		// -----------------------------------------------------------------------
		// KeyNameTable からキーコードを取得
		// -----------------------------------------------------------------------

		// NOTE: KeyNameTable::FromString でキー名から InputKey を取得
		// キー名は小文字の "w", "a", "s", "d", "space" など
		auto optKeyW = Unnamed::KeyNameTable::FromString("w");
		auto optKeyA = Unnamed::KeyNameTable::FromString("a");
		auto optKeyS = Unnamed::KeyNameTable::FromString("s");
		auto optKeyD = Unnamed::KeyNameTable::FromString("d");
		auto optKeySpace = Unnamed::KeyNameTable::FromString("space");

		// キーが正しく取得できたか確認
		if (!optKeyW.has_value() || !optKeyA.has_value() ||
		    !optKeyS.has_value() || !optKeyD.has_value() ||
		    !optKeySpace.has_value()) {
			return;
		}

		// -----------------------------------------------------------------------
		// WASD キーを MoveAxis に バインド
		// -----------------------------------------------------------------------

		// NOTE: W キー - 前方移動（Y軸の正方向）
		inputSystem->BindAxis2D(_moveAxisName, optKeyW.value(), Unnamed::INPUT_AXIS::Y, 1.0f);

		// NOTE: S キー - 後方移動（Y軸の負方向）
		inputSystem->BindAxis2D(_moveAxisName, optKeyS.value(), Unnamed::INPUT_AXIS::Y, -1.0f);

		// NOTE: A キー - 左移動（X軸の負方向）
		inputSystem->BindAxis2D(_moveAxisName, optKeyA.value(), Unnamed::INPUT_AXIS::X, -1.0f);

		// NOTE: D キー - 右移動（X軸の正方向）
		inputSystem->BindAxis2D(_moveAxisName, optKeyD.value(), Unnamed::INPUT_AXIS::X, 1.0f);

		// -----------------------------------------------------------------------
		// スペースキーを Jump アクションに バインド
		// -----------------------------------------------------------------------

		// NOTE: Space キー - ジャンプアクション
		inputSystem->BindAction(_jumpActionName, optKeySpace.value());
	}

	void PlayerControlComponent::OnTick(float ) {
		// NOTE: 毎フレーム入力を処理
		ProcessInput();
	}

	void PlayerControlComponent::OnDetached() {
		// NOTE: クリーンアップ処理
		_playerMoveComponent = nullptr;
	}

	// -----------------------------------------------------------------------
	// 入力設定
	// -----------------------------------------------------------------------

	void PlayerControlComponent::SetMoveAxisName(const std::string& axisName) {
		_moveAxisName = axisName;
	}

	void PlayerControlComponent::SetJumpActionName(const std::string& actionName) {
		_jumpActionName = actionName;
	}

	Vec2 PlayerControlComponent::GetMoveInput() const {
		return _currentMoveInput;
	}

	// -----------------------------------------------------------------------
	// BaseComponent override
	// -----------------------------------------------------------------------

	std::string_view PlayerControlComponent::GetStableName() const {
		return "mygame.PlayerControlComponent";
	}

	std::string_view PlayerControlComponent::GetComponentName() const {
		return "Player Control Component";
	}

#ifdef _DEBUG
	void PlayerControlComponent::DrawInspectorImGui() {
		ImGui::Text("=== Player Control Component ===");

		ImGui::Separator();
		ImGui::Text("Input Configuration");

		// 入力設定の表示
		ImGui::Text("Move Axis: %s", _moveAxisName.c_str());
		ImGui::Text("Jump Action: %s", _jumpActionName.c_str());

		ImGui::Separator();
		ImGui::Text("Current Input");

		// 現在の入力を表示
		ImGui::Text("Move Input: (%.2f, %.2f)", _currentMoveInput.x, _currentMoveInput.y);

		ImGui::Separator();

		// PlayerMoveComponent との接続状態を表示
		if (_playerMoveComponent) {
			ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "PlayerMoveComponent: Connected");
		} else {
			ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "PlayerMoveComponent: Not Found");
		}
	}
#endif

	void PlayerControlComponent::Deserialize(const Unnamed::JsonReader& reader) {
		// NOTE: JSONから値を読み込む
		if (auto val = reader.Read<std::string>("moveAxisName")) {
			_moveAxisName = val.value();
		}
		if (auto val = reader.Read<std::string>("jumpActionName")) {
			_jumpActionName = val.value();
		}
	}

	void PlayerControlComponent::Serialize(Unnamed::JsonWriter& writer) const {
		// NOTE: 値をJSONに書き込む
		writer.Key("moveAxisName");
		writer.Write(_moveAxisName);
		writer.Key("jumpActionName");
		writer.Write(_jumpActionName);
	}

	// -----------------------------------------------------------------------
	// 内部処理
	// -----------------------------------------------------------------------

	PlayerMoveComponent* PlayerControlComponent::GetOrCachePlayerMoveComponent() {
		// NOTE: キャッシュが無効な場合、再度取得を試みる
		if (!_playerMoveComponent && GetOwner()) {
			_playerMoveComponent = GetOwner()->GetComponent<PlayerMoveComponent>();
		}
		return _playerMoveComponent;
	}

	void PlayerControlComponent::ProcessInput() {
		// NOTE: PlayerMoveComponent が見つからない場合は何もしない
		auto* moveComponent = GetOrCachePlayerMoveComponent();
		if (!moveComponent) {
			return;
		}

		// NOTE: ServiceLocator から InputSystem を取得
		auto* inputSystem = ServiceLocator::Get<Unnamed::InputSystem>();
		if (!inputSystem) {
			return;
		}

		// NOTE: 2D軸から移動入力を取得
		_currentMoveInput = inputSystem->Axis2D(_moveAxisName);

		// NOTE: PlayerMoveComponent に移動方向を設定
		moveComponent->SetMoveDirection(_currentMoveInput);

		// NOTE: ジャンプアクションをチェック
		if (inputSystem->IsPressed(_jumpActionName)) {
			moveComponent->Jump();
		}
	}

	// NOTE: 忘れると死ぬやつ
	REGISTER_COMPONENT(PlayerControlComponent);

} // namespace MyGame
