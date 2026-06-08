#pragma once
#include "engine/unnamed/framework/components/base/BaseComponent.h"
#include <core/io/json/JsonReader.h>
#include <core/io/json/JsonWriter.h>
#include <./core/math/Vec2.h>
#include <./core/math/Vec3.h>
#include <./core/math/Vec4.h>

#include <string>

namespace MyGame {

	class GameCountDownComponent : public Unnamed::BaseComponent {
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



		/// カウントダウン開始
		void Start(float seconds);
		/// 停止
		void Stop();
		/// リセット
		void Reset();
		/// カウントダウンがアクティブかどうか
		[[nodiscard]] bool IsActive() const;
		/// カウントダウンが終了しているかどうか
		[[nodiscard]] bool IsFinished() const;
		/// カウントダウンの残り時間を取得
		[[nodiscard]] float GetRemainingTime() const;
		/// カウントダウンの進行度を0.0～1.0で取得（1.0が開始、0.0が終了）
		[[nodiscard]] float GetProgress01() const;
	private:
		// カウントダウンの残り時間
		float _countDownTime = 0.0f;
		// カウントダウンがアクティブかどうか
		bool _isActive = false;
		// カウントダウンの初期時間
		float _initialTime = 0.0f;

	};
}
