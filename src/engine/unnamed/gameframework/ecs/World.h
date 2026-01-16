#pragma once
#include <memory>
#include <unordered_map>

#include "ComponentStorage.h"
#include "ECSEntity.h"
#include "ECSTypeID.h"

namespace Unnamed::ECS {
	class CommandBuffer;

	class World {
	public:
		World();

		Entity CreateEntity();
		Entity MakeEntity(EntityId entityId) const;
		void   DestroyEntity(Entity entity);

		[[nodiscard]] bool IsAlive(Entity entity) const;

		[[nodiscard]] StableId GetStableId(Entity entity) const;
		[[nodiscard]] Entity   FindByStableId(StableId stableId) const;

		template <typename T>
		[[nodiscard]] bool Has(Entity entity) const {
			const auto* storage = TryGetStorage<T>();
			if (!storage) {
				return false;
			}
			return IsAlive(entity) && storage->Has(entity.id);
		}

		template <typename T>
		T& Get(Entity entity) {
			auto& storage = GetOrCreateStorage<T>();
			return storage.Get(entity.id);
		}

		template <typename T>
		T& Add(Entity entity, T value = {}) {
			auto& storage = GetOrCreateStorage<T>();
			return storage.Add(entity.id, std::move(value));
		}

		template <typename T>
		void Remove(Entity entity) {
			auto* storage = TryGetStorage<T>();
			if (!storage) {
				return;
			}
			storage->Remove(entity.id);
		}

		CommandBuffer Defer();
		void          Flush(CommandBuffer& commandBuffer);

		template <typename T>
		T& AddImmediate(Entity entity, T value = {}) {
			return Add<T>(entity, std::move(value));
		}

		template <typename T>
		void RemoveImmediate(Entity entity) {
			Remove<T>(entity);
		}

		void DestroyImmediate(Entity entity);

		template <typename T>
		ComponentStorage<T>* TryGetStorage() {
			ComponentTypeId typeId = GetComponentTypeID<T>();
			if (typeId >= mStorages.size()) {
				return nullptr;
			}
			return static_cast<ComponentStorage<T>*>(mStorages[typeId].get());
		}

		template <typename T>
		const ComponentStorage<T>* TryGetStorage() const {
			ComponentTypeId typeId = GetComponentTypeID<T>();
			if (typeId >= mStorages.size()) {
				return nullptr;
			}

			return static_cast<const ComponentStorage<T>*>(mStorages[typeId].
				get());
		}

		template <typename T>
		ComponentStorage<T>& GetOrCreateStorage() {
			ComponentTypeId typeId = GetComponentTypeID<T>();
			if (typeId >= mStorages.size()) {
				mStorages.resize(static_cast<std::size_t>(typeId) + 1);
			}

			if (!mStorages[typeId]) {
				mStorages[typeId] = std::make_unique<ComponentStorage<T>>();
				mStorages[typeId]->EnsureCapacity(mGenerations.size());
				mAllStorages.emplace_back(mStorages[typeId].get());
			}
			return *static_cast<ComponentStorage<T>*>(mStorages[typeId].get());
		}

		[[nodiscard]] std::size_t EntityCapacity() const {
			return mGenerations.size();
		}

	private:
		Entity CreateEntityInternal();
		void   EnsureEntityCapacity(std::size_t capacity);

	private:
		std::vector<Generation> mGenerations;
		std::vector<bool>       mAlive;
		std::vector<EntityId>   mFreeList;

		std::vector<std::unique_ptr<IComponentStorage>> mStorages;
		std::vector<IComponentStorage*>                 mAllStorages;

		StableId                               mNextStableId = 1;
		std::vector<StableId>                  mStableIds;
		std::unordered_map<StableId, EntityId> mStableIdToEntityId;
	};
}
