#include "AssetManager.h"

#include <algorithm>
#include <filesystem>

#include <core/UnnamedMacro.h>
#include <core/string/StrUtil.h>

#include <engine/unnamed/subsystem/console/Log.h>

#include "loader/interface/IAssetLoader.h"

#include "types/MaterialAssetData.h"
#include "types/MaterialInstanceAssetData.h"
#include "types/MeshAssetData.h"
#include "types/PostFxChainAssetData.h"
#include "types/ShaderProgramAssetData.h"
#include "types/ShaderSourceAssetData.h"

namespace Unnamed {
	constexpr std::string_view kChannel = "AssetManager";

	AssetManager::AssetManager() = default;

	void AssetManager::RegisterLoader(std::unique_ptr<IAssetLoader> loader) {
		std::scoped_lock lock(mMutex);
		mLoaders.emplace_back(std::move(loader));
	}

	AssetID AssetManager::LoadFromFile(
		const std::string& path, const std::optional<ASSET_TYPE> typeOpt
	) {
		std::scoped_lock lock(mMutex);
		auto             deduced = ASSET_TYPE::UNKNOWN;
		if (!typeOpt.has_value()) {
			// 型がわかんねぇので、ローダに読めるか確認させる
			for (const auto& l : mLoaders) {
				if (l->CanLoad(path, &deduced)) { break; }
			}

			Warning(
				kChannel,
				"型をチェックしました: {}. 型を知っている場合はなるべく指定してください。",
				ToString(deduced)
			);
		} else { deduced = *typeOpt; }

		// 不明の場合はスロットだけ作成
		const AssetID id = FindOrCreateSlotByPath(path, deduced);
		Node&         n  = mNodes[id];

		for (const auto& l : mLoaders) {
			auto t = ASSET_TYPE::UNKNOWN;
			if (!l->CanLoad(path, &t)) { continue; }
			if (typeOpt.has_value() && t != deduced) { continue; }

			LoadResult r     = l->Load(path);
			n.payload        = std::move(r.payload);
			n.meta.type      = deduced == ASSET_TYPE::UNKNOWN ? t : deduced;
			n.meta.loaded    = true;
			n.meta.fileStamp = r.stamp;

			// 名前の解決
			if (!r.resolveName.empty()) {
				n.meta.name            = r.resolveName;
				mNameToID[n.meta.name] = id;
			}

			// 依存の設定
			SetDependencies(id, r.dependencies);
			return id;
		}

		n.meta.loaded = false;
		return id;
	}

	template <class T>
	AssetID AssetManager::CreateRuntimeAsset(
		const ASSET_TYPE            type, std::string name, T&& payload,
		const std::vector<AssetID>& dependencies
	) {
		std::scoped_lock lock(mMutex);
		const AssetID    id    = AllocateID();
		Node&            n     = mNodes[id];
		n.meta.type            = type;
		n.meta.name            = std::move(name);
		n.meta.loaded          = true;
		n.payload              = std::forward<T>(payload);
		mNameToID[n.meta.name] = id;

		SetDependencies(id, dependencies);
		return id;
	}

	template
	AssetID AssetManager::CreateRuntimeAsset<TextureAssetData>(
		ASSET_TYPE,
		std::string,
		TextureAssetData&&,
		const std::vector<AssetID>&


	
	);

	void AssetManager::AddRef(const AssetID id) {
		std::scoped_lock lock(mMutex);
		if (id == kInvalidAssetID || id >= mNextID) { return; }
		mNodes[id].meta.strongRefs++;
	}

	void AssetManager::Release(const AssetID id) {
		std::scoped_lock lock(mMutex);
		if (id == kInvalidAssetID || id >= mNextID) { return; }
		auto& n = mNodes[id].meta;
		if (n.strongRefs > 0) { n.strongRefs--; }
	}

	void AssetManager::SetDependencies(
		AssetID id, const std::vector<AssetID>& dependencies
	) {
		if (id == kInvalidAssetID || id >= mNextID) { return; }
		Node& n = mNodes[id];

		for (const auto dep : n.dependencies) {
			if (dep == kInvalidAssetID || dep >= mNextID) { continue; }
			auto& vec = mNodes[dep].dependents;
			std::erase(vec, id);
		}

		n.dependencies = dependencies;

		// 依存先のdependentsを更新
		for (const auto dep : n.dependencies) {
			if (dep == kInvalidAssetID || dep >= mNextID) { continue; }
			auto& depBy = mNodes[dep].dependents;
			if (std::ranges::find(depBy, id) == depBy.end()) {
				depBy.emplace_back(id);
			}
		}
	}

	const AssetMetaData& AssetManager::Meta(const AssetID id) const {
		std::scoped_lock lock(mMutex);
		UASSERT(id < mNextID);
		return mNodes[id].meta;
	}

	template <class T>
	const T* AssetManager::Get(const AssetID id) const {
		std::scoped_lock lock(mMutex);
		if (id == kInvalidAssetID || id >= mNextID) { return nullptr; }
		const auto& v = mNodes[id].payload;
		return std::get_if<T>(&const_cast<AssetPayload&>(v));
	}

	template const TextureAssetData*          AssetManager::Get(AssetID) const;
	template const ShaderSourceAssetData*     AssetManager::Get(AssetID) const;
	template const MeshAssetData*             AssetManager::Get(AssetID) const;
	template const ShaderProgramAssetData*    AssetManager::Get(AssetID) const;
	template const MaterialAssetData*         AssetManager::Get(AssetID) const;
	template const MaterialInstanceAssetData* AssetManager::Get(AssetID) const;
	template const PostFxChainAssetData*      AssetManager::Get(AssetID) const;

	const std::vector<AssetID>& AssetManager::GetDependencies(
		const AssetID id
	) const {
		std::scoped_lock lock(mMutex);
		UASSERT(id < mNextID);
		return mNodes[id].dependencies;
	}

	const std::vector<AssetID>& AssetManager::GetDependents(
		const AssetID id
	) const {
		std::scoped_lock lock(mMutex);
		UASSERT(id < mNextID);
		return mNodes[id].dependents;
	}

	bool AssetManager::Reload(const AssetID id) {
		std::scoped_lock lock(mMutex);

		// idが無効か?
		if (id == kInvalidAssetID || id >= mNextID) { return false; }

		Node& n = mNodes[id];

		// ソースパスが空か?
		if (n.meta.sourcePath.empty()) { return false; }

		// ローダーを探す
		for (const auto& l : mLoaders) {
			auto t = ASSET_TYPE::UNKNOWN;
			// このローダーで読めるか?
			if (!l->CanLoad(n.meta.sourcePath, &t)) { continue; }
			// アセットタイプが違うか?
			if (t != n.meta.type && n.meta.type != ASSET_TYPE::UNKNOWN) {
				continue;
			}

			// 再読み込み
			LoadResult r     = l->Load(n.meta.sourcePath);
			n.payload        = std::move(r.payload);
			n.meta.loaded    = true;
			n.meta.fileStamp = r.stamp;
			n.meta.version++;

			// 依存の設定
			SetDependencies(id, r.dependencies);

			// コールバックの呼び出し
			const auto callbacks = mReloadCallbacks;
			for (auto& cb : callbacks) { cb(id); }
			return true;
		}

		return false;
	}

	void AssetManager::RegisterReload(ReloadCallback callback) {
		std::scoped_lock lock(mMutex);
		mReloadCallbacks.emplace_back(std::move(callback));
	}

	size_t AssetManager::UnloadUnused() {
		std::scoped_lock lock(mMutex);
		size_t           freed = 0;
		for (AssetID id = 1; id < mNextID; ++id) {
			Node& n = mNodes[id];
			// ロードされていないか?
			if (!n.meta.loaded) { continue; }
			// 参照されているか?
			if (n.meta.strongRefs > 0) { continue; }

			// 依存されているか?
			bool needed = false;
			for (const auto depBy : n.dependents) {
				if (depBy < mNextID && mNodes[depBy].meta.strongRefs > 0) {
					needed = true;
					break;
				}
			}

			// 依存されているならスキップ
			if (needed) { continue; }

			// アンロード
			n.payload     = std::monostate{};
			n.meta.loaded = false;
			freed++;
		}
		return freed;
	}

	AssetID AssetManager::FindByPath(const std::string_view path) const {
		std::scoped_lock lock(mMutex);
		const auto       it = mPathToID.find(std::string(path));
		return it != mPathToID.end() ? it->second : kInvalidAssetID;
	}

	AssetID AssetManager::FindByName(const std::string_view name) const {
		std::scoped_lock lock(mMutex);
		const auto       it = mNameToID.find(std::string(name));
		return it != mNameToID.end() ? it->second : kInvalidAssetID;
	}

	std::vector<AssetID> AssetManager::AllAssets() const {
		std::scoped_lock     lock(mMutex);
		std::vector<AssetID> ids;
		ids.reserve(mNextID - 1);
		for (AssetID id = 1; id < mNextID; ++id) { ids.emplace_back(id); }
		return ids;
	}

	AssetID AssetManager::AllocateID() {
		// ノードのサイズが足りない場合は拡張
		if (mNextID >= mNodes.size()) { mNodes.resize(mNextID + 64); }
		return mNextID++;
	}

	AssetID AssetManager::FindOrCreateSlotByPath(
		const std::string& path, const ASSET_TYPE type
	) {
		const auto normalized = StrUtil::NormalizePath(path);
		const auto it         = mPathToID.find(normalized);
		if (it != mPathToID.end()) { return it->second; }

		const AssetID id      = AllocateID();
		mPathToID[normalized] = id;

		namespace fs = std::filesystem;

		Node& node           = mNodes[id];
		node.meta.type       = type;
		node.meta.sourcePath = normalized;
		node.meta.loaded     = false;
		node.meta.name       = fs::path(normalized).filename().string();

		mNameToID[node.meta.name] = id;
		return id;
	}

	void AssetManager::RebuildDependents(AssetID id) {
		std::scoped_lock lock(mMutex);
		if (id == kInvalidAssetID || id >= mNextID) { return; }

		// とりあえず全ノードからidを取り除く
		for (AssetID i = 1; i < mNextID; ++i) {
			auto& depBy = mNodes[i].dependents;
			if (!depBy.empty()) { std::erase(depBy, id); }
		}

		// idの依存を見て各depの依存に追加
		for (const AssetID d : mNodes[id].dependencies) {
			if (d == kInvalidAssetID || d >= mNextID) {
				auto& depBy = mNodes[d].dependents;
				if (std::ranges::find(depBy, id) == depBy.end()) {
					depBy.emplace_back(id);
				}
			}
		}
	}

	void AssetManager::RebuildAllDependents() {
		std::scoped_lock lock(mMutex);
		for (AssetID i = 1; i < mNextID; ++i) { mNodes[i].dependents.clear(); }

		for (AssetID i = 1; i < mNextID; ++i) {
			for (const AssetID d : mNodes[i].dependencies) {
				if (d == kInvalidAssetID || d >= mNextID) { continue; }
				mNodes[d].dependents.emplace_back(i);
			}
		}
	}

	std::string AssetManager::NormalizePath(std::string path) {
		for (auto& c : path) { if (c == '\\') { c = '/'; } }
		return path;
	}
}
