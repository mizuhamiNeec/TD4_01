#include "ShaderLibrary.h"

#include <fstream>

#include "core/UnnamedMacro.h"
#include "core/assets/AssetManager.h"
#include "core/assets/FileStamp.h"
#include "core/string/StrUtil.h"

#include "engine/rhi/DxcShaderCompiler.h"
#include "engine/unnamed/subsystem/console/Log.h"

namespace Unnamed::Render {
	static constexpr std::string_view kChannel      = "ShaderLibrary";
	static constexpr std::string_view kDxilCacheDir =
		"./content/core/shaders/compiled/";

	static uint64_t HashCombine64(uint64_t a, uint64_t b) {
		return a ^ b + 0x9e3779b97f4a7c15ull + (a << 6) + (a >> 2);
	}

	static uint64_t StampToU64(const FileStamp& stamp) {
		return HashCombine64(
			stamp.sizeInBytes,
			static_cast<uint64_t>(stamp.lastWrite.time_since_epoch().count())
		);
	}

	ShaderLibrary::ShaderLibrary(
		AssetManager& assetManager, Rhi::DxcShaderCompiler& dxcCompiler
	) : mAssetManager(assetManager),
	    mDxcShaderCompiler(dxcCompiler) {}

	const ShaderDxil& ShaderLibrary::GetOrCreateDxil(const ShaderKey& key) {
		if (const auto it = mRuntimeCache.find(key);
			it != mRuntimeCache.end()) { return it->second; }

		const auto dxilPath = GetDxilCachePath(key);
		ShaderDxil out      = {};

		if (std::filesystem::exists(dxilPath)) {
			out.bytes = ReadFileBytes(dxilPath);
		} else {
			const auto* src = mAssetManager.Get<ShaderSourceAssetData>(
				key.shaderSourceId
			);
			if (!src) {
				Error(
					kChannel, "ShaderSource asset payload missing: id={}",
					key.shaderSourceId
				);
				auto [it, _] = mRuntimeCache.emplace(key, ShaderDxil{});
				return it->second;
			}

			const std::wstring sourcePath(src->path.begin(), src->path.end());
			const std::wstring entry(key.entry.begin(), key.entry.end());
			const std::wstring profile(key.profile.begin(), key.profile.end());

			const std::vector<std::wstring> includeDirs;
			const auto                      extraArgs = BuildDxcArgs(key);

			mDxcShaderCompiler.Initialize();

			const bool ok = mDxcShaderCompiler.CompileToFileDXIL(
				sourcePath, entry, profile, includeDirs, extraArgs,
				dxilPath.wstring()
			);

			if (ok) { out.bytes = ReadFileBytes(dxilPath); }
		}

		mReverse[key.shaderSourceId].emplace_back(key);

		auto [insIt, _] = mRuntimeCache.emplace(key, std::move(out));
		return insIt->second;
	}

	void ShaderLibrary::InvalidateByShaderSource(AssetID shaderSourceId) {
		const auto it = mReverse.find(shaderSourceId);
		if (it == mReverse.end()) { return; }

		for (const auto& key : it->second) { mRuntimeCache.erase(key); }
		it->second.clear();
	}

	void ShaderLibrary::InvalidateAll() {
		mRuntimeCache.clear();
		mReverse.clear();
	}

	void ShaderLibrary::SetCacheDirectory(std::filesystem::path dir) {
		mCacheDir = std::move(dir);
	}

	std::filesystem::path ShaderLibrary::GetDxilCachePath(
		const ShaderKey& key
	) const {
		const uint64_t dh = ComputeDerivedHash(key);
		return mCacheDir / StrUtil::ToWString(
			       std::string(kDxilCacheDir) +
			       "shader_" +
			       std::to_string(key.shaderSourceId) +
			       "_" +
			       key.entry +
			       "_" +
			       key.profile +
			       "_" +
			       std::to_string(dh) +
			       ".dxil"
		       );
	}

	uint64_t ShaderLibrary::ComputeDerivedHash(const ShaderKey& key) const {
		uint64_t h = 0;

		const auto* src = mAssetManager.Get<ShaderSourceAssetData>(
			key.shaderSourceId
		);
		if (!src) {
			UASSERT(
				false && "シェーダーソースアセットのペイロードがありません"
			);
		}

		// ソースファイルのパスを正規化してハッシュに含める
		const std::string srcPath = StrUtil::NormalizePath(src->path);
		h = HashCombine64(h, std::hash<std::string>{}(srcPath));

		h = HashCombine64(h, std::hash<std::string>{}(key.entry));
		h = HashCombine64(h, std::hash<std::string>{}(key.profile));
		for (const auto& [name, value] : key.defines) {
			h = HashCombine64(h, std::hash<std::string>{}(name));
			h = HashCombine64(h, std::hash<std::string>{}(value));
		}

		const auto& meta = mAssetManager.Meta(key.shaderSourceId);
		h                = HashCombine64(
			h, StampToU64(meta.fileStamp)
		);

		for (
			const auto depId : mAssetManager.GetDependencies(key.shaderSourceId)
		) {
			const auto& depMeta = mAssetManager.Meta(depId);
			h = HashCombine64(h, depId);
			h = HashCombine64(h, StampToU64(depMeta.fileStamp));
		}

		return h;
	}

	std::vector<std::wstring> ShaderLibrary::BuildDxcArgs(
		const ShaderKey& key
	) {
		std::vector<std::wstring> args;
		constexpr int             baseArgReserve      = 16; // 予備容量
		constexpr int             entryProfileArgCost = 2;  // エントリポイントとプロファイル
		args.reserve(baseArgReserve + key.defines.size() * entryProfileArgCost);

		// defines
		for (const auto& [name, value] : key.defines) {
			std::wstring d = L"-D";
			std::wstring nv;
			nv.assign(name.begin(), name.end());
			nv += L"=";
			nv.append(value.begin(), value.end());
			args.emplace_back(d);
			args.emplace_back(nv);
		}

		args.emplace_back(L"-WX"); // 警告をエラーとして扱う
		args.emplace_back(L"-Zi"); // デバッグ情報を生成

#ifdef _DEBUG
		args.emplace_back(L"-Od"); // 最適化を外す
#else
		args.emplace_back(L"-O3"); // 最適化有効
#endif

		args.emplace_back(L"-Zpr"); // メモリレイアウトは行優先

		return args;
	}

	std::vector<uint8_t> ShaderLibrary::ReadFileBytes(
		const std::filesystem::path& path
	) {
		std::ifstream ifs(path, std::ios::binary);
		if (!ifs) return {};
		ifs.seekg(0, std::ios::end);
		const auto size = static_cast<size_t>(ifs.tellg());
		ifs.seekg(0, std::ios::beg);

		std::vector<uint8_t> bytes(size);
		if (size > 0) {
			ifs.read(
				reinterpret_cast<char*>(bytes.data()),
				static_cast<std::streamsize>(size)
			);
		}
		return bytes;
	}

	void ShaderLibrary::WriteFileBytes(
		const std::filesystem::path& path, const std::vector<uint8_t>& bytes
	) {
		std::filesystem::create_directories(path.parent_path());
		std::ofstream ofs(path, std::ios::binary);
		if (!ofs) return;
		ofs.write(
			reinterpret_cast<const char*>(bytes.data()),
			static_cast<std::streamsize>(bytes.size())
		);
	}
}
