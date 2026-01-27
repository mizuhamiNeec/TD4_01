#pragma once
#include <vector>

#include "game/gameframework/entity/UEntity/UEntity.h"

namespace Unnamed {
	class Map {
	public:
		void Initialize();
		void Update(float deltaTime);
		void Shutdown();



	private:

	private:
		std::vector<UEntity> mEntities;
	};
}
