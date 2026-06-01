#include "UiLayoutComponents.h"

#include <algorithm>

#include "engine/gui/UiSerializationHelpers.h"
#include "engine/gui/UiWidget.h"

namespace Unnamed::Gui {
	void UiLinearLayoutComponent::SetPadding(const LayoutPadding& padding) {
		mPadding = padding;
	}

	const LayoutPadding& UiLinearLayoutComponent::GetPadding() const {
		return mPadding;
	}

	void UiLinearLayoutComponent::SetSpacing(const float spacing) {
		mSpacing = spacing;
	}

	float UiLinearLayoutComponent::GetSpacing() const {
		return mSpacing;
	}

	void UiLinearLayoutComponent::Serialize(JsonWriter& writer) const {
		writer.Key("padding");
		WritePadding(writer, mPadding);
		writer.Key("spacing");
		writer.Write(mSpacing);
	}

	void UiLinearLayoutComponent::Deserialize(const JsonReader& reader) {
		if (reader.Has("padding")) {
			mPadding = ReadPadding(reader["padding"].GetArray());
		}
		if (reader.Has("spacing")) {
			mSpacing = reader["spacing"].GetFloat();
		}
	}

	void UiLinearLayoutComponent::OnAfterLayout(UiWidget& owner) {
		ApplyLayout(owner);
	}

	void UiLinearLayoutComponent::ApplyLayout(UiWidget& owner) const {
		const Rect& selfRect = owner.GetGlobalRect();

		const float contentWidth = std::max(
			0.0f,
			selfRect.width - mPadding.left - mPadding.right
		);
		const float contentHeight = std::max(
			0.0f,
			selfRect.height - mPadding.top - mPadding.bottom
		);

		struct ChildInfo {
			UiWidget* widget       = nullptr;
			float     extent       = 0.0f;
			float     minExtent    = 0.0f;
			float     maxExtent    = 0.0f;
			float     expandWeight = 1.0f;
			bool      expand       = false;
		};

		const bool isVertical = IsVertical();
		std::vector<ChildInfo> infos;
		float                  totalFixedExtent = 0.0f;
		std::vector<size_t>    expandIndices;

		for (const auto& childPtr : owner.GetChildren()) {
			UiWidget* child = childPtr.get();
			if (!child || !child->IsVisible()) {
				continue;
			}

			const UiSizePolicy       policy      = child->GetSizePolicy();
			const UiSizeConstraints& constraints = child->GetSizeConstraints();
			const bool expand = isVertical ?
				                    policy.vertical == UiSizePolicyAxis::EXPAND :
				                    policy.horizontal == UiSizePolicyAxis::EXPAND;

			float preferred = isVertical ?
				                  child->GetPreferredHeight() :
				                  child->GetPreferredWidth();
			if (preferred <= 0.0f) {
				preferred = isVertical ? 32.0f : 64.0f;
			}

			const float minValue = isVertical ?
				                       constraints.minHeight :
				                       constraints.minWidth;
			const float maxValue = isVertical ?
				                       constraints.maxHeight :
				                       constraints.maxWidth;
			const float clamped = std::clamp(preferred, minValue, maxValue);

			ChildInfo info = {};
			info.widget       = child;
			info.expand       = expand;
			info.minExtent    = minValue;
			info.maxExtent    = maxValue;
			info.expandWeight = std::max(child->GetLayoutWeight(), 0.0001f);
			info.extent       = expand ? 0.0f : clamped;
			totalFixedExtent += info.extent;
			if (expand) {
				expandIndices.push_back(infos.size());
			}
			infos.emplace_back(info);
		}

		if (infos.empty()) {
			return;
		}

		const float totalSpacing =
			mSpacing * static_cast<float>(std::max(0, static_cast<int>(infos.size()) - 1));
		const float contentExtent = IsVertical() ? contentHeight : contentWidth;
		float remaining = std::max(
			0.0f,
			contentExtent - totalFixedExtent - totalSpacing
		);

		// Expand対象はweight比率で配分し、min/max制約に当たった要素は先に確定させる。
		if (!expandIndices.empty()) {
			std::vector<size_t> unresolved = expandIndices;

			while (!unresolved.empty()) {
				float unresolvedWeightSum = 0.0f;
				for (const size_t index : unresolved) {
					unresolvedWeightSum += infos[index].expandWeight;
				}

				if (unresolvedWeightSum <= 0.0f) {
					for (const size_t index : unresolved) {
						infos[index].extent = 0.0f;
					}
					break;
				}

				bool resolvedAny = false;
				for (auto it = unresolved.begin(); it != unresolved.end();) {
					const size_t index = *it;
					ChildInfo&   info  = infos[index];
					const float  share =
						remaining * (info.expandWeight / unresolvedWeightSum);

					if (share < info.minExtent) {
						info.extent = info.minExtent;
						remaining = std::max(0.0f, remaining - info.extent);
						it = unresolved.erase(it);
						resolvedAny = true;
						continue;
					}

					if (share > info.maxExtent) {
						info.extent = info.maxExtent;
						remaining = std::max(0.0f, remaining - info.extent);
						it = unresolved.erase(it);
						resolvedAny = true;
						continue;
					}

					++it;
				}

				if (resolvedAny) {
					continue;
				}

				for (const size_t index : unresolved) {
					ChildInfo& info = infos[index];
					info.extent =
						remaining * (info.expandWeight / unresolvedWeightSum);
				}
				break;
			}
		}

		float cursor = IsVertical() ? mPadding.top : mPadding.left;
		for (const ChildInfo& info : infos) {
			if (!info.widget) {
				continue;
			}

			const UiSizeConstraints& constraints = info.widget->GetSizeConstraints();
			float extent = std::clamp(
				info.extent,
				IsVertical() ? constraints.minHeight : constraints.minWidth,
				IsVertical() ? constraints.maxHeight : constraints.maxWidth
			);

			Rect childLocal = info.widget->GetLocalRect();
			if (IsVertical()) {
				childLocal.x      = mPadding.left;
				childLocal.y      = cursor;
				childLocal.width  = contentWidth;
				childLocal.height = extent;
			} else {
				childLocal.x      = cursor;
				childLocal.y      = mPadding.top;
				childLocal.width  = extent;
				childLocal.height = contentHeight;
			}

			info.widget->SetAnchors(
				{.minX = 0.0f, .minY = 0.0f, .maxX = 0.0f, .maxY = 0.0f}
			);
			info.widget->SetPivot({.x = 0.0f, .y = 0.0f});
			info.widget->SetLocalRect(childLocal);

			cursor += extent + mSpacing;
		}
	}
}
