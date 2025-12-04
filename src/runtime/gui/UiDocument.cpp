#include "UiDocument.h"

namespace Unnamed::Gui {
	UiDocument::UiDocument() = default;

	UiDocument::~UiDocument() = default;

	bool UiDocument::Save(const std::string& path) const {
		JsonWriter writer(path);
		// pathはコンストラクタで受け取るスタイル

		writer.BeginObject();
		writer.Key("name");
		writer.Write(mName);

		if (mRootWidget) {
			writer.Key("root");
			mRootWidget->SaveToJson(writer);
		}

		writer.EndObject();

		return writer.Save();
	}

	std::shared_ptr<UiDocument> UiDocument::Load(const std::string& path) {
		JsonReader reader(path); // ファイル読み込みコンストラクタ
		if (!reader.Valid()) {
			return nullptr;
		}

		auto doc = std::make_shared<UiDocument>();

		if (reader.Has("name")) {
			doc->SetName(reader["name"].GetString());
		}

		if (reader.Has("root")) {
			JsonReader rootNode   = reader["root"];
			auto       rootWidget = UiWidget::CreateFromJson(rootNode);
			if (rootWidget) {
				doc->SetRootWidget(std::move(rootWidget));
			}
		}

		return doc;
	}

	void UiDocument::SetName(const std::string& name) {
		mName = name;
	}

	const std::string& UiDocument::GetName() {
		return mName;
	}

	void UiDocument::SetRootWidget(std::unique_ptr<UiWidget> rootWidget) {
		mRootWidget = std::move(rootWidget);
		mNamedWidgets.clear();
		RegisterWidgetRecursive(mRootWidget.get());
	}

	UiWidget* UiDocument::GetRootWidget() const {
		return mRootWidget.get();
	}

	void UiDocument::RegisterNamedWidget(
		const std::string& id, UiWidget* widget
	) {
		mNamedWidgets[id] = widget;
	}

	UiWidget* UiDocument::FindWidgetById(const std::string& id) {
		auto it = mNamedWidgets.find(id);
		if (it != mNamedWidgets.end()) {
			return it->second;
		}
		return nullptr;
	}

	std::unique_ptr<UiWidget> UiDocument::TakeRootWidget() {
		return std::move(mRootWidget);
	}

	void UiDocument::RegisterWidgetRecursive(UiWidget* widget) {
		if (!widget) {
			return;
		}

		const auto name = widget->GetName();
		if (!name.empty()) {
			RegisterNamedWidget(std::string(name), widget);
		}

		for (const auto& child : widget->GetChildren()) {
			RegisterWidgetRecursive(child.get());
		}
	}
}
