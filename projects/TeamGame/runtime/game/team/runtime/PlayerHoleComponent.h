#pragma once

// -----------------------------------------------------------------------
// インクルード
// -----------------------------------------------------------------------

#include "engine/unnamed/framework/components/base/BaseComponent.h"
#include "engine/unnamed/framework/components/TransformComponent.h"
#include <core/io/json/JsonReader.h>
#include <core/io/json/JsonWriter.h>
#include <core/math/Vec3.h>
#include <vector>
#include <string>

namespace MyGame {

	class TrashObjMoverComponent;

	/// @brief プレイヤーの穴システムコンポーネント
	/// 
	/// プレイヤーが持つ穴の範囲を管理し、その範囲内に侵入したゴミオブジェクトを
	/// 落下状態に移行させるコンポーネント。
	/// 
	/// 機能:
	/// - 穴の範囲（半径）を管理
	/// - 穴の位置・サイズを動的に変更
	/// - フレームごとに穴の範囲内にいるゴミを検出
	/// - 穴に入ったゴミを落下させる処理を実行
	class PlayerHoleComponent : public Unnamed::BaseComponent {
	public:
		// ===================================================================
		// セクション1: ライフサイクル メソッド
		// ===================================================================

		/// @brief コンポーネントがアタッチされたときに呼び出される
		void OnAttached() override;

		/// @brief 毎フレーム更新 - 穴の範囲内にいるゴミを検出
		void OnTick(float deltaTime) override;

		/// @brief コンポーネントがデタッチされたときに呼び出される
		void OnDetached() override;

		// ===================================================================
		// セクション2: 公開インターフェース - 穴設定
		// ===================================================================

		/// @brief 穴の半径を設定
		void SetHoleRadius(float radius);

		/// @brief 穴の半径を取得
		[[nodiscard]] float GetHoleRadius() const;

		/// @brief 穴の中心位置を設定（プレイヤー相対座標）
		void SetHoleOffset(const Vec3& offset);

		/// @brief 穴の中心位置を取得（プレイヤー相対座標）
		[[nodiscard]] Vec3 GetHoleOffset() const;

		/// @brief 穴が有効かどうかを設定
		void SetHoleActive(bool active);

		/// @brief 穴が有効かどうかを取得
		[[nodiscard]] bool IsHoleActive() const;

		// ===================================================================
		// セクション2: 公開インターフェース - 穴の操作
		// ===================================================================

		/// @brief 穴を移動する（入力ベース）
		/// @param direction 移動方向（正規化推奨）
		/// @param speed 移動速度
		void MovePole(const Vec3& direction, float speed);

		/// @brief 穴のサイズを即座に変更
		/// @param newSize 新しいサイズ（半径）
		void SetPoleSize(float newSize);

		/// @brief 穴のサイズをスムーズに変更（アニメーション付き）
		/// @param targetSize ターゲットサイズ（半径）
		/// @param speed 変更速度
		void SetPoleSizeSmooth(float targetSize, float speed);

		// ===================================================================
		// セクション2: 公開インターフェース - ゴミ検出・処理
		// ===================================================================

		/// @brief 穴の範囲内に入ったゴミの数を取得
		[[nodiscard]] int GetTrashInHoleCount() const;

		/// @brief 穴の範囲内にいるゴミのリストを取得
		[[nodiscard]] const std::vector<Unnamed::Entity*>& GetTrashInHole() const;

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
		// セクション6: プライベート メンバ変数 - 穴の設定
		// ===================================================================

		/// 穴の半径（単位: units）
		float _holeRadius = 2.0f;

		/// 穴の中心位置（プレイヤー相対座標）
		Vec3 _holeOffset = Vec3(0.0f, -1.0f, 0.0f);

		/// 穴が有効かどうか
		bool _isHoleActive = true;

		/// 穴のサイズ変更用（スムーズアニメーション）ターゲット値
		float _targetHoleRadius = 2.0f;

		/// 穴のサイズ変更速度
		float _holeSizeChangeSpeed = 1.0f;

		// ===================================================================
		// セクション6: プライベート メンバ変数 - 内部状態
		// ===================================================================

		/// 現在穴に入っているゴミのポインタリスト
		std::vector<Unnamed::Entity*> _trashInHole;

		/// 前フレームで穴に入っていたゴミのポインタリスト（追跡用）
		std::vector<Unnamed::Entity*> _previousTrashInHole;

		// ===================================================================
		// セクション6: プライベート メンバ変数 - キャッシュ
		// ===================================================================

		/// 所有者（プレイヤー）のTransformComponent - キャッシュ用
		Unnamed::TransformComponent* _ownerTransform = nullptr;

		// ===================================================================
		// セクション7: プライベート ヘルパーメソッド
		// ===================================================================

		/// @brief 穴の世界座標を計算
		[[nodiscard]] Vec3 GetHoleWorldPosition() const;

		/// @brief シーン内のすべてのゴミエンティティを検索
		void DetectTrashInHole();

		/// @brief エンティティが穴の範囲内にいるかを判定
		[[nodiscard]] bool IsEntityInHoleRange(Unnamed::Entity* entity) const;

		/// @brief ゴミを落下状態に移行させる
		void MakeTrashFall(Unnamed::Entity* trashEntity);

		/// @brief 穴に入ったゴミを管理（新規追加・削除を処理）
		void UpdateTrashList();

		/// @brief 穴のサイズ変更を処理（スムーズアニメーション）
		void UpdateHoleSizeAnimation(float deltaTime);
	};

}
