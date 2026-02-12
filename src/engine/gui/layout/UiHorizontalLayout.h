#pragma once
#include "base/UiLayout.h"

namespace Unnamed::Gui {
	class UiHorizontalLayout : public UiLayout {
	public:
		UiHorizontalLayout()           = default;
		~UiHorizontalLayout() override = default;

		const char* GetTypeName() const override { return "HorizontalLayout"; }

	protected:
		void UpdateLayout(const Rect& parentGlobalRect) override;
	};
}
