#pragma once
#include <mutex>

#include "ConVar.h"
#include "ConVarCache.h"

/// @brief コンソール変数マネージャークラス
class ConVarManager {
public:
	template <typename T>
	static void RegisterConVar(
		const std::string& name,
		const T&           defaultValue,
		const std::string& helpString = "",
		ConVarFlags        flags      = ConVarFlags::ConVarFlags_None,
		bool               bMin       = false,
		float              fMin       = 0.0f,
		bool               bMax       = false,
		float              fMax       = 0.0f
	);

	template <typename T>
	static T GetConVarValue(const std::string& name);

	static IConVar* GetConVar(const std::string& name);
	static void     ToggleConVar(const std::string& name);

	static std::vector<IConVar*> GetAllConVars();

private:
	ConVarManager() = default;
	static std::unordered_map<std::string, std::unique_ptr<IConVar>> mConVars;
	static std::mutex                                                mMutex;
};

template <typename T>
void ConVarManager::RegisterConVar(
	const std::string& name,
	const T&           defaultValue,
	const std::string& helpString,
	ConVarFlags        flags,
	bool               bMin,
	float              fMin,
	bool               bMax,
	float              fMax
) {
	std::lock_guard lock(mMutex);
	auto conVar = std::make_unique<ConVar<T>>(name, defaultValue, helpString,
	                                          flags, bMin, fMin, bMax, fMax);

	ConVarCache::GetInstance().CacheConVar(name, conVar.get());
	mConVars[name] = std::move(conVar);
}

template <typename T>
T ConVarManager::GetConVarValue(const std::string& name) {
	std::lock_guard lock(mMutex);
	auto            it = mConVars.find(name);
	if (it != mConVars.end()) {
		auto* var = dynamic_cast<ConVar<T>*>(it->second.get());
		if (var != nullptr) {
			return var->GetValue();
		}
	}
	Console::Print("ConVar not found: " + name, kConTextColorError);
	return 0;
}
