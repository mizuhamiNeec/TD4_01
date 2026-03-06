#include "TextureLoaderDirectXTex.h"

#include <array>
#include <DirectXTex.h>
#include <filesystem>

#include <core/assets/types/TextureAssetData.h>
#include <core/string/StrUtil.h>

#include <engine/unnamed/subsystem/console/Log.h>

namespace Unnamed {
	static constexpr std::string_view kChannel = "TxLdrDtx";

	static constexpr std::array kSupportedExtensions = {
		// DirectDraw Surface
		".dds",

		// HDR
		".hdr",

		// TGA
		".tga",

		// Windows Imaging Component
		".bmp",
		".png",
		".gif",
		".tif",
		".tiff",
		".jpg",
		".jpeg",
	};

	bool TextureLoaderDirectXTex::CanLoad(
		const std::string_view path, ASSET_TYPE* outType
	) const {
		bool ok = false;

		// サポートされている拡張子か確認
		for (const auto& supported : kSupportedExtensions) {
			if (StrUtil::HasExtension(path, supported)) {
				ok = true;
				break;
			}
		}

		// 出力タイプの設定
		if (outType) {
			if (ok) {
				// テクスチャとして認識
				*outType = ASSET_TYPE::TEXTURE;
			} else {
				// 不明
				*outType = ASSET_TYPE::UNKNOWN;
			}
		}

		return ok;
	}

	LoadResult TextureLoaderDirectXTex::Load(const std::string& path) {
		LoadResult r = {};
		using namespace DirectX;

		ScratchImage img;
		TexMetadata  meta = {};
		HRESULT      hr   = E_FAIL;

		std::wstring wPath = StrUtil::ToWString(path);
		std::string  ext   = StrUtil::ToLowerExt(path);

		if (ext == ".dds") {
			hr = LoadFromDDSFile(
				wPath.c_str(), DDS_FLAGS_NONE, &meta, img
			);
		} else if (ext == ".hdr") {
			hr = LoadFromHDRFile(
				wPath.c_str(), &meta, img
			);
		} else if (ext == ".tga") {
			hr = LoadFromTGAFile(
				wPath.c_str(), &meta, img
			);
		} else {
			hr = LoadFromWICFile(
				wPath.c_str(), WIC_FLAGS_NONE, &meta, img
			);
		}

		if (FAILED(hr)) {
			Error(kChannel, "テクスチャの読み込みに失敗しました: {}", path);
			return r;
		}

		// sRGB形式に変換
		if (meta.format != MakeSRGB(meta.format)) {
			Msg(kChannel, "テクスチャをsRGB形式に変換します: {}", path);
			ScratchImage converted;
			hr = Convert(
				img.GetImages(),
				img.GetImageCount(),
				meta,
				MakeSRGB(meta.format),
				TEX_FILTER_CUBIC,
				0.0,
				converted
			);
			if (FAILED(hr)) {
				Error(kChannel, "テクスチャのsRGB変換に失敗しました: {}, そのまま使用します。", path);
			} else {
				img  = std::move(converted);
				meta = img.GetMetadata();
			}
		}

		// ミップマップがない場合は生成する
		if (meta.mipLevels <= 1) {
			Msg(kChannel, "ミップマップが存在しないため生成します。: {}", path);
			ScratchImage chain;
			hr = GenerateMipMaps(
				img.GetImages(), img.GetImageCount(), meta,
				TEX_FILTER_CUBIC, 0, chain
			);
			if (SUCCEEDED(hr)) {
				img  = std::move(chain);
				meta = img.GetMetadata();
				SpecialMsg(
					LogLevel::Success, kChannel,
					"{} 段階のミップマップを生成しました。", meta.mipLevels
				);
			} else {
				Warning(
					kChannel, "ミップマップ生成に失敗しました: {}。そのまま使用します。", path
				);
			}
		}

		// アセットデータにつめつめ
		TextureAssetData out = {};
		out.width            = static_cast<uint32_t>(meta.width);
		out.height           = static_cast<uint32_t>(meta.height);
		out.sourcePath       = path;
		out.mips.resize(meta.mipLevels);
		for (size_t m = 0; m < meta.mipLevels; ++m) {
			const Image* im  = img.GetImage(m, 0, 0);
			TextureMip   mip = {};
			mip.width        = static_cast<uint32_t>(im->width);
			mip.height       = static_cast<uint32_t>(im->height);
			mip.rowPitch     = static_cast<size_t>(mip.width) * 4; // RGBA8
			mip.bytes.resize(mip.rowPitch * mip.height);

			for (uint32_t y = 0; y < mip.height; ++y) {
				memcpy(
					mip.bytes.data() + y * mip.rowPitch,
					im->pixels + y * im->rowPitch,
					std::min(
						mip.rowPitch,
						static_cast<size_t>(im->rowPitch)
					)
				);
			}
			out.mips[m] = std::move(mip);
		}

		r.payload     = std::move(out);
		r.resolveName = std::filesystem::path(path).filename().string();
		if (std::error_code ec;
			std::filesystem::exists(path, ec)) {
			r.stamp.sizeInBytes = std::filesystem::file_size(path, ec);
		}

		return r;
	}
}
