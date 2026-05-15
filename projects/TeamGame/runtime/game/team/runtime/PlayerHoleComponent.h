#pragma once
#include "engine/unnamed/framework/components/base/BaseComponent.h"
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
	/// - フレームごとに穴の範囲内にいるゴミを検出
	/// - 穴に入ったゴミを落下させる処理を実行
	class PlayerHoleComponent : public Unnamed::BaseComponent {
	public:
		// -----------------------------------------------------------------------
		// ライフサイクル
		// -----------------------------------------------------------------------

		/// コンポーネントがアタッチされたときに呼び出される
		void OnAttached() override;

		/// 毎フレーム更新 - 穴の範囲内にいるゴミを検出
		void OnTick(float deltaTime) override;

		/// コンポーネントがデタッチされたときに呼び出される
		void OnDetached() override;

		// -----------------------------------------------------------------------
		// 穴設定
		// -----------------------------------------------------------------------

		/// @brief 穴の半径を設定
		void SetHoleRadius(float radius);

		/// @brief 穴の半径を取得
		[[nodiscard]] float GetHoleRadius() const;

		/// @brief 穴の中心位置を設定（相対位置）
		void SetHoleOffset(const Vec3& offset);

		/// @brief 穴の中心位置を取得（相対位置）
		[[nodiscard]] Vec3 GetHoleOffset() const;

		/// @brief 穴が有効かどうかを設定
		void SetHoleActive(bool active);

		/// @brief 穴が有効かどうかを取得
		[[nodiscard]] bool IsHoleActive() const;

		// -----------------------------------------------------------------------
		// ゴミ検出・処理
		// -----------------------------------------------------------------------

		/// @brief 穴の範囲内に入ったゴミの数を取得
		[[nodiscard]] int GetTrashInHoleCount() const;

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
		// 穴設定
		// -----------------------------------------------------------------------

		/// 穴の半径（単位: units）
		float _holeRadius = 2.0f;

		/// 穴の中心位置（プレイヤー相対座標）
		Vec3 _holeOffset = Vec3(0.0f, -1.0f, 0.0f);

		/// 穴が有効かどうか
		bool _isHoleActive = true;

		// -----------------------------------------------------------------------
		// 内部状態
		// -----------------------------------------------------------------------

		/// 現在穴に入っているゴミのポインタリスト
		std::vector<Unnamed::Entity*> _trashInHole;

		/// 前フレームで穴に入っていたゴミのポインタリスト（追跡用）
		std::vector<Unnamed::Entity*> _previousTrashInHole;

		// -----------------------------------------------------------------------
		// 内部処理
		// -----------------------------------------------------------------------

		/// @brief 穴の世界座標を計算
		[[nodiscard]] Vec3 GetHoleWorldPosition() const;

		/// @brief シーン内のすべてのゴミエンティティを検索
		/// TrashObjMoverComponent を持つエンティティを探して、穴の範囲内にいるか確認
		void DetectTrashInHole();

		/// @brief エンティティが穴の範囲内にいるかを判定
		[[nodiscard]] bool IsEntityInHoleRange(Unnamed::Entity* entity) const;

		/// @brief ゴミを落下状態に移行させる
		void MakeTrashFall(Unnamed::Entity* trashEntity);

		/// @brief 穴に入ったゴミを管理（新規追加・削除を処理）
		void UpdateTrashList();
	};

}
