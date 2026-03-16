#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/assets/AssetID.h"

#include "UiDocument.h"

namespace Unnamed::Gui {
	class UiDocument;
}

namespace Unnamed {
	class AssetManager;
}

namespace Unnamed::Gui {
	class UiDocumentManager {
	public:
		explicit UiDocumentManager(AssetManager* assetManager = nullptr);
		~UiDocumentManager();

		std::shared_ptr<UiDocument> LoadDocument(const std::string& path);
		void                        UnloadDocument(const std::string& path);

		std::shared_ptr<UiDocument> GetDocument(const std::string& path) const;
		bool                        SaveDocument(
			const std::string&              path,
			const std::shared_ptr<UiDocument>& document
		);

		void MarkDirty(const std::string& path, bool dirty = true);
		[[nodiscard]] bool IsDirty(const std::string& path) const;
		[[nodiscard]] bool HasPendingExternal(const std::string& path) const;
		void ResolvePendingExternal(const std::string& path, bool reloadFromAsset);
		std::vector<std::string> UpdateTrackedDocuments();

	private:
		struct ManagedDocument {
			std::string               normalizedPath;
			AssetID                   assetId = kInvalidAssetID;
			uint64_t                  loadedVersion = 0;
			std::shared_ptr<UiDocument> document;
			bool                      dirty = false;
			bool                      pendingExternal = false;
		};

		static std::string NormalizePath(std::string path);
		bool ReloadDocumentFromAsset(ManagedDocument& managed);
		ManagedDocument* FindManaged(const std::string& path);
		const ManagedDocument* FindManaged(const std::string& path) const;

		AssetManager* mAssetManager = nullptr;
		std::unordered_map<std::string, ManagedDocument> mDocuments;
	};
}
