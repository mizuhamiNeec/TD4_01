#include "RendererDraw.h"

#include <unordered_map>

#include "core/hash/HashBuilder.h"

namespace Unnamed::Render {
	bool DrawKey::operator==(const DrawKey& other) const {
		return pso == other.pso && rootSig == other.rootSig &&
		       vbv.BufferLocation == other.vbv.BufferLocation &&
		       ibv.BufferLocation == other.ibv.BufferLocation &&
		       rtvFormat == other.rtvFormat && dsvFormat == other.dsvFormat;
	}

	size_t DrawKeyHash::operator()(const DrawKey& key) const noexcept {
		HashBuilder hash = {};
		hash.AddPointer(key.pso);
		hash.AddPointer(key.rootSig);
		hash.AddValue(key.vbv.BufferLocation);
		hash.AddValue(key.ibv.BufferLocation);
		hash.AddEnum(key.rtvFormat);
		hash.AddEnum(key.dsvFormat);
		return hash.Value();
	}

	std::vector<DrawBatch> BuildDrawBatchesFromItems(
		const std::vector<MeshDrawItem>& drawList, const DrawKey& baseKey
	) {
		std::vector<DrawBatch> batches;
		batches.reserve(drawList.size());

		std::unordered_map<DrawKey, uint32_t, DrawKeyHash> batchIndexByKey;
		batchIndexByKey.reserve(drawList.size());

		for (const auto& item : drawList) {
			DrawKey key = baseKey;
			key.vbv     = item.vbv;
			key.ibv     = item.ibv;

			auto found = batchIndexByKey.find(key);
			if (found == batchIndexByKey.end()) {
				const auto index = static_cast<uint32_t>(batches.size());
				batchIndexByKey.emplace(key, index);
				batches.emplace_back(DrawBatch{.key = key, .items = {item}});
				continue;
			}

			batches[found->second].items.emplace_back(item);
		}

		return batches;
	}
}
