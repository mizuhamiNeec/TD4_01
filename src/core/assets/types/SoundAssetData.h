#pragma once

#include <cstdint>
#include <vector>

namespace Unnamed {
	struct SoundAssetData {
		uint16_t             formatTag             = 0;
		uint16_t             channels              = 0;
		uint32_t             sampleRate            = 0;
		uint32_t             averageBytesPerSecond = 0;
		uint16_t             blockAlign            = 0;
		uint16_t             bitsPerSample         = 0;
		std::vector<uint8_t> pcmData;
	};
}
