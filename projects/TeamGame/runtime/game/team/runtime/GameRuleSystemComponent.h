#pragma once
#include "engine/unnamed/framework/components/base/BaseComponent.h"
#include <core/io/json/JsonReader.h>
#include <core/io/json/JsonWriter.h>
#include <./core/math/Vec2.h>
#include <./core/math/Vec3.h>
#include <./core/math/Vec4.h>

#include <string>


namespace MyGame {

	// カウントダウンコンポーネント
	class GameCountDownComponent;
	// スコア管理コンポーネント
	class GameScoreComponent;

	// ゲームルール管理コンポーネント
	class GameRuleSystemComponent : public Unnamed::BaseComponent {
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

	private:
		// ゲームフェーズ
		enum class GamePhase {
			Ready,
			Countdown,
			Playing,
			Result
		};

		// ゲーム開始フラグ
		bool _isGameStarted = false;
		// ゲーム終了フラグ
		bool _isGameEnded = false;

		// 場外か　
		bool _isOutOfBounds = false;
		// ホールインワンかどうか
		bool _isHoleInOne = false;
		// ダイレクトホールインワンかどうか
		bool _isDirectHoleInOne = false;
	};
}
