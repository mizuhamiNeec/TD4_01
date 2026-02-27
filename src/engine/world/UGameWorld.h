#pragma once
#include "UWorld.h"

namespace Unnamed {
	class UGameWorld final : public UWorld {
	public:
		void Initialize() override;
		void Tick(float deltaTime) override;
	};
}
