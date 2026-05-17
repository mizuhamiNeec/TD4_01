#include "TrashObjMoverComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "./core/ComponentRegistry.h"

#ifdef _DEBUG
#include "imgui.h"
#endif

#include <engine/unnamed/framework/components/TransformComponent.h>
#include <engine/world/World.h>

namespace MyGame {

	// ===================================================================
	// セクション1: ライフサイクル メソッド
	// ===================================================================

	void TrashObjMoverComponent::OnAttached() {
		// NOTE: コンポーネントがアタッチされたときに初期化

		// 物理エンジンをワールドから取得
		_physicsEngine = GetWorld() ? &GetWorld()->GetPhysicsEngine() : nullptr;

		// 衝突リゾルバを作成
		_collisionResolver = std::make_unique<Unnamed::BoxKinematicCollisionResolver>(
			_physicsEngine
		);

		// TransformComponentをキャッシュ
		if (GetOwner()) {
			_transform = GetOwner()->GetComponent<Unnamed::TransformComponent>();
			if (_transform) {
				_position = _transform->GetPosition();
			}
		}

		// 衝突レゾルバのハルを初期化
		if (_collisionResolver) {
			_collisionResolver->UpdateHull(_position, _halfSize);
		}
	}

	void TrashObjMoverComponent::OnTick(float deltaTime) {
		// NOTE: 落下状態でない場合のみ物理更新を実行
		if (_bIsFalling) {
			// 落下状態：重力のみを適用
			_velocity.y -= _gravity * deltaTime;
			_position += _velocity * deltaTime;
		} else {
			// -----------------------------------------------------------------------
			// 1️⃣ 物理計算（重力）
			// -----------------------------------------------------------------------
			UpdatePhysics(deltaTime);

			// -----------------------------------------------------------------------
			// 2️⃣ 衝突応答 & SlideMove（リゾルバが位置を更新）
			// -----------------------------------------------------------------------
			// NOTE: SlideMove が _position と _velocity を更新する
			// 地面衝突、バウンス、速度クリップはすべてリゾルバが処理
			if (_collisionResolver) {
				auto* boxKCR = dynamic_cast<Unnamed::BoxKinematicCollisionResolver*>(_collisionResolver.get());
				if (boxKCR) {
					boxKCR->SlideMove(_position, _velocity, deltaTime);
				}
			}

			// -----------------------------------------------------------------------
			// 3️⃣ 速度上限でクランプ
			// -----------------------------------------------------------------------
			ClampVelocity();
		}

		// -----------------------------------------------------------------------
		// 4️⃣ TransformComponent と同期
		// -----------------------------------------------------------------------
		if (_transform) {
			_transform->SetPosition(_position);
			_transform->RequestInterpolationResync();
		}
	}

	void TrashObjMoverComponent::OnDetached() {
		// NOTE: クリーンアップ処理
		_collisionResolver.reset();
		_transform = nullptr;
	}

	// ===================================================================
	// セクション2: 公開インターフェース - ゴミオブジェクト設定
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
	// セクション2: 公開インターフェース - 落下状態管理
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
	// セクション3: BaseComponent の必須オーバーライド
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
		// 位置・速度表示
		// -----------------------------------------------------------------------
		ImGui::Text("Position: (%.2f, %.2f, %.2f)", _position.x, _position.y, _position.z);
		ImGui::Text("Velocity: (%.2f, %.2f, %.2f)", _velocity.x, _velocity.y, _velocity.z);
		float speed = _velocity.Length();
		ImGui::Text("Speed: %.2f units/sec", speed);

		ImGui::Separator();
		ImGui::Text("Is Grounded: %s", _bIsGrounded ? "YES" : "NO");
		ImGui::TextWrapped("Note: This object is always under gravity. When falling, gravity-only movement is used (no collision response).");
	}
	#endif

	void TrashObjMoverComponent::Deserialize(const Unnamed::JsonReader& reader) {
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
	}

	void TrashObjMoverComponent::Serialize(Unnamed::JsonWriter& writer) const {
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
	}

	// ===================================================================
	// セクション7: プライベート ヘルパーメソッド
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

	// NOTE: 忘れると死ぬやつ
	REGISTER_COMPONENT(TrashObjMoverComponent);

} // namespace MyGame
