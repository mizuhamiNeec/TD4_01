#include "ShaderKey.h"

namespace Unnamed::Render {
	namespace {
		size_t HashCombine(const size_t a, const size_t b) {
			return a ^ b + 0x9e3779b97f4a7c15ull + (a << 6) + (a >> 2);
		}
	}


	size_t ShaderKeyHash::operator()(const ShaderKey& key) const noexcept {
		size_t h = 0;
		h        = HashCombine(
			h, std::hash<uint64_t>{}(
				static_cast<uint64_t>(key.shaderSourceId)
			)
		);
		h = HashCombine(h, std::hash<std::string>{}(key.entry));
		h = HashCombine(h, std::hash<std::string>{}(key.profile));
		for (const auto& [name, value] : key.defines) {
			h = HashCombine(h, std::hash<std::string>{}(name));
			h = HashCombine(h, std::hash<std::string>{}(value));
		}
		return h;
	}
}
