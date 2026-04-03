#include "BinaryWriter.h"

#include <bit>

namespace Unnamed {
	BinaryWriter::BinaryWriter(const std::string& path) : mStream(
		path, std::ios::binary | std::ios::trunc
	) {}

	bool BinaryWriter::IsOpen() const {
		return mStream.is_open();
	}

	bool BinaryWriter::Good() const {
		return static_cast<bool>(mStream);
	}

	uint64_t BinaryWriter::Tell() {
		return static_cast<uint64_t>(mStream.tellp());
	}

	bool BinaryWriter::Seek(const uint64_t offset) {
		mStream.seekp(static_cast<std::streamoff>(offset), std::ios::beg);
		return Good();
	}

	bool BinaryWriter::WriteBytes(const void* data, const size_t sizeBytes) {
		if (sizeBytes == 0) {
			return true;
		}
		if (data == nullptr) {
			return false;
		}
		mStream.write(
			static_cast<const char*>(data),
			static_cast<std::streamsize>(sizeBytes)
		);
		return Good();
	}

	template <typename T>
	bool BinaryWriter::WritePod(const T& value) {
		static_assert(std::is_standard_layout_v<T>);
		static_assert(std::endian::native == std::endian::little);
		return WriteBytes(&value, sizeof(T));
	}

	bool BinaryWriter::WriteString(const std::string_view text) {
		if (text.size() > static_cast<size_t>(std::numeric_limits<
			    uint32_t>::max())) {
			return false;
		}
		const uint32_t sizeBytes = static_cast<uint32_t>(text.size());
		return WritePod(sizeBytes) &&
		       WriteBytes(text.data(), sizeBytes);
	}

	template <typename T>
	bool BinaryWriter::WriteArray(const T* values, const size_t count) {
		static_assert(std::is_standard_layout_v<T>);
		return WriteBytes(values, sizeof(T) * count);
	}

	template <typename T>
	bool BinaryWriter::WriteVector(const std::vector<T>& values) {
		static_assert(std::is_standard_layout_v<T>);
		if (values.size() > static_cast<size_t>(std::numeric_limits<
			    uint32_t>::max())) {
			return false;
		}
		const uint32_t count = static_cast<uint32_t>(values.size());
		return WritePod(count) &&
		       WriteArray(values.data(), values.size());
	}

	bool BinaryWriter::Flush() {
		mStream.flush();
		return Good();
	}
}
