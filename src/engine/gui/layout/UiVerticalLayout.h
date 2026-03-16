#pragma once
#include "base/UiLayout.h"

namespace Unnamed::Gui {
	class UiVerticalLayout : public UiLayout {
	public:
		UiVerticalLayout();
		~UiVerticalLayout() override;

		const char* GetTypeName() const override {
			return "VerticalLayoutPreset";
		}
	};
}
