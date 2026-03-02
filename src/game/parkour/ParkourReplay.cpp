#include "ParkourReplay.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <fstream>

#include <json.hpp>

namespace Unnamed {
	namespace {
		constexpr uint32_t kReplayVersion   = 2;
		constexpr uint32_t kDefaultTickRate = 66;

		float ReadFloatOrDefault(
			const nlohmann::json& source,
			const char*           key,
			const float           fallback
		) {
			if (!source.contains(key) || !source[key].is_number()) {
				return fallback;
			}
			return source[key].get<float>();
		}
	}

	bool ParkourReplayManager::LoadClipFromFile(
		const std::string& path, ReplayClip& outClip
	) const {
		std::ifstream stream(path);
		if (!stream.is_open()) { return false; }

		nlohmann::json root;
		try {
			stream >> root;
		} catch (const std::exception&) { return false; }

		if (!root.is_object() || !root.contains("frames") ||
		    !root["frames"].is_array()) {
			return false;
		}

		outClip.version  = root.value("version", kReplayVersion);
		outClip.tickRate = root.value("tickRate", kDefaultTickRate);
		outClip.frames.clear();
		outClip.frames.reserve(root["frames"].size());

		for (const auto& jsonFrame : root["frames"]) {
			if (!jsonFrame.is_object()) { continue; }

			ReplayUserCmdFrame frame = {};
			frame.moveX        = ReadFloatOrDefault(jsonFrame, "moveX", 0.0f);
			frame.moveY        = ReadFloatOrDefault(jsonFrame, "moveY", 0.0f);
			frame.buttons      = jsonFrame.value("buttons", ReplayButton_None);
			frame.viewYawDeg   = ReadFloatOrDefault(
				jsonFrame, "viewYawDeg", 0.0f
			);
			frame.viewPitchDeg = ReadFloatOrDefault(
				jsonFrame, "viewPitchDeg", 0.0f
			);
			frame.hasVaultState   = jsonFrame.value("hasVaultState", false);
			frame.isSpeedVaulting = jsonFrame.value("isSpeedVaulting", false);
			frame.vaultProgress = ReadFloatOrDefault(
				jsonFrame, "vaultProgress", 0.0f
			);
			frame.vaultStartX = ReadFloatOrDefault(
				jsonFrame, "vaultStartX", 0.0f
			);
			frame.vaultStartY = ReadFloatOrDefault(
				jsonFrame, "vaultStartY", 0.0f
			);
			frame.vaultStartZ = ReadFloatOrDefault(
				jsonFrame, "vaultStartZ", 0.0f
			);
			frame.vaultApexX = ReadFloatOrDefault(
				jsonFrame, "vaultApexX", 0.0f
			);
			frame.vaultApexY = ReadFloatOrDefault(
				jsonFrame, "vaultApexY", 0.0f
			);
			frame.vaultApexZ = ReadFloatOrDefault(
				jsonFrame, "vaultApexZ", 0.0f
			);
			frame.vaultEndX = ReadFloatOrDefault(
				jsonFrame, "vaultEndX", 0.0f
			);
			frame.vaultEndY = ReadFloatOrDefault(
				jsonFrame, "vaultEndY", 0.0f
			);
			frame.vaultEndZ = ReadFloatOrDefault(
				jsonFrame, "vaultEndZ", 0.0f
			);
			outClip.frames.emplace_back(frame);
		}

		return !outClip.frames.empty();
	}

	ParkourReplayManager::ReplayClip
	ParkourReplayManager::BuildDefaultTitleDemoClip() const {
		ReplayClip clip = {};
		clip.version    = kReplayVersion;
		clip.tickRate   = kDefaultTickRate;
		clip.frames.reserve(static_cast<size_t>(clip.tickRate) * 12u);

		const float tickRateF = static_cast<float>(clip.tickRate);
		for (uint32_t tick = 0; tick < clip.tickRate * 12u; ++tick) {
			const float t = static_cast<float>(tick) / tickRateF;

			ReplayUserCmdFrame frame = {};
			frame.moveX        = std::clamp(
				0.45f * std::sin(t * 1.25f), -1.0f, 1.0f
			);
			frame.moveY        = 1.0f;
			frame.viewYawDeg   = 10.0f + 12.0f * std::sin(t * 0.35f);
			frame.viewPitchDeg = -4.0f + 1.5f * std::sin(t * 0.6f);

			if (tick == clip.tickRate * 2u || tick == clip.tickRate * 5u) {
				frame.buttons |= ReplayButton_Jump;
			}
			if (tick == clip.tickRate * 8u) {
				frame.buttons |= ReplayButton_Blink;
			}
			if (tick >= clip.tickRate * 9u && tick < clip.tickRate * 10u) {
				frame.buttons |= ReplayButton_Crouch;
			}

			clip.frames.emplace_back(frame);
		}

		return clip;
	}
}
