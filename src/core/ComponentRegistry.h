#pragma once
#include <memory>
#include <string>
#include <unordered_map>

#include "engine/unnamed/framework/components/base/BaseComponent.h"

namespace Unnamed {
	/// @brief コンポーネントレジストリ
	/// @details 
	class ComponentRegistry {
	public:
		using CreateFn = std::unique_ptr<BaseComponent>(*)();

		struct Entry {
			CreateFn    create = nullptr;
			std::string displayName;
		};

		ComponentRegistry()  = default;
		~ComponentRegistry() = default;

		ComponentRegistry(const ComponentRegistry&)            = delete;
		ComponentRegistry& operator=(const ComponentRegistry&) = delete;

		bool Register(
			std::string_view stableName, CreateFn createFn,
			std::string_view displayName
		);

		[[nodiscard]] std::unique_ptr<BaseComponent> Create(
			std::string_view stableName
		) const;

		[[nodiscard]] bool IsRegistered(std::string_view stableName) const;

		[[nodiscard]] const Entry* Find(std::string_view stableName) const;

		void Clear();

		static ComponentRegistry& Get();

	private:
		std::unordered_map<std::string, Entry> mEntries;
	};

	namespace detail {
		struct AutoComponentRegister final {
			AutoComponentRegister(
				std::string_view            stableName,
				std::string_view            displayName,
				ComponentRegistry::CreateFn createFn
			);
		};
	}
}

// コンポーネント登録マクロ
#define REGISTER_COMPONENT(T)								\
namespace {													\
std::unique_ptr<Unnamed::BaseComponent> Create_##T() {		\
return std::make_unique<T>();								\
}															\
const Unnamed::detail::AutoComponentRegister gAutoReg_##T(	\
T{}.GetStableName(), T{}.GetComponentName(), &Create_##T);	\
}

// 拡張コンポーネント登録マクロ
#define REGISTER_COMPONENT_EX(T, stableNameLiteral, displayNameLiteral) \
namespace {																\
std::unique_ptr<Unnamed::BaseComponent> Create_##T() {					\
return std::make_unique<T>();											\
}																		\
const Unnamed::detail::AutoComponentRegister gAutoReg_##T(				\
(stableNameLiteral), (displayNameLiteral), &Create_##T);				\
}
