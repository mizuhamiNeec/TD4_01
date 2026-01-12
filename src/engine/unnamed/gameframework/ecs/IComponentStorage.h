#pragma once
#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

#include "ECSEntity.h"

namespace Unnamed::ECS {
	static constexpr
	uint32_t kInvalidIndex = std::numeric_limits<uint32_t>::max();

	class IComponentStorage {
	public:
		virtual ~IComponentStorage() = default;

		virtual void RemoveForEntity(EntityId entityId) = 0;
		virtual void EnsureCapacity(std::size_t entityCapacity) = 0;
		[[nodiscard]] virtual std::size_t Size() const = 0;

		[[nodiscard]] virtual const std::vector<EntityId>& DenseEntitiesBase() const = 0;
	};
}
