#include "PlayerMoveComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "./core/ComponentRegistry.h"
#include <core/math/Vec2.h>
#include <core/math/Vec3.h>

#ifdef _DEBUG
#include "imgui.h"
#endif
#include <engine/unnamed/framework/components/TransformComponent.h>

namespace MyGame {

	// -----------------------------------------------------------------------
	// ライフサイクル
	// -----------------------------------------------------------------------

	void PlayerMoveComponent::OnAttached() {
		// NOTE: コンポーネントがアタッチされたときに初期化
		_moveDirection = Vec2(0.0f, 0.0f);
		_verticalVelocity = 0.0f;
		_bIsGrounded = true;
	}

	void PlayerMoveComponent::OnTick(float deltaTime) {
		// NOTE: 毎フレーム更新 - Transform の位置を更新する

		// TransformComponent を取得
		if (!GetOwner()) {
			return;
		}
		auto *transform = GetOwner()->GetComponent<Unnamed::TransformComponent>();
		if (!transform) {
			return;
		}

		// 現在の位置を取得
		Vec3 currentPos = transform->GetPosition();

		// 水平移動の計算
		// NOTE: Transform の Forward と Right を利用して移動方向を計算
		Vec3 moveDirection = transform->Forward() * _moveDirection.y + transform->Right() * _moveDirection.x;
		Vec3 horizontalMovement = moveDirection * _moveSpeed * deltaTime;

		// 垂直移動の計算（重力）
		if (!_bIsGrounded) {
			_verticalVelocity -= _gravity * deltaTime;
		} else {
			_verticalVelocity = 0.0f;
		}

		Vec3 verticalMovement = Vec3(0.0f, _verticalVelocity * deltaTime, 0.0f);

		// 新しい位置を計算
		Vec3 newPos = currentPos + horizontalMovement + verticalMovement;

		// NOTE: 簡易的なグラウンドチェック（Y < 0ならグラウンド）
		if (newPos.y <= 0.0f) {
			newPos.y = 0.0f;
			_bIsGrounded = true;
		} else {
			_bIsGrounded = false;
		}

		// 位置を更新
		transform->SetPosition(newPos);
	}

	void PlayerMoveComponent::OnDetached() {
		// NOTE: クリーンアップ処理が必要であればここで実施
	}

	// -----------------------------------------------------------------------
	// 移動制御
	// -----------------------------------------------------------------------

	void PlayerMoveComponent::SetMoveDirection(const Vec2& direction) {
		// NOTE: 移動方向をクランプして保存
		Vec2 clamped = direction;
		clamped.Clamp(Vec2(-1.0f, -1.0f), Vec2(1.0f, 1.0f));
		_moveDirection = clamped;
	}

	Vec2 PlayerMoveComponent::GetMoveDirection() const {
		return _moveDirection;
	}

	void PlayerMoveComponent::SetMoveSpeed(float speed) {
		_moveSpeed = speed;
	}

	float PlayerMoveComponent::GetMoveSpeed() const {
		return _moveSpeed;
	}

	void PlayerMoveComponent::Jump() {
		// NOTE: 地面に接触しているときのみジャンプ可能
		if (_bIsGrounded) {
			// ジャンプ初速度を設定
			_verticalVelocity = _jumpForce;
			_bIsGrounded = false;

			// NOTE: 補間履歴を再同期
			// ジャンプは急激な速度変化なので、補間履歴をリセット
			if (GetOwner()) {
				auto *transform = GetOwner()->GetComponent<Unnamed::TransformComponent>();
				if (transform) {
					transform->RequestInterpolationResync();
				}
			}
		}
	}

	bool PlayerMoveComponent::IsGrounded() const {
		return _bIsGrounded;
	}

	// -----------------------------------------------------------------------
	// BaseComponent override
	// -----------------------------------------------------------------------

	std::string_view PlayerMoveComponent::GetStableName() const {
		return "mygame.PlayerMoveComponent";
	}

	std::string_view PlayerMoveComponent::GetComponentName() const {
		return "Player Move Component";
	}

#ifdef _DEBUG
	void PlayerMoveComponent::DrawInspectorImGui() {
		ImGui::Text("=== Player Move Component ===");

		// 移動パラメータ表示
		ImGui::Separator();
		ImGui::Text("Move Parameters");

		// 移動方向表示
		float dirX = _moveDirection.x;
		float dirY = _moveDirection.y;
		ImGui::SliderFloat("Direction X", &dirX, -1.0f, 1.0f);
		ImGui::SliderFloat("Direction Y", &dirY, -1.0f, 1.0f);
		_moveDirection = Vec2(dirX, dirY);

		// 移動速度スライダー
		ImGui::SliderFloat("Move Speed", &_moveSpeed, 0.0f, 20.0f);

		ImGui::Separator();
		ImGui::Text("Jump Parameters");

		// ジャンプパラメータ
		ImGui::SliderFloat("Jump Force", &_jumpForce, 0.0f, 15.0f);
		ImGui::SliderFloat("Gravity", &_gravity, 0.0f, 20.0f);

		ImGui::Separator();
		ImGui::Text("Status");

		// 状態表示
		ImGui::Checkbox("Is Grounded", &_bIsGrounded);
		ImGui::Text("Vertical Velocity: %.3f", _verticalVelocity);

		ImGui::Separator();

		// ジャンプボタン
		if (ImGui::Button("Jump", ImVec2(100, 0))) {
			Jump();
		}
	}
#endif

	void PlayerMoveComponent::Deserialize(const Unnamed::JsonReader& reader) {
		// NOTE: JSONから値を読み込む
		// Read() は std::optional<T> を返すため、value_or() で既定値を指定
		if (auto val = reader.Read<float>("moveSpeed")) {
			_moveSpeed = val.value();
		}
		if (auto val = reader.Read<float>("jumpForce")) {
			_jumpForce = val.value();
		}
		if (auto val = reader.Read<float>("gravity")) {
			_gravity = val.value();
		}
	}

	void PlayerMoveComponent::Serialize(Unnamed::JsonWriter& writer) const {
		// NOTE: 値をJSONに書き込む
		// key, value の順序で Write() を呼び出す
		writer.Key("moveSpeed");
		writer.Write(_moveSpeed);
		writer.Key("jumpForce");
		writer.Write(_jumpForce);
		writer.Key("gravity");
		writer.Write(_gravity);
	}

	// NOTE: 忘れると死ぬやつ
	REGISTER_COMPONENT(PlayerMoveComponent);

} // namespace MyGame
