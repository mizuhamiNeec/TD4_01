#pragma once
#include <chrono>

namespace Unnamed {
	struct FileStamp {
		std::chrono::system_clock::time_point lastWrite   = {};
		uint64_t                              sizeInBytes = 0;
	};
}
