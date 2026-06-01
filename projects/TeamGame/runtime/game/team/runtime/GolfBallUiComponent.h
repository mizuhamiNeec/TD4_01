#pragma once
#include "engine/unnamed/framework/components/base/BaseComponent.h"
#include <core/io/json/JsonReader.h>
#include <core/io/json/JsonWriter.h>
#include <./core/math/Vec2.h>
#include <./core/math/Vec3.h>
#include <./core/math/Vec4.h>

#include <string>

namespace MyGame {
	class GolfBallUiComponent : public Unnamed::BaseComponent {
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
		/// ワールド座標を画面座標へ変換した結果
		struct ScreenProjection {
			/// 画面上のピクセル座標
			Vec2 position = Vec2::zero;

			/// 対象がカメラ内に収まっているか
			bool onScreen = false;
		};

		/// UIを追従させる対象Entityを取得する
		[[nodiscard]] Unnamed::Entity* ResolveTargetEntity() const;

		/// ワールド座標を画面座標へ投影する
		[[nodiscard]] bool ProjectWorldToScreen(
			const Vec3& worldPosition,
			const Vec2& viewportSize,
			ScreenProjection& outProjection
		) const;

		/// 画面外の座標を画面端へクランプする
		[[nodiscard]] Vec2 ClampToScreenEdge(
			const Vec2& screenPosition,
			const Vec2& viewportSize
		) const;

		/// 追従対象のEntity GUID。0の場合はタグで探す
		uint64_t _targetEntityGuid = 0;

		/// GUID未指定時に追従対象を探すためのタグ
		std::string _targetTag = "Ball";

		/// 画面に表示するUI画像のパス
		std::string _texturePath =
			"projects/TeamGame/content/textures/UI/golfball_Loupe/golfball_Loupe.png";

		/// UIスプライトの描画サイズ
		Vec2 _spriteSize = Vec2(96.0f, 96.0f);

		/// 投影された画面座標からずらす量
		Vec2 _screenOffset = Vec2::zero;

		/// 画面端に表示するときの内側余白
		float _edgePadding = 48.0f;

		/// 他の画面スプライトとの描画順
		int32_t _sortKey = 1000;

		/// 対象が画面内にいるときもUIを描画するか
		bool _drawWhenOnScreen = false;

		/// 対象が画面外にいるときも画面端にUIを描画するか
		bool _drawWhenOffScreen = true;

		/// UIスプライトに乗算する色
		Vec4 _color = Vec4::one;
	};
}
