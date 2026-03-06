#include "UiHorizontalLayout.h"

#include <algorithm>

namespace Unnamed::Gui {
	void UiHorizontalLayout::UpdateLayout(const Rect& parentGlobalRect) {
		// 自分のは既定に任せる
		UiLayout::UpdateLayout(parentGlobalRect);

		const Rect& selfRect = GetGlobalRect();

		const float contentLeft = selfRect.x + mPadding.left;
		const float contentTop = selfRect.y + mPadding.top;
		const float contentRight = selfRect.x + selfRect.width - mPadding.right;
		const float contentBottom =
			selfRect.y + selfRect.height - mPadding.bottom;

		const float contentWidth  = contentRight - contentLeft;
		const float contentHeight = contentBottom - contentTop;

		struct ChildInfo {
			UiWidget* widget;
			float     fixedWidth;
			bool      isExpand;
		};

		std::vector<ChildInfo> childrenInfo;
		childrenInfo.reserve(GetChildren().size());

		float totalFixedWidth   = 0.f;
		int   expandCount       = 0;
		int   visibleChildCount = 0;

		for (auto& pChild : GetChildren()) {
			UiWidget* child = pChild.get();
			if (!child || !child->IsVisible()) {
				continue;
			}

			const UiSizePolicy       policy     = child->GetSizePolicy();
			const UiSizeConstraints& constraint = child->GetSizeConstraints();

			float preferred = child->GetPreferredWidth();
			if (preferred <= 0.0f) {
				preferred = 64.0f; // TODO: パラメータ化
			}

			const float clamped = std::clamp(
				preferred, constraint.minWidth,
				constraint.maxWidth
			);

			ChildInfo info{};
			info.widget = child;
			if (policy.horizontal == UiSizePolicyAxis::FIXED) {
				info.isExpand   = false;
				info.fixedWidth = clamped;
				totalFixedWidth += clamped;
			} else {
				info.isExpand   = true;
				info.fixedWidth = 0.0f;
				expandCount++;
			}

			childrenInfo.emplace_back(info);
			visibleChildCount++;
		}

		// スペーシングからの幅
		const float totalSpacing =
			mSpacing * static_cast<float>(std::max(0, visibleChildCount - 1));

		// Expand に割り当てる幅
		float remainingWidth = contentWidth - totalFixedWidth - totalSpacing;
		remainingWidth       = std::max(remainingWidth, 0.0f);

		float expandWidthEach = 0.0f;
		if (expandCount > 0) {
			expandWidthEach = remainingWidth / static_cast<float>(expandCount);
		}

		// 子のローカル矩形を設定していく
		float currentXLocal = mPadding.left;

		for (const auto& info : childrenInfo) {
			UiWidget* child = info.widget;

			Rect childLocal = child->GetLocalRect();

			const UiSizePolicy       policy     = child->GetSizePolicy();
			const UiSizeConstraints& constraint = child->GetSizeConstraints();

			float width;
			if (policy.horizontal == UiSizePolicyAxis::FIXED) {
				width = info.fixedWidth; // さっき計算した値
			} else {
				float w = expandWidthEach;
				// 制約で clamp
				w = std::max(
					constraint.minWidth,
					std::min(w, constraint.maxWidth)
				);
				width = w;
			}

			// 縦方向はとりあえず「高さいっぱい、Fixed扱い」にする
			childLocal.x      = currentXLocal;
			childLocal.y      = mPadding.top;
			childLocal.width  = width;
			childLocal.height = contentHeight;

			// レイアウト管理下なのでアンカーは親ローカル左上基準
			child->SetAnchors({0.0f, 0.0f, 0.0f, 0.0f});
			child->SetPivot({0.0f, 0.0f});
			child->SetLocalRect(childLocal);

			currentXLocal += width + mSpacing;
		}
	}
}
