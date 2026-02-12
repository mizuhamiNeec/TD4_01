#include "UScene.h"

#include <string>

#include "engine/unnamed/framework/entity/UEntity.h"

namespace Unnamed {
	UScene::UScene()  = default;
	UScene::~UScene() = default;

	UEntity& UScene::CreateEntity(
		const std::string_view name, uint64_t id, bool isEditorOnly
	) {
		auto entity = std::make_unique<UEntity>(
			std::string(name), id, isEditorOnly
		);
		UEntity* raw = entity.get();

		mEntities.emplace_back(std::move(entity));
		mEntityById[id] = raw;
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

	void UScene::Serialize(const JsonWriter& writer) const {
		writer; // 未実装
	}

	void UScene::Deserialize(const JsonReader& reader) {
		reader; // 未実装
	}

	void UScene::OnPostLoad() {
		// 未実装
	}

	EntityId UScene::AllocateEntityId() { return mNextEntityId++; }
}
