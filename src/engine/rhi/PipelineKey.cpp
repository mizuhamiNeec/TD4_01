#include "PipelineKey.h"

namespace Unnamed ::Rhi {
	bool VertexLayoutDesc::operator==(
		const VertexLayoutDesc& rhs
	) const {
		if (stride != rhs.stride) {
			return false;
		}
		if (elements.size() != rhs.elements.size()) {
			return false;
		}
		for (size_t i = 0; i < elements.size(); ++i) {
			const auto& a = elements[i];
			const auto& b = rhs.elements[i];
			if (a.semantic != b.semantic) {
				return false;
			}
			if (a.semanticIndex != b.semanticIndex) {
				return false;
			}
			if (a.format != b.format) {
				return false;
			}
			if (a.offset != b.offset) {
				return false;
			}
			if (a.inputSlot != b.inputSlot) {
				return false;
			}
			if (a.perInstance != b.perInstance) {
				return false;
			}
			if (a.instanceStepRate != b.instanceStepRate) {
				return false;
			}
		}
		return true;
	}
}
