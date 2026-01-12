#pragma once
#include <cstdint>
#include <limits>

namespace Unnamed::ECS {
	using EntityId   = uint32_t;
	using Generation = uint32_t;
	using StableId   = uint64_t;

	static constexpr
	EntityId kInvalidEntityId = std::numeric_limits<EntityId>::max();

	struct Entity final {
		EntityId   id         = kInvalidEntityId;
		Generation generation = 0;

		constexpr bool operator==(const Entity& other) const noexcept {
			return id == other.id && generation == other.generation;
		}

		constexpr bool operator!=(const Entity& other) const noexcept {
			return !(*this == other);
		}
	};
}
