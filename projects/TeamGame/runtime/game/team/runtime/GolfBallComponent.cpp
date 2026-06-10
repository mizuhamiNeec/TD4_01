#include "GolfBallComponent.h"
#include "./core/ComponentRegistry.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include <engine/unnamed/framework/components/TransformComponent.h>
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"
#include <core/math/Math.h>
#include <core/math/random/random.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>

#include <engine/world/World.h>
#include <engine/scene/Scene.h>

#include "collision/SphereKinematicCollisionResolver.h"

#include "GolfBallStartPosComponent.h"
#include "GolfBallEndPosComponent.h"

namespace MyGame {

	// -----------------------------------------------------------------------
	// ライフサイクル
	// -----------------------------------------------------------------------

	void GolfBallComponent::OnAttached() {
		// NOTE: コンポーネント初期化時に状態をリセット
		_position = _startPoint;
		_velocity = Vec3(0.0f, 0.0f, 0.0f);
		_elapsedTime = 0.0f;
		_bounceCount = 0;
		_bIsInFlight = false;
		_bIsExternalMotion = false;
		_bIsBeingSucked = false;
		_bIsInsideHole = false;
		_bHasEnteredHole = false;
		_bInitialSetupApplied = false;

		// 物理エンジンをワールドから取得
		_physicsEngine = GetWorld() ? &GetWorld()->GetPhysicsEngine() : nullptr;
		
		mCollisionResolver = std::make_unique<Unnamed::SphereKinematicCollisionResolver>(
			_physicsEngine
		);
		
		ApplyPositionToRuntime();
	}

	void GolfBallComponent::OnTick(float deltaTime) {
		ResolveSavedEntityReferences();
		if (!_bInitialSetupApplied) {
			SyncSetupFromReferencedEntities();
			SetStartPoint(_startPoint);
			_bInitialSetupApplied = true;
		}

		// -----------------------------------------------------------------------
		// 0️⃣ 参照エンティティの位置を同期（毎フレーム更新）
		// -----------------------------------------------------------------------
		// 理由：発射位置・着弾位置が Editor で変更されても常に同期する
		if (_startPosEntity && !_bIsInFlight && !_bIsBeingSucked) {
			auto* startTransform = _startPosEntity->GetComponent<Unnamed::TransformComponent>();
			if (startTransform) {
				_startPoint = startTransform->GetPosition();
				_position = _startPoint;  // ← 毎フレーム発射位置を同期
				ApplyPositionToRuntime();
			}
		}

		if (_targetPosEntity && !_bIsInFlight && !_bIsBeingSucked) {
			// NOTE: フライト中でない場合のみ着弾位置を更新
			auto* targetTransform = _targetPosEntity->GetComponent<Unnamed::TransformComponent>();
			if (targetTransform) {
				_targetBase = targetTransform->GetPosition();  // ← 着弾位置を同期
			}
		}

		// NOTE: フライト中でない場合は更新不要
		if (!_bIsInFlight && !_bIsBeingSucked) {
			return;
		}

		// -----------------------------------------------------------------------
		// 1️⃣ 物理計算（重力・風・速度制限）
		// -----------------------------------------------------------------------
		// 理由：基礎的な物理挙動をまず適用し、その後に誘導補正を加える
		UpdatePhysics(deltaTime);

		// -----------------------------------------------------------------------
		// 2️⃣ 穴への吸い込み処理
		// -----------------------------------------------------------------------
		if (_bIsBeingSucked) {
			Vec3 directionToHole = _holeSuckPosition - _position;
			_bIsInsideHole = (directionToHole.Length() < 1.5f);
			if (_bHasEnteredHole) {
				_bIsInsideHole = true;
			}
		} else {
			_bIsInsideHole = false;
		}

		if (_bIsBeingSucked) {
			UpdateHoleSuck(deltaTime);
			_position += _velocity * deltaTime;
			ClampVelocity();

			auto* transform = GetOwner()->GetComponent<Unnamed::TransformComponent>();
			if (transform) {
				transform->SetPosition(_position);
				transform->RequestInterpolationResync();

				GetWorld()->GetDebugDraw().DrawSphere(
					transform->GetPosition(),
					transform->GetRotation(),
					_radius,
					Vec4::cyan,
					16
				);
			}
			return;
		}

		// -----------------------------------------------------------------------
		// 3️⃣ 地面衝突処理（バウンス）
		// -----------------------------------------------------------------------
		// 理由：地面に衝突したときの反射を計算し、リアルな物理挙動を実現
		if (!_bIsInsideHole) {
			HandleGroundCollision(deltaTime);
		} else {
			_bIsGrounded = false;
		}

		// -----------------------------------------------------------------------
		// 4️⃣ 摩擦を適用
		// -----------------------------------------------------------------------
		// 理由：地面上の速度を段階的に減衰させ、最終的に停止させる
		if (!_bIsInsideHole) {
			ApplyFriction();
		}

		// -----------------------------------------------------------------------
		// 5️⃣ 誘導機能（ターゲット追従・収束）
		// -----------------------------------------------------------------------
		// 理由：物理と分離することで、自然で段階的なホーミング効果を実現
		if (!_bIsExternalMotion && !_bIsBeingSucked) {
			ApplyHoming(deltaTime);
		}

		// -----------------------------------------------------------------------
		// 6️⃣ 時間更新
		// -----------------------------------------------------------------------
		_elapsedTime += deltaTime;

		// -----------------------------------------------------------------------
		// 7️⃣ 停止判定（完全に停止したか確認）
		// -----------------------------------------------------------------------
		// 理由：速度が十分に小さくなったら、フライトを終了する
		float speed = _velocity.Length();
		if (!_bIsBeingSucked && _bIsGrounded && speed < _stopVelocityThreshold) {
			_bIsInFlight = false;
			_bIsExternalMotion = false;
			_velocity = Vec3(0.0f, 0.0f, 0.0f);  // 完全に停止
		}

		// -----------------------------------------------------------------------
		// 8️⃣ 衝突応答 & 速度クリップ
		// -----------------------------------------------------------------------
		if (!_bIsInsideHole) {
			auto* sphereKCR = dynamic_cast<Unnamed::SphereKinematicCollisionResolver*>(mCollisionResolver.get());
			if (sphereKCR) {
				const Vec3 velocityBeforeSlide = _velocity;
				// ↓衝突応答の責任者
				sphereKCR->SlideMove(
					_position, _velocity, deltaTime
				);
				HandlePostSlideGroundImpact(velocityBeforeSlide);
			}
		} else {
			_position += _velocity * deltaTime;
		}
		
		// -----------------------------------------------------------------------
		// 9 TransformComponent と同期
		// -----------------------------------------------------------------------
		// 理由：計算した位置をエンティティの Transform に反映
		auto* transform = GetOwner()->GetComponent<Unnamed::TransformComponent>();
		if (transform) {
			transform->SetPosition(_position);
			// NOTE: 瞬間的な位置変更なので補間をリセット
			transform->RequestInterpolationResync();

			// Transformと同期できた位置だけを可視化する。
			GetWorld()->GetDebugDraw().DrawSphere(
				transform->GetPosition(),
				transform->GetRotation(),
				_radius, 
				Vec4::cyan,
				16
			);
		}
	}

	void GolfBallComponent::OnDetached() {
		// NOTE: 特別なクリーンアップは不要（メモリはスコープ外で自動解放）
	}

	// -----------------------------------------------------------------------
	// セットアップ
	// -----------------------------------------------------------------------

	void GolfBallComponent::SetStartPoint(const Vec3& startPos) {
		// NOTE: 開始位置を設定
		_startPoint = startPos;
		_position = startPos;
		_elapsedTime = 0.0f;
		_bHasEnteredHole = false;
		ApplyPositionToRuntime();
	}

	void GolfBallComponent::SetTargetPoint(const Vec3& targetPos) {
		// NOTE: ターゲット基準位置を設定
		_targetBase = targetPos;
		RefreshTargetRandomOffset();
	}

	void GolfBallComponent::SetStartPosEntity(Unnamed::Entity* entity) {
		// -----------------------------------------------------------------------
		// エンティティ参照を保存
		// -----------------------------------------------------------------------
		// 理由：後で設定ボタン押下時に使用
		_startPosEntity = entity;

		// NOTE: GUID を保存（JSON 復元時に使用）
		if (entity) {
			_startPosEntityGuid = std::to_string(entity->GetGuid());
		} else {
			_startPosEntityGuid = "";
		}

		// NOTE: 即座に発射位置を設定
		if (!entity) {
			return;
		}

		// -----------------------------------------------------------------------
		// TransformComponent から位置を取得
		// -----------------------------------------------------------------------
		// 理由：別エンティティの位置情報をゴルフボールの発射地点として使用
		auto* transform = entity->GetComponent<Unnamed::TransformComponent>();
		if (!transform) {
			return;  // TransformComponent がない場合は処理しない
		}

		// NOTE: エンティティのワールド座標を発射位置として設定
		Vec3 startPos = transform->GetPosition();
		SetStartPoint(startPos);
	}

	void GolfBallComponent::SetTargetPosEntity(Unnamed::Entity* entity) {
		// -----------------------------------------------------------------------
		// エンティティ参照を保存
		// -----------------------------------------------------------------------
		// 理由：後で設定ボタン押下時に使用
		_targetPosEntity = entity;

		// NOTE: GUID を保存（JSON 復元時に使用）
		if (entity) {
			_targetPosEntityGuid = std::to_string(entity->GetGuid());
		} else {
			_targetPosEntityGuid = "";
		}

		// NOTE: 即座に着弾位置を設定
		if (!entity) {
			return;
		}

		// -----------------------------------------------------------------------
		// TransformComponent から位置を取得
		// -----------------------------------------------------------------------
		// 理由：別エンティティの位置情報をゴルフボールの着弾地点として使用
		auto* transform = entity->GetComponent<Unnamed::TransformComponent>();
		if (!transform) {
			return;  // TransformComponent がない場合は処理しない
		}

		// NOTE: エンティティのワールド座標をターゲット位置として設定
		Vec3 targetPos = transform->GetPosition();
		SetTargetPoint(targetPos);
	}

	void GolfBallComponent::Launch() {
		ResolveSavedEntityReferences();
		SyncSetupFromReferencedEntities();
		RefreshTargetRandomOffset();

		// -----------------------------------------------------------------------
		// 初速を逆算で計算
		// -----------------------------------------------------------------------
		// 理由：指定時間でターゲットに到達するための初速を数学的に計算
		// 式：s = v*t + 0.5*a*t^2 を整理して、v = (s - 0.5*a*t^2) / t

		Vec3 targetPoint = _targetBase + _targetRandomOffset;
		Vec3 displacement = targetPoint - _position;

		// NOTE: 重力の影響を考慮した逆算
		// y成分の計算：h = v_y*t - 0.5*g*t^2
		//           v_y = (h + 0.5*g*t^2) / t
		float gravityOffset = 0.5f * _gravity * _flightTime * _flightTime;
		_velocity.y = (displacement.y + gravityOffset) / _flightTime;

		// NOTE: x, z成分は時間で均等配分
		_velocity.x = displacement.x / _flightTime;
		_velocity.z = displacement.z / _flightTime;

		// NOTE: フライト開始
		_elapsedTime = 0.0f;
		_bounceCount = 0;
		_bIsInFlight = true;
		_bIsExternalMotion = false;
		_bIsBeingSucked = false;
		_bIsInsideHole = false;
		_bHasEnteredHole = false;
	}

	// -----------------------------------------------------------------------
	// 外部からの力（衝撃波など）
	// -----------------------------------------------------------------------

	void GolfBallComponent::ApplyForce(const Vec3& force) {
		// NOTE: 水平方向はゴミと同じく衝撃波をしっかり受け、Y方向だけ質量と上限で抑える。
		// 理由：全成分を質量で割ると、ボールが衝撃波でほとんど動かなくなる。
		Vec3 velocityDelta = force;
		velocityDelta.y = force.y * (1.0f / std::max(0.1f, _mass));
		velocityDelta.y = std::clamp(velocityDelta.y, -_maxExternalUpwardVelocity, _maxExternalUpwardVelocity);

		_velocity += velocityDelta;
		if (!_bIsInFlight) {
			_bounceCount = 0;
		}
		_bIsInFlight = true;
		_bIsExternalMotion = true;
		_bIsGrounded = false;
		_bIsBeingSucked = false;
		_bIsInsideHole = false;
		_bHasEnteredHole = false;
		
		// NOTE: 速度上限を適用して不自然な加速を防止
		ClampVelocity();
	}

	void GolfBallComponent::SetHoleSuckPosition(const Vec3& holePosition, float suckPower) {
		_holeSuckPosition = holePosition;
		_holeSuckPower = std::clamp(suckPower, 0.0f, 1.0f);
		_bIsBeingSucked = true;
		if (!_bIsInFlight) {
			_bounceCount = 0;
		}
		_bIsInFlight = true;
		_bIsExternalMotion = true;
	}

	void GolfBallComponent::ClearHoleSuckPosition() {
		if (_bHasEnteredHole) {
			return;
		}
		_holeSuckPower = 0.0f;
		_bIsBeingSucked = false;
		_bIsInsideHole = false;
	}

	// -----------------------------------------------------------------------
	// 状態取得
	// -----------------------------------------------------------------------

	Vec3 GolfBallComponent::GetCurrentPosition() const {
		return _position;
	}

	Vec3 GolfBallComponent::GetCurrentVelocity() const {
		return _velocity;
	}

	bool GolfBallComponent::IsInFlight() const {
		return _bIsInFlight;
	}

	float GolfBallComponent::GetElapsedTime() const {
		return _elapsedTime;
	}

	int GolfBallComponent::GetBounceCount() const {
		return _bounceCount;
	}

	bool GolfBallComponent::HasBounced() const {
		return _bounceCount > 0;
	}

	void GolfBallComponent::ResolveSavedEntityReferences() {
		auto resolve = [this](const std::string& guidText) -> Unnamed::Entity* {
			if (guidText.empty() || !GetScene()) {
				return nullptr;
			}

			char* end = nullptr;
			const uint64_t guid = std::strtoull(guidText.c_str(), &end, 10);
			if (end == guidText.c_str() || guid == 0) {
				return nullptr;
			}

			return GetScene()->FindEntity(guid);
		};

		if (!_startPosEntity) {
			if (auto* entity = resolve(_startPosEntityGuid)) {
				if (entity->GetComponent<GolfBallStartPosComponent>()) {
					_startPosEntity = entity;
				}
			}
		}

		if (!_targetPosEntity) {
			if (auto* entity = resolve(_targetPosEntityGuid)) {
				if (entity->GetComponent<GolfBallEndPosComponent>()) {
					_targetPosEntity = entity;
				}
			}
		}
	}

	void GolfBallComponent::SyncSetupFromReferencedEntities() {
		if (_startPosEntity) {
			if (auto* transform = _startPosEntity->GetComponent<Unnamed::TransformComponent>()) {
				_startPoint = transform->GetPosition();
			}
		}

		if (_targetPosEntity) {
			if (auto* transform = _targetPosEntity->GetComponent<Unnamed::TransformComponent>()) {
				_targetBase = transform->GetPosition();
			}
		}
	}

	void GolfBallComponent::ApplyPositionToRuntime() {
		if (auto* transform = GetOwner()->GetComponent<Unnamed::TransformComponent>()) {
			transform->SetPosition(_position);
			transform->RequestInterpolationResync();
		}

		if (mCollisionResolver) {
			if (auto* sphereKCR = dynamic_cast<Unnamed::SphereKinematicCollisionResolver*>(mCollisionResolver.get())) {
				sphereKCR->UpdateHull(_position, _radius);
			}
		}
	}

	void GolfBallComponent::RefreshTargetRandomOffset() {
		float angle = Random::FloatRange(0.0f, 2.0f * 3.14159265358979f);
		float radius = Random::FloatRange(0.0f, _randomRadius);

		_targetRandomOffset = Vec3(
			std::cos(angle) * radius,
			0.0f,
			std::sin(angle) * radius
		);
	}

	// -----------------------------------------------------------------------
	// BaseComponent override
	// -----------------------------------------------------------------------

	std::string_view GolfBallComponent::GetStableName() const {
		return "mygame.GolfBall";
	}

	std::string_view GolfBallComponent::GetComponentName() const {
		return "Golf Ball";
	}

	void GolfBallComponent::Deserialize(const Unnamed::JsonReader& reader) {
		// NOTE: JSON から パラメータを読み込む
		// Read() は std::optional<T> を返すため、value() で値を取得
		if (auto val = reader.Read<float>("radius")) {
			_radius = val.value();
		}
		if (auto val = reader.Read<float>("mass")) {
			_mass = std::max(0.1f, val.value());
		}
		if (auto val = reader.Read<float>("maxExternalUpwardVelocity")) {
			_maxExternalUpwardVelocity = std::max(0.0f, val.value());
		}
		if (auto val = reader.Read<float>("startPointX")) {
			_startPoint.x = val.value();
		}
		if (auto val = reader.Read<float>("startPointY")) {
			_startPoint.y = val.value();
		}
		if (auto val = reader.Read<float>("startPointZ")) {
			_startPoint.z = val.value();
		}
		if (auto val = reader.Read<float>("targetBaseX")) {
			_targetBase.x = val.value();
		}
		if (auto val = reader.Read<float>("targetBaseY")) {
			_targetBase.y = val.value();
		}
		if (auto val = reader.Read<float>("targetBaseZ")) {
			_targetBase.z = val.value();
		}
		if (auto val = reader.Read<float>("windX")) {
			_wind.x = val.value();
		}
		if (auto val = reader.Read<float>("windY")) {
			_wind.y = val.value();
		}
		if (auto val = reader.Read<float>("windZ")) {
			_wind.z = val.value();
		}
		if (auto val = reader.Read<float>("gravity")) {
			_gravity = val.value();
		}
		if (auto val = reader.Read<float>("flightTime")) {
			_flightTime = val.value();
		}
		if (auto val = reader.Read<float>("randomRadius")) {
			_randomRadius = val.value();
		}
		if (auto val = reader.Read<float>("maxSpeedClamp")) {
			_maxSpeedClamp = val.value();
		}
		if (auto val = reader.Read<float>("homingStrength")) {
			_homingStrength = val.value();
		}
		if (auto val = reader.Read<float>("homingStartTime")) {
			_homingStartTime = val.value();
		}
		if (auto val = reader.Read<float>("homingEndTime")) {
			_homingEndTime = val.value();
		}
		if (auto val = reader.Read<float>("bounceDamping")) {
			_bounceDamping = std::clamp(val.value(), 0.0f, 1.0f);
		}
		if (auto val = reader.Read<float>("minBounceVerticalSpeed")) {
			_minBounceVerticalSpeed = std::max(0.0f, val.value());
		}
		if (auto val = reader.Read<float>("groundCollisionDamping")) {
			_groundCollisionDamping = std::clamp(val.value(), 0.0f, 1.0f);
		}
		if (auto val = reader.Read<float>("frictionCoefficient")) {
			_frictionCoefficient = std::clamp(val.value(), 0.0f, 1.0f);
		}
		if (auto val = reader.Read<float>("groundLevel")) {
			_groundLevel = val.value();
		}
		if (auto val = reader.Read<float>("stopVelocityThreshold")) {
			_stopVelocityThreshold = val.value();
		}

		// -----------------------------------------------------------------------
		// エンティティ参照のGUID を読み込む
		// -----------------------------------------------------------------------
		// 理由：保存されたエンティティ参照を復元
		if (auto val = reader.Read<std::string>("startPosEntityGuid")) {
			_startPosEntityGuid = val.value();
		}
		if (auto val = reader.Read<std::string>("targetPosEntityGuid")) {
			_targetPosEntityGuid = val.value();
		}
	}

	void GolfBallComponent::Serialize(Unnamed::JsonWriter& writer) const {
		// NOTE: パラメータを JSON に書き込む
		// Key(), Write() の順序で呼び出す
		writer.Key("radius");
		writer.Write(_radius);
		writer.Key("mass");
		writer.Write(_mass);
		writer.Key("maxExternalUpwardVelocity");
		writer.Write(_maxExternalUpwardVelocity);
		writer.Key("startPointX");
		writer.Write(_startPoint.x);
		writer.Key("startPointY");
		writer.Write(_startPoint.y);
		writer.Key("startPointZ");
		writer.Write(_startPoint.z);
		writer.Key("targetBaseX");
		writer.Write(_targetBase.x);
		writer.Key("targetBaseY");
		writer.Write(_targetBase.y);
		writer.Key("targetBaseZ");
		writer.Write(_targetBase.z);
		writer.Key("windX");
		writer.Write(_wind.x);
		writer.Key("windY");
		writer.Write(_wind.y);
		writer.Key("windZ");
		writer.Write(_wind.z);
		writer.Key("gravity");
		writer.Write(_gravity);
		writer.Key("flightTime");
		writer.Write(_flightTime);
		writer.Key("randomRadius");
		writer.Write(_randomRadius);
		writer.Key("maxSpeedClamp");
		writer.Write(_maxSpeedClamp);
		writer.Key("homingStrength");
		writer.Write(_homingStrength);
		writer.Key("homingStartTime");
		writer.Write(_homingStartTime);
		writer.Key("homingEndTime");
		writer.Write(_homingEndTime);
		writer.Key("bounceDamping");
		writer.Write(_bounceDamping);
		writer.Key("minBounceVerticalSpeed");
		writer.Write(_minBounceVerticalSpeed);
		writer.Key("groundCollisionDamping");
		writer.Write(_groundCollisionDamping);
		writer.Key("frictionCoefficient");
		writer.Write(_frictionCoefficient);
		writer.Key("groundLevel");
		writer.Write(_groundLevel);
		writer.Key("stopVelocityThreshold");
		writer.Write(_stopVelocityThreshold);

		// -----------------------------------------------------------------------
		// エンティティ参照のGUID を書き込む
		// -----------------------------------------------------------------------
		// 理由：エンティティ参照をGUID形式で保存
		writer.Key("startPosEntityGuid");
		writer.Write(_startPosEntityGuid);
		writer.Key("targetPosEntityGuid");
		writer.Write(_targetPosEntityGuid);
	}

	// -----------------------------------------------------------------------
	// 物理計算（内部実装）
	// -----------------------------------------------------------------------

	void GolfBallComponent::UpdatePhysics(float deltaTime) {
		// -----------------------------------------------------------------------
		// 1️⃣ 風を加算（加速度）
		// -----------------------------------------------------------------------
		// 理由：風は常に一定の加速度を加えるため、最初に適用
		_velocity += _wind * deltaTime;

		// -----------------------------------------------------------------------
		// 3️⃣ 重力を加算（Y軸の負方向加速度）
		// -----------------------------------------------------------------------
		// 理由：重力は常に下向きに作用し、Y速度を減少させる
		_velocity.y -= _gravity * deltaTime;

		// -----------------------------------------------------------------------
		// 4️⃣ 速度クランプ（上限制限）
		// -----------------------------------------------------------------------
		// 理由：物理計算後に速度を制限し、不自然な加速や無限速度を防止
		ClampVelocity();
	}

	void GolfBallComponent::ClampVelocity() {
		// NOTE: 速度の大きさ（スピード）を計算
		float speed = _velocity.Length();

		// NOTE: 上限を超えている場合は正規化して制限
		// 理由：方向は保持しながら、スピードのみ制限
		if (speed > _maxSpeedClamp && speed > 0.001f) {
			_velocity = _velocity * (_maxSpeedClamp / speed);
		}
	}

	// -----------------------------------------------------------------------
	// 誘導計算（内部実装）
	// -----------------------------------------------------------------------

	void GolfBallComponent::ApplyHoming(float deltaTime) {
		// -----------------------------------------------------------------------
		// 時間範囲チェック
		// -----------------------------------------------------------------------
		// 理由：誘導機能は指定時間帯のみ有効にする
		if (_elapsedTime < _homingStartTime || _elapsedTime > _homingEndTime) {
			return;
		}

		// -----------------------------------------------------------------------
		// 誘導強度を計算（時間ベース）
		// -----------------------------------------------------------------------
		// 理由：段階的に誘導を効かせることで、不自然な急転換を防止
		float alpha = GetHomingAlpha();
		if (alpha <= 0.0f) {
			return;
		}

		// -----------------------------------------------------------------------
		// 誘導方向を計算
		// -----------------------------------------------------------------------
		// 理由：ボール → ターゲットへ向かう方向ベクトルを導出
		Vec3 homingDir = CalcHomingDirection();

		// -----------------------------------------------------------------------
		// 速度に誘導補正を加算
		// -----------------------------------------------------------------------
		// 理由：速度を直接上書きではなく「加算」することで、物理挙動との融合を実現
		Vec3 homingForce = homingDir * (_homingStrength * alpha * deltaTime);
		_velocity += homingForce;
	}

	float GolfBallComponent::GetHomingAlpha() const {
		// NOTE: ホーミング開始時刻より前の場合は無効
		if (_elapsedTime < _homingStartTime) {
			return 0.0f;
		}

		// NOTE: ホーミング終了時刻以降は最大値を維持
		if (_elapsedTime >= _homingEndTime) {
			return 1.0f;
		}

		// -----------------------------------------------------------------------
		// フェーズ1: 段階的に増加（ランプアップ）
		// -----------------------------------------------------------------------
		// 理由：最初から100%効果させると不自然な急転換になるため、
		//      緩和曲線（二次関数）で徐々に増加させる

		float totalDuration = _homingEndTime - _homingStartTime;
		float rampUpDuration = totalDuration * 0.5f;  // 前半で急速に増加
		float elapsedSinceStart = _elapsedTime - _homingStartTime;

		if (elapsedSinceStart < rampUpDuration) {
			// NOTE: t^2 による緩和曲線（スムーズなイージング）
			// 理由：t^2 は初期の増加が緩く、時間とともに加速する
			float t = elapsedSinceStart / rampUpDuration;
			return t * t;  // 0 → 1 へ滑らかに変化
		}

		// -----------------------------------------------------------------------
		// フェーズ2: 最大値を維持
		// -----------------------------------------------------------------------
		// 理由：後半は最大強度で誘導を継続
		return 1.0f;
	}

	Vec3 GolfBallComponent::CalcHomingDirection() const {
		// NOTE: ターゲント最終位置を計算
		Vec3 targetFinal = _targetBase + _targetRandomOffset;

		// NOTE: ボール → ターゲットへのベクトルを計算
		Vec3 directionToTarget = targetFinal - _position;

		// NOTE: ベクトルの大きさ（距離）を取得
		float distanceToTarget = directionToTarget.Length();

		// NOTE: ほぼ同じ位置の場合は誘導不要
		if (distanceToTarget < 0.001f) {
			return Vec3(0.0f, 0.0f, 0.0f);
		}

		// NOTE: 方向ベクトルを正規化（長さ1にする）
		// 理由：誘導力は方向のみに依存し、距離に関わらず均一の加速度を与える
		return directionToTarget * (1.0f / distanceToTarget);
	}

	void GolfBallComponent::UpdateHoleSuck(float deltaTime) {
		if (!_bIsBeingSucked || _holeSuckPower <= 0.0f) {
			return;
		}

		if (_bIsInsideHole || _bHasEnteredHole) {
			_bHasEnteredHole = true;
			_bIsInsideHole = true;

			const float kHoleHorizontalDamping = 12.0f;
			const float kHoleFallGravity = 28.0f;
			const float kHoleMinFallSpeed = 4.0f;
			const float damping = std::exp(-kHoleHorizontalDamping * deltaTime);
			_velocity.x *= damping;
			_velocity.z *= damping;
			_velocity.y -= kHoleFallGravity * deltaTime;
			_velocity.y = std::min(_velocity.y, -kHoleMinFallSpeed);
			_bIsGrounded = false;
			return;
		}

		Vec3 directionToHole = _holeSuckPosition - _position;
		float distanceToHole = directionToHole.Length();
		if (distanceToHole < 0.01f) {
			return;
		}

		Vec3 directionNormalized = directionToHole.Normalized();

		const float kBaseSuckAcceleration = 60.0f;
		Vec3 suckAcceleration = directionNormalized * kBaseSuckAcceleration * _holeSuckPower;
		_velocity += suckAcceleration * deltaTime;
	}

	// -----------------------------------------------------------------------
	// 状態管理（内部実装）
	// -----------------------------------------------------------------------

	void GolfBallComponent::HandleGroundCollision(const float deltaTime) {
		// -----------------------------------------------------------------------
		// 地面衝突判定
		// -----------------------------------------------------------------------
		// 理由：Y座標が地面レベル以下に到達したら、衝突と判定して反射を計算
		
		const bool reachesGroundThisFrame =
			_velocity.y < 0.0f &&
			_position.y + _velocity.y * deltaTime <= _groundLevel;
		const bool restsOrFallsOnGround =
			_position.y <= _groundLevel && _velocity.y <= 0.0f;
		if (restsOrFallsOnGround || reachesGroundThisFrame) {
			const bool wasGrounded = _bIsGrounded;
			// -----------------------------------------------------------------------
			// 地面に到達した場合の処理
			// -----------------------------------------------------------------------

			// NOTE: 位置を地面にクリップ（貫通を防止）
			_position.y = _groundLevel;

			const float impactSpeed = std::max(0.0f, -_velocity.y);
			ResolveGroundImpact(impactSpeed, wasGrounded);
		} else {
			// -----------------------------------------------------------------------
			// 空中にいる場合
			// -----------------------------------------------------------------------
			_bIsGrounded = false;
		}
	}

	void GolfBallComponent::ResolveGroundImpact(const float impactSpeed, const bool wasGrounded) {
		if (!wasGrounded && impactSpeed > _stopVelocityThreshold) {
			// エース判定では、跳ね返りが小さくても地面に触れた事実を使う。
			++_bounceCount;
		}

		// NOTE: 縦方向（Y）の速度を反転して反発係数を適用
		// 理由：完全な弾性衝突ではなく、エネルギー損失を伴わせる
		const bool shouldBounce = impactSpeed >= _minBounceVerticalSpeed;
		if (shouldBounce) {
			_velocity.y = impactSpeed * _bounceDamping;
		} else {
			_velocity.y = 0.0f;
		}

		// 反発したフレームを接地扱いにすると摩擦と停止判定が跳ね返りを潰す。
		_bIsGrounded = !shouldBounce;

		// NOTE: 接地衝撃で横方向速度も少し落とし、跳ねながら滑りすぎる挙動を抑える。
		_velocity.x *= _groundCollisionDamping;
		_velocity.z *= _groundCollisionDamping;
	}

	void GolfBallComponent::HandlePostSlideGroundImpact(const Vec3& velocityBeforeSlide) {
		if (_bIsInsideHole || _bIsBeingSucked) {
			return;
		}
		if (velocityBeforeSlide.y >= -_minBounceVerticalSpeed) {
			return;
		}
		if (_velocity.y < -0.001f) {
			return;
		}

		auto* sphereKCR = dynamic_cast<Unnamed::SphereKinematicCollisionResolver*>(mCollisionResolver.get());
		if (!sphereKCR) {
			return;
		}

		Unnamed::Physics::Hit hit{};
		const float probeDistance = std::max(_radius + 0.05f, 0.25f);
		if (!sphereKCR->ProbeGround(_position + Vec3::up * 0.02f, probeDistance, &hit)) {
			return;
		}
		if (hit.normal.y < 0.6f) {
			return;
		}

		ResolveGroundImpact(-velocityBeforeSlide.y, _bIsGrounded);
	}

	void GolfBallComponent::ApplyFriction() {
		// -----------------------------------------------------------------------
		// 地表摩擦を適用（地面上にいる場合のみ）
		// -----------------------------------------------------------------------
		// 理由：地面上の横方向速度を段階的に減衰させ、自然な停止を実現

		if (!_bIsGrounded) {
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

	bool GolfBallComponent::CheckLanding() const {
		// -----------------------------------------------------------------------
		// 着地判定：Y座標がターゲット高さ以下かを確認
		// -----------------------------------------------------------------------
		// 理由：フライト終了条件を判定（実際のゲームではコライダーでの判定も可能）

		Vec3 targetFinal = _targetBase + _targetRandomOffset;

		// NOTE: 下降中にターゲット高さ以下に到達した場合は着地と判定
		// （さらなる落下を防ぐため、速度が下向きのチェックも併用可能）
		return _position.y <= targetFinal.y && _velocity.y <= 0.0f;
	}

	// NOTE: コンポーネント登録マクロ
	REGISTER_COMPONENT(GolfBallComponent);
}
