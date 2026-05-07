#include "GolfBallComponent.h"
#include "./core/ComponentRegistry.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include <engine/unnamed/framework/components/TransformComponent.h>
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"
#include <core/math/Math.h>
#include <core/math/random/random.h>
#include <cmath>

#include <engine/ImGui/ImGuiWidgets.h>
#include <engine/world/World.h>

#include "collision/SphereKinematicCollisionResolver.h"

#ifdef _DEBUG
#include "imgui.h"
#endif

namespace MyGame {

	// -----------------------------------------------------------------------
	// ライフサイクル
	// -----------------------------------------------------------------------

	void GolfBallComponent::OnAttached() {
		// NOTE: コンポーネント初期化時に状態をリセット
		_position = Vec3(0.0f, 0.0f, 0.0f);
		_velocity = Vec3(0.0f, 0.0f, 0.0f);
		_elapsedTime = 0.0f;
		_bIsInFlight = false;

		// NOTE: TransformComponent と同期
		auto* transform = GetOwner()->GetComponent<Unnamed::TransformComponent>();
		if (transform) {
			_position = transform->GetPosition();
		}
		
		// 物理エンジンをワールドから取得
		_physicsEngine = GetWorld() ? &GetWorld()->GetPhysicsEngine() : nullptr;
		
		mCollisionResolver = std::make_unique<Unnamed::SphereKinematicCollisionResolver>(
			_physicsEngine
		);
		
		auto* sphereKCR = dynamic_cast<Unnamed::SphereKinematicCollisionResolver*>(mCollisionResolver.get());
		sphereKCR->UpdateHull(_position, _radius); // 初期位置と半径でコリジョンハルを設定 
	}

	void GolfBallComponent::OnTick(float deltaTime) {
		// NOTE: フライト中でない場合は更新不要
		if (!_bIsInFlight) {
			return;
		}

		// -----------------------------------------------------------------------
		// 1️⃣ 物理計算（重力・風・速度制限）
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
		// 4️⃣ 誘導機能（ターゲット追従・収束）
		// -----------------------------------------------------------------------
		// 理由：物理と分離することで、自然で段階的なホーミング効果を実現
		ApplyHoming(deltaTime);

		// -----------------------------------------------------------------------
		// 5️⃣ 時間更新
		// -----------------------------------------------------------------------
		_elapsedTime += deltaTime;

		// -----------------------------------------------------------------------
		// 6️⃣ 停止判定（完全に停止したか確認）
		// -----------------------------------------------------------------------
		// 理由：速度が十分に小さくなったら、フライトを終了する
		float speed = _velocity.Length();
		if (_bIsGrounded && speed < _stopVelocityThreshold) {
			_bIsInFlight = false;
			_velocity = Vec3(0.0f, 0.0f, 0.0f);  // 完全に停止
		}

		// -----------------------------------------------------------------------
		// 7️⃣ 衝突応答 & 速度クリップ
		// -----------------------------------------------------------------------
		auto* sphereKCR = dynamic_cast<Unnamed::SphereKinematicCollisionResolver*>(mCollisionResolver.get());
		sphereKCR->SlideMove(
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

		// デバッグ描画
		GetWorld()->GetDebugDraw().DrawSphere(
			transform->GetPosition(),
			transform->GetRotation(),
			_radius, 
			Vec4::cyan,
			16
		);
	}

	void GolfBallComponent::OnDetached() {
		// NOTE: 特別なクリーンアップは不要（メモリはスコープ外で自動解放）
	}

	// -----------------------------------------------------------------------
	// セットアップ
	// -----------------------------------------------------------------------

	void GolfBallComponent::SetStartPoint(const Vec3& startPos) {
		// NOTE: 開始位置を設定
		_position = startPos;
		_elapsedTime = 0.0f;

		// NOTE: TransformComponent と同期
		auto* transform = GetOwner()->GetComponent<Unnamed::TransformComponent>();
		if (transform) {
			transform->SetPosition(_position);
		}
	}

	void GolfBallComponent::SetTargetPoint(const Vec3& targetPos) {
		// NOTE: ターゲット基準位置を設定
		_targetBase = targetPos;

		// -----------------------------------------------------------------------
		// ランダムオフセットを計算
		// -----------------------------------------------------------------------
		// 理由：毎回同じ着地点になるのは不自然なため、ターゲット周辺のランダム位置を決定
		
		float angle = Random::FloatRange(0.0f, 2.0f * 3.14159265358979f);
		float radius = Random::FloatRange(0.0f, _randomRadius);

		// NOTE: 水平面（XZ平面）でランダムオフセットを計算
		// Y成分は0（高さは重力により自動調整）
		_targetRandomOffset = Vec3(
			std::cos(angle) * radius,
			0.0f,
			std::sin(angle) * radius
		);
	}

	void GolfBallComponent::Launch() {
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
		_bIsInFlight = true;
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

	// -----------------------------------------------------------------------
	// BaseComponent override
	// -----------------------------------------------------------------------

	std::string_view GolfBallComponent::GetStableName() const {
		return "mygame.GolfBall";
	}

	std::string_view GolfBallComponent::GetComponentName() const {
		return "Golf Ball";
	}

#ifdef _DEBUG
	void GolfBallComponent::DrawInspectorImGui() {
		ImGui::Text("=== Golf Ball Settings ===");
		if (ImGui::DragFloat("Radius", &_radius, 0.25f, 0.1f, 100.0f, "%.2f")) {
			// NOTE: 半径を変更したらコリジョンハルも更新
			auto* sphereKCR = dynamic_cast<Unnamed::SphereKinematicCollisionResolver*>(mCollisionResolver.get());
			sphereKCR->UpdateHull(_position, _radius);
		}
		
		if (ImGuiWidgets::DragVec3("Velocity(override)", _velocity, Vec3::zero, 0.1f, "%.2f[m/s]")) {
			// NOTE: デバッグ用に速度を直接編集可能にする
			_bIsInFlight = true; // 直接編集したらフライト状態にする
		}
		
		ImGui::Text("=== Golf Ball Projectile Motion ===");
		ImGui::Separator();

		// -----------------------------------------------------------------------
		// フライト状態表示
		// -----------------------------------------------------------------------
		ImGui::Text("Flight Status: %s", _bIsInFlight ? "IN FLIGHT" : "LANDED");
		ImGui::Text("Elapsed Time: %.2f sec", _elapsedTime);
		ImGui::ProgressBar(_elapsedTime / _flightTime, ImVec2(0, 0), "Progress");

		ImGui::Separator();

		// -----------------------------------------------------------------------
		// 位置・速度表示
		// -----------------------------------------------------------------------
		ImGui::Text("Position: (%.2f, %.2f, %.2f)", _position.x, _position.y, _position.z);
		ImGui::Text("Velocity: (%.2f, %.2f, %.2f)", _velocity.x, _velocity.y, _velocity.z);
		float speed = _velocity.Length();
		ImGui::Text("Speed: %.2f units/sec", speed);

		ImGui::Separator();

		// -----------------------------------------------------------------------
		// 発射地点設定セクション
		// -----------------------------------------------------------------------
		ImGui::Text("=== Start Point Setup ===");
		
		// NOTE: 発射地点を直接編集可能にする
		// 理由：テスト時に発射地点を自由に変更したい
		static float startPoint[3] = { 0.0f, 0.0f, 0.0f };
		ImGui::InputFloat3("Start Point##input", startPoint, "%.2f");
		
		if (ImGui::Button("Set Start Point", ImVec2(150, 0))) {
			SetStartPoint(Vec3(startPoint[0], startPoint[1], startPoint[2]));
		}
		ImGui::SameLine();
		if (ImGui::Button("Set From Current", ImVec2(150, 0))) {
			// NOTE: 現在位置を発射地点として設定
			startPoint[0] = _position.x;
			startPoint[1] = _position.y;
			startPoint[2] = _position.z;
			SetStartPoint(_position);
		}

		ImGui::Text("Current Start: (%.2f, %.2f, %.2f)", _position.x, _position.y, _position.z);

		ImGui::Separator();

		// -----------------------------------------------------------------------
		// 着弾地点設定セクション
		// -----------------------------------------------------------------------
		ImGui::Text("=== Target Point Setup ===");
		
		// NOTE: ターゲット基準位置を直接編集可能にする
		// 理由：テスト時に着弾地点を自由に変更したい
		static float targetPoint[3] = { 10.0f, 0.0f, 0.0f };
		ImGui::InputFloat3("Target Point##input", targetPoint, "%.2f");
		
		if (ImGui::Button("Set Target Point", ImVec2(150, 0))) {
			SetTargetPoint(Vec3(targetPoint[0], targetPoint[1], targetPoint[2]));
		}
		ImGui::SameLine();
		if (ImGui::Button("Update Offset", ImVec2(150, 0))) {
			// NOTE: ランダムオフセットを再計算（同じターゲットで別の着地点を試す）
			SetTargetPoint(_targetBase);
		}

		ImGui::Text("Target Base: (%.2f, %.2f, %.2f)", _targetBase.x, _targetBase.y, _targetBase.z);
		ImGui::Text("Target Offset: (%.2f, %.2f, %.2f)", _targetRandomOffset.x, _targetRandomOffset.y, _targetRandomOffset.z);
		Vec3 finalTarget = _targetBase + _targetRandomOffset;
		ImGui::Text("Final Target: (%.2f, %.2f, %.2f)", finalTarget.x, finalTarget.y, finalTarget.z);

		ImGui::Separator();

		// -----------------------------------------------------------------------
		// パラメータ調整
		// -----------------------------------------------------------------------
		ImGui::Text("=== Physics Parameters ===");
		ImGui::SliderFloat("Gravity", &_gravity, 0.0f, 20.0f, "%.2f");
		ImGui::SliderFloat("Flight Time", &_flightTime, 0.1f, 10.0f, "%.2f");
		ImGui::SliderFloat("Random Radius", &_randomRadius, 0.0f, 10.0f, "%.2f");
		ImGui::SliderFloat("Max Speed Clamp", &_maxSpeedClamp, 0.0f, 100.0f, "%.2f");

		ImGui::Separator();

		// -----------------------------------------------------------------------
		// 風パラメータ
		// -----------------------------------------------------------------------
		ImGui::Text("=== Wind Parameters ===");
		ImGui::SliderFloat("Wind X", &_wind.x, -10.0f, 10.0f, "%.2f");
		ImGui::SliderFloat("Wind Y", &_wind.y, -10.0f, 10.0f, "%.2f");
		ImGui::SliderFloat("Wind Z", &_wind.z, -10.0f, 10.0f, "%.2f");

		ImGui::Separator();

		// -----------------------------------------------------------------------
		// ホーミングパラメータ
		// -----------------------------------------------------------------------
		ImGui::Text("=== Homing Parameters ===");
		ImGui::SliderFloat("Homing Strength", &_homingStrength, 0.0f, 5.0f, "%.2f");
		ImGui::SliderFloat("Homing Start Time", &_homingStartTime, 0.0f, _flightTime, "%.2f");
		ImGui::SliderFloat("Homing End Time", &_homingEndTime, _homingStartTime, _flightTime, "%.2f");

		ImGui::Separator();

		// -----------------------------------------------------------------------
		// バウンス・摩擦パラメータ
		// -----------------------------------------------------------------------
		ImGui::Text("=== Bounce & Friction Parameters ===");
		ImGui::SliderFloat("Bounce Damping", &_bounceDamping, 0.0f, 1.0f, "%.3f");
		ImGui::SliderFloat("Friction Coefficient", &_frictionCoefficient, 0.0f, 1.0f, "%.3f");
		ImGui::SliderFloat("Ground Level", &_groundLevel, -10.0f, 10.0f, "%.2f");
		ImGui::SliderFloat("Stop Velocity Threshold", &_stopVelocityThreshold, 0.001f, 0.1f, "%.4f");

		// NOTE: パラメータ説明
		ImGui::TextDisabled("Bounce: 0=no bounce, 1=full energy | Friction: 0=fast stop, 1=no friction");
		ImGui::Checkbox("Is Grounded", &_bIsGrounded);

		ImGui::Separator();

		// -----------------------------------------------------------------------
		// コントロールボタン
		// -----------------------------------------------------------------------
		ImGui::Text("=== Control ===");
		
		if (ImGui::Button("Launch", ImVec2(100, 30))) {
			// NOTE: 現在設定されている発射地点とターゲット地点で発射
			Launch();
		}
		ImGui::SameLine();
		if (ImGui::Button("Reset", ImVec2(100, 30))) {
			// NOTE: 状態をリセット
			_bIsInFlight = false;
			_elapsedTime = 0.0f;
			_velocity = Vec3(0.0f, 0.0f, 0.0f);
		}
		ImGui::SameLine();
		if (ImGui::Button("Stop", ImVec2(100, 30))) {
			// NOTE: 即座に着地させる
			_bIsInFlight = false;
		}
	}
#endif

	void GolfBallComponent::Deserialize(const Unnamed::JsonReader& reader) {
		// NOTE: JSON から パラメータを読み込む
		// Read() は std::optional<T> を返すため、value() で値を取得
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
			_bounceDamping = val.value();
		}
		if (auto val = reader.Read<float>("frictionCoefficient")) {
			_frictionCoefficient = val.value();
		}
		if (auto val = reader.Read<float>("groundLevel")) {
			_groundLevel = val.value();
		}
		if (auto val = reader.Read<float>("stopVelocityThreshold")) {
			_stopVelocityThreshold = val.value();
		}
	}

	void GolfBallComponent::Serialize(Unnamed::JsonWriter& writer) const {
		// NOTE: パラメータを JSON に書き込む
		// Key(), Write() の順序で呼び出す
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
		writer.Key("frictionCoefficient");
		writer.Write(_frictionCoefficient);
		writer.Key("groundLevel");
		writer.Write(_groundLevel);
		writer.Key("stopVelocityThreshold");
		writer.Write(_stopVelocityThreshold);
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
		// 2️⃣ 速度を位置に加算
		// -----------------------------------------------------------------------
		// 理由：現在の速度に基づいて位置を更新
		_position += _velocity * deltaTime;

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

	// -----------------------------------------------------------------------
	// 状態管理（内部実装）
	// -----------------------------------------------------------------------

	void GolfBallComponent::HandleGroundCollision() {
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
			_bIsGrounded = true;

			// -----------------------------------------------------------------------
			// 横方向速度の減衰（衝突時のエネルギー損失）
			// -----------------------------------------------------------------------
			// 理由：地面との衝突によって、横方向速度も一定程度失われる
			float collisionDamping = 0.9f;
			_velocity.x *= collisionDamping;
			_velocity.z *= collisionDamping;
		} else {
			// -----------------------------------------------------------------------
			// 空中にいる場合
			// -----------------------------------------------------------------------
			_bIsGrounded = false;
		}
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
