#include "CommandBuffer.h"

namespace Unnamed::ECS {
	void CommandBuffer::Apply(World& world) {
		for (auto& cmd : mCommands) {
			cmd->Apply(world);
		}
	}
}
