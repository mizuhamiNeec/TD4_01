#pragma once
#include "UiWidget.h"

namespace Unnamed::Gui {
	class UiPanel : public UiWidget {
	public:
		UiPanel();
		~UiPanel() override;

		void                       SetBackgroundColor(const Color& c);
		[[nodiscard]] const Color& GetBackgroundColor() const;

		void                SetCornerRadius(float r);
		[[nodiscard]] float GetCornerRadius() const;

		[[nodiscard]] const char* GetTypeName() const override;

	protected:
		void BuildDrawCommands(std::vector<UiDrawCommand>& out) const override;

	private:
		Color mBackgroundColor = {0.15f, 0.15f, 0.18f, 1.0f};
		float mCornerRadius    = 4.0f;
	};
}
