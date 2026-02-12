#include "GameInstance.h"

namespace Unnamed {
	void GameInstance::SetView(
		const std::shared_ptr<IGameView>& view
	) { mView = view; }

	GameInstance::GameInstance(UWorld& world) : mWorld(world) {}

	void GameInstance::StartPlay() { mIsPlaying = true; }

	void GameInstance::StopPlay() { mIsPlaying = false; }

	void GameInstance::Tick(const float deltaTime) {
		deltaTime;

		if (!mIsPlaying || !mView) { return; }

		uint32_t w = 0, h = 0;
		mView->GetRenderSize(w, h);

		mView->BeginFrame();

		mView->EndFrame();
	}

	bool GameInstance::IsPlaying() const { return mIsPlaying; }
}
