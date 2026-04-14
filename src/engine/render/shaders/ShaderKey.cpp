#include "ShaderKey.h"

#include "core/hash/HashBuilder.h"

namespace Unnamed::Render {
	size_t ShaderKeyHash::operator()(const ShaderKey& key) const noexcept {
		HashBuilder hash = {};
		hash.AddValue(static_cast<uint64_t>(key.shaderSourceId));
		hash.AddValue(key.entry);
		hash.AddValue(key.profile);
		for (const auto& [name, value] : key.defines) {
			hash.AddValue(name);
			hash.AddValue(value);
		}
		return hash.Value();
	}
}
