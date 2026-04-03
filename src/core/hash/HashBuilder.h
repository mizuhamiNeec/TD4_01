#pragma once

#include <cstdint>
#include <functional>
#include <type_traits>

namespace Unnamed {
	/// @brief 汎用ハッシュ生成クラス
	/// @details 複数の値を順番に組み合わせ、安定したハッシュ値を生成する。
	class HashBuilder final {
	public:
		/// @brief デフォルトシード（0）でハッシュ生成器を初期化する。
		constexpr HashBuilder() noexcept = default;

		/// @brief シード値を指定してハッシュ生成器を初期化する。
		/// @param seed 初期シード値
		explicit constexpr HashBuilder(const size_t seed) noexcept :
			mSeed(seed) {}

		/// @brief `std::hash` で値をハッシュ化して現在のシードに混ぜる。
		/// @tparam T ハッシュ化対象の型
		/// @param value ハッシュ化対象の値
		template <typename T>
		void AddValue(const T& value) {
			AddHashed(std::hash<T>{}(value));
		}

		/// @brief 列挙値を基底型に変換して現在のシードに混ぜる。
		/// @tparam T 列挙型
		/// @param value ハッシュ化対象の列挙値
		template <typename T>
		void AddEnum(const T value) {
			static_assert(std::is_enum_v<T>);
			using UnderlyingType = std::underlying_type_t<T>;
			AddValue(static_cast<UnderlyingType>(value));
		}

		/// @brief ポインタ値を現在のシードに混ぜる。
		/// @tparam T ポインタの指す型
		/// @param value ハッシュ化対象のポインタ
		template <typename T>
		void AddPointer(const T* value) noexcept {
			AddHashed(reinterpret_cast<size_t>(value));
		}

		/// @brief 既にハッシュ化済みの値を現在のシードに混ぜる。
		/// @param value 既にハッシュ化済みの値
		void AddHashed(const size_t value) noexcept {
			mSeed = Combine(mSeed, value);
		}

		/// @brief 現在のハッシュ値を返す。
		/// @return 現在のハッシュ値
		[[nodiscard]] constexpr size_t Value() const noexcept {
			return mSeed;
		}

		/// @brief 2つの `size_t` ハッシュ値を組み合わせる。
		/// @param seed ベースとなるハッシュ値
		/// @param value 混ぜるハッシュ値
		/// @return 組み合わせ結果
		[[nodiscard]] static constexpr size_t Combine(
			const size_t seed, const size_t value
		) noexcept {
			return seed ^ (value + kMixConstant + (seed << 6) + (seed >> 2));
		}

		/// @brief 2つの64ビットハッシュ値を組み合わせる。
		/// @param seed ベースとなるハッシュ値
		/// @param value 混ぜるハッシュ値
		/// @return 組み合わせ結果
		[[nodiscard]] static constexpr uint64_t Combine64(
			const uint64_t seed, const uint64_t value
		) noexcept {
			return seed ^ (value + kMixConstant + (seed << 6) + (seed >> 2));
		}

	private:
		static constexpr uint64_t kMixConstant = 0x9e3779b97f4a7c15ull;

		size_t mSeed = 0;
	};
}
