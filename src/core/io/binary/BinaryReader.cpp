#include "BinaryReader.h"

#include <bit>

namespace Unnamed {
	BinaryReader::BinaryReader(const std::string& path) : mStream(
		path, std::ios::binary
	) {
		if (!IsOpen()) {
			return;
		}
		mStream.seekg(0, std::ios::end);
		mFileSize = static_cast<uint64_t>(mStream.tellg());
		mStream.seekg(0, std::ios::beg);
	}

	bool BinaryReader::IsOpen() const {
		return mStream.is_open();
	}

	bool BinaryReader::Good() const {
		return static_cast<bool>(mStream);
	}

	uint64_t BinaryReader::Tell() {
		return static_cast<uint64_t>(mStream.tellg());
	}

	bool BinaryReader::Seek(const uint64_t offset) {
		mStream.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
		return Good();
	}

	bool BinaryReader::Skip(const uint64_t bytes) {
		mStream.seekg(static_cast<std::streamoff>(bytes), std::ios::cur);
		return Good();
	}

	uint64_t BinaryReader::Remaining() {
		const uint64_t cursor = Tell();
		return cursor <= mFileSize ? mFileSize - cursor : 0ull;
	}

	bool BinaryReader::ReadBytes(void* outData, const size_t sizeBytes) {
		if (sizeBytes == 0) {
			return true;
		}
		if (outData == nullptr) {
			return false;
		}
		if (Remaining() < sizeBytes) {
			return false;
		}
		mStream.read(
			static_cast<char*>(outData),
			static_cast<std::streamsize>(sizeBytes)
		);
		return Good();
	}

	template <typename T>
	bool BinaryReader::ReadPod(T& outValue) {
		static_assert(std::is_standard_layout_v<T>);
		static_assert(std::endian::native == std::endian::little);
		return ReadBytes(&outValue, sizeof(T));
	}

	bool BinaryReader::ReadString(std::string& outText) {
		uint32_t sizeBytes = 0;
		if (!ReadPod(sizeBytes)) {
			return false;
		}
		if (Remaining() < sizeBytes) {
			return false;
		}
		outText.resize(sizeBytes);
		return ReadBytes(outText.data(), sizeBytes);
	}

	template <typename T>
	bool BinaryReader::ReadArray(T* outValues, const size_t count) {
		static_assert(std::is_standard_layout_v<T>);
		return ReadBytes(outValues, sizeof(T) * count);
	}

	template <typename T>
	bool BinaryReader::ReadVector(std::vector<T>& outValues) {
		static_assert(std::is_standard_layout_v<T>);
		uint32_t count = 0;
		if (!ReadPod(count)) {
			return false;
		}
		if (sizeof(T) > 0 &&
		    Remaining() / sizeof(T) < static_cast<size_t>(count)) {
			return false;
		}
		outValues.resize(count);
		return ReadArray(outValues.data(), outValues.size());
	}
}
