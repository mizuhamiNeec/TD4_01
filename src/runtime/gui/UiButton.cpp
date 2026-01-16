#include "UiButton.h"

#include "engine/unnamed/subsystem/console/Log.h"

namespace Unnamed::Gui {
	UiButton::UiButton() = default;

	UiButton::~UiButton() = default;

	void UiButton::SetText(const std::string_view& text) { mText = text; }

	std::string_view UiButton::GetText() const { return mText; }

	void UiButton::SetOnClick(const std::function<void()>& callback) {
		mOnClick = callback;
	}

	void UiButton::SetColors(
		const Color& normal, const Color& hovered, const Color& pressed
	) {
		mColorNormal  = normal;
		mColorHovered = hovered;
		mColorPressed = pressed;
	}

	void UiButton::BuildDrawCommands(std::vector<UiDrawCommand>& out) const {
		if (!IsVisible()) {
			return;
		}

		const Rect& r = GetGlobalRect();

		// 状態で色を変える
		Color bg = mColorNormal;
		if (IsPressed()) {
			bg = mColorPressed;
		} else if (IsHovered()) {
			bg = mColorHovered;
		}

		UiDrawCommand rectCmd;
		rectCmd.type                 = UiDrawCommandType::RECT;
		rectCmd.rect.rect            = r;
		rectCmd.rect.fillColor       = bg;
		rectCmd.rect.cornerRadius    = mCornerRadius;
		rectCmd.rect.borderThickness = 1.0f;
		rectCmd.rect.borderColor     = mBorderColor;

		out.emplace_back(rectCmd);

		// テキストがあれば描く
		if (!mText.empty()) {
			UiDrawCommand textCmd;
			textCmd.type          = UiDrawCommandType::TEXT;
			textCmd.text.text     = mText;
			textCmd.text.color    = mTextColor;
			textCmd.text.fontSize = mFontSize;

			textCmd.text.position = Vec2(
				r.x + 8.0f,
				r.y + r.height * 0.5f
			);

			out.emplace_back(textCmd);
		}

		UiWidget::BuildDrawCommands(out);
	}

	void UiButton::OnClick() {
		UiWidget::OnClick();
		if (mOnClick) {
			mOnClick();
		}
	}

	void UiButton::OnSerialize(JsonWriter& writer) const {
		if (!mText.empty()) {
			writer.Key("text");
			writer.Write(mText);
		}
	}

	void UiButton::OnDeserialize(const JsonReader& reader) {
		if (reader.Has("text")) {
			SetText(reader["text"].GetString());
		}
	}
}
