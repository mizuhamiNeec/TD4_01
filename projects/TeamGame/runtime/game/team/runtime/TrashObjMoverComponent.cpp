#include "TrashObjMoverComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "./core/ComponentRegistry.h"

#ifdef _DEBUG
#include "imgui.h"
#endif
#include <engine/unnamed/framework/components/TransformComponent.h>
#include "collision/SphereKinematicCollisionResolver.h"
#include "collision/BoxKinematicCollisionResolver.h"

#include <engine/world/World.h>

namespace MyGame {

	// -----------------------------------------------------------------------
	// ライフサイクル
	// -----------------------------------------------------------------------

	void TrashObjMoverComponent::OnAttached() {
		// NOTE: コンポーネントがアタッチされたときに初期化
		// 物理エンジンの設定などはここで行う場合がある

		// 発生位置
		_occurrenceLocation = {};

		// 衝撃波強度
		_shockWaveStrength = 0.0f;

		// 衝撃波方向
		_shockWaveDirection = {};

		// 質量
		_mass = 1.0f;

		/// 物理挙動フラグ
		_isRigidBoby = false;
		/// 重力フラグ
		_isGravity = false;
		/// アクションフラグ
		_isAction = false;
		/// アニメーションフラグ
		_isAnimation = false;

		// 物理エンジンをワールドから取得
		_physicsEngine = GetWorld() ? &GetWorld()->GetPhysicsEngine() : nullptr;

		mCollisionResolver = std::make_unique<Unnamed::BoxKinematicCollisionResolver>(
			_physicsEngine
		);

		// 位置
		_position = _startPosition;
	}

	void TrashObjMoverComponent::OnTick(float deltaTime) {
		// NOTE: 基本的に何もしない - 物理エンジンが動きを制御
		// 必要に応じてゴミの状態をチェックする処理をここに追加


		// 状態処理
		StateProcess();


		// -----------------------------------------------------------------------
		// 1️⃣ 物理計算（重力・速度制限）
		// -----------------------------------------------------------------------
		// 理由：基礎的な物理挙動をまず適用し、その後に誘導補正を加える
		UpdatePhysics(deltaTime);

		// -----------------------------------------------------------------------
		// 2️⃣ 地面衝突処理（バウンス）
		// -----------------------------------------------------------------------
		// 理由：地面に衝突したときの反射を計算し、リアルな物理挙動を実現
		HandleGroundCollision();

		// -----------------------------------------------------------------------
		// 3️⃣ 摩擦を適用
		// -----------------------------------------------------------------------
		// 理由：地面上の速度を段階的に減衰させ、最終的に停止させる
		ApplyFriction();

		// -----------------------------------------------------------------------
		// 6️⃣ 停止判定（完全に停止したか確認）
		// -----------------------------------------------------------------------
		// 理由：速度が十分に小さくなったら、フライトを終了する
		float speed = _velocity.Length();
		if (_isOnGround && speed < _stopVelocityThreshold) {
			_state = State::Idle;	// 待機状態に
			_isAction = false;		// アクションフラグOFF
			_velocity = Vec3(0.0f, 0.0f, 0.0f);  // 完全に停止
		}

		// -----------------------------------------------------------------------
		// 7️⃣ 衝突応答 & 速度クリップ
		// -----------------------------------------------------------------------
		auto* boxKCR = dynamic_cast<Unnamed::BoxKinematicCollisionResolver*>(mCollisionResolver.get());
		// ↓衝突応答の責任者
		boxKCR->SlideMove(
			_position, _velocity, deltaTime
		);

		// -----------------------------------------------------------------------
		// 8 TransformComponent と同期
		// -----------------------------------------------------------------------
		// 理由：計算した位置をエンティティの Transform に反映
		auto* transform = GetOwner()->GetComponent<Unnamed::TransformComponent>();
		if (transform) {
			transform->SetPosition(_position);
			// NOTE: 瞬間的な位置変更なので補間をリセット
			transform->RequestInterpolationResync();
		}


	}

	void TrashObjMoverComponent::OnDetached() {
		// NOTE: クリーンアップ処理
	}

	// -----------------------------------------------------------------------
	// ゴミオブジェクト設定
	// -----------------------------------------------------------------------

	void TrashObjMoverComponent::SetMass(float mass) {
		_mass = mass;
	}

	float TrashObjMoverComponent::GetMass() const {
		return _mass;
	}

	TrashObjMoverComponent::State TrashObjMoverComponent::GetState() const {
		return _state;
	}

	Vec3 TrashObjMoverComponent::GetCurrentPosition() const
	{
		return _position;
	}

	Vec3 TrashObjMoverComponent::GetCurrentVelocity() const
	{
		return _velocity;
	}


	// -----------------------------------------------------------------------
	// BaseComponent override
	// -----------------------------------------------------------------------

	std::string_view TrashObjMoverComponent::GetStableName() const {
		return "mygame.TrashObjMoverComponent";
	}

	std::string_view TrashObjMoverComponent::GetComponentName() const {
		return "Trash Object Mover Component";
	}

#ifdef _DEBUG
	void TrashObjMoverComponent::DrawInspectorImGui() {
		ImGui::Text("=== Trash Object Mover Component ===");
		if (ImGui::DragFloat3("BoxhalfSize", &_halfSize.x, 0.25f, 0.1f, 100.0f, "%.2f")) {
			// NOTE: 半径を変更したらコリジョンハルも更新
			auto* boxKCR = dynamic_cast<Unnamed::BoxKinematicCollisionResolver*>(mCollisionResolver.get());
			boxKCR->UpdateHull(_position, _halfSize);
		}

		ImGui::Separator();
		ImGui::Text("Trash Object Properties");
		
		// -----------------------------------------------------------------------
		// パラメータ調整
		// -----------------------------------------------------------------------
		ImGui::Text("=== Physics Parameters ===");
		ImGui::SliderFloat("Mass", &_mass, 0.1f, 100.0f, "%.2f kg");					// ゴミの重量
		ImGui::SliderFloat("Gravity", &_gravity, 0.0f, 20.0f, "%.2f");					// 重力
		ImGui::SliderFloat("Max Speed Clamp", &_maxSpeedClamp, 0.0f, 100.0f, "%.2f");	// 最大速度
		ImGui::SliderFloat("Bounce Damping", &_bounceDamping, 0.0f, 1.0f, "%.3f");
		ImGui::Separator();

		
		// -----------------------------------------------------------------------
		// 衝撃波表示
		// -----------------------------------------------------------------------
		ImGui::InputFloat3("OccurrenceLocation", &_occurrenceLocation.x, "%.2");		// 発生位置
		ImGui::InputFloat("ShockWaveStrength", &_shockWaveStrength);					// 衝撃波強度
		ImGui::InputFloat3("ShockWaveDirection", &_shockWaveDirection.x);				// 衝撃波
		// -----------------------------------------------------------------------
		// 位置・速度表示
		// -----------------------------------------------------------------------
		ImGui::Text("Position: (%.2f, %.2f, %.2f)", _position.x, _position.y, _position.z);
		ImGui::Text("Velocity: (%.2f, %.2f, %.2f)", _velocity.x, _velocity.y, _velocity.z);
		float speed = _velocity.Length();
		ImGui::Text("Speed: %.2f units/sec", speed);


		ImGui::Separator();
		ImGui::TextWrapped("Note: This object is controlled by physics engine when collided with player.");
	}
#endif

	void TrashObjMoverComponent::Deserialize(const Unnamed::JsonReader& reader) {
		// NOTE: JSONから値を読み込む
		if (auto val = reader.Read<float>("mass")) {
			_mass = val.value();
		}
		
	}

	void TrashObjMoverComponent::Serialize(Unnamed::JsonWriter& writer) const {
		// NOTE: 値をJSONに書き込む
		
		// 質量
		writer.Key("mass");
		writer.Write(_mass);
		// 重力
		writer.Key("gravity");
		writer.Write(_gravity);
		// 初期位置
		writer.Key("startPosition");
		writer.Write(_startPosition);


	}

	void TrashObjMoverComponent::UpdatePhysics(float deltaTime){
		// -----------------------------------------------------------------------
		// 3️⃣ 重力を加算（Y軸の負方向加速度）
		// -----------------------------------------------------------------------
		// 理由：重力は常に下向きに作用し、Y速度を減少させる
		_velocity.y -= _gravity * deltaTime;

		// NOTE: 速度の大きさ（スピード）を計算
		float speed = _velocity.Length();

		// NOTE: 上限を超えている場合は正規化して制限
		// 理由：方向は保持しながら、スピードのみ制限
		if (speed > _maxSpeedClamp && speed > 0.001f) {
			_velocity = _velocity * (_maxSpeedClamp / speed);
		}
	}

	void TrashObjMoverComponent::StateProcess() {
	
		switch (_state)
		{
		case MyGame::TrashObjMoverComponent::State::Idle:	// 待機
			// 物理挙動OFF
			_isRigidBoby = false;	
			// 重力OFF
			_isGravity = false;
			// アニメーション演出OFF
			_isAnimation = false;
			break;
		case MyGame::TrashObjMoverComponent::State::Flying:	// 飛行
			// 物理挙動ON
			_isRigidBoby = true;
			// 重力ON
			_isGravity = true;
			// アニメーション演出OFF
			_isAnimation = false;
			break;
		case MyGame::TrashObjMoverComponent::State::Landing:// 着地
			// アニメーション演出OFF
			_isAnimation = true;

			break;
		case MyGame::TrashObjMoverComponent::State::Settled:// 
			break;
		default:
			break;
		}
	
	}

	void TrashObjMoverComponent::HandleGroundCollision() {
		// -----------------------------------------------------------------------
			// 地面衝突判定
			// -----------------------------------------------------------------------
			// 理由：Y座標が地面レベル以下に到達したら、衝突と判定して反射を計算

		if (_position.y <= _groundLevel) {
			// -----------------------------------------------------------------------
			// 地面に到達した場合の処理
			// -----------------------------------------------------------------------

			// NOTE: 位置を地面にクリップ（貫通を防止）
			_position.y = _groundLevel;

			// NOTE: 縦方向（Y）の速度を反転して反発係数を適用
			// 理由：完全な弾性衝突ではなく、エネルギー損失を伴わせる
			// 反発係数が低いほど、バウンドは小さくなる
			_velocity.y = -_velocity.y * _bounceDamping;

			// NOTE: 地面接触フラグを有効化
			_isOnGround = true;

			// -----------------------------------------------------------------------
			// 横方向速度の減衰（衝突時のエネルギー損失）
			// -----------------------------------------------------------------------
			// 理由：地面との衝突によって、横方向速度も一定程度失われる
			float collisionDamping = 0.9f;
			_velocity.x *= collisionDamping;
			_velocity.z *= collisionDamping;
		}
		else {
			// -----------------------------------------------------------------------
			// 空中にいる場合
			// -----------------------------------------------------------------------
			_isOnGround = false;
		}
	}

	void TrashObjMoverComponent::ApplyFriction() {
		// -----------------------------------------------------------------------
			// 地表摩擦を適用（地面上にいる場合のみ）
			// -----------------------------------------------------------------------
			// 理由：地面上の横方向速度を段階的に減衰させ、自然な停止を実現

		if (!_isOnGround) {
			return;  // 空中にいる場合は摩擦なし
		}

		// -----------------------------------------------------------------------
		// 横方向速度に摩擦を適用
		// -----------------------------------------------------------------------
		// 理由：毎フレーム、摩擦係数を掛けることで指数関数的な速度減衰を実現
		// 例：0.95^30 ≈ 0.21 で、約30フレーム後に約80%減少

		_velocity.x *= _frictionCoefficient;
		_velocity.z *= _frictionCoefficient;

		// NOTE: 縦方向（Y）の速度も地面上で少し減衰（跳ね返りが減る）
		// ただし、次フレームの重力計算で再び速度が加わるため、
		// 摩擦の効果は主に横方向に現れる
		_velocity.y *= _frictionCoefficient;

		// -----------------------------------------------------------------------
		// 速度が非常に小さい場合は完全に0に設定
		// -----------------------------------------------------------------------
		// 理由：数値誤差で無限に微小な速度が残ることを防止
		float speed = _velocity.Length();
		if (speed < _stopVelocityThreshold) {
			_velocity = Vec3(0.0f, 0.0f, 0.0f);
		}
	}

	void TrashObjMoverComponent::SetShockWave(const Vec3& occurrenceLocation, const float strength) {
		// 発生位置
		_occurrenceLocation = occurrenceLocation;
	
		// 衝撃波強度
		_shockWaveStrength = strength;

		// アクション開始
		_isAction = true;

		// 状態飛行へ
		_state = State::Flying;



		// TransformComponent を取得
		if (!GetOwner()) {
			return;
		}
		auto* transform = GetOwner()->GetComponent<Unnamed::TransformComponent>();

		_position = transform->GetPosition();

		// 衝撃波方向設定
		_shockWaveDirection = Vec3(_occurrenceLocation - _position).Normalized();

		// 回転方向
		_shockWaveRotation = _occurrenceLocation * _shockWaveStrength;

		// 速度設定
		_velocity = Vec3(_shockWaveDirection * _shockWaveStrength) / _mass;

	}

	// NOTE: 忘れると死ぬやつ
	REGISTER_COMPONENT(TrashObjMoverComponent);

} // namespace MyGame
