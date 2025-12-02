#include "EmptyScene.h"

#include "engine/Engine.h"
#include "engine/Input/InputSystem.h"
#include "engine/subsystem/console/Log.h"
#include "engine/OldConsole/Console.h"
#include "runtime/gui/UiButton.h"
#include "runtime/gui/UiDocumentManager.h"
#include "runtime/gui/UiRoot.h"
#include "runtime/gui/UiScreenStack.h"
#include "runtime/gui/UiWidget.h"
#include "runtime/gui/editor/GuiEditor.h"

EmptyScene::~EmptyScene() = default;

using namespace Unnamed::Gui;

std::unique_ptr<UiRoot>            uiRoot;
std::unique_ptr<UiDocumentManager> uiDocManager;
std::unique_ptr<UiScreenStack>     uiScreenStack;

namespace {
	constexpr std::string_view kPlayButtonId  = "PlayButton";
	constexpr std::string_view kQuitButtonId  = "QuitButton";
	constexpr std::string_view kGameSceneName = "GameScene";
}

void EmptyScene::Init() {
	mRenderer        = Unnamed::Engine::GetRenderer();
	mSrvManager      = Unnamed::Engine::GetSrvManager();
	mResourceManager = Unnamed::Engine::GetResourceManager();

	uiRoot        = std::make_unique<UiRoot>();
	uiDocManager  = std::make_unique<UiDocumentManager>();
	uiScreenStack = std::make_unique<UiScreenStack>(uiRoot.get());

	auto mainMenuDoc = uiDocManager->LoadDocument(
		"./content/parkour/ui/MainMenu.ui.json"
	);
	mActiveDocument     = mainMenuDoc;
	auto mainMenuScreen = std::make_shared<UiScreen>(mainMenuDoc);

	uiScreenStack->PushScreen(mainMenuScreen);

	BindUiCallbacks();
}

#ifdef _DEBUG
void DrawUiWithImGui(const std::vector<UiDrawCommand>& commands) {
	using namespace Unnamed::Gui;

	ImDrawList* dl = ImGui::GetForegroundDrawList();

	for (const auto& cmd : commands) {
		switch (cmd.type) {
		case UiDrawCommandType::RECT: {
			const Rect&  r = cmd.rect.rect;
			const Color& c = cmd.rect.fillColor;

			const ImU32 col = IM_COL32(
				static_cast<int>(c.r * 255.0f),
				static_cast<int>(c.g * 255.0f),
				static_cast<int>(c.b * 255.0f),
				static_cast<int>(c.a * 255.0f)
			);

			dl->AddRectFilled(
				ImVec2(+r.x, +r.y),
				ImVec2(+r.x + r.width, +r.y + r.height),
				col,
				cmd.rect.cornerRadius
			);

			break;
		}
		case UiDrawCommandType::TEXT: {
			const Vec2&  p   = cmd.text.position;
			const Color& c   = cmd.text.color;
			ImU32        col = IM_COL32(
				static_cast<int>(c.r * 255.0f),
				static_cast<int>(c.g * 255.0f),
				static_cast<int>(c.b * 255.0f),
				static_cast<int>(c.a * 255.0f)
			);

			dl->AddText(
				ImVec2(+p.x, +p.y),
				col,
				cmd.text.text.c_str()
			);
			break;
		}
		default: ;
		}
	}
}
#endif

void EmptyScene::Update(const float deltaTime) {
	// シーン内のすべてのエンティティを更新
	for (const auto entity : mEntities) {
		if (entity->IsActive() && !entity->GetParent()) {
			entity->Update(deltaTime);
		}
	}
#ifdef _DEBUG
	DrawUiHierarchyWindow(*uiRoot);
	DrawUiInspectorWindow();

	DrawUiEditorMenu(*uiDocManager, mActiveDocument, uiScreenStack.get());
#endif
	uiScreenStack->Tick(deltaTime);
	uiRoot->SetRootRect(
		{
			Unnamed::Engine::GetViewportLT().x,
			Unnamed::Engine::GetViewportLT().y,
			Unnamed::Engine::GetViewportSize().x,
			Unnamed::Engine::GetViewportSize().y
		}
	);
	uiRoot->Tick(deltaTime);
	uiRoot->UpdateLayout();

	const auto  mPos         = InputSystem::GetMousePosition();
	const float mouseX       = mPos.x;
	const float mouseY       = mPos.y;
	const bool  leftDown     = InputSystem::IsPressed("+attack1");
	const bool  leftPressed  = InputSystem::IsTriggered("+attack1");
	const bool  leftReleased = InputSystem::IsReleased("+attack1");
	uiRoot->ProcessMouse(
		mouseX, mouseY,
		leftDown, leftPressed, leftReleased
	);

	std::vector<UiDrawCommand> commands;
	uiRoot->BuildDrawCommands(commands);

#ifdef _DEBUG
	UiWidget::DebugDrawUi(uiRoot->GetRootWidget());

	DrawUiWithImGui(commands);
#endif

	HandleLaunchInput();
}

void EmptyScene::Render() {
	for (auto entity : mEntities) {
		if (entity->IsActive()) {
			entity->Render(mRenderer->GetCommandList());
		}
	}
}

void EmptyScene::Shutdown() {
}

void EmptyScene::HandleLaunchInput() {
	if (mGameLaunchQueued) {
		return;
	}

	const bool pressed = InputSystem::IsTriggered("+attack1") ||
		InputSystem::IsTriggered("+jump") ||
		InputSystem::IsTriggered("+use");
	if (pressed) {
		mGameLaunchQueued = true;
		Unnamed::Engine::RequestSceneChange(kGameSceneName);
	}
}

void EmptyScene::BindUiCallbacks() const {
	if (!mActiveDocument) {
		return;
	}

	if (auto* widget = mActiveDocument->FindWidgetById(
		std::string(kPlayButtonId))) {
		if (auto* button = dynamic_cast<Unnamed::Gui::UiButton*>(widget)) {
			button->SetOnClick([] {
				Unnamed::Engine::RequestSceneChange(kGameSceneName);
			});
		}
	}

	if (auto* widget = mActiveDocument->FindWidgetById(
		std::string(kQuitButtonId))) {
		if (auto* button = dynamic_cast<Unnamed::Gui::UiButton*>(widget)) {
			button->SetOnClick([] {
				Console::SubmitCommand("quit");
			});
		}
	}
}
