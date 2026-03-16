#pragma once

#include "UiWidget.h"

namespace Unnamed::Gui {
	class UiPanelStyleComponent;

	class UiPanel : public UiWidget {
	public:
		UiPanel();
		~UiPanel() override;

		void                       SetBackgroundColor(const Color& color);
		[[nodiscard]] const Color& GetBackgroundColor() const;

		void                SetCornerRadius(float radius);
		[[nodiscard]] float GetCornerRadius() const;

		[[nodiscard]] const char* GetTypeName() const override;

	private:
		[[nodiscard]] UiPanelStyleComponent* GetStyleComponent() const;
	};
}
