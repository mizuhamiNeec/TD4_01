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

	TEXTURE_FORMAT ToTextureFormat(const DXGI_FORMAT format) {
		switch (format) {
			case DXGI_FORMAT_R8G8B8A8_UNORM: return
					TEXTURE_FORMAT::R8G8B8A8_UNORM;

			case DXGI_FORMAT_R10G10B10A2_UNORM: return
					TEXTURE_FORMAT::R10G10B10A2_UNORM;
			default: return TEXTURE_FORMAT::R8G8B8A8_UNORM;
		}
	}
}
