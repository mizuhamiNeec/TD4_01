#pragma once
#include "IComponentStorage.h"

namespace Unnamed::ECS {
	template <typename T>
	class ComponentStorage final : public IComponentStorage {
	public:
		ComponentStorage() = default;

		[[nodiscard]] bool Has(EntityId entityId) const {
			if (entityId >= mSparse.size()) {
				return false;
			}
			uint32_t denseIndex = mSparse[entityId];
			if (denseIndex == kInvalidIndex) {
				return false;
			}
			return denseIndex < mDenseEntities.size() && mDenseEntities[
				denseIndex] == entityId;
		}

		T& Get(EntityId entityId) {
			uint32_t denseIndex = mSparse[entityId];
			return mDenseData[denseIndex];
		}

		const T& Get(const EntityId entityId) const {
			uint32_t denseIndex = mSparse[entityId];
			return mDenseData[denseIndex];
		}

		T& Add(EntityId entityId, T value = {}) {
			if (Has(entityId)) {
				mDenseData[mSparse[entityId]] = std::move(value);
				return mDenseData[mSparse[entityId]];
			}

			EnsureCapacity(entityId + 1);

			uint32_t denseIndex = static_cast<uint32_t>(mDenseEntities.size());
			mDenseEntities.emplace_back(entityId);
			mDenseData.emplace_back(std::move(value));
			mSparse[entityId] = denseIndex;
			return mDenseData[denseIndex];
		}

		void Remove(EntityId entityId) {
			if (!Has(entityId)) {
				return;
			}

			uint32_t removeIndex = mSparse[entityId];
			uint32_t lastIndex   = static_cast<uint32_t>(mDenseEntities.size() -
				1);

			if (removeIndex != lastIndex) {
				mDenseEntities[removeIndex] = mDenseEntities[lastIndex];
				mDenseData[removeIndex]     = std::move(mDenseData[lastIndex]);

				EntityId movedEntity = mDenseEntities[removeIndex];
				mSparse[movedEntity] = removeIndex;
			}

			mDenseEntities.pop_back();
			mDenseData.pop_back();
			mSparse[entityId] = kInvalidIndex;
		}

		void RemoveForEntity(const EntityId entityId) override {
			Remove(entityId);
		}

		void EnsureCapacity(const std::size_t entityCapacity) override {
			if (mSparse.size() < entityCapacity) {
				const std::size_t oldSize = mSparse.size();
				mSparse.resize(entityCapacity, kInvalidIndex);
				(void)oldSize;
			}
		}

		[[nodiscard]] std::size_t Size() const override {
			return mDenseEntities.size();
		}

		[[nodiscard]] const std::vector<EntityId>& DenseEntitiesBase() const override;

		[[nodiscard]] const std::vector<EntityId>& DenseEntities() const {
			return mDenseEntities;
		}

	private:
		std::vector<EntityId> mDenseEntities;
		std::vector<T>        mDenseData;
		std::vector<uint32_t> mSparse;
	};

	template <typename T>
	const std::vector<EntityId>& ComponentStorage<T>::
	DenseEntitiesBase() const {
		return mDenseEntities;
	}
}
