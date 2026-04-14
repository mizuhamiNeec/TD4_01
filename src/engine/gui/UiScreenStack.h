#pragma once
#include <memory>

#include "UiRoot.h"
#include "UiScreen.h"

namespace Unnamed::Gui {
	class UiScreenStack {
	public:
		explicit UiScreenStack(UiRoot* uiRoot);

		void PushScreen(std::shared_ptr<UiScreen> screen);

		void PopScreen();
		void Clear();

		void Tick(float deltaTime);

		[[nodiscard]] const std::vector<std::shared_ptr<UiScreen>>&
		GetScreens() const {
			return mScreens;
		}

		[[nodiscard]] UiRoot* GetUiRoot() const {
			return mUiRoot;
		}

	private:
		UiRoot*                                mUiRoot = nullptr;
		std::vector<std::shared_ptr<UiScreen>> mScreens;
	};
}
