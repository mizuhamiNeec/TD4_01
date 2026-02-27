#pragma once
#include <string>
#include <vector>

#include <wrl/client.h>

#include <dxcapi.h>

namespace Unnamed::Rhi {
	class DxcShaderCompiler {
	public:
		DxcShaderCompiler();
		bool Initialize();

		bool CompileToFileDXIL(
			const std::wstring&              sourcePath,
			const std::wstring&              entryPoint,
			const std::wstring&              targetProfile,
			const std::vector<std::wstring>& includeDirs,
			const std::vector<std::wstring>& extraArgs,
			const std::wstring&              outputPath
		);

	private:
		bool CompileInternal(
			const std::wstring&              sourcePath,
			const std::wstring&              entryPoint,
			const std::wstring&              targetProfile,
			const std::vector<std::wstring>& includeDirs,
			const std::vector<std::wstring>& extraArgs,
			const std::wstring&              outputPath
		);

		Microsoft::WRL::ComPtr<IDxcUtils>          mUtils;
		Microsoft::WRL::ComPtr<IDxcCompiler3>      mCompiler;
		Microsoft::WRL::ComPtr<IDxcIncludeHandler> mIncludeHandler;
	};
}
