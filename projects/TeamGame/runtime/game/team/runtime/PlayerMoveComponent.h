#pragma once
#include "engine/unnamed/framework/components/base/BaseComponent.h"
#include <core/io/json/JsonReader.h>
#include <core/io/json/JsonWriter.h>
#include <./core/math/Vec2.h>
#include <./core/math/Vec3.h>

namespace MyGame {

/// @brief プレイヤーの移動を管理するコンポーネント
/// 
/// TransformComponent API に準拠した移動実装:
/// - ローカル位置を GetPosition() / SetPosition() で取得・設定
/// - ワールド方向は Right() / Forward() で取得（親の回転を反映）
/// - 瞬間移動時は RequestInterpolationResync() で補間をリセット
/// 
/// 参考: TransformComponent ドキュメント
/// - 役割: ローカル姿勢の保持（position / rotation / scale）
/// - 座標系: ローカル値(親基準) vs ワールド値(ルート基準)
/// - 補間: シミュレーション姿勢 vs 描画補間姿勢の管理
	class PlayerMoveComponent : public Unnamed::BaseComponent {
	public:
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
		// 移動制御
		// -----------------------------------------------------------------------

		/// @brief 移動方向を設定（-1.0～1.0の範囲）
		/// @param direction 移動方向 (x=水平, z=前後)
		void SetMoveDirection(const Vec2& direction);

		/// @brief 現在の移動方向を取得
		[[nodiscard]] Vec2 GetMoveDirection() const;

		/// @brief 移動速度を設定（単位: units/sec）
		void SetMoveSpeed(float speed);

		/// @brief 現在の移動速度を取得
		[[nodiscard]] float GetMoveSpeed() const;

		/// @brief ジャンプを実行
		void Jump();

		/// @brief 地面に接触しているかを取得
		[[nodiscard]] bool IsGrounded() const;

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
		// 移動パラメータ
		// -----------------------------------------------------------------------

		/// 移動方向（-1.0～1.0の範囲）
		Vec2 _moveDirection = Vec2(0.0f, 0.0f);

		/// 移動速度（単位: units/sec）
		float _moveSpeed = 5.0f;

		/// 重力加速度
		float _gravity = 9.81f;

		/// ジャンプ力
		float _jumpForce = 5.0f;

		/// 現在の垂直速度
		float _verticalVelocity = 0.0f;

		/// 地面に接触しているかのフラグ
		bool _bIsGrounded = true;

		/// 地面レイキャストの距離
		float _groundCheckDistance = 0.1f;
	};

}
