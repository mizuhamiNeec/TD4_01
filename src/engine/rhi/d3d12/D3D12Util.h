#pragma once
#include <dxgiformat.h>
#include <intsafe.h>
#include <vector>

#include "engine/rhi/RhiTypes.h"

namespace Unnamed::Rhi {
	void        Throw(HRESULT hr);
	DXGI_FORMAT ToDxgiFormat(TEXTURE_FORMAT format);

	std::vector<uint8_t> LoadFileBytes(const wchar_t* path);
}
