#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "core/math/Vec3.h"

namespace Unnamed {
	class Entity;

	/// @brief Floatプロパティのアクセサです。
	struct SequenceFloatAccessor final {
		std::function<bool(Entity&, float&)>       get = nullptr;
		std::function<bool(Entity&, const float&)> set = nullptr;
	};

	/// @brief Boolプロパティのアクセサです。
	struct SequenceBoolAccessor final {
		std::function<bool(Entity&, bool&)>       get = nullptr;
		std::function<bool(Entity&, const bool&)> set = nullptr;
	};

	/// @brief Vec3プロパティのアクセサです。
	struct SequenceVec3Accessor final {
		std::function<bool(Entity&, Vec3&)>       get = nullptr;
		std::function<bool(Entity&, const Vec3&)> set = nullptr;
	};

	/// @brief プロパティアクセスのレジストリです。
	class SequencePropertyAccessorRegistry final {
	public:
		/// @brief コンストラクタです。
		SequencePropertyAccessorRegistry();

		/// @brief Floatアクセサを登録します。
		void RegisterFloat(
			std::string componentStableName,
			std::string propertyPath,
			SequenceFloatAccessor accessor
		);

		/// @brief Boolアクセサを登録します。
		void RegisterBool(
			std::string componentStableName,
			std::string propertyPath,
			SequenceBoolAccessor accessor
		);

		/// @brief Vec3アクセサを登録します。
		void RegisterVec3(
			std::string componentStableName,
			std::string propertyPath,
			SequenceVec3Accessor accessor
		);

		/// @brief Floatアクセサを検索します。
		[[nodiscard]] const SequenceFloatAccessor* FindFloat(
			std::string_view componentStableName,
			std::string_view propertyPath
		) const;

		/// @brief Boolアクセサを検索します。
		[[nodiscard]] const SequenceBoolAccessor* FindBool(
			std::string_view componentStableName,
			std::string_view propertyPath
		) const;

		/// @brief Vec3アクセサを検索します。
		[[nodiscard]] const SequenceVec3Accessor* FindVec3(
			std::string_view componentStableName,
			std::string_view propertyPath
		) const;

	private:
		[[nodiscard]] static std::string BuildKey(
			std::string_view componentStableName,
			std::string_view propertyPath
		);

		std::unordered_map<std::string, SequenceFloatAccessor> mFloatAccessors = {};
		std::unordered_map<std::string, SequenceBoolAccessor>  mBoolAccessors  = {};
		std::unordered_map<std::string, SequenceVec3Accessor>  mVec3Accessors  = {};
	};
}
