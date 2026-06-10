#include "PlayerMoveComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "./core/ComponentRegistry.h"
#include <algorithm>
#include <cmath>
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
		_horizontalVelocity = Vec3::zero;
		_verticalVelocity = 0.0f;
		_bIsGrounded = true;
		ClearMoveBasis();
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
		// 入力を直接位置に反映すると急停止になるため、速度を介して加減速させる。
		const Vec3 basisForward =
			_hasMoveBasis ? _moveBasisForward : transform->Forward();
		const Vec3 basisRight =
			_hasMoveBasis ? _moveBasisRight : transform->Right();
		Vec3 moveDirection =
			basisForward * _moveDirection.y + basisRight * _moveDirection.x;
		moveDirection.y = 0.0f;
		if (!moveDirection.IsZero(0.0001f)) {
			moveDirection = moveDirection.Normalized();
		}

		const Vec3 desiredHorizontalVelocity = moveDirection * _moveSpeed;
		const float response = moveDirection.IsZero(0.0001f) ? _deceleration : _acceleration;
		const float velocityAlpha = 1.0f - std::exp(-std::max(0.0f, response) * deltaTime);
		_horizontalVelocity = _horizontalVelocity + (desiredHorizontalVelocity - _horizontalVelocity) * velocityAlpha;
		if (_horizontalVelocity.Length() < 0.001f && moveDirection.IsZero(0.0001f)) {
			_horizontalVelocity = Vec3::zero;
		}

		Vec3 horizontalMovement = _horizontalVelocity * deltaTime;

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

		// 移動制限を適用
		newPos = ClampMoveLimit(newPos);

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
		clamped.ClampLength(0.0f,1.0f);
		_moveDirection = clamped;
	}

	void PlayerMoveComponent::SetMoveBasis(
		const Vec3& forward,
		const Vec3& right
	) {
		Vec3 planarForward = forward;
		Vec3 planarRight = right;
		planarForward.y = 0.0f;
		planarRight.y = 0.0f;

		if (planarForward.SqrLength() <= 0.0001f ||
		    planarRight.SqrLength() <= 0.0001f) {
			ClearMoveBasis();
			return;
		}

		_moveBasisForward = planarForward.Normalized();
		_moveBasisRight = planarRight.Normalized();
		_hasMoveBasis = true;
	}

	void PlayerMoveComponent::ClearMoveBasis() {
		_hasMoveBasis = false;
		_moveBasisForward = Vec3::forward;
		_moveBasisRight = Vec3::right;
	}

	Vec2 PlayerMoveComponent::GetMoveDirection() const {
		return _moveDirection;
	}

	void PlayerMoveComponent::SetMoveSpeed(float speed) {
		_moveSpeed = std::max(0.0f, speed);
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

	void PlayerMoveComponent::SetMoveLimitEnabled(bool enabled) {
		_bUseMoveLimit = enabled;
	}

	bool PlayerMoveComponent::IsMoveLimitEnabled() const {
		return _bUseMoveLimit;
	}

	void PlayerMoveComponent::SetMoveLimitCenter(const Vec3& center) {
		_moveLimitCenter = center;
	}

	Vec3 PlayerMoveComponent::GetMoveLimitCenter() const {
		return _moveLimitCenter;
	}

	void PlayerMoveComponent::SetMoveLimitRadius(float radius) {
		_moveLimitRadius = std::max(0.0f, radius);
	}

	float PlayerMoveComponent::GetMoveLimitRadius() const {
		return _moveLimitRadius;
	}

	Vec3 PlayerMoveComponent::ClampMoveLimit(const Vec3& position) const {
		if (!_bUseMoveLimit || _moveLimitRadius <= 0.0f) {
			return position;
		}

		Vec3 clampedPosition = position;

		// NOTE: 移動制限はXZ平面の円形範囲として扱い、Yはジャンプ/重力のためそのまま残す
		const float offsetX = position.x - _moveLimitCenter.x;
		const float offsetZ = position.z - _moveLimitCenter.z;
		const float distanceSq = offsetX * offsetX + offsetZ * offsetZ;
		const float radiusSq = _moveLimitRadius * _moveLimitRadius;

		if (distanceSq <= radiusSq) {
			return clampedPosition;
		}

		const float distance = std::sqrt(distanceSq);
		if (distance <= 0.0001f) {
			clampedPosition.x = _moveLimitCenter.x;
			clampedPosition.z = _moveLimitCenter.z;
			return clampedPosition;
		}

		const float invDistance = 1.0f / distance;
		clampedPosition.x = _moveLimitCenter.x + offsetX * invDistance * _moveLimitRadius;
		clampedPosition.z = _moveLimitCenter.z + offsetZ * invDistance * _moveLimitRadius;
		return clampedPosition;
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
		ImGui::SliderFloat("Acceleration", &_acceleration, 0.0f, 80.0f);
		ImGui::SliderFloat("Deceleration", &_deceleration, 0.0f, 80.0f);

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
		ImGui::Text("Move Limit");
		ImGui::Checkbox("Use Move Limit", &_bUseMoveLimit);
		ImGui::DragFloat3("Limit Center", &_moveLimitCenter.x, 0.1f);
		ImGui::DragFloat("Limit Radius", &_moveLimitRadius, 0.1f, 0.0f, 10000.0f);

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
			_moveSpeed = std::max(0.0f, val.value());
		}
		if (auto val = reader.Read<float>("acceleration")) {
			_acceleration = std::max(0.0f, val.value());
		}
		if (auto val = reader.Read<float>("deceleration")) {
			_deceleration = std::max(0.0f, val.value());
		}
		if (auto val = reader.Read<float>("jumpForce")) {
			_jumpForce = val.value();
		}
		if (auto val = reader.Read<float>("gravity")) {
			_gravity = val.value();
		}
		if (auto val = reader.Read<bool>("useMoveLimit")) {
			_bUseMoveLimit = val.value();
		}
		if (auto val = reader.Read<float>("moveLimitCenterX")) {
			_moveLimitCenter.x = val.value();
		}
		if (auto val = reader.Read<float>("moveLimitCenterY")) {
			_moveLimitCenter.y = val.value();
		}
		if (auto val = reader.Read<float>("moveLimitCenterZ")) {
			_moveLimitCenter.z = val.value();
		}
		if (auto val = reader.Read<float>("moveLimitRadius")) {
			SetMoveLimitRadius(val.value());
		}
	}

	void PlayerMoveComponent::Serialize(Unnamed::JsonWriter& writer) const {
		// NOTE: 値をJSONに書き込む
		// key, value の順序で Write() を呼び出す
		writer.Key("moveSpeed");
		writer.Write(_moveSpeed);
		writer.Key("acceleration");
		writer.Write(_acceleration);
		writer.Key("deceleration");
		writer.Write(_deceleration);
		writer.Key("jumpForce");
		writer.Write(_jumpForce);
		writer.Key("gravity");
		writer.Write(_gravity);
		writer.Key("useMoveLimit");
		writer.Write(_bUseMoveLimit);
		writer.Key("moveLimitCenterX");
		writer.Write(_moveLimitCenter.x);
		writer.Key("moveLimitCenterY");
		writer.Write(_moveLimitCenter.y);
		writer.Key("moveLimitCenterZ");
		writer.Write(_moveLimitCenter.z	);


		writer.Key("moveLimitRadius");
		writer.Write(_moveLimitRadius);
	}

	// NOTE: 忘れると死ぬやつ
	REGISTER_COMPONENT(PlayerMoveComponent);

} // namespace MyGame
