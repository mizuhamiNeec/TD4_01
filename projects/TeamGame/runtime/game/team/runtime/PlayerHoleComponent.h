#pragma once

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
	/// 穴の状態管理と吸い込み処理:
	/// - 穴の範囲（半径）、位置、サイズを動的に変更
	/// - 毎フレーム穴の範囲内のゴミを検出し、吸い込み力を適用
	/// - 地上のゴミを穴に吸い込む（落下中のゴミは吸い込まない）
	/// - 吸い込み強度を距離に基づいて計算（近いほど強い）
	class PlayerHoleComponent : public Unnamed::BaseComponent {
	public:
		// -----------------------------------------------------------------------
		// ライフサイクル
		// -----------------------------------------------------------------------

		/// コンポーネントがアタッチされたときに呼び出される
		void OnAttached() override;

		/// 毎フレーム更新 - 穴の範囲内にいるゴミを検出し吸い込み力を適用
		void OnTick(float deltaTime) override;

		/// コンポーネントがデタッチされたときに呼び出される
		void OnDetached() override;

		// -----------------------------------------------------------------------
		// 穴の設定
		// -----------------------------------------------------------------------

		/// 穴の半径を設定
		void SetHoleRadius(float radius);

		/// 穴の半径を取得
		[[nodiscard]] float GetHoleRadius() const;

		/// 穴の中心位置を設定（プレイヤー相対座標）
		void SetHoleOffset(const Vec3& offset);

		/// 穴の中心位置を取得（プレイヤー相対座標）
		[[nodiscard]] Vec3 GetHoleOffset() const;

		/// 穴が有効かどうかを設定
		void SetHoleActive(bool active);

		/// 穴が有効かどうかを取得
		[[nodiscard]] bool IsHoleActive() const;

		// -----------------------------------------------------------------------
		// 穴の操作
		// -----------------------------------------------------------------------

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

		// -----------------------------------------------------------------------
		// ゴミ検出・処理
		// -----------------------------------------------------------------------

		/// 穴の範囲内に入ったゴミの数を取得
		[[nodiscard]] int GetTrashInHoleCount() const;

		/// 穴の範囲内にいるゴミのリストを取得
		[[nodiscard]] const std::vector<Unnamed::Entity*>& GetTrashInHole() const;

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
		// 穴の設定パラメータ
		// -----------------------------------------------------------------------

		/// 穴の半径（単位: units）
		float _holeRadius = 2.0f;

		/// 穴の中心位置（プレイヤー相対座標）
		Vec3 _holeOffset = Vec3(0.0f, -1.0f, 0.0f);

		/// 穴が有効かどうか
		bool _isHoleActive = true;

		/// 穴のサイズ変更用ターゲット値
		float _targetHoleRadius = 2.0f;

		/// 穴のサイズ変更速度
		float _holeSizeChangeSpeed = 1.0f;

		// -----------------------------------------------------------------------
		// 吸い込み力パラメータ
		// -----------------------------------------------------------------------

		/// 地上のゴミへの吸い込み力（0.0～1.0）
		float _suckPower = 0.7f;

		/// 吸い込み効果が適用される範囲（穴の半径から何単位内か）
		float _suckEffectDistance = 6.0f;

		/// 吸い込み強度曲線（近づくほど吸い込みが強くなる効果を調整）
		/// 1.0=線形 / 2.0=二乗（急に増加） / 0.5=ルート（ゆっくり増加）
		float _suckIntensityCurve = 1.0f;

		// -----------------------------------------------------------------------
		// 内部状態
		// -----------------------------------------------------------------------

		/// 現在穴に入っているゴミのポインタリスト
		std::vector<Unnamed::Entity*> _trashInHole;

		/// 前フレームで穴に入っていたゴミのポインタリスト（追跡用）
		std::vector<Unnamed::Entity*> _previousTrashInHole;

		/// 所有者（プレイヤー）のTransformComponent - キャッシュ用
		Unnamed::TransformComponent* _ownerTransform = nullptr;

		// -----------------------------------------------------------------------
		// ヘルパーメソッド
		// -----------------------------------------------------------------------

		/// 穴の世界座標を計算
		[[nodiscard]] Vec3 GetHoleWorldPosition() const;

		/// シーン内のすべてのゴミエンティティを検索
		void DetectTrashInHole();

		/// エンティティが穴の範囲内にいるかを判定
		[[nodiscard]] bool IsEntityInHoleRange(Unnamed::Entity* entity) const;

		/// ゴミを落下状態に移行させる
		void MakeTrashFall(Unnamed::Entity* trashEntity);

		/// 穴に入ったゴミを管理（新規追加・削除を処理）
		void UpdateTrashList();

		/// 穴のサイズ変更を処理（スムーズアニメーション）
		void UpdateHoleSizeAnimation(float deltaTime);

		/// 穴に入ったゴミに吸い込み力を適用
		void ApplySuckForceToTrash(float deltaTime);
	};

}
