#pragma once

#include <cstdint>
#include <type_traits>

namespace Unnamed {
	constexpr uint32_t MakeFourCC(
		const char a,
		const char b,
		const char c,
		const char d
	) {
		return static_cast<uint32_t>(static_cast<uint8_t>(a)) |
		       (static_cast<uint32_t>(static_cast<uint8_t>(b)) << 8u) |
		       (static_cast<uint32_t>(static_cast<uint8_t>(c)) << 16u) |
		       (static_cast<uint32_t>(static_cast<uint8_t>(d)) << 24u);
	}

	constexpr uint32_t kDemoBinaryMagic   = MakeFourCC('U', 'D', 'E', 'M');
	constexpr uint32_t kDemoBinaryVersion = 2u;

	constexpr uint32_t kDemoChunkHead = MakeFourCC('H', 'E', 'A', 'D');
	constexpr uint32_t kDemoChunkInit = MakeFourCC('I', 'N', 'I', 'T');
	constexpr uint32_t kDemoChunkCmds = MakeFourCC('C', 'M', 'D', 'S');
	constexpr uint32_t kDemoChunkSnap = MakeFourCC('S', 'N', 'A', 'P');

	struct DemoBinaryFileHeader {
		uint32_t magic      = kDemoBinaryMagic;
		uint32_t version    = kDemoBinaryVersion;
		uint32_t chunkCount = 0;
		uint32_t reserved   = 0;
	};

	struct DemoBinaryChunkHeader {
		uint32_t id   = 0;
		uint32_t size = 0;
	};

	struct DemoBinaryPackedPlayerCommand {
		uint64_t tick              = 0;
		uint64_t subjectEntityGuid = 0;
		uint8_t  commandType       = 0;
		uint8_t  flags             = 0;
		uint16_t reserved0         = 0;
		float    moveAxis[3]       = {};
		float    right[3]          = {};
		float    up[3]             = {};
		float    forward[3]        = {};
		float    viewYawDeg        = 0.0f;
		float    viewPitchDeg      = 0.0f;
	};

	static_assert(std::is_trivially_copyable_v<DemoBinaryFileHeader>);
	static_assert(std::is_trivially_copyable_v<DemoBinaryChunkHeader>);
	static_assert(std::is_trivially_copyable_v<DemoBinaryPackedPlayerCommand>);
}
