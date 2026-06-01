#pragma once

#include "UiComponent.h"

#include "engine/gui/Rect.h"
#include "engine/gui/UiWidget.h"

namespace Unnamed::Gui {
	class UiTransformComponent final : public UiComponent {
	public:
		[[nodiscard]] std::string_view GetTypeName() const override {
			return "Transform";
		}

		void OnAttached(UiWidget& owner) override;
		void OnBeforeLayout(UiWidget& owner) override;

		void Serialize(JsonWriter& writer) const override;
		void Deserialize(const JsonReader& reader) override;

		void SyncFromWidget(const UiWidget& owner);

		void SetRect(const Rect& rect);
		void SetAnchors(const Anchors& anchors);
		void SetMargins(const Margins& margins);
		void SetPivot(const Pivot& pivot);
		void SetSizePolicy(UiSizePolicy sizePolicy);
		void SetSizeConstraints(UiSizeConstraints constraints);
		/// @brief レイアウトのExpand配分で使う重みを設定します。
		/// @param weight 0より大きい値を推奨（0以下は内部で補正）
		void SetLayoutWeight(float weight);

		[[nodiscard]] const Rect& GetRect() const;
		[[nodiscard]] const Anchors& GetAnchors() const;
		[[nodiscard]] const Margins& GetMargins() const;
		[[nodiscard]] const Pivot& GetPivot() const;
		[[nodiscard]] UiSizePolicy GetSizePolicy() const;
		[[nodiscard]] const UiSizeConstraints& GetSizeConstraints() const;
		/// @brief レイアウトのExpand配分で使う重みを取得します。
		/// @return Expand配分用の重み
		[[nodiscard]] float GetLayoutWeight() const;

	private:
		Rect              mRect            = {};
		Anchors           mAnchors         = {};
		Margins           mMargins         = {};
		Pivot             mPivot           = {};
		UiSizePolicy      mSizePolicy      = {};
		UiSizeConstraints mSizeConstraints = {};
		float             mLayoutWeight    = 1.0f;
		bool              mNeedsApply      = true;
	};
}
