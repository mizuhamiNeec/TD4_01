#pragma once
#include "base/UiLayout.h"

namespace Unnamed::Gui {
	class UiVerticalLayout : public UiLayout {
	public:
		UiVerticalLayout()           = default;
		~UiVerticalLayout() override = default;

		char const* GetTypeName() const override { return "VerticalLayout"; }

	protected:
		void UpdateLayout(const Rect& parentGlobalRect) override;
	};
}
