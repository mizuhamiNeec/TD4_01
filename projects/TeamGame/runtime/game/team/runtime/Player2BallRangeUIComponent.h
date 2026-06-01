#pragma once

#include "engine/unnamed/framework/components/base/BaseComponent.h"

#include <core/io/json/JsonReader.h>
#include <core/io/json/JsonWriter.h>

#include <cstdint>
#include <string>
#include <string_view>

namespace Unnamed {
	class Entity;
}

namespace Unnamed::Gui {
	class UiDigitStripComponent;
	class UiWidget;
}

namespace MyGame {
	class Player2BallRangeUIComponent : public Unnamed::BaseComponent {
	public:
		void OnAttached() override;
		void OnRenderTick(float renderDeltaTime, float interpolationAlpha) override;
		void OnDetached() override;

		[[nodiscard]] std::string_view GetStableName() const override;
		[[nodiscard]] std::string_view GetComponentName() const override;

#ifdef _DEBUG
		void DrawInspectorImGui() override;
#endif

		void Deserialize(const Unnamed::JsonReader& reader) override;
		void Serialize(Unnamed::JsonWriter& writer) const override;

	private:
		/// @brief 距離計算の基準になるプレイヤーEntityを解決する
		[[nodiscard]] Unnamed::Entity* ResolvePlayerEntity() const;

		/// @brief 距離計算の対象になるボールEntityを解決する
		[[nodiscard]] Unnamed::Entity* ResolveBallEntity() const;

		/// @brief GUID、タグ、名前の順でEntityを探す
		[[nodiscard]] Unnamed::Entity* ResolveEntity(
			uint64_t entityGuid,
			const std::string& tag,
			const std::string& name
		) const;

		/// @brief UI JSON 内の数値表示コンポーネントを取得する
		[[nodiscard]] Unnamed::Gui::UiDigitStripComponent* ResolveDigitStrip()
		const;

		/// @brief UI階層から指定名のWidgetを再帰的に探す
		[[nodiscard]] Unnamed::Gui::UiWidget* FindWidgetByName(
			Unnamed::Gui::UiWidget* widget,
			std::string_view name
		) const;

		/// @brief オーナーのUiCanvasが距離表示用UIを読むように揃える
		void EnsureUiCanvasAsset() const;

		/// @brief 0の場合はタグまたは名前でプレイヤーを探す
		uint64_t _playerEntityGuid = 0;

		/// @brief 0の場合はタグまたは名前でボールを探す
		uint64_t _ballEntityGuid = 0;

		/// @brief 空の場合は名前検索にフォールバックする
		std::string _playerTag;

		/// @brief ボールはシーン側でタグが付いているため既定値を持つ
		std::string _ballTag = "Ball";

		/// @brief タグ未指定時のプレイヤー検索名
		std::string _playerName = "Player";

		/// @brief タグ検索に失敗した場合のボール検索名
		std::string _ballName = "Ball";

		/// @brief 距離表示に使うUIドキュメント
		std::string _uiAssetPath =
			"projects/TeamGame/content/ui/number_UI_distanceText.ui.json";

		/// @brief 距離値を流し込むDigitStrip付きWidget名
		std::string _digitWidgetName = "number_UI";

		/// @brief ワールド単位と表示単位が違う場合に調整する倍率
		float _distanceScale = 1.0f;

		/// @brief 桁数を超えた値で表示が伸びすぎないようにする上限
		int32_t _maxDisplayValue = 999;
	};
}

