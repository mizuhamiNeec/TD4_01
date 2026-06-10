#pragma once

#include "engine/unnamed/framework/components/base/BaseComponent.h"
#include <core/io/json/JsonReader.h>
#include <core/io/json/JsonWriter.h>
#include <core/math/Vec3.h>

#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace Unnamed {
	class Entity;
}

namespace MyGame {

	// ゴミの波生成を担当するコンポーネント
	class TrashObjSpawnerComponent : public Unnamed::BaseComponent {
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

		// -----------------------------------------------------------------------
		// 生成操作
		// -----------------------------------------------------------------------

		/// 指定数のゴミを落下開始状態で生成
		void SpawnWave(int count);

	private:
		// テンプレート指定。空ならtemplateTagまたはTrashObjMoverComponent検索を使う。
		std::vector<uint64_t> _templateEntityGuids;
		// テンプレート候補をタグ検索するための名前
		std::string _templateTag = "Trash";
		// 生成したゴミに付けるタグ
		std::string _spawnedTrashTag = "Trash";
		// 生成名の接頭辞
		std::string _spawnedNamePrefix = "AutoTrash";
		// エディタ階層上の生成先フォルダ
		std::string _spawnedFolderPath = "Generated/Trash";

		// 落下開始位置の中心
		Vec3 _spawnCenter = Vec3(0.0f, 10.0f, 10.0f);
		// XZ方向のランダム範囲。Yは高さMin/Maxで制御する。
		Vec3 _spawnHalfExtent = Vec3(10.0f, 0.0f, 8.0f);
		// 落下開始高さの最小値
		float _spawnHeightMin = 8.0f;
		// 落下開始高さの最大値
		float _spawnHeightMax = 14.0f;
		// 初速の最小値
		Vec3 _initialVelocityMin = Vec3(-1.0f, -1.0f, -1.0f);
		// 初速の最大値
		Vec3 _initialVelocityMax = Vec3(1.0f, 0.0f, 1.0f);
		// スケールの最小倍率
		float _scaleMin = 0.85f;
		// スケールの最大倍率
		float _scaleMax = 1.15f;
		// ランダムYaw回転を付けるかどうか
		bool _randomYaw = true;
		// 生成を有効にするかどうか
		bool _spawnEnabled = true;
		// デバッグ用の生成済み数
		int _spawnedTotal = 0;

		std::mt19937 _randomEngine;

		/// 候補テンプレートをシーンから収集
		[[nodiscard]] std::vector<Unnamed::Entity*> CollectTemplateEntities() const;

		/// テンプレートから1つのゴミEntityを生成
		[[nodiscard]] Unnamed::Entity* SpawnOne(Unnamed::Entity& templateEntity);

		/// 自分が生成した実体をテンプレート候補から除外するか判定
		[[nodiscard]] bool IsGeneratedTrashEntity(const Unnamed::Entity& entity) const;

		/// 指定範囲の乱数を取得
		[[nodiscard]] float RandomRange(float minValue, float maxValue);

		/// ランダムな生成位置を取得
		[[nodiscard]] Vec3 BuildRandomSpawnPosition();

		/// ランダムな初速を取得
		[[nodiscard]] Vec3 BuildRandomInitialVelocity();
	};
}
