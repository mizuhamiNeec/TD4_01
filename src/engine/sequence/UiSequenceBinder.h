#pragma once

#include <string_view>

#include "SequenceBinder.h"

namespace Unnamed {
	class Scene;
}

namespace Unnamed::Gui {
	class UiWidget;
	class UiTransformComponent;
	class UiLinearLayoutComponent;
	class UiPanelStyleComponent;
}

namespace Unnamed {
	class UiSequenceBinder final : public ISequenceBinder {
	public:
		explicit UiSequenceBinder(Scene* scene = nullptr);

		void SetScene(Scene* scene);
		[[nodiscard]] Scene* GetScene() const;

		[[nodiscard]] bool SupportsTarget(
			SEQUENCE_BINDING_TARGET target
		) const override;
		bool ApplyValue(const SequenceBinding& binding, float value) override;

	private:
		[[nodiscard]] Gui::UiWidget* FindWidgetByNameRecursive(
			Gui::UiWidget* root, std::string_view widgetName
		) const;

		[[nodiscard]] bool ApplyTransformProperty(
			Gui::UiTransformComponent& component,
			std::string_view           propertyPath,
			float                      value
		) const;
		[[nodiscard]] bool ApplyLayoutProperty(
			Gui::UiLinearLayoutComponent& component,
			std::string_view              propertyPath,
			float                         value
		) const;
		[[nodiscard]] bool ApplyPanelStyleProperty(
			Gui::UiPanelStyleComponent& component,
			std::string_view            propertyPath,
			float                       value
		) const;

		Scene* mScene = nullptr;
	};
}
