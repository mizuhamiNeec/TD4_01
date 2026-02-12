#pragma once
#include "UWorld.h"

namespace Unnamed {
	class UGameWorld;

	class UEditorWorld final : public UWorld {
	public:
		void Initialize() override;
		void Tick(float deltaTime) override;

		void               StartPlayInEditor();
		void               StopPlayInEditor();
		[[nodiscard]] bool IsPlaying() const { return mPlayWorld != nullptr; }

	private:
		// プレイ中のゲームワールド
		std::unique_ptr<UGameWorld> mPlayWorld;
	};
}
