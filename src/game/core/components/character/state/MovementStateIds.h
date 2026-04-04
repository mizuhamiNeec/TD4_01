#pragma once

#include <string_view>

namespace Unnamed::MovementStateIds {
	/// @brief 基本移動の空中状態IDです。
	inline constexpr std::string_view Air = "AirMove";

	/// @brief 基本移動の地上状態IDです。
	inline constexpr std::string_view Ground = "GroundMove";

	/// @brief デバッグ用ノークリップ状態IDです。
	inline constexpr std::string_view Noclip = "NoclipMove";

	/// @brief Parkour拡張の空中状態IDです。
	inline constexpr std::string_view ParkourAir = "ParkourAirMove";

	/// @brief Parkour拡張の地上状態IDです。
	inline constexpr std::string_view ParkourGround = "ParkourGroundMove";

	/// @brief Parkour拡張のしゃがみ移動状態IDです。
	inline constexpr std::string_view ParkourDuck = "ParkourDuckMove";

	/// @brief Parkour拡張のスライド状態IDです。
	inline constexpr std::string_view ParkourSlide = "ParkourSlideMove";

	/// @brief Parkour拡張のウォールラン状態IDです。
	inline constexpr std::string_view ParkourWallRun = "ParkourWallRunMove";

	/// @brief Parkour拡張のスピードボルト状態IDです。
	inline constexpr std::string_view ParkourSpeedVault = "ParkourSpeedVaultMove";

	/// @brief Parkour拡張のブリンク状態IDです。
	inline constexpr std::string_view ParkourBlink = "ParkourBlinkMove";
}
