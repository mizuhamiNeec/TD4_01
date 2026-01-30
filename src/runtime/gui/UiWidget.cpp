#include "UiWidget.h"

#include <engine/unnamed/subsystem/console/Log.h>

#include <runtime/core/math/Math.h>

#include "UiButton.h"
#include "UiPanel.h"
#include "UiSerializationHelpers.h"

#include "layout/UiHorizontalLayout.h"
#include "layout/UiVerticalLayout.h"

namespace Unnamed::Gui {
	static constexpr std::string_view kChannel = "UiWidget";

	DIRTY_FLAGS operator|(DIRTY_FLAGS a, DIRTY_FLAGS b) {
		return static_cast<DIRTY_FLAGS>(
			static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
	}

	DIRTY_FLAGS operator|=(DIRTY_FLAGS& a, const DIRTY_FLAGS b) {
		return a = a | b;
	}

	bool HasFlag(DIRTY_FLAGS flags, DIRTY_FLAGS test) {
		return
			(static_cast<uint32_t>(flags) & static_cast<uint32_t>(test)) != 0;
	}

	const char* ToString(const DIRTY_FLAGS e) {
		switch (e) {
			case DIRTY_FLAGS::NONE: return "NONE";
			case DIRTY_FLAGS::LAYOUT: return "LAYOUT";
			case DIRTY_FLAGS::STYLE: return "STYLE";
			case DIRTY_FLAGS::DRAW: return "DRAW";
			case DIRTY_FLAGS::ALL: return "ALL";
			default: return "unknown";
		}
	}

	UiWidget::UiWidget() : mVisible(true),
	                       mEnabled(true) {}

	UiWidget::~UiWidget() = default;

	void UiWidget::AddChild(std::unique_ptr<UiWidget> child) {
		if (!child) {
			Warning(kChannel, "nullptrを追加しようとしました");
			return;
		}

		child->mParent = this;
		mChildren.emplace_back(std::move(child));

		MarkDirty(DIRTY_FLAGS::LAYOUT | DIRTY_FLAGS::DRAW);
	}

	void UiWidget::RemoveChild(const UiWidget* child) {
		if (!child) {
			Warning(kChannel, "nullptrを削除しようとしました");
			return;
		}

		for (auto it = mChildren.begin(); it != mChildren.end(); ++it) {
			if (it->get() == child) {
				(*it)->mParent = nullptr;
				mChildren.erase(it);
				MarkDirty(DIRTY_FLAGS::LAYOUT | DIRTY_FLAGS::DRAW);
				return;
			}
		}
	}

	std::unique_ptr<UiWidget> UiWidget::TakeChild(const UiWidget* child) {
		if (!child) {
			Warning(kChannel, "nullptrを取り出そうとしました");
			return nullptr;
		}

		for (auto it = mChildren.begin(); it != mChildren.end(); ++it) {
			if (it->get() == child) {
				(*it)->mParent = nullptr;
				auto result    = std::move(*it);
				mChildren.erase(it);
				MarkDirty(DIRTY_FLAGS::LAYOUT | DIRTY_FLAGS::DRAW);
				return result;
			}
		}
		return nullptr;
	}

	void UiWidget::AddChildReference(UiWidget* child) {
		if (!child) return; // 安全策
		child->mParent = this;
		mReferenceChildren.push_back(child);
		MarkDirty(DIRTY_FLAGS::LAYOUT | DIRTY_FLAGS::DRAW);
	}

	void UiWidget::RemoveChildReference(const UiWidget* child) {
		if (!child) return;

		for (auto it = mReferenceChildren.begin(); it != mReferenceChildren.
		                                           end(); ++it) {
			if (*it == child) {
				// 親の参照を切るかは設計によりますが、通常はnullptrに戻します
				if ((*it)->mParent == this) { (*it)->mParent = nullptr; }
				mReferenceChildren.erase(it);
				MarkDirty(DIRTY_FLAGS::LAYOUT | DIRTY_FLAGS::DRAW);
				return;
			}
		}
	}

	const std::vector<UiWidget*>& UiWidget::GetReferenceChildren() const {
		return mReferenceChildren;
	}

	void UiWidget::SetLocalRect(const Rect& rect) {
		mLocalRect = rect;
		MarkDirty(DIRTY_FLAGS::LAYOUT | DIRTY_FLAGS::DRAW);
	}

	const Rect& UiWidget::GetLocalRect() const { return mLocalRect; }

	const Rect& UiWidget::GetGlobalRect() const { return mGlobalRect; }

	void UiWidget::SetAnchors(const Anchors& anchors) {
		mAnchors = anchors;
		MarkDirty(DIRTY_FLAGS::LAYOUT | DIRTY_FLAGS::DRAW);
	}

	const Anchors& UiWidget::GetAnchors() const { return mAnchors; }

	void UiWidget::SetMargins(const Margins& margins) {
		mMargins = margins;
		MarkDirty(DIRTY_FLAGS::LAYOUT | DIRTY_FLAGS::DRAW);
	}

	const Margins& UiWidget::GetMargins() const { return mMargins; }

	void UiWidget::SetPivot(const Pivot& pivot) {
		mPivot = pivot;
		MarkDirty(DIRTY_FLAGS::LAYOUT | DIRTY_FLAGS::DRAW);
	}

	const Pivot& UiWidget::GetPivot() const { return mPivot; }

	void UiWidget::SetVisible(const bool visible) {
		if (mVisible == visible) return;
		mVisible = visible;
		MarkDirty(DIRTY_FLAGS::DRAW);
	}

	bool UiWidget::IsVisible() const { return mVisible; }

	void UiWidget::SetEnabled(const bool enabled) {
		if (mEnabled == enabled) return;
		mEnabled = enabled;
	}

	bool UiWidget::IsEnabled() const { return mEnabled; }

	void UiWidget::MarkDirty(const DIRTY_FLAGS flags) {
		const DIRTY_FLAGS oldFlags = mDirtyFlags;
		mDirtyFlags                |= flags;
		if (mDirtyFlags != oldFlags) { OnDirtyFlagAdded(flags); }
	}

	DIRTY_FLAGS UiWidget::GetDirtyFlags() const { return mDirtyFlags; }

	void UiWidget::ClearDirtyFlags(DIRTY_FLAGS flags) {
		uint32_t       raw   = static_cast<uint32_t>(mDirtyFlags);
		const uint32_t clear = static_cast<uint32_t>(flags);
		raw                  &= ~clear;
		mDirtyFlags          = static_cast<DIRTY_FLAGS>(raw);
	}

	void UiWidget::SetSizePolicy(
		UiSizePolicyAxis horizontal, UiSizePolicyAxis vertical
	) {
		mSizePolicy.horizontal = horizontal;
		mSizePolicy.vertical   = vertical;
		MarkDirty(DIRTY_FLAGS::LAYOUT | DIRTY_FLAGS::DRAW);
	}

	UiSizePolicy UiWidget::GetSizePolicy() const { return mSizePolicy; }

	void UiWidget::SetSizeConstraints(const UiSizeConstraints& constraints) {
		mSizeConstraints = constraints;
		MarkDirty(DIRTY_FLAGS::LAYOUT | DIRTY_FLAGS::DRAW);
	}

	const UiSizeConstraints& UiWidget::GetSizeConstraints() const {
		return mSizeConstraints;
	}

	float UiWidget::GetPreferredWidth() const { return mLocalRect.width; }

	float UiWidget::GetPreferredHeight() const { return mLocalRect.height; }

	void UiWidget::Tick(float) {
		// DO SOMETHING
	}

	void UiWidget::UpdateLayoutRecursive(const Rect& parentGlobalRect) {
		if (HasFlag(mDirtyFlags, DIRTY_FLAGS::LAYOUT) ||
		    HasFlag(mDirtyFlags, DIRTY_FLAGS::ALL)) {
			UpdateLayout(parentGlobalRect);
			ClearDirtyFlags(DIRTY_FLAGS::LAYOUT);
		}

		// 所有している子
		for (auto& child : mChildren) {
			if (child) { child->UpdateLayoutRecursive(mGlobalRect); }
		}

		// 参照だけ持っている子（AddChildReferenceでぶら下げたやつ）
		for (auto* refChild : mReferenceChildren) {
			if (refChild) { refChild->UpdateLayoutRecursive(mGlobalRect); }
		}
	}

	void UiWidget::BuildDrawCommands(std::vector<UiDrawCommand>& out) const {
		// 自分が非表示なら何も描画しない（子も含めて）
		if (!IsVisible()) { return; }

		// 所有している子
		for (const auto& child : mChildren) {
			if (child) { child->BuildDrawCommands(out); }
		}

		// 参照だけ持っている子
		for (auto* refChild : mReferenceChildren) {
			if (refChild) { refChild->BuildDrawCommands(out); }
		}
	}

	void UiWidget::DebugDrawUi(const UiWidget* w) {
		if (!w) return;
#ifdef  _DEBUG
		const auto& r  = w->GetGlobalRect();
		auto*       dl = ImGui::GetForegroundDrawList();

		dl->AddRect(
			ImVec2(+r.x, +r.y),
			ImVec2(+r.x + r.width, +r.y + r.height),
			IM_COL32(255, 0, 0, 255)
		);

		if (const auto* parent = w->GetParent()) {
			const auto& pr = parent->GetGlobalRect();
			dl->AddLine(
				ImVec2(+r.x + r.width * 0.5f, +r.y + r.height * 0.5f),
				ImVec2(+pr.x, +pr.y),
				IM_COL32(0, 255, 0, 255)
			);
		}

		// 所有している子
		for (const auto& child : w->GetChildren()) {
			if (child) { DebugDrawUi(child.get()); }
		}

		// 参照だけ持っている子も再帰
		// mReferenceChildren は非 const getterがないので、static 関数を
		// UiWidget 内に追加するか、friend 的にアクセスできるようにする必要あり。
		for (auto* refChild : w->mReferenceChildren) {
			// 同じクラス内なのでアクセス可能
			if (refChild) { DebugDrawUi(refChild); }
		}
#endif
	}

	UiWidget* UiWidget::HitTest(float x, float y) {
		if (!mVisible || !mEnabled) return nullptr;

		// まず z-order 的に後ろの方から（所有子）
		for (auto it = mChildren.rbegin(); it != mChildren.rend(); ++it) {
			if (*it) {
				if (UiWidget* hit = (*it)->HitTest(x, y)) { return hit; }
			}
		}

		// 次に参照子も逆順でチェック
		for (auto it = mReferenceChildren.rbegin(); it != mReferenceChildren.
		                                            rend(); ++it) {
			UiWidget* child = *it;
			if (child) {
				if (UiWidget* hit = child->HitTest(x, y)) { return hit; }
			}
		}

		const Rect& r      = mGlobalRect;
		const bool  inside =
			(x >= r.x) && (x <= r.x + r.width) &&
			(y >= r.y) && (y <= r.y + r.height);

		return inside ? this : nullptr;
	}

	bool UiWidget::IsHovered() const { return mHovered; }

	bool UiWidget::IsPressed() const { return mPressed; }

	void UiWidget::OnMouseEnter() { mHovered = true; }

	void UiWidget::OnMouseLeave() { mHovered = false; }

	void UiWidget::OnMouseDown() { mPressed = true; }

	void UiWidget::OnMouseUp() { mPressed = false; }

	void UiWidget::OnClick() {
		// DO SOMETHING
	}

	const char* UiWidget::GetTypeName() const { return "Widget"; }

	void UiWidget::SaveToJson(JsonWriter& writer) const {
		writer.BeginObject();

		// 基本情報
		writer.Key("type");
		writer.Write(std::string(GetTypeName()));
		writer.Key("name");
		writer.Write(std::string(GetName()));

		DevMsg(
			kChannel, "Saving widget '{}': visible={}, enabled={}",
			GetName(), IsVisible(), IsEnabled()
		);

		writer.Key("visible");
		writer.Write(IsVisible());
		writer.Key("enabled");
		writer.Write(IsEnabled());

		// Rect / Anchors / Pivot / Margins
		writer.Key("rect");
		WriteRect(writer, GetLocalRect());
		writer.Key("anchors");
		WriteAnchors(writer, GetAnchors());
		writer.Key("pivot");
		WritePivot(writer, GetPivot());
		writer.Key("margins");
		WriteMargins(writer, GetMargins());

		// SizePolicy / Constraints
		writer.Key("sizePolicy");
		WriteSizePolicy(writer, GetSizePolicy());

		writer.Key("constraints");
		WriteConstraints(writer, GetSizeConstraints());

		OnSerialize(writer);

		// 子
		writer.Key("children");
		writer.BeginArray();
		for (const auto& child : GetChildren()) {
			if (child) { child->SaveToJson(writer); }
		}
		writer.EndArray();

		writer.EndObject();
	}

	void UiWidget::LoadFromJson(const JsonReader& reader) {
		if (!reader.Valid()) { return; }

		// 基本情報
		if (reader.Has("name")) { SetName(reader["name"].GetString()); }
		if (reader.Has("visible")) { SetVisible(reader["visible"].GetBool()); }
		if (reader.Has("enabled")) { SetEnabled(reader["enabled"].GetBool()); }

		// Rect
		if (reader.Has("rect")) {
			Rect r = ReadRect(reader["rect"].GetArray());
			SetLocalRect(r);
		}

		// Anchors / Pivot / Margins
		if (reader.Has("anchors")) {
			SetAnchors(ReadAnchors(reader["anchors"].GetArray()));
		}
		if (reader.Has("pivot")) {
			SetPivot(ReadPivot(reader["pivot"].GetArray()));
		}
		if (reader.Has("margins")) {
			SetMargins(ReadMargins(reader["margins"].GetArray()));
		}

		// SizePolicy / Constraints
		if (reader.Has("sizePolicy")) {
			SetSizePolicy(
				ReadSizePolicy(reader["sizePolicy"].GetArray()).horizontal,
				ReadSizePolicy(reader["sizePolicy"].GetArray()).vertical
			);
		}
		if (reader.Has("constraints")) {
			SetSizeConstraints(ReadConstraints(reader["constraints"]));
		}

		OnDeserialize(reader);
	}

	std::unique_ptr<UiWidget> UiWidget::
	CreateFromJson(const JsonReader& reader) {
		if (!reader.Valid()) { return nullptr; }

		// type 取得
		std::string type = "Widget";
		if (reader.Has("type")) { type = reader["type"].GetString(); }

		std::unique_ptr<UiWidget> widget;

		if (type == "Panel") { widget = std::make_unique<UiPanel>(); } else if (
			type == "Button") { widget = std::make_unique<UiButton>(); } else if
		(type == "VerticalLayout") {
			widget = std::make_unique<UiVerticalLayout>();
		} else if (type == "HorizontalLayout") {
			widget = std::make_unique<UiHorizontalLayout>();
		} else {
			// 謎 → とりあえず UiWidget
			widget = std::make_unique<UiWidget>();
		}

		widget->LoadFromJson(reader);

		// 子
		if (reader.Has("children")) {
			JsonReader   children = reader["children"].GetArray();
			const size_t count    = children.Size();
			for (size_t i = 0; i < count; ++i) {
				JsonReader childNode   = children[i];
				auto       childWidget = CreateFromJson(childNode);
				if (childWidget) { widget->AddChild(std::move(childWidget)); }
			}
		}

		return widget;
	}

	void UiWidget::UpdateLayout(const Rect& parentGlobalRect) {
		const float parentLeft   = parentGlobalRect.x;
		const float parentTop    = parentGlobalRect.y;
		const float parentRight  = parentGlobalRect.x + parentGlobalRect.width;
		const float parentBottom = parentGlobalRect.y + parentGlobalRect.height;

		const float anchorLeft = Math::Lerp(
			parentLeft, parentRight, mAnchors.minX
		);
		const float anchorTop = Math::Lerp(
			parentTop, parentBottom, mAnchors.minY
		);
		const float anchorRight = Math::Lerp(
			parentLeft, parentRight, mAnchors.maxX
		);
		const float anchorBottom = Math::Lerp(
			parentTop, parentBottom, mAnchors.maxY
		);

		float x;
		float y;
		float w;
		float h;

		// Horizontal
		if (mAnchors.minX == mAnchors.maxX) {
			x = anchorLeft + mLocalRect.x;
			w = mLocalRect.width;
		} else {
			const float available = (anchorRight - anchorLeft);
			x                     = anchorLeft + mMargins.left;
			w                     = available - mMargins.left - mMargins.right;
		}

		// Vertical
		if (mAnchors.minY == mAnchors.maxY) {
			y = anchorTop + mLocalRect.y;
			h = mLocalRect.height;
		} else {
			const float available = (anchorBottom - anchorTop);
			y                     = anchorTop + mMargins.top;
			h                     = available - mMargins.top - mMargins.bottom;
		}

		// Pivot
		float finalX = x;
		float finalY = y;

		if (mAnchors.minX == mAnchors.maxX) { finalX = x - w * mPivot.x; }
		if (mAnchors.minY == mAnchors.maxY) { finalY = y - h * mPivot.y; }

		mGlobalRect.x      = finalX;
		mGlobalRect.y      = finalY;
		mGlobalRect.width  = w;
		mGlobalRect.height = h;
	}

	void UiWidget::OnDirtyFlagAdded(const DIRTY_FLAGS flags) {
		if (HasFlag(flags, DIRTY_FLAGS::LAYOUT)) {
			for (auto& child : mChildren) {
				if (child) { child->MarkDirty(DIRTY_FLAGS::LAYOUT); }
			}
			for (auto* refChild : mReferenceChildren) {
				if (refChild) { refChild->MarkDirty(DIRTY_FLAGS::LAYOUT); }
			}
		}
	}

	void UiWidget::OnSerialize(JsonWriter& writer) const { (void)writer; }

	void UiWidget::OnDeserialize(const JsonReader& reader) { (void)reader; }

	const std::vector<std::unique_ptr<UiWidget>>&
	UiWidget::GetChildren() const { return mChildren; }

	UiWidget* UiWidget::GetParent() const { return mParent; }

	void UiWidget::SetName(const std::string_view& name) { mName = name; }

	std::string_view UiWidget::GetName() const { return mName; }
}
