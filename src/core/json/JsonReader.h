#pragma once
#include <fstream>
#include <json.hpp>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/math/Vec3.h"
#include "core/math/Vec4.h"

namespace Unnamed {
	/// @brief JSON読み込みクラス
	/// @details JSON形式のデータを読み込み、型安全にアクセスするためのラッパークラスです
	class JsonReader final {
	public:
		/// @brief デフォルトコンストラクタ
		JsonReader() = default;


		/// @brief JSONオブジェクトから初期化するコンストラクタ
		/// @param root JSONルートオブジェクト
		explicit JsonReader(const nlohmann::json& root)
			: mStorage(std::make_shared<nlohmann::json>(root)),
			  mNode(mStorage.get()),
			  mValid(true) {}

		/// @brief ファイルパスから読み込むコンストラクタ
		/// @param path JSONファイルのパス
		explicit JsonReader(const std::string& path) {
			std::ifstream ifs(path);
			if (!ifs) {
				mValid = false;
				return;
			}
			try {
				mStorage = std::make_shared<nlohmann::json>();
				ifs >> *mStorage;
				mNode  = mStorage.get();
				mValid = true;
			} catch (...) {
				mStorage.reset();
				mNode  = nullptr;
				mValid = false;
			}
		}

		[[nodiscard]] bool Valid() const {
			return mValid && mNode;
		}

		[[nodiscard]] bool Has(const std::string_view& key) const {
			return mNode && mNode->is_object() && mNode->contains(key);
		}

		// operator[] (オブジェクトキー)
		JsonReader operator[](const std::string_view& key) const {
			if (!Has(key)) {
				return {};
			}
			return JsonReader(mStorage, &(*mNode)[std::string(key)], mValid);
		}

		// operator[] (配列インデックス)
		JsonReader operator[](const size_t index) const {
			if (!mNode || !mNode->is_array() || index >= mNode->size()) {
				return {};
			}
			return JsonReader(mStorage, &(*mNode)[index], mValid);
		}

		// 配列サイズ
		[[nodiscard]] size_t Size() const {
			if (!mNode || !mNode->is_array()) {
				return 0;
			}
			return mNode->size();
		}

		[[nodiscard]] bool GetBool() const {
			if (!mNode) {
				return false;
			}
			if (mNode->is_boolean()) {
				return mNode->get<bool>();
			}
			return false;
		}

		[[nodiscard]] std::string GetString() const {
			if (!mNode) {
				return {};
			}
			if (mNode->is_string()) {
				return mNode->get<std::string>();
			}
			// 数値を文字列化など最低限のフォールバック
			if (mNode->is_number()) {
				return mNode->dump();
			}
			return {};
		}

		[[nodiscard]] float GetFloat() const {
			if (!mNode) {
				return 0.f;
			}
			if (mNode->is_number_float() || mNode->is_number_integer() || mNode
			    ->
			    is_number_unsigned()) {
				return mNode->get<float>();
			}
			return 0.f;
		}

		[[nodiscard]] int GetInt() const {
			if (!mNode) {
				return 0;
			}
			if (mNode->is_number_integer() || mNode->is_number_unsigned()) {
				return mNode->get<int>();
			}
			if (mNode->is_number_float()) {
				return static_cast<int>(mNode->get<float>());
			}
			return 0;
		}

		[[nodiscard]] Vec3 GetVec3() const {
			if (!mNode || !mNode->is_array() || mNode->size() != 3) {
				return Vec3::zero;
			}
			try {
				return Vec3(
					mNode->at(0).get<float>(),
					mNode->at(1).get<float>(),
					mNode->at(2).get<float>()
				);
			} catch (...) {
				return Vec3::zero;
			}
		}

		[[nodiscard]] Vec4 GetVec4() const {
			if (!mNode || !mNode->is_array() || mNode->size() != 4) {
				return Vec4::zero;
			}
			try {
				return Vec4(
					mNode->at(0).get<float>(),
					mNode->at(1).get<float>(),
					mNode->at(2).get<float>(),
					mNode->at(3).get<float>()
				);
			} catch (...) {
				return Vec4::zero;
			}
		}

		[[nodiscard]] uint64_t GetUint64() const {
			// 互換: 失敗時は 0 を返す
			return TryGetUint64().value_or(0ull);
		}

		/// @brief uint64 として厳密に取得する(変換できない場合は nullopt)
		/// @details 許容: unsigned整数, >=0 の signed整数, 0以上の float(整数部のみ), 10進文字列
		[[nodiscard]] std::optional<uint64_t> TryGetUint64() const {
			if (!mNode) {
				return std::nullopt;
			}

			try {
				if (mNode->is_number_unsigned()) {
					return mNode->get<uint64_t>();
				}
				if (mNode->is_number_integer()) {
					const auto s = mNode->get<int64_t>();
					if (s < 0) {
						return std::nullopt;
					}
					return static_cast<uint64_t>(s);
				}
				if (mNode->is_number_float()) {
					const auto d = mNode->get<double>();
					if (!(d >= 0.0)) {
						return std::nullopt;
					}
					if (d > static_cast<double>((std::numeric_limits<
						    uint64_t>::max)())) {
						return std::nullopt;
					}
					return static_cast<uint64_t>(d);
				}
				if (mNode->is_string()) {
					const auto&        s = mNode->get_ref<const std::string&>();
					size_t             idx = 0;
					unsigned long long v = std::stoull(s, &idx, 10);
					if (idx != s.size()) {
						return std::nullopt;
					}
					return v;
				}
			} catch (...) {
				return std::nullopt;
			}
			return std::nullopt;
		}

		[[nodiscard]] JsonReader GetArray() const {
			return *this;
		}

		template <typename T>
		std::optional<T> Read(const std::string_view& key) const {
			if (!Has(key)) {
				return std::nullopt;
			}
			try {
				return (*mNode)[std::string(key)].get<T>();
			} catch (...) {
				return std::nullopt;
			}
		}

		/// @brief uint64 をキーから取得する(失敗時は nullopt)
		[[nodiscard]] std::optional<uint64_t> ReadUint64(
			const std::string_view& key
		) const {
			const JsonReader v = (*this)[key];
			if (!v.Valid()) {
				return std::nullopt;
			}
			return v.TryGetUint64();
		}

		template <typename T>
		std::vector<T> ReadArray(const std::string_view& key) const {
			std::vector<T> out;
			if (!Has(key)) {
				return out;
			}
			const auto& j = (*mNode)[std::string(key)];
			if (!j.is_array()) {
				return out;
			}
			out.reserve(j.size());
			for (auto& v : j) {
				try {
					out.emplace_back(v.get<T>());
				} catch (...) {
					/* skip */
				}
			}
			return out;
		}

	private:
		JsonReader(
			std::shared_ptr<nlohmann::json> storage,
			const nlohmann::json*           node, const bool valid
		)
			: mStorage(std::move(storage)),
			  mNode(node),
			  mValid(valid && node != nullptr) {}

		std::shared_ptr<nlohmann::json> mStorage;
		const nlohmann::json*           mNode{nullptr};
		bool                            mValid{false};
	};
}
