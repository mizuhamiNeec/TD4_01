#pragma once
#include "runtime/gui/UiWidget.h"


namespace Unnamed::Gui {
	struct LayoutPadding {
		float left   = 0.0f;
		float top    = 0.0f;
		float right  = 0.0f;
		float bottom = 0.0f;
	};

	class UiLayout : public UiWidget {
	public:
		UiLayout();
		~UiLayout() override;

		/// @brief パディングを設定します。
		/// @param padding 新しいパディング値
		void SetPadding(const LayoutPadding& padding);

		/// @brief パディング設定を取得します。
		/// @return パディング設定への参照
		[[nodiscard]] const LayoutPadding& GetPadding() const;

		/// @brief アイテム間のスペーシングを設定します。
		/// @param spacing 新しいスペーシング値
		void SetSpacing(float spacing);

		/// @brief アイテム間のスペーシングを取得します。
		/// @return スペーシング値
		[[nodiscard]] float GetSpacing() const;

	protected:
		void OnSerialize(JsonWriter& writer) const override;
		void OnDeserialize(JsonReader& reader);

		LayoutPadding mPadding;
		float         mSpacing = 0.0f;
	};
}
