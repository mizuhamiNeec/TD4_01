#include "UiLayout.h"

#include "runtime/gui/UiSerializationHelpers.h"

namespace Unnamed::Gui {
	UiLayout::UiLayout()  = default;
	UiLayout::~UiLayout() = default;

	void UiLayout::SetPadding(const LayoutPadding& padding) {
		mPadding = padding;
		MarkDirty(DIRTY_FLAGS::LAYOUT | DIRTY_FLAGS::DRAW);
	}

	const LayoutPadding& UiLayout::GetPadding() const { return mPadding; }

	void UiLayout::SetSpacing(const float spacing) {
		mSpacing = spacing;
		MarkDirty(DIRTY_FLAGS::LAYOUT | DIRTY_FLAGS::DRAW);
	}

	float UiLayout::GetSpacing() const { return mSpacing; }

	void UiLayout::OnSerialize(JsonWriter& writer) const {
		writer.Key("padding");
		WritePadding(writer, mPadding);

		writer.Key("spacing");
		writer.Write(mSpacing);
	}

	void UiLayout::OnDeserialize(JsonReader& reader) {
		if (reader.Has("padding")) {
			SetPadding(ReadPadding(reader["padding"].GetArray()));
		}
		if (reader.Has("spacing")) { SetSpacing(reader["spacing"].GetFloat()); }
	}
}
