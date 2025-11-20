#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <d3d12.h>

class SrvManager;

/// @brief マテリアルインスタンスクラス
class MaterialInstance {
public:
	void SetTexture(const std::string& slot, const std::string& texturePath) {
		mTextureSlots[slot] = texturePath;
	}

	std::string GetTexture(const std::string& slot) {
		auto it = mTextureSlots.find(slot);
		return it != mTextureSlots.end() ? it->second : "";
	}

	void Apply(
		UINT                            rootParameterIndex,
		SrvManager*                     srvManager,
		const std::vector<std::string>& textureOrder);

private:
	std::unordered_map<std::string, std::string> mTextureSlots;
};
