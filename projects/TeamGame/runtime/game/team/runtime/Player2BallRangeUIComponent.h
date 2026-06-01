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
		[[nodiscard]] Unnamed::Entity* ResolvePlayerEntity() const;
		[[nodiscard]] Unnamed::Entity* ResolveBallEntity() const;
		[[nodiscard]] Unnamed::Entity* ResolveEntity(
			uint64_t entityGuid,
			const std::string& tag,
			const std::string& name
		) const;
		[[nodiscard]] Unnamed::Gui::UiDigitStripComponent* ResolveDigitStrip()
		const;
		[[nodiscard]] Unnamed::Gui::UiWidget* FindWidgetByName(
			Unnamed::Gui::UiWidget* widget,
			std::string_view name
		) const;
		void EnsureUiCanvasAsset() const;

		uint64_t _playerEntityGuid = 0;
		uint64_t _ballEntityGuid = 0;
		std::string _playerTag;
		std::string _ballTag = "Ball";
		std::string _playerName = "Player";
		std::string _ballName = "Ball";
		std::string _uiAssetPath =
			"projects/TeamGame/content/ui/number_UI_distanceText.ui.json";
		std::string _digitWidgetName = "number_UI";
		float _distanceScale = 1.0f;
		int32_t _maxDisplayValue = 999;
	};
}

