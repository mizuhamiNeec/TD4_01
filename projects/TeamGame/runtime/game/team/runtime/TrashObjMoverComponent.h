#pragma once
#include "engine/unnamed/framework/components/base/BaseComponent.h"
#include <core/io/json/JsonReader.h>
#include <core/io/json/JsonWriter.h>
#include <string>

#include <core/math/Vec3.h>         // 3D ベクトル

#include <engine/physics/core/Physics.h>	// 物理

namespace Unnamed {
	class BaseKinematicCollisionResolver;
}

namespace MyGame {

	/// @brief ゴミオブジェクトのコンポーネント
	/// ゴミの重量やタイプを管理するだけで、動きは物理エンジンに任せる
	class TrashObjMoverComponent : public Unnamed::BaseComponent {
	public:
		/// 状態
		enum class State {
			Idle,		// 待機
			Flying,		// 飛行
			Landing,	// 着地
			Settled,	// 
		};


		// -----------------------------------------------------------------------
		// ライフサイクル
		// -----------------------------------------------------------------------

		/// コンポーネントがアタッチされたときに呼び出される
		void OnAttached() override;

		/// 毎フレーム更新
		void OnTick(float deltaTime) override;

		/// コンポーネントがデタッチされたときに呼び出される
		void OnDetached() override;

		// -----------------------------------------------------------------------
		// ゴミオブジェクト設定
		// -----------------------------------------------------------------------

		/// @brief ゴミの重量を設定
		void SetMass(float mass);

		/// @brief ゴミの重量を取得
		[[nodiscard]] float GetMass() const;

		/// @brief ゴミの状態取得
		[[nodiscard]] State GetState() const;

		/// @brief 現在の位置を取得
		[[nodiscard]] Vec3 GetCurrentPosition() const;

		/// @brief 現在の速度を取得
		[[nodiscard]] Vec3 GetCurrentVelocity() const;


		/// 衝撃波設定
		void SetShockWave(const Vec3& occurrenceLocation, const float strength);

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

		/// @brief 物理ベースの更新（重力・風・位置更新）
		/// @param deltaTime フレーム時間
		/// @reason 基礎となる物理挙動を分離することで、誘導補正との混同を避ける
		void UpdatePhysics(float deltaTime);



		/// 状態によっての処理
		void StateProcess();

		/// @brief 地面衝突判定と反射を処理
		/// @reason ボールが地面に到達したときの物理的な反射を計算
		void HandleGroundCollision();

		/// @brief 地表摩擦を適用
		/// @reason 地面上での速度減衰を計算
		void ApplyFriction();


	private:
		// -----------------------------------------------------------------------
		// 物理
		// -----------------------------------------------------------------------	
		Unnamed::Physics::Engine* _physicsEngine = nullptr;

		/// 物理衝突解決用のリゾルバ
		std::unique_ptr<Unnamed::BaseKinematicCollisionResolver> mCollisionResolver;

		/// サイズ半径
		Vec3 _halfSize = { 0.5f,0.5f ,0.5f };

		/// 速度
		Vec3 _velocity = {};

		/// 重力
		float _gravity = 9.81f;

		/// 反発係数（0.0～1.0、地面衝突時の速度反転スケール）
		/// 理由：低いほどエネルギーを失い、バウンドが減衰する
		float _bounceDamping = 0.7f;

		/// 地表摩擦係数（0.0～1.0、毎フレームの速度減衰）
		/// 理由：地面上での横方向速度を徐々に減衰させる
		float _frictionCoefficient = 0.95f;

		/// 地面判定の高さ閾値（単位: units）
		/// 理由：Y <= この値で地面衝突と判定
		float _groundLevel = 0.0f;

		/// 停止判定の速度閾値（単位: units/sec）
		/// 理由：速度がこれ以下で完全に停止と判定
		float _stopVelocityThreshold = 0.01f;

		/// 速度上限クランプ値（単位: units/sec）
		float _maxSpeedClamp = 50.0f;
	private: /// ゴミ自体の情報
		/// ゴミの重量（物理エンジンが使用）
		float _mass = 1.0f;

		/// 初期位置
		Vec3 _startPosition = {};

		/// 位置 
		Vec3 _position = {};

		/// 状態
		State _state = State::Idle;

		/// 物理挙動フラグ
		bool _isRigidBoby = false;

		/// 重力フラグ
		bool _isGravity = false;

		/// 着地
		bool _isOnGround = true;

		/// アクションフラグ
		bool _isAction = false;

		/// アニメーションフラグ
		bool _isAnimation = false;

	private: /// 衝撃波系
		/// 衝撃波方向
		Vec3 _shockWaveDirection = {};

		/// 衝撃波強度
		float _shockWaveStrength = 0.0f;

		/// 発生位置
		Vec3 _occurrenceLocation = {};

		/// 衝撃による回転処理
		Vec3 _shockWaveRotation = {};
	};

}
