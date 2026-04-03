#pragma once

#include <cstdint>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace Unnamed {
	class BinaryWriter {
	public:
		explicit BinaryWriter(const std::string& path);

		/// @brief ファイルが正常に開けているか
		/// @return ファイルが正常に開けているならtrue、開けなかった場合はfalse
		[[nodiscard]] bool IsOpen() const;

		/// @brief ストリームが正常な状態か
		/// @return ストリームが正常な状態ならtrue、エラーが発生している場合はfalse
		[[nodiscard]] bool Good() const;

		/// @brief 現在の書き込み位置を取得する
		/// @return 書き込み位置のオフセット（バイト単位）
		[[nodiscard]] uint64_t Tell();

		/// @brief 書き込み位置を移動する
		/// @param offset 移動先のオフセット（バイト単位）
		/// @return 書き込み位置の移動に成功したか
		bool Seek(const uint64_t offset);

		/// @brief バイト列を書き込む
		/// @param data 書き込むデータの先頭ポインタ
		/// @param sizeBytes 書き込むデータのサイズ（バイト単位）
		/// @return 書き込みに成功したか
		bool WriteBytes(const void* data, const size_t sizeBytes);

		/// @brief 標準レイアウトな値をバイト列として書き込む。
		/// @tparam T 書き込む値の型。標準レイアウトである必要がある。
		/// @param value 書き込む値
		/// @return 書き込みに成功したか
		template <typename T>
		bool WritePod(const T& value);

		/// @brief 文字列を書き込む。文字列の長さはuint32_tで表現できる必要がある。
		/// @param text 書き込む文字列
		/// @return 書き込みに成功したか
		bool WriteString(const std::string_view text);

		/// @brief 標準レイアウトな配列を連続バイト列として書き込む。
		/// @tparam T 書き込む要素型。標準レイアウトである必要がある。
		/// @param values 要素先頭ポインタ
		/// @param count 要素数
		/// @return 書き込みに成功したか
		template <typename T>
		bool WriteArray(const T* values, const size_t count);

		/// @brief 標準レイアウトなベクターを連続バイト列として書き込む。ベクターのサイズはuint32_tで表現できる必要がある。
		/// @tparam T 書き込む要素型。標準レイアウトである必要がある。
		/// @param values 書き込むベクター
		/// @return 書き込みに成功したか
		template <typename T>
		bool WriteVector(const std::vector<T>& values);

		/// @brief ストリームをフラッシュする
		/// @return フラッシュ後のストリームの状態が正常ならtrue、エラーが発生している場合はfalse
		bool Flush();

	private:
		std::ofstream mStream;
	};
}
