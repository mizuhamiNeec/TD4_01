#include "AssetType.h"

#include <algorithm>
#include <cctype>
#include <string_view>

std::string Unnamed::ToString(const ASSET_TYPE e) {
	switch (e) {
		case ASSET_TYPE::UNKNOWN: return "UNKNOWN";
		case ASSET_TYPE::TEXTURE: return "TEXTURE";
		case ASSET_TYPE::SHADER_SOURCE: return "SHADER_SOURCE";
		case ASSET_TYPE::MATERIAL: return "MATERIAL";
		case ASSET_TYPE::MESH: return "MESH";
		case ASSET_TYPE::SOUND: return "SOUND";
		case ASSET_TYPE::RAW_FILE: return "RAW_FILE";
		case ASSET_TYPE::SHADER_PROGRAM: return "SHADER_PROGRAM";
		case ASSET_TYPE::MATERIAL_INSTANCE: return "MATERIAL_INSTANCE";
		case ASSET_TYPE::POST_FX_CHAIN: return "POST_FX_CHAIN";
		case ASSET_TYPE::UI_DOCUMENT: return "UI_DOCUMENT";
		default: return "unknown";
	}
}

Unnamed::ASSET_TYPE Unnamed::GuessAssetTypeFromPath(const std::string_view path) {
	auto toLower = [](std::string value) {
		std::ranges::transform(
			value,
			value.begin(),
			[](const unsigned char c) {
				return static_cast<char>(std::tolower(c));
			}
		);
		return value;
	};
	const std::string lower = toLower(std::string(path));

	auto endsWith = [&](const std::string_view suffix) {
		return lower.size() >= suffix.size() &&
		       lower.compare(lower.size() - suffix.size(), suffix.size(), suffix)
		       == 0;
	};

	if (
		endsWith(".png") || endsWith(".jpg") || endsWith(".jpeg") ||
		endsWith(".dds") || endsWith(".bmp") || endsWith(".tga") ||
		endsWith(".tif") || endsWith(".tiff") || endsWith(".hdr")
	) {
		return ASSET_TYPE::TEXTURE;
	}
	if (
		endsWith(".wav") || endsWith(".ogg") || endsWith(".mp3") ||
		endsWith(".flac")
	) {
		return ASSET_TYPE::SOUND;
	}
	if (
		endsWith(".gltf") || endsWith(".glb") || endsWith(".fbx") ||
		endsWith(".obj")
	) {
		return ASSET_TYPE::MESH;
	}
	if (endsWith(".shader.json")) {
		return ASSET_TYPE::SHADER_PROGRAM;
	}
	if (endsWith(".material.json")) {
		return ASSET_TYPE::MATERIAL;
	}
	if (endsWith(".matinst.json")) {
		return ASSET_TYPE::MATERIAL_INSTANCE;
	}
	if (endsWith(".postfx.json")) {
		return ASSET_TYPE::POST_FX_CHAIN;
	}
	if (endsWith(".ui.json")) {
		return ASSET_TYPE::UI_DOCUMENT;
	}
	if (endsWith(".hlsl") || endsWith(".hlsli")) {
		return ASSET_TYPE::SHADER_SOURCE;
	}
	return ASSET_TYPE::UNKNOWN;
}
