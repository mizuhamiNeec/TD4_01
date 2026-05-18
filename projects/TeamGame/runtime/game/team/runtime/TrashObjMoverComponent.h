#pragma once

#include "engine/unnamed/framework/components/base/BaseComponent.h"
#include "engine/unnamed/framework/components/TransformComponent.h"
#include <core/io/json/JsonReader.h>
#include <core/io/json/JsonWriter.h>
#include <core/math/Vec3.h>
#include <core/math/Quaternion.h>
#include <string>
#include <memory>
#include <algorithm>
#include <cmath>

#include <engine/physics/core/Physics.h>
#include "collision/BoxKinematicCollisionResolver.h"

namespace MyGame {

	/// @brief ゴミオブジェクトの移動・回転・吸い込みを管理するコンポーネント
	/// 
	/// ゴミの物理演算と挙動制御:
	/// - 重力により常に下方向に加速（自由落下）
	/// - 地面衝突時に反発・摩擦を適用
	/// - 穴から吸い込み力を受けると移動方向が変化
	/// - 落下中は移動方向に傾いて回転（自然な挙動）
	/// - 着地時は元の回転へゆっくり戻す
	class TrashObjMoverComponent : public Unnamed::BaseComponent {
	public:
		// -----------------------------------------------------------------------
		// ライフサイクル
		// -----------------------------------------------------------------------

		/// コンポーネントがアタッチされたときに呼び出される
		void OnAttached() override;

		/// 毎フレーム更新 - 物理演算、回転、吸い込み処理
		void OnTick(float deltaTime) override;

		/// コンポーネントがデタッチされたときに呼び出される
		void OnDetached() override;

		// -----------------------------------------------------------------------
		// ゴミの状態制御
		// -----------------------------------------------------------------------

		/// ゴミの重量を設定
		void SetMass(float mass);

		/// ゴミの重量を取得
		[[nodiscard]] float GetMass() const;

		/// 現在の位置を取得
		[[nodiscard]] Vec3 GetCurrentPosition() const;

		/// 現在の速度を取得
		[[nodiscard]] Vec3 GetCurrentVelocity() const;

		/// ゴミが落下状態かを取得
		[[nodiscard]] bool IsFalling() const;

		/// @brief ゴミを落下状態に設定
		/// @param isFalling true: 落下開始、false: 落下終了
		void SetFalling(bool isFalling);

		// -----------------------------------------------------------------------
		// 穴への吸い込み制御
		// -----------------------------------------------------------------------

		/// @brief 穴の位置と吸い込み力を設定
		/// @param holePosition 穴の世界座標
		/// @param suckPower 吸い込み力（0.0～1.0、大きいほど強く）
		void SetHoleSuckPosition(const Vec3& holePosition, float suckPower);

		/// 吸い込み処理を無効化（吸い込み力をクリア）
		void ClearHoleSuckPosition();

		// -----------------------------------------------------------------------
		// BaseComponent override
		// -----------------------------------------------------------------------

		[[nodiscard]] std::string_view GetStableName() const override;
		[[nodiscard]] std::string_view GetComponentName() const override;

#ifdef _DEBUG
		void DrawInspectorImGui() override;
#endif

		/// コンポーネントの値を読み込む際に使用されます
		void Deserialize(const Unnamed::JsonReader& reader) override;

		/// コンポーネントの値を書き込む際に使用されます
		void Serialize(Unnamed::JsonWriter& writer) const override;

	private:
		// -----------------------------------------------------------------------
		// 物理パラメータ
		// -----------------------------------------------------------------------

		/// 物理エンジンのポインタ
		Unnamed::Physics::Engine* _physicsEngine = nullptr;

		/// 物理衝突解決用のリゾルバ
		std::unique_ptr<Unnamed::BoxKinematicCollisionResolver> _collisionResolver;

		/// ボックスサイズ（半径）
		Vec3 _halfSize = { 0.5f, 0.5f, 0.5f };

		/// 現在の速度
		Vec3 _velocity = {};

		/// 現在位置
		Vec3 _position = {};

		/// 重力加速度
		float _gravity = 9.81f;

		/// 反発係数（0.0～1.0）
		float _bounceDamping = 0.7f;

		/// 地表摩擦係数（0.0～1.0）
		float _frictionCoefficient = 0.95f;

		/// 地面判定の高さ閾値
		float _groundLevel = 0.0f;

		/// 停止判定の速度閾値
		float _stopVelocityThreshold = 0.01f;

		/// 速度上限クランプ値
		float _maxSpeedClamp = 50.0f;

		// -----------------------------------------------------------------------
		// ゴミの状態
		// -----------------------------------------------------------------------

		/// ゴミの重量
		float _mass = 1.0f;

		/// 地面接触フラグ
		bool _bIsGrounded = true;

		/// 落下状態フラグ
		bool _bIsFalling = false;

		// -----------------------------------------------------------------------
		// 回転パラメータ
		// -----------------------------------------------------------------------

		/// 空中時の回転速度（度/秒）
		float _airRotationSpeed = 360.0f;

		/// 初期回転（着地時に戻す目標値）
		Quaternion _initialRotation = Quaternion::identity;

		/// 現在の回転（空中時に更新）
		Quaternion _currentRotation = Quaternion::identity;

		/// 着地時に回転をリセット中
		bool _bIsResetingRotation = false;

		/// 回転リセットの速度
		float _rotationResetSpeed = 5.0f;

		/// 移動方向への傾き強度（0.0=傾かない、1.0=完全に傾く）
		float _tiltTowardMovementStrength = 0.8f;

		/// 移動方向への傾きの滑らかさ（小=ゆっくり、大=急激）
		float _tiltLerpSpeed = 0.15f;

		// -----------------------------------------------------------------------
		// 吸い込み機能
		// -----------------------------------------------------------------------

		/// 吸い込み対象の穴の位置
		Vec3 _holeSuckPosition = {};

		/// 吸い込み力（0.0～1.0、大きいほど強く）
		float _holeSuckPower = 0.0f;

		/// 吸い込み中フラグ
		bool _bIsBeingSucked = false;

		// -----------------------------------------------------------------------
		// キャッシュ
		// -----------------------------------------------------------------------

		/// TransformComponentのキャッシュ
		Unnamed::TransformComponent* _transform = nullptr;

		// -----------------------------------------------------------------------
		// ヘルパーメソッド
		// -----------------------------------------------------------------------

		/// 物理ベースの更新（重力・速度制限）
		void UpdatePhysics(float deltaTime);

		/// 速度を上限でクランプ
		void ClampVelocity();

		/// 地面衝突判定と反射を処理
		void HandleGroundCollision();

		/// 地表摩擦を適用
		void ApplyFriction();

		/// 空中での回転を更新（移動方向に傾く）
		void UpdateAirRotation(float deltaTime);

		/// 着地時の回転リセット処理
		void UpdateRotationReset(float deltaTime);

		/// 穴への吸い込み処理
		void UpdateHoleSuck(float deltaTime);
	};

}
