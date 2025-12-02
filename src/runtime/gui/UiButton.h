#pragma once
#include <functional>

#include "UiWidget.h"

namespace Unnamed::Gui {
	class UiButton : public UiWidget {
	public:
		UiButton();
		~UiButton() override;

		/// @brief ボタンのテキストを設定します。
		/// @param text 新しいテキスト
		void SetText(const std::string_view& text);

		/// @brief ボタンのテキストを取得します。
		/// @return ボタンのテキスト
		[[nodiscard]] std::string_view GetText() const;

		/// @brief クリック時のコールバックを設定します。
		/// @param callback クリック時に呼び出される関数
		void SetOnClick(const std::function<void()>& callback);

		/// @brief ボタンの各状態の色を設定します。
		/// @param normal 通常時の色
		/// @param hovered ホバー時の色
		/// @param pressed 押下時の色
		void SetColors(const Color& normal, const Color& hovered,
		               const Color& pressed);

		char const* GetTypeName() const override { return "Button"; }

	protected:
		/// @brief 描画コマンドを構築します。
		/// @param out 描画コマンドの出力先ベクター
		void BuildDrawCommands(std::vector<UiDrawCommand>& out) const override;

		void OnClick() override;

		void OnSerialize(JsonWriter& writer) const override;
		void OnDeserialize(const JsonReader& reader) override;

	private:
		std::string           mText;
		std::function<void()> mOnClick;

		Color mColorNormal{0.25f, 0.25f, 0.3f, 1.0f};
		Color mColorHovered{0.32f, 0.32f, 0.38f, 1.0f};
		Color mColorPressed{0.18f, 0.18f, 0.25f, 1.0f};

		Color mBorderColor{0.05f, 0.05f, 0.07f, 1.0f};
		Color mTextColor{1.0f, 1.0f, 1.0f, 1.0f};

		float mCornerRadius = 4.0f;
		float mFontSize     = 16.0f;
	};
}
