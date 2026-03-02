#include "UScene.h"

#include <algorithm>
#include <string>

#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/entity/UEntity.h"

namespace Unnamed {
	namespace {
		std::string NormalizeFolderPath(std::string_view folderPath) {
			std::string path(folderPath);
			std::replace(path.begin(), path.end(), '\\', '/');
			while (!path.empty() && path.front() == '/') {
				path.erase(path.begin());
			}
			while (!path.empty() && path.back() == '/') { path.pop_back(); }
			return path;
		}

		bool IsFolderEqualOrDescendant(
			std::string_view path, std::string_view ancestor
		) {
			if (ancestor.empty()) { return true; }
			if (path == ancestor) { return true; }
			if (path.size() <= ancestor.size()) { return false; }
			return path.substr(0, ancestor.size()) == ancestor &&
			       path[ancestor.size()] == '/';
		}

		std::string ReplaceFolderPrefix(
			std::string_view path, std::string_view source,
			std::string_view destination
		) {
			if (!IsFolderEqualOrDescendant(path, source)) {
				return std::string(path);
			}

			const std::string_view suffix = path.size() == source.size() ?
				                                std::string_view{} :
				                                path.substr(source.size() + 1);
			if (destination.empty()) { return std::string(suffix); }
			if (suffix.empty()) { return std::string(destination); }
			return std::string(destination) + "/" + std::string(suffix);
		}

		std::string GetFolderParentPath(std::string_view folderPath) {
			const size_t slash = folderPath.find_last_of('/');
			return slash == std::string_view::npos ?
				       std::string() :
				       std::string(folderPath.substr(0, slash));
		}

		std::string GetFolderLeafName(std::string_view folderPath) {
			const size_t slash = folderPath.find_last_of('/');
			return slash == std::string_view::npos ?
				       std::string(folderPath) :
				       std::string(folderPath.substr(slash + 1));
		}

		std::string JoinFolderPath(
			std::string_view parent, std::string_view child
		) {
			if (parent.empty()) { return std::string(child); }
			if (child.empty()) { return std::string(parent); }
			return std::string(parent) + "/" + std::string(child);
		}
	}

	UScene::UScene()  = default;
	UScene::~UScene() = default;

	UEntity& UScene::CreateEntity(
		const std::string_view name, uint64_t id, bool isEditorOnly
	) {
		if (id == 0 || mEntityById.contains(id)) { id = AllocateEntityId(); }

		auto entity = std::make_unique<UEntity>(
			std::string(name), id, isEditorOnly
		);
		UEntity* raw = entity.get();

		mEntities.emplace_back(std::move(entity));
		mEntityById[id] = raw;
		mNextEntityId   = std::max<EntityId>(mNextEntityId, id + 1);
		return *raw;
	}

	void UScene::DestroyEntity(const EntityId id) {
		const auto it = mEntityById.find(id);
		if (it == mEntityById.end()) { return; }

		UEntity* target = it->second;

		target->OnDestroy();

		for (size_t i = 0; i < mEntities.size(); ++i) {
			if (mEntities[i].get() == target) {
				mEntities.erase(
					mEntities.begin() + static_cast<std::ptrdiff_t>(i)
				);
				break;
			}
		}

		mEntityById.erase(it);
	}

	UEntity* UScene::FindEntity(const EntityId id) {
		const auto it = mEntityById.find(id);
		return it != mEntityById.end() ? it->second : nullptr;
	}

	const UEntity* UScene::FindEntity(const EntityId id) const {
		const auto it = mEntityById.find(id);
		return it != mEntityById.end() ? it->second : nullptr;
	}

	size_t UScene::GetEntityCount() const { return mEntities.size(); }

	const std::vector<std::unique_ptr<UEntity>>& UScene::GetEntities() const {
		return mEntities;
	}

	const std::vector<std::string>& UScene::GetFolders() const {
		return mFolders;
	}

	void UScene::AddFolder(const std::string_view folderPath) {
		const std::string normalized = NormalizeFolderPath(folderPath);
		if (normalized.empty()) { return; }
		if (
			std::ranges::find(mFolders, normalized) == mFolders.end()
		) { mFolders.emplace_back(normalized); }
		std::sort(mFolders.begin(), mFolders.end());
	}

	void UScene::RemoveFolder(const std::string_view folderPath) {
		const std::string normalized = NormalizeFolderPath(folderPath);
		std::erase(mFolders, normalized);
	}

	void UScene::RenameFolderSubtree(
		const std::string_view sourceFolderPath,
		const std::string_view newLeafName
	) {
		const std::string source = NormalizeFolderPath(sourceFolderPath);
		const std::string leaf   = NormalizeFolderPath(newLeafName);
		if (source.empty() || leaf.empty()) { return; }

		const std::string destination = JoinFolderPath(
			GetFolderParentPath(source), leaf
		);
		if (destination == source) { return; }

		for (std::string& folder : mFolders) {
			folder = NormalizeFolderPath(
				ReplaceFolderPrefix(folder, source, destination)
			);
		}
		for (const auto& entityPtr : mEntities) {
			if (!entityPtr) { continue; }
			const std::string current(entityPtr->GetFolderPath());
			if (!IsFolderEqualOrDescendant(current, source)) { continue; }
			entityPtr->SetFolderPath(
				ReplaceFolderPrefix(current, source, destination)
			);
		}
		std::sort(mFolders.begin(), mFolders.end());
		mFolders.erase(
			std::unique(mFolders.begin(), mFolders.end()),
			mFolders.end()
		);
	}

	void UScene::DeleteFolderSubtree(const std::string_view folderPath) {
		const std::string source = NormalizeFolderPath(folderPath);
		if (source.empty()) { return; }

		for (auto it = mFolders.begin(); it != mFolders.end();) {
			if (IsFolderEqualOrDescendant(*it, source)) {
				it = mFolders.erase(it);
			} else { ++it; }
		}

		for (const auto& entityPtr : mEntities) {
			if (!entityPtr) { continue; }
			const std::string current(entityPtr->GetFolderPath());
			if (!IsFolderEqualOrDescendant(current, source)) { continue; }
			entityPtr->SetFolderPath("");
		}
	}

	void UScene::MoveFolderSubtree(
		const std::string_view sourceFolderPath,
		const std::string_view targetParentPath
	) {
		const std::string source = NormalizeFolderPath(sourceFolderPath);
		const std::string targetParent = NormalizeFolderPath(targetParentPath);
		if (source.empty()) { return; }

		const size_t lastSlash = source.find_last_of('/');
		const std::string leafName = lastSlash == std::string::npos ?
			                             source :
			                             source.substr(lastSlash + 1);
		const std::string destination = targetParent.empty() ?
			                                leafName :
			                                targetParent + "/" + leafName;

		if (destination == source || IsFolderEqualOrDescendant(
			targetParent, source
		)) {
			return;
		}

		for (std::string& folder : mFolders) {
			folder = NormalizeFolderPath(
				ReplaceFolderPrefix(folder, source, destination)
			);
		}
		for (const auto& entityPtr : mEntities) {
			if (!entityPtr) { continue; }
			const std::string current(entityPtr->GetFolderPath());
			if (!IsFolderEqualOrDescendant(current, source)) { continue; }
			entityPtr->SetFolderPath(
				ReplaceFolderPrefix(current, source, destination)
			);
		}

		std::sort(mFolders.begin(), mFolders.end());
		mFolders.erase(
			std::unique(mFolders.begin(), mFolders.end()),
			mFolders.end()
		);
	}

	void UScene::Serialize(const JsonWriter& writer) const {
		writer; // 未実装
	}

	void UScene::Deserialize(const JsonReader& reader) {
		reader; // 未実装
	}

	void UScene::OnPostLoad() {
		for (const auto& entityPtr : mEntities) {
			if (!entityPtr) { continue; }
			if (auto* transform = entityPtr->GetComponent<TransformComponent>()) {
				transform->ResolveDeferredParent(*this);
			}
			AddFolder(entityPtr->GetFolderPath());
		}
	}

	void UScene::Reset() {
		mEntities.clear();
		mEntityById.clear();
		mFolders.clear();
		mNextEntityId = 1;
	}

	EntityId UScene::AllocateEntityId() {
		while (mEntityById.contains(mNextEntityId) || mNextEntityId == 0) {
			++mNextEntityId;
		}
		return mNextEntityId++;
	}
}
