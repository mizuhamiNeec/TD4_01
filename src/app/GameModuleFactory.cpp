#include "GameModuleFactory.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <exception>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <json.hpp>
#include <Windows.h>

#include "engine/game/IDemoService.h"
#include "engine/physics/core/Physics.h"
#include "engine/scene/Scene.h"
#include "engine/unnamed/subsystem/console/Log.h"
#include "engine/world/World.h"

namespace Unnamed {
	namespace {
		constexpr std::string_view kChannel = "GameModuleFactory";

		struct RegisteredGameModule {
			std::optional<GameModulePaths> paths;
			GameModuleCreateFunction createFunction = nullptr;
		};

		struct LoadedGameProfile {
			GameModulePaths            paths;
			std::vector<std::string> aliases;
		};

		struct ManifestCandidatePath {
			std::filesystem::path path;
			std::string           reason;
		};

		struct ManifestLoadResult {
			std::optional<LoadedGameProfile> profile;
			std::string                      failureReason;
		};

		struct ManifestSearchConfiguration {
			std::optional<std::filesystem::path> explicitRepoRootOverride;
		};

		struct GameModuleRegistryState {
			std::unordered_map<std::string, RegisteredGameModule> modulesByName;
			std::unordered_map<std::string, std::string> aliasToCanonical;
			ManifestSearchConfiguration manifestSearch = {};
			bool defaultsRegistered = false;
		};

		[[nodiscard]] GameModuleRegistryState& GetRegistryState() {
			static GameModuleRegistryState state;
			return state;
		}

		[[nodiscard]] std::string NormalizeGameName(std::string_view value) {
			std::string normalized(value);
			std::transform(
				normalized.begin(),
				normalized.end(),
				normalized.begin(),
				[](unsigned char ch) { return static_cast<char>(std::tolower(ch)); }
			);
			return normalized;
		}

		[[nodiscard]] bool IsRepositoryRoot(
			const std::filesystem::path& path
		) {
			std::error_code ec;
			if (!std::filesystem::exists(path, ec) || ec) {
				return false;
			}

			return std::filesystem::exists(path / "premake5.lua", ec) && !ec &&
			       std::filesystem::exists(path / "src", ec) && !ec &&
			       std::filesystem::exists(path / "projects", ec) && !ec;
		}

		[[nodiscard]] std::optional<std::filesystem::path> TryFindRepositoryRoot(
			std::filesystem::path startPath
		) {
			std::error_code ec;
			if (startPath.empty()) {
				return std::nullopt;
			}

			startPath = std::filesystem::weakly_canonical(startPath, ec);
			if (ec) {
				return std::nullopt;
			}

			if (!std::filesystem::is_directory(startPath, ec) || ec) {
				startPath = startPath.parent_path();
			}

			for (std::filesystem::path cursor = startPath;; cursor = cursor.parent_path()) {
				if (IsRepositoryRoot(cursor)) {
					return cursor;
				}
				if (!cursor.has_parent_path() || cursor == cursor.parent_path()) {
					break;
				}
			}

			return std::nullopt;
		}

		[[nodiscard]] std::optional<std::filesystem::path> TryGetExecutableDirectory() {
			std::vector<wchar_t> buffer(260, L'\0');
			DWORD                copied = 0;
			while (true) {
				copied = ::GetModuleFileNameW(
					nullptr,
					buffer.data(),
					static_cast<DWORD>(buffer.size())
				);
				if (copied == 0) {
					return std::nullopt;
				}

				if (copied < buffer.size() - 1) {
					break;
				}
				buffer.resize(buffer.size() * 2, L'\0');
			}

			const std::filesystem::path exePath(
				std::wstring_view(buffer.data(), copied)
			);
			return exePath.parent_path();
		}

		[[nodiscard]] std::optional<std::filesystem::path> TryGetEnvironmentRepoRoot() {
			constexpr wchar_t kEnvVarName[] = L"UNNAMED_REPO_ROOT";
			const DWORD       requiredChars =
				::GetEnvironmentVariableW(kEnvVarName, nullptr, 0);
			if (requiredChars == 0) {
				return std::nullopt;
			}

			std::vector<wchar_t> buffer(requiredChars, L'\0');
			const DWORD          writtenChars = ::GetEnvironmentVariableW(
				kEnvVarName,
				buffer.data(),
				requiredChars
			);
			if (writtenChars == 0 || writtenChars >= requiredChars) {
				return std::nullopt;
			}

			const std::filesystem::path envPath(
				std::wstring_view(buffer.data(), writtenChars)
			);
			return envPath;
		}

		[[nodiscard]] std::optional<std::filesystem::path> TryResolveRepositoryRootFromExplicitPath(
			const std::filesystem::path& explicitPath
		) {
			std::error_code ec;
			const std::filesystem::path canonicalPath =
				std::filesystem::weakly_canonical(explicitPath, ec);
			if (ec) {
				return std::nullopt;
			}
			return TryFindRepositoryRoot(canonicalPath);
		}

		[[nodiscard]] std::vector<ManifestCandidatePath> BuildManifestCandidates(
			const std::string_view          relativeManifestPath,
			const ManifestSearchConfiguration& config
		) {
			std::vector<ManifestCandidatePath> candidates;
			std::unordered_set<std::string>    dedupe;
			const std::filesystem::path relativePath{
				std::string(relativeManifestPath)
			};

			const auto addCandidate = [&](const std::filesystem::path& base, const std::string_view reason) {
				std::error_code ec;
				const std::filesystem::path normalized =
					std::filesystem::weakly_canonical(base / relativePath, ec);
				const std::filesystem::path candidatePath =
					ec ? (base / relativePath).lexically_normal() : normalized;
				const std::string key = candidatePath.generic_string();
				if (dedupe.insert(key).second) {
					candidates.push_back(ManifestCandidatePath{
						.path = candidatePath,
						.reason = std::string(reason),
					});
				}
			};

			if (config.explicitRepoRootOverride.has_value()) {
				const std::filesystem::path& explicitPath =
					*config.explicitRepoRootOverride;
				if (const auto resolved = TryResolveRepositoryRootFromExplicitPath(
						explicitPath
					);
					resolved.has_value()) {
					addCandidate(*resolved, "cli-repo-root");
				} else {
					DevMsg(
						kChannel,
						"ignored invalid --repo-root '{}' while searching '{}'",
						explicitPath.generic_string(),
						relativeManifestPath
					);
				}
			}

			if (const auto envRepoRoot = TryGetEnvironmentRepoRoot();
				envRepoRoot.has_value()) {
				if (const auto resolved = TryResolveRepositoryRootFromExplicitPath(
						*envRepoRoot
					);
					resolved.has_value()) {
					addCandidate(*resolved, "env:UNNAMED_REPO_ROOT");
				} else {
					DevMsg(
						kChannel,
						"ignored invalid UNNAMED_REPO_ROOT '{}' while searching '{}'",
						envRepoRoot->generic_string(),
						relativeManifestPath
					);
				}
			}

			const std::filesystem::path currentPath = std::filesystem::current_path();
			if (const auto repoRoot = TryFindRepositoryRoot(currentPath); repoRoot.has_value()) {
				addCandidate(*repoRoot, "cwd-upward");
			} else {
				DevMsg(
					kChannel,
					"repository root not found from cwd '{}' while searching '{}'",
					currentPath.generic_string(),
					relativeManifestPath
				);
			}

			if (const auto exeDirectory = TryGetExecutableDirectory();
				exeDirectory.has_value()) {
				for (std::filesystem::path cursor = *exeDirectory;; cursor = cursor.parent_path()) {
					addCandidate(cursor, "exe-parent-upward");
					if (!cursor.has_parent_path() || cursor == cursor.parent_path()) {
						break;
					}
				}
			} else {
				DevMsg(
					kChannel,
					"failed to resolve executable directory while searching '{}'",
					relativeManifestPath
				);
			}

			return candidates;
		}

		void RegisterProfile(GameModuleRegistryState& state, GameModulePaths paths) {
			const std::string canonicalName = NormalizeGameName(paths.gameName);
			if (canonicalName.empty()) {
				return;
			}

			// Game 固有情報はプロファイルとして保持し、生成関数は後から差し込めるようにする。
			RegisteredGameModule& entry = state.modulesByName[canonicalName];
			entry.paths = std::move(paths);
			state.aliasToCanonical[canonicalName] = canonicalName;
		}

		[[nodiscard]] bool TryReadRequiredStringField(
			const nlohmann::json& root,
			const std::string_view fieldName,
			const std::string_view manifestPath,
			std::string&           outValue
		) {
			const auto it = root.find(std::string(fieldName));
			if (it == root.end() || !it->is_string()) {
				DevMsg(
					kChannel,
					"manifest '{}' missing required string field '{}'",
					manifestPath,
					fieldName
				);
				return false;
			}

			outValue = it->get<std::string>();
			return true;
		}

		[[nodiscard]] bool TryReadRequiredIntegerField(
			const nlohmann::json& root,
			const std::string_view fieldName,
			const std::string_view manifestPath,
			int&                   outValue
		) {
			const auto it = root.find(std::string(fieldName));
			if (it == root.end() || !it->is_number_integer()) {
				DevMsg(
					kChannel,
					"manifest '{}' missing required integer field '{}'",
					manifestPath,
					fieldName
				);
				return false;
			}

			outValue = it->get<int>();
			return true;
		}

		[[nodiscard]] ManifestLoadResult LoadGameProfileManifest(
			const std::string_view manifestPath
		) {
			ManifestLoadResult result = {};

			std::ifstream input(std::string(manifestPath), std::ios::binary);
			if (!input.is_open()) {
				result.failureReason = "file not found";
				return result;
			}

			nlohmann::json root = nlohmann::json::object();
			try {
				input >> root;
			} catch (const std::exception& ex) {
				result.failureReason = std::format("parse error: {}", ex.what());
				return result;
			}

			if (!root.is_object()) {
				result.failureReason = "invalid root type: expected object";
				return result;
			}

			int schemaVersion = 0;
			if (!TryReadRequiredIntegerField(
					root,
					"schemaVersion",
					manifestPath,
					schemaVersion
				)) {
				result.failureReason =
					"missing or invalid required integer field 'schemaVersion'";
				return result;
			}

			if (schemaVersion != 1) {
				result.failureReason = "unsupported schemaVersion " +
				                       std::to_string(schemaVersion) +
				                       " (supported: 1)";
				DevMsg(
					kChannel,
					"manifest '{}' has unsupported schemaVersion {} (supported: 1)",
					manifestPath,
					schemaVersion
				);
				return result;
			}

			LoadedGameProfile profile;
			if (!TryReadRequiredStringField(
					root,
					"gameName",
					manifestPath,
					profile.paths.gameName
				) ||
				!TryReadRequiredStringField(
					root,
					"gameRoot",
					manifestPath,
					profile.paths.gameRoot
				) ||
				!TryReadRequiredStringField(
					root,
					"contentRoot",
					manifestPath,
					profile.paths.contentRoot
				) ||
				!TryReadRequiredStringField(
					root,
					"configRoot",
					manifestPath,
					profile.paths.configRoot
				) ||
				!TryReadRequiredStringField(
					root,
					"defaultStartupScene",
					manifestPath,
					profile.paths.defaultStartupScene
				)) {
				result.failureReason = "missing required string field(s)";
				return result;
			}

			const auto aliasesIt = root.find("aliases");
			if (aliasesIt == root.end() || !aliasesIt->is_array()) {
				DevMsg(
					kChannel,
					"manifest '{}' missing required string array field 'aliases'",
					manifestPath
				);
				result.failureReason = "missing required string array field 'aliases'";
				return result;
			}

			for (const nlohmann::json& aliasNode : *aliasesIt) {
				if (!aliasNode.is_string()) {
					DevMsg(
						kChannel,
						"manifest '{}' contains non-string alias entry",
						manifestPath
					);
					result.failureReason = "aliases contains non-string entry";
					return result;
				}
				profile.aliases.emplace_back(aliasNode.get<std::string>());
			}

			DevMsg(
				kChannel,
				"loaded manifest '{}' for game '{}'",
				manifestPath,
				profile.paths.gameName
			);
			result.profile = std::move(profile);
			return result;
		}

		[[nodiscard]] std::optional<LoadedGameProfile> LoadGameProfileManifestFromCandidates(
			const std::string_view          relativeManifestPath,
			const ManifestSearchConfiguration& config
		) {
			const std::vector<ManifestCandidatePath> candidates =
				BuildManifestCandidates(relativeManifestPath, config);
			const bool hasExplicitRepoRoot =
				config.explicitRepoRootOverride.has_value();

			DevMsg(
				kChannel,
				"manifest search start: target='{}' repoRootOverride={} envRepoRoot={}",
				relativeManifestPath,
				hasExplicitRepoRoot ? "set" : "unset",
				TryGetEnvironmentRepoRoot().has_value() ? "set" : "unset"
			);
			for (const ManifestCandidatePath& candidate : candidates) {
				const std::string manifestPath = candidate.path.generic_string();
				DevMsg(
					kChannel,
					"trying manifest candidate '{}' ({})",
					manifestPath,
					candidate.reason
				);
				ManifestLoadResult loadResult =
					LoadGameProfileManifest(manifestPath);
				if (loadResult.profile.has_value()) {
					loadResult.profile->paths.resolvedManifestPath = manifestPath;
					DevMsg(
						kChannel,
						"manifest resolved '{}' from '{}'",
						relativeManifestPath,
						manifestPath
					);
					Msg(
						kChannel,
						"manifest profile loaded: game='{}' manifest='{}' gameRoot='{}' contentRoot='{}' configRoot='{}' defaultStartupScene='{}'",
						loadResult.profile->paths.gameName,
						manifestPath,
						loadResult.profile->paths.gameRoot,
						loadResult.profile->paths.contentRoot,
						loadResult.profile->paths.configRoot,
						loadResult.profile->paths.defaultStartupScene
					);
					return loadResult.profile;
				}

				DevMsg(
					kChannel,
					"manifest candidate rejected '{}' ({}): {}",
					manifestPath,
					candidate.reason,
					loadResult.failureReason.empty() ? "unknown reason" :
					                                  loadResult.failureReason
				);
			}

			DevMsg(
				kChannel,
				"manifest search failed: target='{}' candidatesTried={} repoRootOverride={} envRepoRoot={}",
				relativeManifestPath,
				candidates.size(),
				hasExplicitRepoRoot ? "set" : "unset",
				TryGetEnvironmentRepoRoot().has_value() ? "set" : "unset"
			);
			return std::nullopt;
		}

		[[nodiscard]] bool RegisterAliasInternal(
			GameModuleRegistryState& state,
			std::string_view aliasName,
			std::string_view targetGameName
		) {
			const std::string alias = NormalizeGameName(aliasName);
			const std::string target = NormalizeGameName(targetGameName);
			if (alias.empty() || target.empty()) {
				return false;
			}

			if (!state.modulesByName.contains(target)) {
				return false;
			}

			state.aliasToCanonical[alias] = target;
			return true;
		}

		void RegisterAliases(
			GameModuleRegistryState&      state,
			const std::vector<std::string>& aliases,
			const std::string_view         canonicalName
		) {
			for (const std::string& alias : aliases) {
				if (!RegisterAliasInternal(state, alias, canonicalName)) {
					DevMsg(
						kChannel,
						"failed to register alias '{}' for game '{}'",
						alias,
						canonicalName
					);
				}
			}
		}

		void RegisterDefaultProfilesIfNeeded() {
			GameModuleRegistryState& state = GetRegistryState();
			if (state.defaultsRegistered) {
				return;
			}

			state.defaultsRegistered = true;
			constexpr std::array<std::string_view, 2> kRelativeManifestPaths = {
				"projects/ParkourGame/config/game_profile.json",
				"projects/TeamGame/config/game_profile.json",
			};

			for (const std::string_view manifestPath : kRelativeManifestPaths) {
				const std::optional<LoadedGameProfile> profile =
					LoadGameProfileManifestFromCandidates(
						manifestPath,
						state.manifestSearch
					);
				if (!profile.has_value()) {
					continue;
				}

				const std::string canonicalName =
					NormalizeGameName(profile->paths.gameName);
				RegisterProfile(state, profile->paths);
				RegisterAliases(state, profile->aliases, canonicalName);
			}
		}

		[[nodiscard]] std::string ResolveCanonicalName(
			const GameModuleRegistryState& state,
			std::string_view gameName
		) {
			const std::string normalized = NormalizeGameName(gameName);
			if (normalized.empty()) {
				return {};
			}

			const auto aliasIt = state.aliasToCanonical.find(normalized);
			if (aliasIt != state.aliasToCanonical.end()) {
				return aliasIt->second;
			}
			return normalized;
		}

		[[nodiscard]] const RegisteredGameModule* FindRegisteredGameModule(
			std::string_view gameName
		) {
			RegisterDefaultProfilesIfNeeded();
			const GameModuleRegistryState& state = GetRegistryState();
			const std::string canonicalName = ResolveCanonicalName(state, gameName);
			if (canonicalName.empty()) {
				return nullptr;
			}

			const auto entryIt = state.modulesByName.find(canonicalName);
			if (entryIt == state.modulesByName.end()) {
				return nullptr;
			}
			return &entryIt->second;
		}

		class DefaultGameModule final : public IGameModule {
		public:
			explicit DefaultGameModule(GameModulePaths paths)
				: mPaths(std::move(paths)) {
			}

			void Initialize(EngineServices& services) override {
				(void)services;
			}

			[[nodiscard]] std::unique_ptr<World> CreateRuntimeWorld(
				const WorldServices& services
			) override {
				auto world = std::make_unique<World>();
				world->SetServices(services);
				return world;
			}

			[[nodiscard]] std::unique_ptr<World> CreatePlayWorld(
				const WorldServices& services
			) override {
				auto world = std::make_unique<World>();
				world->SetServices(services);
				return world;
			}

			[[nodiscard]] std::unique_ptr<IDemoService> CreateDemoService() override {
				return nullptr;
			}

			void RegisterGameComponents(ComponentRegistry& componentRegistry) override {
				(void)componentRegistry;
			}

			[[nodiscard]] GameModulePaths GetGameModulePaths() const override {
				return mPaths;
			}

			[[nodiscard]] std::string GetDefaultUiDocumentPath() const override {
				return {};
			}

		private:
			GameModulePaths mPaths;
		};

		class ProfileBoundGameModule final : public IGameModule {
		public:
			ProfileBoundGameModule(
				std::unique_ptr<IGameModule> innerModule,
				GameModulePaths              profilePaths
			)
				: mInnerModule(std::move(innerModule))
				, mProfilePaths(std::move(profilePaths)) {
			}

			void Initialize(EngineServices& services) override {
				mInnerModule->Initialize(services);
			}

			[[nodiscard]] std::unique_ptr<World> CreateRuntimeWorld(
				const WorldServices& services
			) override {
				return mInnerModule->CreateRuntimeWorld(services);
			}

			[[nodiscard]] std::unique_ptr<World> CreatePlayWorld(
				const WorldServices& services
			) override {
				return mInnerModule->CreatePlayWorld(services);
			}

			[[nodiscard]] std::unique_ptr<IDemoService> CreateDemoService() override {
				return mInnerModule->CreateDemoService();
			}

			void RegisterGameComponents(ComponentRegistry& componentRegistry) override {
				mInnerModule->RegisterGameComponents(componentRegistry);
			}

			[[nodiscard]] GameModulePaths GetGameModulePaths() const override {
				return mProfilePaths;
			}

			[[nodiscard]] std::string GetDefaultStartupScenePath() const override {
				return mProfilePaths.defaultStartupScene;
			}

			[[nodiscard]] std::string GetDefaultUiDocumentPath() const override {
				return mInnerModule->GetDefaultUiDocumentPath();
			}

		private:
			std::unique_ptr<IGameModule> mInnerModule;
			GameModulePaths              mProfilePaths;
		};
	}

	void RegisterDefaultGameModuleProfiles() {
		RegisterDefaultProfilesIfNeeded();
	}

	void SetGameModuleManifestRepoRootOverride(
		const std::filesystem::path& repoRootPath
	) {
		GameModuleRegistryState& state = GetRegistryState();
		if (repoRootPath.empty()) {
			DevMsg(kChannel, "ignored empty --repo-root override");
			return;
		}

		state.manifestSearch.explicitRepoRootOverride = repoRootPath;
		DevMsg(
			kChannel,
			"configured --repo-root override: '{}'",
			repoRootPath.generic_string()
		);
		if (state.defaultsRegistered) {
			DevMsg(
				kChannel,
				"--repo-root override was set after profile registration; restart app to apply new manifest search root"
			);
		}
	}

	bool RegisterGameModule(
		const std::string_view gameName,
		const GameModuleCreateFunction createFunction
	) {
		RegisterDefaultProfilesIfNeeded();
		if (createFunction == nullptr) {
			return false;
		}

		const std::string canonicalName = NormalizeGameName(gameName);
		if (canonicalName.empty()) {
			return false;
		}

		GameModuleRegistryState& state = GetRegistryState();
		if (!state.modulesByName.contains(canonicalName)) {
			DevMsg(
				kChannel,
				"cannot register runtime module for '{}': profile is not loaded from manifest",
				gameName
			);
			return false;
		}

		RegisteredGameModule& entry = state.modulesByName[canonicalName];
		entry.createFunction = createFunction;
		state.aliasToCanonical[canonicalName] = canonicalName;
		return true;
	}

	bool RegisterGameModuleAlias(
		const std::string_view aliasName,
		const std::string_view targetGameName
	) {
		RegisterDefaultProfilesIfNeeded();
		GameModuleRegistryState& state = GetRegistryState();
		const std::string canonicalTarget = ResolveCanonicalName(state, targetGameName);
		if (canonicalTarget.empty()) {
			return false;
		}

		return RegisterAliasInternal(state, aliasName, canonicalTarget);
	}

	GameModulePaths ResolveGameModulePaths(const std::string_view gameName) {
		if (const RegisteredGameModule* entry = FindRegisteredGameModule(gameName);
			entry != nullptr && entry->paths.has_value()) {
			return *entry->paths;
		}

		DevMsg(
			kChannel,
			"no profile registered for '{}'; returning empty GameModulePaths",
			gameName
		);
		return {};
	}

	std::unique_ptr<IGameModule> CreateGameModule(const std::string_view gameName) {
		const RegisteredGameModule* entry = FindRegisteredGameModule(gameName);
		if (entry == nullptr) {
			DevMsg(
				kChannel,
				"CreateGameModule('{}') failed: game profile is not registered",
				gameName
			);
			return nullptr;
		}

		if (entry->createFunction != nullptr) {
			std::unique_ptr<IGameModule> module = entry->createFunction();
			if (!module) {
				DevMsg(
					kChannel,
					"CreateGameModule('{}') failed: registered createFunction returned null",
					gameName
				);
				return nullptr;
			}

			if (entry->paths.has_value()) {
				return std::make_unique<ProfileBoundGameModule>(
					std::move(module),
					*entry->paths
				);
			}
			return module;
		}

		// Editor など未リンク App では、Paths だけ持つ既定モジュールで起動する。
		if (entry->paths.has_value()) {
			return std::make_unique<DefaultGameModule>(*entry->paths);
		}

		DevMsg(
			kChannel,
			"CreateGameModule('{}') failed: profile exists but paths are missing",
			gameName
		);
		return nullptr;
	}
}
