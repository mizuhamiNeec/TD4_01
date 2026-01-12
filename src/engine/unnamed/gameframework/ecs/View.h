#pragma once
#include <tuple>
#include "World.h"

namespace Unnamed::ECS {
	template <typename... Ts>
	class View final {
	public:
		explicit View(World& world)
			: mWorld(world)
			  , mStorages(&world.GetOrCreateStorage<Ts>()...) {
			// 型消去配列を作る（Primary選択とdense走査に使う）
			FillStorageBases(std::index_sequence_for<Ts...>{});

			// Primary（最小Sizeのストレージ）を選ぶ
			mPrimaryIndex         = FindPrimaryIndex();
			mPrimaryDenseEntities = &mStorageBases[mPrimaryIndex]->
				DenseEntitiesBase();
		}

		class Iterator final {
		public:
			Iterator(View* view, std::size_t index)
				: mView(view)
				  , mIndex(index) {
				SkipToValid();
			}

			Entity operator*() const {
				EntityId entityId = (*mView->mPrimaryDenseEntities)[mIndex];
				return mView->mWorld.MakeEntity(entityId);
			}

			Iterator& operator++() {
				++mIndex;
				SkipToValid();
				return *this;
			}

			bool operator!=(const Iterator& other) const {
				return mIndex != other.mIndex || mView != other.mView;
			}

		private:
			void SkipToValid() {
				const auto& dense = *mView->mPrimaryDenseEntities;

				while (mIndex < dense.size()) {
					EntityId entityId = dense[mIndex];

					// Destroyは全ストレージから消される前提だが、防御的にaliveも見る
					Entity entity = mView->mWorld.MakeEntity(entityId);
					if (entity.id != kInvalidEntityId && mView->
						AllHas(entityId)) {
						return;
					}
					++mIndex;
				}
			}

		private:
			View*       mView  = nullptr;
			std::size_t mIndex = 0;
		};

		Iterator begin() { return Iterator(this, 0); }
		Iterator end() { return Iterator(this, mPrimaryDenseEntities->size()); }

		template <typename T>
		T& Get(Entity entity) {
			// entityがvalidであることは呼び出し側（View列挙）で保証される想定
			auto& storage = *std::get<ComponentStorage<T>*>(mStorages);
			return storage.Get(entity.id);
		}

		template <typename T>
		const T& Get(Entity entity) const {
			const auto& storage = *std::get<ComponentStorage<T>*>(mStorages);
			return storage.Get(entity.id);
		}

	private:
		template <std::size_t... Is>
		void FillStorageBases(std::index_sequence<Is...>) {
			((mStorageBases[Is] = std::get<Is>(mStorages)), ...);
		}

		std::size_t FindPrimaryIndex() const {
			std::size_t bestIndex = 0;
			std::size_t bestSize  = std::numeric_limits<std::size_t>::max();

			for (std::size_t i = 0; i < mStorageBases.size(); ++i) {
				std::size_t size = mStorageBases[i]->Size();
				if (size < bestSize) {
					bestSize  = size;
					bestIndex = i;
				}
			}
			return bestIndex;
		}

		bool AllHas(EntityId entityId) const {
			return AllHasImpl(entityId, std::index_sequence_for<Ts...>{});
		}

		template <std::size_t... Is>
		bool AllHasImpl(EntityId entityId, std::index_sequence<Is...>) const {
			return (std::get<Is>(mStorages)->Has(entityId) && ...);
		}

	private:
		World& mWorld;

		// 型付きストレージ（Get用）
		std::tuple<ComponentStorage<Ts>*...> mStorages;

		// 型消去ストレージ（Primary選択とDenseEntities取得用）
		std::array<IComponentStorage*, sizeof...(Ts)> mStorageBases{};
		std::size_t mPrimaryIndex = 0;
		const std::vector<EntityId>* mPrimaryDenseEntities = nullptr;
	};

	template <typename... Ts>
	View<Ts...> MakeView(World& world) {
		return View<Ts...>(world);
	}
}
