#pragma once
#include <string>
#include <vector>

#include "core/assets/AssetID.h"

namespace Unnamed::Render {
	struct ShaderKey {
		AssetID shaderSourceId = kInvalidAssetID;
		std::string entry; // エントリポイント名
		std::string profile; // シェーダープロファイル
		std::vector<std::pair<std::string, std::string>> defines; // プリプロセッサ定義

		bool operator==(const ShaderKey& rhs) const {
			return shaderSourceId ==
			       rhs.shaderSourceId &&
			       entry == rhs.entry &&
			       profile == rhs.profile &&
			       defines == rhs.defines;
		}
	};

	struct ShaderKeyHash {
		size_t operator()(const ShaderKey& key) const noexcept;
	};
}
