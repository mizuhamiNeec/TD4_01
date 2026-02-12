#include "D3D12Util.h"

#include <stdexcept>

#include "engine/platform/WindowsUtils.h"

namespace Unnamed::Rhi {
	void Throw(const HRESULT hr) {
		if (FAILED(hr)) {
			throw std::runtime_error(
				"D3D12 HRESULT failed: " +
				WindowsUtils::GetHresultMessage(hr)
			);
		}
	}

	DXGI_FORMAT ToDxgiFormat(const TEXTURE_FORMAT format) {
		switch (format) {
			case TEXTURE_FORMAT::R8G8B8A8_UNORM: return
					DXGI_FORMAT_R8G8B8A8_UNORM;

			case TEXTURE_FORMAT::R10G10B10A2_UNORM: return
					DXGI_FORMAT_R10G10B10A2_UNORM;
			default: return DXGI_FORMAT_R8G8B8A8_UNORM;
		}
	}

	std::vector<uint8_t> LoadFileBytes(const wchar_t* path) {
		std::vector<uint8_t> data;
		FILE*                fp = nullptr;
		_wfopen_s(&fp, path, L"rb");
		if (!fp) { return data; }
		fseek(fp, 0, SEEK_END);
		const long size = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		data.resize(static_cast<size_t>(size));
		fread(data.data(), 1, data.size(), fp);
		fclose(fp);
		return data;
	}
}
