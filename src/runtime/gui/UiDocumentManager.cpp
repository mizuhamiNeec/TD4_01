#include "UiDocumentManager.h"

#include "UiButton.h"
#include "UiPanel.h"

#include "engine/unnamed/subsystem/console/Log.h"

#include "layout/UiVerticalLayout.h"

namespace Unnamed::Gui {
	static constexpr std::string_view kChannel = "UiDocument";

	UiDocumentManager::UiDocumentManager()  = default;
	UiDocumentManager::~UiDocumentManager() = default;

	std::shared_ptr<UiDocument> UiDocumentManager::LoadDocument(
		const std::string& path
	) {
		// キャッシュから削除して強制的に再読み込み
		mDocuments.erase(path);

		// 1. JSON からロードを試みる
		auto document = UiDocument::Load(path);
		if (!document) {
			// 2. 失敗したら「今まで通り」のハードコードUIを作る（保険）
			document = std::make_shared<UiDocument>();
			document->SetName(path);

			auto root = std::make_unique<UiVerticalLayout>();
			root->SetName("RootLayout");
			root->SetPadding({16, 16, 16, 16});
			root->SetSpacing(8.0f);
			root->SetAnchors({0, 0, 1, 1});
			root->SetPivot({0, 0});
			root->SetLocalRect({0, 0, 0, 0});

			auto panel = std::make_unique<UiPanel>();
			panel->SetName("MainPanel");
			panel->SetAnchors({0.5f, 0.5f, 0.5f, 0.5f});
			panel->SetPivot({0.5f, 0.5f});
			panel->SetLocalRect({640.0f, 360.0f, 400.0f, 300.0f});

			auto menuLayout = std::make_unique<UiVerticalLayout>();
			menuLayout->SetName("MenuLayout");
			menuLayout->SetPadding({16, 16, 16, 16});
			menuLayout->SetSpacing(8.0f);
			menuLayout->SetAnchors({0.0f, 0.0f, 1.0f, 1.0f});
			menuLayout->SetPivot({0.0f, 0.0f});
			menuLayout->SetLocalRect({0.0f, 0.0f, 0.0f, 0.0f});

			auto makeButton = [&](
				const std::string& name, const std::string& text
			) {
				auto button = std::make_unique<UiButton>();
				button->SetName(name);
				button->SetText(text);
				button->SetLocalRect({0.0f, 0.0f, 0.0f, 40.0f});
				button->SetSizePolicy(
					UiSizePolicyAxis::FIXED,
					UiSizePolicyAxis::FIXED
				);
				return button;
			};

			menuLayout->AddChild(makeButton("PlayButton", "Play"));
			menuLayout->AddChild(makeButton("OptionsButton", "Options"));
			menuLayout->AddChild(makeButton("QuitButton", "Quit"));

			panel->AddChild(std::move(menuLayout));
			root->AddChild(std::move(panel));

			document->SetRootWidget(std::move(root));
		}

		DevMsg(kChannel, "Loaded UiDocument: {}", path.c_str());
		mDocuments.emplace(path, document);
		return document;
	}

	void UiDocumentManager::UnloadDocument(const std::string& path) {
		mDocuments.erase(path);
	}

	std::shared_ptr<UiDocument> UiDocumentManager::GetDocument(
		const std::string& path
	) {
		const auto it = mDocuments.find(path);
		if (it != mDocuments.end()) { return it->second; }
		return nullptr;
	}
}
