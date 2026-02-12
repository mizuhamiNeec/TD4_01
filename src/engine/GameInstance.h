#pragma once
#include <memory>

#include "gameview/interface/IGameView.h"

namespace Unnamed {
	class UWorld;

	class GameInstance {
	public:
		explicit GameInstance(UWorld& world);

		void StartPlay();
		void StopPlay();
		void Tick(float deltaTime);

		void SetView(const std::shared_ptr<IGameView>& view);

		[[nodiscard]] bool IsPlaying() const;

	private:
		std::shared_ptr<IGameView> mView;
		UWorld&                    mWorld;
		bool                       mIsPlaying = true;
	};
}
