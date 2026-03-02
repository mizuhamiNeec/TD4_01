#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Unnamed {
	enum ReplayButtonFlags : uint32_t {
		ReplayButton_None   = 0u,
		ReplayButton_Jump   = 1u << 0,
		ReplayButton_Crouch = 1u << 1,
		ReplayButton_Blink  = 1u << 2,
	};

	struct ReplayUserCmdFrame {
		float    moveX        = 0.0f;
		float    moveY        = 0.0f;
		uint32_t buttons      = ReplayButton_None;
		float    viewYawDeg   = 0.0f;
		float    viewPitchDeg = 0.0f;
		bool     hasVaultState   = false;
		bool     isSpeedVaulting = false;
		float    vaultProgress   = 0.0f;
		float    vaultStartX     = 0.0f;
		float    vaultStartY     = 0.0f;
		float    vaultStartZ     = 0.0f;
		float    vaultApexX      = 0.0f;
		float    vaultApexY      = 0.0f;
		float    vaultApexZ      = 0.0f;
		float    vaultEndX       = 0.0f;
		float    vaultEndY       = 0.0f;
		float    vaultEndZ       = 0.0f;
	};

	class ParkourReplayManager {
	public:
		static constexpr std::string_view kDefaultTitleDemoPath =
			"./content/parkour/replay/title_demo.json";

		struct ReplayClip {
			uint32_t version  = 2;
			uint32_t tickRate = 66;
			std::vector<ReplayUserCmdFrame> frames;
		};

		bool LoadClipFromFile(const std::string& path, ReplayClip& outClip) const;
		ReplayClip BuildDefaultTitleDemoClip() const;
	};
}
