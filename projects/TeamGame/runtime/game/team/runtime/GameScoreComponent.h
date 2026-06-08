#pragma once
#include "engine/unnamed/framework/components/base/BaseComponent.h"
#include <core/io/json/JsonReader.h>
#include <core/io/json/JsonWriter.h>
#include <./core/math/Vec2.h>
#include <./core/math/Vec3.h>
#include <./core/math/Vec4.h>

#include <cstdint>
#include <string>
#include <unordered_set>

namespace MyGame {

	class GameScoreComponent : public Unnamed::BaseComponent {
	public:
		// -----------------------------------------------------------------------
		// ライフサイクル
		// -----------------------------------------------------------------------

		/// コンポーネントがアタッチされたときに呼び出される
		void OnAttached() override;

		/// 毎フレーム更新
		void OnTick(float deltaTime) override;

		/// 描画フレーム更新
		void OnRenderTick(float renderDeltaTime, float interpolationAlpha) override;

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
		// スコア操作
		// -----------------------------------------------------------------------

		/// スコアと一度だけ加算するための記録をリセット
		void ResetScore();

		/// 任意のスコアを加算
		void AddScore(int value);

		/// ゴミが海へ落ちたときのスコアを加算
		bool AddTrashToSeaScore(uint64_t trashGuid);

		/// ゴミが穴へ入ったときのスコアを加算
		bool AddTrashIntoHoleScore(uint64_t trashGuid);

		/// ボールを穴でキャッチしたときのボーナスを加算
		bool AddBallCatchScore();

		/// ホールインワンのボーナスを加算
		bool AddHoleInOneBonus();

		/// ダイレクトホールインワンのボーナスを加算
		bool AddDirectHoleInOneBonus();

		/// OBペナルティを加算
		bool AddOutOfBoundsPenalty();

		/// 現在の合計スコアを取得
		[[nodiscard]] int GetScore() const;

	private:
		// スコア
		int _score = 0;
		// ゴミを海へ落としたときのスコア
		int _trashToSeaScore = 100;
		// ゴミを穴へ入れたときのスコア
		int _trashIntoHoleScore = 100;
		// ボールをキャッチしたときのスコア
		int _ballCatchScore = 10000;
		// ホールインワンのスコア
		int _holeInOneScore = 10000;
		// ダイレクトホールインワンのスコア
		int _directHoleInOneScore = 20000;
		// OBのペナルティスコア
		int _obPenaltyScore = -1000;
		// ボールキャッチスコアを加算済みかどうか
		bool _hasAddedBallCatchScore = false;
		// ホールインワンスコアを加算済みかどうか
		bool _hasAddedHoleInOneScore = false;
		// ダイレクトホールインワンスコアを加算済みかどうか
		bool _hasAddedDirectHoleInOneScore = false;
		// OBペナルティを加算済みかどうか
		bool _hasAddedOutOfBoundsPenalty = false;
		// 海へ落ちたゴミのGUID記録
		std::unordered_set<uint64_t> _scoredTrashToSeaGuids;
		// 穴へ入ったゴミのGUID記録
		std::unordered_set<uint64_t> _scoredTrashIntoHoleGuids;

	};
}
