#include "PlayerControlComponent.h"
#include "PlayerMoveComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/unnamed/subsystem/input/InputSystem.h"
#include "engine/unnamed/subsystem/input/KeyNameTable.h"
#include "engine/unnamed/subsystem/input/device/base/BaseInputDevice.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"
#include "./core/ComponentRegistry.h"
#include <core/math/Vec2.h>

#include <engine/unnamed/subsystem/input/device/gamepad/GamepadDevice.h>

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
		_currentMoveInput    = Vec2::zero;

		// NOTE: PlayerMoveComponent をキャッシュ
		GetOrCachePlayerMoveComponent();

		// NOTE: InputSystem からキーバインディングを設定
		SetupInputBindings();
	}

	void PlayerControlComponent::SetupInputBindings() {
		// キーバインドはコンフィグファイルに移動しました! そのほうが便利だからね!

		// コントローラー軸を登録
		auto* inputSystem = GetInputSystem();
		if (!inputSystem) { return; }

		using namespace Unnamed;

		inputSystem->BindAxis2D(
			_moveAxisName,
			{
				.device = InputDeviceType::GAMEPAD,
				.code   = VG_LX
			},
			INPUT_AXIS::X,
			1.0f
		);

		inputSystem->BindAxis2D(
			_moveAxisName,
			{
				.device = InputDeviceType::GAMEPAD,
				.code   = VG_LY
			},
			INPUT_AXIS::Y,
			1.0f
		);
	}

	void PlayerControlComponent::OnTick(float) {
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

		ImGui::Separator();
		ImGui::Text("Current Input");

		// 現在の入力を表示
		ImGui::Text(
			"Move Input: (%.2f, %.2f)", _currentMoveInput.x, _currentMoveInput.y
		);

		ImGui::Separator();

		// PlayerMoveComponent との接続状態を表示
		if (_playerMoveComponent) {
			ImGui::TextColored(
				ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "PlayerMoveComponent: Connected"
			);
		} else {
			ImGui::TextColored(
				ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "PlayerMoveComponent: Not Found"
			);
		}
	}
#endif

	void PlayerControlComponent::Deserialize(
		const Unnamed::JsonReader& reader
	) {
		// NOTE: JSONから値を読み込む
		if (auto val = reader.Read<std::string>("moveAxisName")) {
			_moveAxisName = val.value();
		}
	}

	void PlayerControlComponent::Serialize(Unnamed::JsonWriter& writer) const {
		// NOTE: 値をJSONに書き込む
		writer.Key("moveAxisName");
		writer.Write(_moveAxisName);
	}

	// -----------------------------------------------------------------------
	// 内部処理
	// -----------------------------------------------------------------------

	PlayerMoveComponent*
	PlayerControlComponent::GetOrCachePlayerMoveComponent() {
		// NOTE: キャッシュが無効な場合、再度取得を試みる
		if (!_playerMoveComponent && GetOwner()) {
			_playerMoveComponent = GetOwner()->GetComponent<
				PlayerMoveComponent>();
		}
		return _playerMoveComponent;
	}

	void PlayerControlComponent::ProcessInput() {
		// NOTE: PlayerMoveComponent が見つからない場合は何もしない
		auto* moveComponent = GetOrCachePlayerMoveComponent();
		if (!moveComponent) { return; }

		// NOTE: BaseComponent から InputSystem を取得
		const auto* inputSystem = GetInputSystem();
		if (!inputSystem) { return; }

		// NOTE: 各アクションから入力を作成

		_currentMoveInput = Vec2::zero;

		// キーボード入力
		{
			// 左右
			{
				if (inputSystem->IsHeld("moveright")) {
					_currentMoveInput.x += 1.0f;
				}
				if (inputSystem->IsHeld("moveleft")) {
					_currentMoveInput.x -= 1.0f;
				}
			}

			// 前後
			{
				if (inputSystem->IsHeld("forward")) {
					_currentMoveInput.y += 1.0f;
				}
				if (inputSystem->IsHeld("back")) {
					_currentMoveInput.y -= 1.0f;
				}
			}
		}

		// ゲームパッド入力
		{
			_currentMoveInput += inputSystem->Axis2D(_moveAxisName);
		}

		// NOTE: PlayerMoveComponent に移動方向を設定
		moveComponent->SetMoveDirection(_currentMoveInput);

		// NOTE: ジャンプアクションをチェック
		if (inputSystem->IsPressed("jump")) { moveComponent->Jump(); }
	}

	// NOTE: 忘れると死ぬやつ
	REGISTER_COMPONENT(PlayerControlComponent);
} // namespace MyGame
