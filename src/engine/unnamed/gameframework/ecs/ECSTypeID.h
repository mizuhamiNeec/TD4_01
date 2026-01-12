#pragma once
#include <atomic>
#include <cstdint>

namespace Unnamed::ECS {
	using ComponentTypeId = uint32_t;

	class ComponentTypeIdGenerator {
	public:
		static ComponentTypeId Allocate() noexcept {
			static std::atomic<ComponentTypeId> nextId{0};
			return nextId.fetch_add(1, std::memory_order_relaxed);
		}
	};

	template <typename T>
	ComponentTypeId GetComponentTypeID() noexcept {
		static const ComponentTypeId kTypeId =
			ComponentTypeIdGenerator::Allocate();
		return kTypeId;
	}
}
