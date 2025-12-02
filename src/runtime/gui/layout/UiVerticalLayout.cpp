#include "UiVerticalLayout.h"

#include <algorithm>

namespace Unnamed::Gui {
	void UiVerticalLayout::UpdateLayout(const Rect& parentGlobalRect) {
		// 自分のは既定に任せる
		UiLayout::UpdateLayout(parentGlobalRect);

		const Rect& selfRect = GetGlobalRect();

		const float contentLeft = selfRect.x + mPadding.left;
		const float contentTop = selfRect.y + mPadding.top;
		const float contentRight = selfRect.x + selfRect.width - mPadding.right;
		const float contentBottom = selfRect.y + selfRect.height - mPadding.
			bottom;

		const float contentWidth  = contentRight - contentLeft;
		const float contentHeight = contentBottom - contentTop;

		struct ChildInfo {
			UiWidget* widget;
			float     fixedHeight;
			bool      isExpand;
		};

		std::vector<ChildInfo> childrenInfo;
		childrenInfo.reserve(GetChildren().size());

		float totalFixedHeight  = 0.f;
		int   expandCount       = 0;
		int   visibleChildCount = 0;

		for (auto& pChild : GetChildren()) {
			UiWidget* child = pChild.get();
			if (!child || !child->IsVisible()) {
				continue;
			}

			const UiSizePolicy       policy     = child->GetSizePolicy();
			const UiSizeConstraints& constraint = child->GetSizeConstraints();

			float preferred = child->GetPreferredHeight();
			if (preferred <= 0.0f) {
				preferred = 32.0f; // TODO: パラメータ化
			}

			float clamped = std::clamp(preferred, constraint.minHeight,
			                           constraint.maxHeight);

			ChildInfo info{};
			info.widget = child;
			if (policy.vertical == UiSizePolicyAxis::FIXED) {
				info.isExpand    = false;
				info.fixedHeight = clamped;
				totalFixedHeight += clamped;
			} else {
				info.isExpand    = true;
				info.fixedHeight = 0.0f;
				expandCount++;
			}

			childrenInfo.emplace_back(info);
			visibleChildCount++;
		}

		// スペーシングからの高さ
		const float totalSpacing =
			mSpacing * static_cast<float>(std::max(0, visibleChildCount - 1));

		// Expand に割り当てる高さ
		float remainingHeight = contentHeight - totalFixedHeight - totalSpacing;
		remainingHeight       = std::max(remainingHeight, 0.0f);

		float expandHeightEach = 0.0f;
		if (expandCount > 0) {
			expandHeightEach = remainingHeight / static_cast<float>(
				expandCount);
		}

		// 子のローカル矩形を設定していく
		float currentYLocal = mPadding.top;

		for (auto& info : childrenInfo) {
			UiWidget* child = info.widget;

			Rect childLocal = child->GetLocalRect();

			const UiSizePolicy       policy     = child->GetSizePolicy();
			const UiSizeConstraints& constraint = child->GetSizeConstraints();

			float height;
			if (policy.vertical == UiSizePolicyAxis::FIXED) {
				height = info.fixedHeight; // さっき計算した値
			} else {
				float h = expandHeightEach;
				// 制約で clamp
				h = std::max(constraint.minHeight,
				             std::min(h, constraint.maxHeight));
				height = h;
			}

			// 横方向はとりあえず「幅いっぱい、Fixed扱い」にする
			childLocal.x      = mPadding.left;
			childLocal.y      = currentYLocal;
			childLocal.width  = contentWidth;
			childLocal.height = height;

			// レイアウト管理下なのでアンカーは親ローカル左上基準
			child->SetAnchors(
				{.minX = 0.0f, .minY = 0.0f, .maxX = 0.0f, .maxY = 0.0f}
			);
			child->SetPivot({.x = 0.0f, .y = 0.0f});
			child->SetLocalRect(childLocal);

			currentYLocal += height + mSpacing;
		}
	}
}
