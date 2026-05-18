#include "ParticleModuleRegistry.h"

#include "ParticleModules.h" // 組み込みモジュール群

ParticleModuleRegistry& ParticleModuleRegistry::Get()
{
	// 関数内 static なので初期化順序の問題が起きない
	static ParticleModuleRegistry instance;
	return instance;
}

ParticleModuleRegistry::ParticleModuleRegistry()
{
	// ---- 組み込みモジュールの登録 ----
	Register("Lifetime", [] { return std::make_unique<LifetimeModule>(); });
	Register("Location", [] { return std::make_unique<LocationModule>(); });
	Register("Velocity", [] { return std::make_unique<VelocityModule>(); });
	Register("Rotation", [] { return std::make_unique<RotationModule>(); });
	Register("Size",     [] { return std::make_unique<SizeModule>(); });
	Register("Color",    [] { return std::make_unique<ColorModule>(); });

	// 既定スタック順（従来の固定順と同じ。Lifetime を先頭に置く）
	defaultOrder_ = { "Lifetime", "Location", "Velocity", "Rotation", "Size", "Color" };
}

void ParticleModuleRegistry::Register(const std::string& typeName, Factory factory)
{
	for (auto& entry : factories_) {
		if (entry.first == typeName) {
			entry.second = std::move(factory); // 同名は上書き
			return;
		}
	}
	factories_.emplace_back(typeName, std::move(factory));
}

std::unique_ptr<ParticleModule> ParticleModuleRegistry::Create(const std::string& typeName) const
{
	for (const auto& entry : factories_) {
		if (entry.first == typeName) {
			return entry.second();
		}
	}
	return nullptr;
}

bool ParticleModuleRegistry::IsRegistered(const std::string& typeName) const
{
	for (const auto& entry : factories_) {
		if (entry.first == typeName) {
			return true;
		}
	}
	return false;
}

std::vector<std::string> ParticleModuleRegistry::GetRegisteredTypes() const
{
	std::vector<std::string> names;
	names.reserve(factories_.size());
	for (const auto& entry : factories_) {
		names.push_back(entry.first);
	}
	return names;
}
