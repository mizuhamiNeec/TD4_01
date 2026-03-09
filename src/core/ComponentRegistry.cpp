#include "ComponentRegistry.h"

#include "engine/unnamed/subsystem/console/Log.h"

namespace Unnamed {
	bool ComponentRegistry::Register(
		std::string_view       stableName, const CreateFn createFn,
		const std::string_view displayName
	) {
		if (stableName.empty() || createFn == nullptr) {
			return false;
		}

		std::string key(stableName);
		const auto  it = mEntries.find(key);
		if (it != mEntries.end()) {
			Warning(
				"ComponentRegistry",
				"Component '{}' is already registered.",
				stableName
			);
			return false;
		}

		Entry e;
		e.create      = createFn;
		e.displayName = displayName;

		mEntries.emplace(std::move(key), std::move(e));
		return true;
	}


	std::unique_ptr<BaseComponent> ComponentRegistry::Create(
		const std::string_view stableName
	) const {
		const Entry* e = Find(stableName);
		if (!e || !e->create) {
			return nullptr;
		}

		return e->create();
	}

	bool ComponentRegistry::IsRegistered(
		const std::string_view stableName
	) const {
		return Find(stableName) != nullptr;
	}

	const ComponentRegistry::Entry* ComponentRegistry::Find(
		const std::string_view stableName
	) const {
		if (stableName.empty()) {
			return nullptr;
		}

		const auto it = mEntries.find(std::string(stableName));
		if (it == mEntries.end()) return nullptr;
		return &it->second;
	}

	void ComponentRegistry::Clear() {
		mEntries.clear();
	}

	ComponentRegistry& ComponentRegistry::Get() {
		static ComponentRegistry sRegistry;
		return sRegistry;
	}

	namespace detail {
		AutoComponentRegister::AutoComponentRegister(
			const std::string_view            stableName,
			const std::string_view            displayName,
			const ComponentRegistry::CreateFn createFn
		) {
			ComponentRegistry::Get().Register(
				stableName, createFn, displayName
			);
		}
	}
}
