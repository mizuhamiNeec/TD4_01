#include "World.h"

#include "CommandBuffer.h"

namespace Unnamed::ECS {
	World::World() = default;

	Entity World::CreateEntity() {
		return CreateEntityInternal();
	}

	Entity World::MakeEntity(EntityId entityId) const {
		Entity entity{};
		if (entityId >= mGenerations.size()) {
			return entity;
		}
		if (!mAlive[entityId]) {
			return entity;
		}
		entity.id         = entityId;
		entity.generation = mGenerations[entityId];
		return entity;
	}

	void World::DestroyEntity(Entity entity) {
		DestroyImmediate(entity);
	}

	bool World::IsAlive(Entity entity) const {
		if (entity.id >= mGenerations.size()) {
			return false;
		}
		if (!mAlive[entity.id]) {
			return false;
		}
		return mGenerations[entity.id] == entity.generation;
	}

	StableId World::GetStableId(Entity entity) const {
		if (!IsAlive(entity)) {
			return 0;
		}
		return mStableIds[entity.id];
	}

	Entity World::FindByStableId(StableId stableId) const {
		Entity result{};
		auto   it = mStableIdToEntityId.find(stableId);
		if (it == mStableIdToEntityId.end()) {
			return result;
		}

		EntityId id = it->second;
		if (id >= mGenerations.size() || !mAlive[id]) {
			return result;
		}

		result.id         = id;
		result.generation = mGenerations[id];
		return result;
	}

	CommandBuffer World::Defer() {
		return CommandBuffer{};
	}

	void World::Flush(CommandBuffer& commandBuffer) {
		commandBuffer.Apply(*this);
		commandBuffer.Clear();
	}

	void World::DestroyImmediate(Entity entity) {
		if (!IsAlive(entity)) {
			return;
		}

		EntityId id = entity.id;

		for (IComponentStorage* storage : mAllStorages) {
			storage->RemoveForEntity(id);
		}

		StableId stableId = mStableIds[id];
		if (stableId != 0) {
			mStableIdToEntityId.erase(stableId);
			mStableIds[id] = 0;
		}

		mAlive[id]       = false;
		mGenerations[id] += 1;
		mFreeList.emplace_back(id);
	}

	Entity World::CreateEntityInternal() {
		EntityId id = 0;

		if (!mFreeList.empty()) {
			id = mFreeList.back();
			mFreeList.pop_back();
		} else {
			id = static_cast<EntityId>(mGenerations.size());
			EnsureEntityCapacity(static_cast<std::size_t>(id) + 1);
		}

		mAlive[id] = true;

		StableId stableId             = mNextStableId++;
		mStableIds[id]                = stableId;
		mStableIdToEntityId[stableId] = id;

		Entity entity;
		entity.id         = id;
		entity.generation = mGenerations[id];
		return entity;
	}

	void World::EnsureEntityCapacity(std::size_t capacity) {
		std::size_t oldCap = mGenerations.size();
		if (oldCap >= capacity) {
			return;
		}

		mGenerations.resize(capacity, 0);
		mAlive.resize(capacity, false);
		mStableIds.resize(capacity, 0);

		for (IComponentStorage* storage : mAllStorages) {
			storage->EnsureCapacity(capacity);
		}
	}
}
