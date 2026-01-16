#include "UiPanel.h"

#include "engine/unnamed/subsystem/console/Log.h"

namespace Unnamed::Gui {
	UiPanel::UiPanel() = default;

	UiPanel::~UiPanel() = default;

	void UiPanel::SetBackgroundColor(const Color& c) {
		mBackgroundColor = c;
	}

	const Color& UiPanel::GetBackgroundColor() const {
		return mBackgroundColor;
	}

	void UiPanel::SetCornerRadius(const float r) { mCornerRadius = r; }

	float UiPanel::GetCornerRadius() const {
		return mCornerRadius;
	}

	const char* UiPanel::GetTypeName() const {
		return "Panel";
	}

	void UiPanel::BuildDrawCommands(std::vector<UiDrawCommand>& out) const {
		if (!IsVisible()) {
			return;
		}

		// 背景を描画
		const Rect& r = GetGlobalRect();

		UiDrawCommand cmd;
		cmd.type                 = UiDrawCommandType::RECT;
		cmd.rect.rect            = r;
		cmd.rect.fillColor       = mBackgroundColor;
		cmd.rect.cornerRadius    = mCornerRadius;
		cmd.rect.borderThickness = 0.0f;

		out.emplace_back(cmd);

		// 子も
		UiWidget::BuildDrawCommands(out);
	}
}
