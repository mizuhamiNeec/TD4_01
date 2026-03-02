#pragma once
#include <cstdint>

namespace Unnamed {
	struct FileStamp {
		int64_t  lastWriteTicks = 0;
		uint64_t sizeInBytes    = 0;
	};
}
