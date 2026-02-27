#include "RendererDraw.h"

#include <unordered_map>

namespace Unnamed::Render {
	bool DrawKey::operator==(const DrawKey& other) const {
		return pso == other.pso && rootSig == other.rootSig &&
		       vbv.BufferLocation == other.vbv.BufferLocation &&
		       ibv.BufferLocation == other.ibv.BufferLocation &&
		       rtvFormat == other.rtvFormat && dsvFormat == other.dsvFormat;
	}

	size_t DrawKeyHash::operator()(const DrawKey& key) const noexcept {
		size_t hash = 0;
		auto   mix  = [&hash](const size_t value) {
			hash ^= value + 0x9e3779b97f4a7c15ULL + (hash << 6) + (hash >> 2);
		};

		mix(reinterpret_cast<size_t>(key.pso));
		mix(reinterpret_cast<size_t>(key.rootSig));
		mix(key.vbv.BufferLocation);
		mix(key.ibv.BufferLocation);
		mix(static_cast<size_t>(key.rtvFormat));
		mix(static_cast<size_t>(key.dsvFormat));

		return hash;
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
