#pragma once

#include "engine/unnamed/framework/components/base/BaseComponent.h"

#include <core/io/json/JsonReader.h>
#include <core/io/json/JsonWriter.h>

#include <string>
#include <string_view>

namespace Unnamed::Gui {
	class UiDigitStripComponent;
	class UiWidget;
}

namespace MyGame {
	class ResultScoreUiComponent : public Unnamed::BaseComponent {
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
		/// @brief Applies the latest stored score snapshot to the result UI.
		/// @param progress Count-up progress in [0, 1]; 1 shows the final score.
		void UpdateScoreUi(float progress) const;

		/// @brief Finds a DigitStrip component by widget name inside the owner's UI canvas.
		[[nodiscard]] Unnamed::Gui::UiDigitStripComponent* ResolveDigitStrip(
			std::string_view widgetName
		) const;

		/// @brief Recursively finds a widget by name in the UI tree.
		[[nodiscard]] Unnamed::Gui::UiWidget* FindWidgetByName(
			Unnamed::Gui::UiWidget* widget,
			std::string_view name
		) const;

		/// @brief Keeps the owner canvas pointed at the result UI document.
		void EnsureUiCanvasAsset() const;

		std::string _uiAssetPath = "projects/TeamGame/content/ui/result.ui.json";
		std::string _totalWidgetName = "score_total";
		std::string _trashToSeaWidgetName = "score_trash_to_sea";
		std::string _trashIntoHoleWidgetName = "score_trash_into_hole";
		std::string _ballCatchWidgetName = "score_ball_catch";
		std::string _holeInOneWidgetName = "score_hole_in_one";
		std::string _directHoleInOneWidgetName = "score_direct_hole_in_one";
		std::string _outOfBoundsPenaltyWidgetName = "score_ob_penalty";

		/// @brief Seconds the score count-up animation takes to reach the final value.
		float _countUpDuration = 2.0f;
		/// @brief Seconds elapsed since the result UI was attached.
		float _elapsed = 0.0f;
	};
}
