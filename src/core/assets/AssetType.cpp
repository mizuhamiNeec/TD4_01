#include "AssetType.h"

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
		default: return "unknown";
	}
}
