#pragma once
#include <memory>

#include "UiDocument.h"

namespace Unnamed::Gui {
	class UiDocumentManager {
	public:
		UiDocumentManager();
		~UiDocumentManager();

		std::shared_ptr<UiDocument> LoadDocument(const std::string& path);
		void                        UnloadDocument(const std::string& path);

		std::shared_ptr<UiDocument> GetDocument(const std::string& path);

	private:
		std::unordered_map<std::string, std::shared_ptr<UiDocument>> mDocuments;
	};
}
