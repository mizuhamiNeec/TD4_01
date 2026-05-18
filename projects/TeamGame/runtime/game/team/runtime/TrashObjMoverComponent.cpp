#include "TrashObjMoverComponent.h"
#include "./core/ComponentRegistry.h"
#include "engine/unnamed/framework/entity/Entity.h"

#ifdef _DEBUG
#include "imgui.h"
#endif

#include <engine/unnamed/framework/components/TransformComponent.h>
#include <engine/world/World.h>

namespace MyGame {

	// ===================================================================
	// ライフサイクル メソッド
	// ===================================================================

	void TrashObjMoverComponent::OnAttached() {
		// NOTE: コンポーネントがアタッチされたときに初期化

		// 物理エンジンをワールドから取得
		_physicsEngine = GetWorld() ? &GetWorld()->GetPhysicsEngine() : nullptr;

		// 衝突リゾルバを作成
		_collisionResolver = std::make_unique<Unnamed::BoxKinematicCollisionResolver>(
			_physicsEngine);

		// TransformComponentをキャッシュ
		if (GetOwner()) {
			_transform = GetOwner()->GetComponent<Unnamed::TransformComponent>();
			if (_transform) {
				_position = _transform->GetPosition();
				// 初期回転を保存（着地時にこれに戻す）
				_initialRotation = _transform->GetRotation();
				_currentRotation = _initialRotation;
			}
		}

		// 衝突レゾルバのハルを初期化
		if (_collisionResolver) {
			_collisionResolver->UpdateHull(_position, _halfSize);
		}
	}

	void TrashObjMoverComponent::OnTick(float deltaTime) {
		// -----------------------------------------------------------------------
		// 吸い込み処理（落下状態 または 地上で吸い込み中）
		// -----------------------------------------------------------------------
		if (_bIsBeingSucked && _holeSuckPower > 0.0f) {
			// 落下状態でも地上でも、吸い込み力があれば適用
			UpdateHoleSuck(deltaTime);
		}

		// NOTE: 落下状態でない場合のみ物理更新を実行
		if (_bIsFalling) {
			// 落下状態：重力のみを適用
			_velocity.y -= _gravity * deltaTime;
			_position += _velocity * deltaTime;

			// 空中での回転を更新
			UpdateAirRotation(deltaTime);
		} else {
			// -----------------------------------------------------------------------
			// 物理計算（重力）
			// -----------------------------------------------------------------------
			UpdatePhysics(deltaTime);

			// -----------------------------------------------------------------------
			// 衝突応答 & SlideMove（リゾルバが位置を更新）
			// -----------------------------------------------------------------------
			// NOTE: SlideMove が _position と _velocity を更新する
			// 地面衝突、バウンス、速度クリップはすべてリゾルバが処理
			if (_collisionResolver) {
				auto *boxKCR = dynamic_cast<Unnamed::BoxKinematicCollisionResolver *>(_collisionResolver.get());
				if (boxKCR) {
					boxKCR->SlideMove(_position, _velocity, deltaTime);
				}
			}

			// -----------------------------------------------------------------------
			// 速度上限でクランプ
			// -----------------------------------------------------------------------
			ClampVelocity();

			// -----------------------------------------------------------------------
			// 落下中・衝突中による回転更新
			// -----------------------------------------------------------------------
			bool bWasGrounded = _bIsGrounded;
			_bIsGrounded = (_velocity.y >= -_stopVelocityThreshold && _velocity.y <= _stopVelocityThreshold);

			if (!_bIsGrounded) {
				// 空中：速度方向に回転
				UpdateAirRotation(deltaTime);
			} else if (bWasGrounded != _bIsGrounded && _bIsGrounded) {
				// 着地した：回転をリセット開始
				_bIsResetingRotation = true;
			}

			// 回転リセット中なら続行
			if (_bIsResetingRotation) {
				UpdateRotationReset(deltaTime);
			}
		}

		// -----------------------------------------------------------------------
		// TransformComponent と同期
		// -----------------------------------------------------------------------
		if (_transform) {
			_transform->SetPosition(_position);
			_transform->SetRotation(_currentRotation);
			_transform->RequestInterpolationResync();
		}
	}

	void TrashObjMoverComponent::OnDetached() {
		// NOTE: クリーンアップ処理
		_collisionResolver.reset();
		_transform = nullptr;
	}

	// ===================================================================
	// 公開インターフェース - ゴミオブジェクト設定
	// ===================================================================

	void TrashObjMoverComponent::SetMass(float mass) {
		_mass = std::max(0.1f, mass);
	}

	float TrashObjMoverComponent::GetMass() const {
		return _mass;
	}

	Vec3 TrashObjMoverComponent::GetCurrentPosition() const {
		return _position;
	}

	Vec3 TrashObjMoverComponent::GetCurrentVelocity() const {
		return _velocity;
	}

	// ===================================================================
	// 公開インターフェース - 落下状態管理
	// ===================================================================

	bool TrashObjMoverComponent::IsFalling() const {
		return _bIsFalling;
	}

	void TrashObjMoverComponent::SetFalling(bool isFalling) {
		// NOTE: ゴミを落下状態に設定

		_bIsFalling = isFalling;

		if (isFalling) {
			// NOTE: 落下状態に移行
			// 現在位置をTransformから取得して同期
			if (_transform) {
				_position = _transform->GetPosition();
			}
		}
	}

	// ===================================================================
	// 公開インターフェース - 穴への吸い込み
	// ===================================================================

	void TrashObjMoverComponent::SetHoleSuckPosition(const Vec3 &holePosition, float suckPower) {
		// NOTE: 穴への吸い込み処理を開始

		_holeSuckPosition = holePosition;
		_holeSuckPower = std::max(0.0f, std::min(1.0f, suckPower)); // 0.0～1.0に正規化
		_bIsBeingSucked = true;
	}

	void TrashObjMoverComponent::ClearHoleSuckPosition() {
		// NOTE: 穴への吸い込み処理を停止

		_holeSuckPower = 0.0f;
		_bIsBeingSucked = false;
	}

	// ===================================================================
	//  BaseComponent の必須オーバーライド
	// ===================================================================

	std::string_view TrashObjMoverComponent::GetStableName() const {
		return "mygame.TrashObjMoverComponent";
	}

	std::string_view TrashObjMoverComponent::GetComponentName() const {
		return "Trash Object Mover Component";
	}

#ifdef _DEBUG
	void TrashObjMoverComponent::DrawInspectorImGui() {
		ImGui::Text("=== Trash Object Mover Component ===");

		// ボックスサイズ調整
		if (ImGui::DragFloat3("Box Half Size", &_halfSize.x, 0.25f, 0.1f, 100.0f, "%.2f")) {
			if (_collisionResolver) {
				_collisionResolver->UpdateHull(_position, _halfSize);
			}
		}

		ImGui::Separator();

		// -----------------------------------------------------------------------
		// 落下状態表示
		// -----------------------------------------------------------------------
		ImGui::Text("Falling State: %s", _bIsFalling ? "YES" : "NO");

		if (ImGui::Button("Set Falling")) {
			SetFalling(true);
		}
		ImGui::SameLine();
		if (ImGui::Button("Stop Falling")) {
			SetFalling(false);
		}

		ImGui::Separator();
		ImGui::Text("Trash Object Properties");

		// -----------------------------------------------------------------------
		// パラメータ調整
		// -----------------------------------------------------------------------
		ImGui::Text("=== Physics Parameters ===");
		ImGui::SliderFloat("Mass", &_mass, 0.1f, 100.0f, "%.2f kg");
		ImGui::SliderFloat("Gravity", &_gravity, 0.0f, 20.0f, "%.2f");
		ImGui::SliderFloat("Max Speed Clamp", &_maxSpeedClamp, 0.0f, 100.0f, "%.2f");
		ImGui::SliderFloat("Bounce Damping", &_bounceDamping, 0.0f, 1.0f, "%.3f");
		ImGui::SliderFloat("Friction Coefficient", &_frictionCoefficient, 0.0f, 1.0f, "%.3f");

		ImGui::Separator();

		// -----------------------------------------------------------------------
		// 回転パラメータ
		// -----------------------------------------------------------------------
		ImGui::Text("=== Air Rotation Parameters ===");
		ImGui::SliderFloat("Air Rotation Speed (deg/sec)", &_airRotationSpeed, 0.0f, 1080.0f, "%.2f");
		ImGui::SliderFloat("Rotation Reset Speed", &_rotationResetSpeed, 0.0f, 10.0f, "%.2f");

		ImGui::Separator();
		ImGui::Text("=== Tilt Toward Movement ===");
		ImGui::SliderFloat("Tilt Strength", &_tiltTowardMovementStrength, 0.0f, 1.0f, "%.2f");
		ImGui::TextWrapped("※ 移動方向に傾く強度（0.0=傾かない、1.0=完全に傾く）");

		ImGui::SliderFloat("Tilt Lerp Speed", &_tiltLerpSpeed, 0.0f, 1.0f, "%.2f");
		ImGui::TextWrapped("※ 傾きの滑らかさ（小さいほどゆっくり、大きいほど急激）");

		ImGui::Text("Is Resetting Rotation: %s", _bIsResetingRotation ? "YES" : "NO");

		if (ImGui::Button("Reset Rotation Now")) {
			_bIsResetingRotation = true;
		}

		ImGui::Separator();

		// -----------------------------------------------------------------------
		// 位置・速度表示
		// -----------------------------------------------------------------------
		ImGui::Text("Position: (%.2f, %.2f, %.2f)", _position.x, _position.y, _position.z);
		ImGui::Text("Velocity: (%.2f, %.2f, %.2f)", _velocity.x, _velocity.y, _velocity.z);
		float speed = _velocity.Length();
		ImGui::Text("Speed: %.2f units/sec", speed);

		ImGui::Separator();
		ImGui::Text("Is Grounded: %s", _bIsGrounded ? "YES" : "NO");
		ImGui::TextWrapped("Note: This object is always under gravity. When falling, gravity-only movement is used (no collision response). Air rotation applies when airborne.");
	}
#endif

	void TrashObjMoverComponent::Deserialize(const Unnamed::JsonReader &reader) {
		// NOTE: JSONから値を読み込む

		if (auto val = reader.Read<float>("mass")) {
			_mass = std::max(0.1f, val.value());
		}
		if (auto val = reader.Read<float>("gravity")) {
			_gravity = val.value();
		}
		if (auto val = reader.Read<float>("bounceDamping")) {
			_bounceDamping = val.value();
		}
		if (auto val = reader.Read<float>("frictionCoefficient")) {
			_frictionCoefficient = val.value();
		}
		if (auto val = reader.Read<float>("maxSpeedClamp")) {
			_maxSpeedClamp = val.value();
		}

		// ボックスサイズ
		float halfSizeX = _halfSize.x;
		float halfSizeY = _halfSize.y;
		float halfSizeZ = _halfSize.z;

		if (auto val = reader.Read<float>("halfSizeX")) {
			halfSizeX = val.value();
		}
		if (auto val = reader.Read<float>("halfSizeY")) {
			halfSizeY = val.value();
		}
		if (auto val = reader.Read<float>("halfSizeZ")) {
			halfSizeZ = val.value();
		}

		_halfSize = Vec3(halfSizeX, halfSizeY, halfSizeZ);

		// 落下状態フラグ
		if (auto val = reader.Read<bool>("isFalling")) {
			_bIsFalling = val.value();
		}

		// 回転パラメータ
		if (auto val = reader.Read<float>("airRotationSpeed")) {
			_airRotationSpeed = val.value();
		}
		if (auto val = reader.Read<float>("rotationResetSpeed")) {
			_rotationResetSpeed = val.value();
		}
		if (auto val = reader.Read<float>("tiltTowardMovementStrength")) {
			_tiltTowardMovementStrength = val.value();
		}
		if (auto val = reader.Read<float>("tiltLerpSpeed")) {
			_tiltLerpSpeed = val.value();
		}
	}

	void TrashObjMoverComponent::Serialize(Unnamed::JsonWriter &writer) const {
		// NOTE: 値をJSONに書き込む

		writer.Key("mass");
		writer.Write(_mass);

		writer.Key("gravity");
		writer.Write(_gravity);

		writer.Key("bounceDamping");
		writer.Write(_bounceDamping);

		writer.Key("frictionCoefficient");
		writer.Write(_frictionCoefficient);

		writer.Key("maxSpeedClamp");
		writer.Write(_maxSpeedClamp);

		writer.Key("halfSizeX");
		writer.Write(_halfSize.x);
		writer.Key("halfSizeY");
		writer.Write(_halfSize.y);
		writer.Key("halfSizeZ");
		writer.Write(_halfSize.z);

		writer.Key("isFalling");
		writer.Write(_bIsFalling);

		writer.Key("airRotationSpeed");
		writer.Write(_airRotationSpeed);

		writer.Key("rotationResetSpeed");
		writer.Write(_rotationResetSpeed);

		writer.Key("tiltTowardMovementStrength");
		writer.Write(_tiltTowardMovementStrength);

		writer.Key("tiltLerpSpeed");
		writer.Write(_tiltLerpSpeed);
	}

	// ===================================================================
	// プライベート ヘルパーメソッド
	// ===================================================================

	void TrashObjMoverComponent::UpdatePhysics(float deltaTime) {
		// NOTE: 重力を加算（Y軸の負方向加速度）
		_velocity.y -= _gravity * deltaTime;
	}

	void TrashObjMoverComponent::ClampVelocity() {
		// NOTE: 速度の大きさ（スピード）を計算
		float speed = _velocity.Length();

		// NOTE: 上限を超えている場合は正規化して制限
		if (speed > _maxSpeedClamp && speed > 0.001f) {
			_velocity = _velocity * (_maxSpeedClamp / speed);
		}
	}

	void TrashObjMoverComponent::HandleGroundCollision() {
		// NOTE: このメソッドは現在使用されていません
		// SlideMove() が衝突処理をすべて担当します
	}

	void TrashObjMoverComponent::ApplyFriction() {
		// NOTE: このメソッドは現在使用されていません
		// SlideMove() が衝突処理をすべて担当します
	}

	void TrashObjMoverComponent::UpdateAirRotation(float deltaTime) {
		// 速度がほぼゼロなら回転しない
		if (_velocity.Length() < 0.1f) {
			return;
		}

		// 速度ベクトルを正規化して方向を取得
		Vec3 velocityDirection = _velocity.Normalized();

		// 移動方向に傾く回転を計算（落下中のゴミが移動方向に傾いて見える効果）
		// Y軸（上向き）を参考にして、速度方向へ傾く
		Vec3 upVector = {0.0f, 1.0f, 0.0f};

		// 移動方向への傾きを適用
		// 水平方向の移動成分を強調する
		Vec3 velocityForwardDir = velocityDirection;

		// 傾きの強度を調整（_tiltTowardMovementStrength で制御）
		// 1.0 = 完全に速度方向に傾く、0.0 = 傾かない
		Vec3 horizontalVelocity = velocityDirection;
		horizontalVelocity.y = 0.0f;

		if (horizontalVelocity.Length() > 0.1f) {
			// 水平方向の移動がある場合
			horizontalVelocity = horizontalVelocity.Normalized();

			// 傾きを混合
			velocityForwardDir = velocityDirection * (1.0f - _tiltTowardMovementStrength) +
								 horizontalVelocity * _tiltTowardMovementStrength;
			velocityForwardDir = velocityForwardDir.Normalized();
		}

		// ターゲット回転を計算
		// forward方向が混合された速度方向になるようなクォータニオン
		Quaternion targetRotation = Quaternion::LookRotation(velocityForwardDir, upVector);

		// 指定された回転速度でターゲット回転へ向かう
		const float kPi = 3.14159265f;
		float rotationRadPerSec = _airRotationSpeed * kPi / 180.0f;
		float rotationThisFrame = rotationRadPerSec * deltaTime;

		// クォータニオン球面線形補間（SLERP）を使用
		// 傾きの滑らかさを適用（_tiltLerpSpeed で制御）
		float lerpAmount = std::min(1.0f, rotationThisFrame / kPi);
		lerpAmount *= _tiltLerpSpeed; // 滑らかさを調整
		_currentRotation = Quaternion::Slerp(_currentRotation, targetRotation, lerpAmount);
	}

	void TrashObjMoverComponent::UpdateRotationReset(float deltaTime) {
		// 初期回転へ戻す（補間速度で徐々に）
		float lerpSpeed = _rotationResetSpeed * deltaTime;
		_currentRotation = Quaternion::Slerp(_currentRotation, _initialRotation, lerpSpeed);

		// 十分に近ければリセット完了
		// クォータニオン間の角度差を計算
		float dotProduct = std::abs(
			_currentRotation.x * _initialRotation.x +
			_currentRotation.y * _initialRotation.y +
			_currentRotation.z * _initialRotation.z +
			_currentRotation.w * _initialRotation.w);
		dotProduct = std::min(1.0f, std::max(-1.0f, dotProduct));
		float angleDiff = std::acos(dotProduct);

		if (angleDiff < 0.01f) { // 約0.5度以下
			_currentRotation = _initialRotation;
			_bIsResetingRotation = false;
		}
	}

	void TrashObjMoverComponent::UpdateHoleSuck(float deltaTime) {
		// 穴への吸い込み処理（落下状態・地上両方に対応）
		if (!_bIsBeingSucked || _holeSuckPower <= 0.0f) {
			return;
		}

		// 穴への方向ベクトルを計算
		Vec3 directionToHole = _holeSuckPosition - _position;
		float distanceToHole = directionToHole.Length();

		// 距離が非常に小さい場合はスキップ
		if (distanceToHole < 0.01f) {
			// 穴の中心に吸い込まれた - 吸い込み処理完了
			return;
		}

		// 方向ベクトルを正規化
		Vec3 directionNormalized = directionToHole.Normalized();

		// 吸い込み力に基づいて加速度を計算
		// suckPower = 1.0の時: 60単位/秒²の加速度（穴への引力）
		// より強い吸い込み感を出すために数値を大きくした
		const float kBaseSuckAcceleration = 60.0f;
		Vec3 suckAcceleration = directionNormalized * kBaseSuckAcceleration * _holeSuckPower;

		// 速度に吸い込み加速度を追加
		_velocity += suckAcceleration * deltaTime;

		// 位置を更新
		_position += _velocity * deltaTime;

		// 吸い込み中も回転を更新
		UpdateAirRotation(deltaTime);
	}

} // namespace MyGame
