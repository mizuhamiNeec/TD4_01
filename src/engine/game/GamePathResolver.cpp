#include "GamePathResolver.h"

#include <filesystem>

namespace Unnamed {
	namespace {
		[[nodiscard]] bool IsAbsoluteOrCurrentRelative(
			const std::string_view path
		) {
			if (path.empty()) {
				return false;
			}
			const std::filesystem::path fsPath(path);
			return fsPath.is_absolute() || path.rfind("./", 0) == 0 ||
			       path.rfind("../", 0) == 0;
		}

		[[nodiscard]] bool IsRelativeToCurrentDir(const std::string_view path) {
			return path.rfind("./", 0) == 0 || path.rfind("../", 0) == 0;
		}

		[[nodiscard]] std::string NormalizePath(std::string_view path) {
			return std::filesystem::path(path).lexically_normal().generic_string();
		}

		[[nodiscard]] std::string ResolveRootWithGameRootFallback(
			const std::string_view gameRoot,
			const std::string_view explicitRoot
		) {
			if (explicitRoot.empty()) {
				return NormalizePath(gameRoot);
			}

			if (IsAbsoluteOrCurrentRelative(explicitRoot)) {
				return NormalizePath(explicitRoot);
			}

			if (gameRoot.empty()) {
				return NormalizePath(explicitRoot);
			}

			return std::filesystem::path(gameRoot)
			       .append(explicitRoot)
			       .lexically_normal()
			       .generic_string();
		}

		[[nodiscard]] std::string ResolveAgainstRoot(
			const std::string_view root,
			const std::string_view path
		) {
			if (path.empty()) {
				return {};
			}

			const std::filesystem::path fsPath(path);
			if (fsPath.is_absolute() || IsRelativeToCurrentDir(path)) {
				return NormalizePath(path);
			}

			if (root.empty()) {
				return NormalizePath(path);
			}

			return std::filesystem::path(root)
			       .append(path)
			       .lexically_normal()
			       .generic_string();
		}
	}

	std::string ResolveGameRootPath(
		const GameModulePaths& paths,
		std::string_view       path
	) {
		return ResolveAgainstRoot(paths.gameRoot, path);
	}

	std::string ResolveGameContentPath(
		const GameModulePaths& paths,
		std::string_view       path
	) {
		const std::string contentRoot = ResolveRootWithGameRootFallback(
			paths.gameRoot,
			paths.contentRoot
		);
		return ResolveAgainstRoot(contentRoot, path);
	}

	std::string ResolveGameConfigPath(
		const GameModulePaths& paths,
		std::string_view       path
	) {
		const std::string configRoot = ResolveRootWithGameRootFallback(
			paths.gameRoot,
			paths.configRoot
		);
		return ResolveAgainstRoot(configRoot, path);
	}

	std::string ResolveStartupScenePath(const IGameModule& gameModule) {
		const GameModulePaths paths = gameModule.GetGameModulePaths();
		std::string startupScene = gameModule.GetDefaultStartupScenePath();
		if (startupScene.empty()) {
			startupScene = paths.defaultStartupScene;
		}
		return ResolveGameContentPath(paths, startupScene);
	}
}
