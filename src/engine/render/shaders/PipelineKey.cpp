#include "PipelineKey.h"

namespace Unnamed::Render {
	bool PipelineKey::operator==(
		const ComputePipelineKey& rhs
	) const { return cs == rhs.cs && rootSignature == rhs.rootSignature; }
}
