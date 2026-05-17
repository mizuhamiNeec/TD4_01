#pragma once

// -----------------------------------------------------------------------
// インクルード
// -----------------------------------------------------------------------

#include "engine/unnamed/framework/components/base/BaseComponent.h"
#include "engine/unnamed/framework/components/TransformComponent.h"
#include <core/io/json/JsonReader.h>
#include <core/io/json/JsonWriter.h>
#include <core/math/Vec3.h>
#include <string>
#include <memory>

#include <engine/physics/core/Physics.h>	// 物理
#include "collision/BoxKinematicCollisionResolver.h"

namespace MyGame {

	/// @brief ゴミオブジェクトのコンポーネント
	/// 
	/// ゴミオブジェクトの物理演算と落下状態を管理するコンポーネント。
	/// GolfBallComponentを参考にエンジンとの相性を最適化。
	class TrashObjMoverComponent : public Unnamed::BaseComponent {
	public:
		// ===================================================================
		// セクション1: ライフサイクル メソッド
		// ===================================================================

		/// @brief コンポーネントがアタッチされたときに呼び出される
		void OnAttached() override;

		/// @brief 毎フレーム更新
		void OnTick(float deltaTime) override;

		/// @brief コンポーネントがデタッチされたときに呼び出される
		void OnDetached() override;

		// ===================================================================
		// セクション2: 公開インターフェース
		// ===================================================================

		/// @brief ゴミの重量を設定
		void SetMass(float mass);

		/// @brief ゴミの重量を取得
		[[nodiscard]] float GetMass() const;

		/// @brief 現在の位置を取得
		[[nodiscard]] Vec3 GetCurrentPosition() const;

		/// @brief 現在の速度を取得
		[[nodiscard]] Vec3 GetCurrentVelocity() const;

		/// @brief ゴミが落下状態かを取得
		[[nodiscard]] bool IsFalling() const;

		/// @brief ゴミを落下状態に設定
		/// @param isFalling true: 落下開始（地面判定無効化）、false: 落下終了
		void SetFalling(bool isFalling);

		// ===================================================================
		// セクション3: BaseComponent の必須オーバーライド
		// ===================================================================

		[[nodiscard]] std::string_view GetStableName() const override;
		[[nodiscard]] std::string_view GetComponentName() const override;

#ifdef _DEBUG
		void DrawInspectorImGui() override;
#endif

		/// @brief JSONからコンポーネントのデータを読み込む
		void Deserialize(const Unnamed::JsonReader& reader) override;

		/// @brief コンポーネントのデータをJSONに書き込む
		void Serialize(Unnamed::JsonWriter& writer) const override;

	private:
		// ===================================================================
		// セクション6: プライベート メンバ変数 - 物理設定
		// ===================================================================

		/// 物理エンジンのポインタ
		Unnamed::Physics::Engine* _physicsEngine = nullptr;

		/// 物理衝突解決用のリゾルバ
		std::unique_ptr<Unnamed::BoxKinematicCollisionResolver> _collisionResolver;

		/// ボックスサイズ（半径）
		Vec3 _halfSize = { 0.5f, 0.5f, 0.5f };

		/// 速度
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

		// ===================================================================
		// セクション6: プライベート メンバ変数 - ゴミ情報
		// ===================================================================

		/// ゴミの重量
		float _mass = 1.0f;

		/// 地面接触フラグ
		bool _bIsGrounded = true;

		/// 落下状態フラグ（穴に吸い込まれているか）
		bool _bIsFalling = false;

		// ===================================================================
		// セクション6: プライベート メンバ変数 - キャッシュ
		// ===================================================================

		/// TransformComponentのキャッシュ
		Unnamed::TransformComponent* _transform = nullptr;

		// ===================================================================
		// セクション7: プライベート ヘルパーメソッド
		// ===================================================================

		/// @brief 物理ベースの更新（重力・速度制限）
		void UpdatePhysics(float deltaTime);

		/// @brief 速度を上限でクランプ
		void ClampVelocity();

		/// @brief 地面衝突判定と反射を処理
		void HandleGroundCollision();

		/// @brief 地表摩擦を適用
		void ApplyFriction();
	};

}
