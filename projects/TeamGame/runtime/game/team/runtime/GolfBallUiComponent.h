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
		struct ScreenProjection {
			Vec2 position = Vec2::zero;
			bool onScreen = false;
		};

		[[nodiscard]] Unnamed::Entity* ResolveTargetEntity() const;
		[[nodiscard]] bool ProjectWorldToScreen(
			const Vec3& worldPosition,
			const Vec2& viewportSize,
			ScreenProjection& outProjection
		) const;
		[[nodiscard]] Vec2 ClampToScreenEdge(
			const Vec2& screenPosition,
			const Vec2& viewportSize
		) const;

		uint64_t _targetEntityGuid = 0;
		std::string _targetTag = "Ball";
		std::string _texturePath =
			"projects/TeamGame/content/textures/UI/golfball_Loupe/golfball_Loupe.png";
		Vec2 _spriteSize = Vec2(96.0f, 96.0f);
		Vec2 _screenOffset = Vec2::zero;
		float _edgePadding = 48.0f;
		int32_t _sortKey = 1000;
		bool _drawWhenOnScreen = true;
		bool _drawWhenOffScreen = true;
		Vec4 _color = Vec4::one;
	};
}
