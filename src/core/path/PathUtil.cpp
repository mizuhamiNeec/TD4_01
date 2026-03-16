#include "PathUtil.h"

#include "core/string/StrUtil.h"

namespace Path {
	std::string ResolveRelativePath(
		const std::filesystem::path& baseDir, std::string path
	) {
		if (path.empty()) {
			return path;
		}

		std::filesystem::path p(path);
		if (p.is_relative()) {
			p = baseDir / p;
		}
		return Unnamed::StrUtil::NormalizePath(p.lexically_normal().string());
	}
}
