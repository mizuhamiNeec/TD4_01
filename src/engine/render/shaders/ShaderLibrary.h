#pragma once
#include <cstdint>
#include <filesystem>
#include <unordered_map>
#include <vector>

#include "ShaderKey.h"

namespace Unnamed {
	namespace Rhi {
		class DxcShaderCompiler;
	}

	class AssetManager;
}

namespace Unnamed::Render {
	struct ShaderDxil {
		std::vector<uint8_t> bytes;
	};

	class ShaderLibrary {
	public:
		ShaderLibrary(
			AssetManager& assetManager, Rhi::DxcShaderCompiler& dxcCompiler
		);

		const ShaderDxil& GetOrCreateDxil(const ShaderKey& key);

		void InvalidateByShaderSource(AssetID shaderSourceId);

		void InvalidateAll();
		void SetCacheDirectory(std::filesystem::path dir);

	private:
		[[nodiscard]] std::filesystem::path GetDxilCachePath(
			const ShaderKey& key
		) const;
		[[nodiscard]] uint64_t ComputeDerivedHash(const ShaderKey& key) const;

		static std::vector<std::wstring> BuildDxcArgs(const ShaderKey& key);

		static std::vector<uint8_t> ReadFileBytes(
			const std::filesystem::path& path
		);
		static void WriteFileBytes(
			const std::filesystem::path& path, const std::vector<uint8_t>& bytes
		);

		AssetManager&           mAssetManager;
		Rhi::DxcShaderCompiler& mDxcShaderCompiler;

		std::filesystem::path mCacheDir = {};

		std::unordered_map<ShaderKey, ShaderDxil, ShaderKeyHash> mRuntimeCache;

		std::unordered_map<AssetID, std::vector<ShaderKey>> mReverse;
	};
}
