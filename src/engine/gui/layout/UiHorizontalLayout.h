#pragma once
#include "base/UiLayout.h"

namespace Unnamed::Gui {
	class UiHorizontalLayout : public UiLayout {
	public:
		UiHorizontalLayout();
		~UiHorizontalLayout() override;

		const char* GetTypeName() const override {
			return "HorizontalLayoutPreset";
		}
	};
}
