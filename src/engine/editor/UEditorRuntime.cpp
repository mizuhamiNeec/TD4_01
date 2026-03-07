#ifdef _DEBUG
#include "UEditorRuntime.h"

#include <algorithm>
#include <array>
#include <format>
#include <functional>
#include <imgui.h>
#include <imgui_internal.h>
#include <map>
#include <set>

#include "EditorNotification.h"

#include "core/json/JsonReader.h"
#include "core/string/StrUtil.h"

#include "engine/Properties.h"
#include "engine/ImGui/Icons.h"
#include "engine/ImGui/ImGuiUtil.h"
#include "engine/ImGui/ImGuiWidgets.h"
#include "engine/platform/Window.h"
#include "engine/platform/WindowManager.h"
#include "engine/profiler/UProfiler.h"
#include "engine/render/RenderModule.h"
#include "engine/scene/USceneSerializer.h"
#include "engine/ui/UImGuiLayer.h"
#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/components/editor/EditorCameraComponent.h"
#include "engine/unnamed/framework/components/mesh/SkeletalMeshRendererComponent.h"
#include "engine/unnamed/framework/components/mesh/StaticMeshRendererComponent.h"
#include "engine/unnamed/framework/components/portal/PortalComponent.h"
#include "engine/unnamed/framework/entity/UEntity.h"
#include "engine/unnamed/subsystem/console/ConsoleSystem.h"
#include "engine/unnamed/subsystem/console/concommand/UnnamedConVar.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"
#include "engine/world/UEditorWorld.h"

#include "thirdparty/ImGuizmo/ImGuizmo.h"

namespace Unnamed {
	namespace {
		struct HierarchyFolderNode {
			std::map<std::string, HierarchyFolderNode> children;
			std::vector<UEntity*>                      entities;
		};

		std::vector<std::string> SplitFolderPath(std::string_view folderPath) {
			std::vector<std::string> parts;
			size_t                   begin = 0;
			while (begin < folderPath.size()) {
				const size_t end = folderPath.find('/', begin);
				const size_t len = (end == std::string_view::npos ?
					                    folderPath.size() :
					                    end) - begin;
				if (len > 0) {
					parts.emplace_back(folderPath.substr(begin, len));
				}
				if (end == std::string_view::npos) {
					break;
				}
				begin = end + 1;
			}
			return parts;
		}

		std::string NormalizeFolderPath(std::string_view folderPath) {
			std::string path(folderPath);
			std::replace(path.begin(), path.end(), '\\', '/');
			while (!path.empty() && path.front() == '/') {
				path.erase(path.begin());
			}
			while (!path.empty() && path.back() == '/') {
				path.pop_back();
			}
			return path;
		}

		std::string JoinFolderPath(
			std::string_view parent, std::string_view child
		) {
			if (parent.empty()) {
				return std::string(child);
			}
			if (child.empty()) {
				return std::string(parent);
			}
			return std::string(parent) + "/" + std::string(child);
		}

		std::string GetFolderLeafName(std::string_view folderPath) {
			const size_t slashPos = folderPath.find_last_of('/');
			return slashPos == std::string_view::npos ?
				       std::string(folderPath) :
				       std::string(folderPath.substr(slashPos + 1));
		}

		bool IsFolderEqualOrDescendant(
			std::string_view path, std::string_view ancestor
		) {
			if (ancestor.empty()) {
				return true;
			}
			if (path == ancestor) {
				return true;
			}
			if (path.size() <= ancestor.size()) {
				return false;
			}
			return path.substr(0, ancestor.size()) == ancestor &&
			       path[ancestor.size()] == '/';
		}

		HierarchyFolderNode* EnsureFolderNode(
			HierarchyFolderNode& root, std::string_view folderPath
		) {
			HierarchyFolderNode* node = &root;
			for (const auto& part : SplitFolderPath(folderPath)) {
				node = &node->children[part];
			}
			return node;
		}

		void BuildHierarchyTree(
			HierarchyFolderNode& root, const UScene& scene
		) {
			for (const std::string& folder : scene.GetFolders()) {
				EnsureFolderNode(root, folder);
			}
			for (const auto& ePtr : scene.GetEntities()) {
				if (!ePtr) {
					continue;
				}
				UEntity*             entity = ePtr.get();
				HierarchyFolderNode* node   = EnsureFolderNode(
					root, entity->GetFolderPath()
				);
				node->entities.emplace_back(entity);
			}
		}

		std::string MakeUniqueFolderPath(
			const UScene& scene, std::string_view parentFolderPath
		) {
			const std::string parent = NormalizeFolderPath(parentFolderPath);
			int               suffix = 0;
			for (;;) {
				const std::string candidateName =
					suffix == 0 ?
						"NewFolder" :
						"NewFolder" + std::to_string(suffix);
				const std::string candidatePath = JoinFolderPath(
					parent, candidateName
				);
				if (
					std::ranges::find(scene.GetFolders(), candidatePath) ==
					scene.GetFolders().end()
				) {
					return candidatePath;
				}
				++suffix;
			}
		}

		bool IsTransformAncestor(
			const TransformComponent* possibleAncestor,
			const TransformComponent* node
		) {
			const TransformComponent* current = node;
			while (current) {
				if (current == possibleAncestor) {
					return true;
				}
				current = current->Parent();
			}
			return false;
		}
	}

	UEditorRuntime::UEditorRuntime(
		UEditorWorld&         editorWorld,
		WindowManager&        windowManager,
		Render::RenderModule& renderModule,
		UImGuiLayer&          imGuiLayer
	) : mEditorWorld(editorWorld),
	    mWindowManager(windowManager),
	    mRenderModule(renderModule),
	    mImGuiLayer(imGuiLayer) {
		LoadImGuizmoSettings();
		mNotification = std::make_unique<EditorNotification>();
	}

	void UEditorRuntime::BeginUI() {
		ImGuizmo::BeginFrame();

		DrawMainMenu();
		DrawSideBar();
		DrawStatusBar();

		// DockSpaceの開始
		{
			const ImGuiViewport* viewport = ImGui::GetMainViewport();
			ImGui::SetNextWindowPos(viewport->WorkPos);
			ImGui::SetNextWindowSize(viewport->WorkSize);
			ImGui::SetNextWindowViewport(viewport->ID);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
			ImGui::PushStyleVar(
				ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f)
			);

			constexpr ImGuiDockNodeFlags dockSpaceFlags =
				ImGuiDockNodeFlags_None;
			constexpr ImGuiWindowFlags windowFlags =
				ImGuiWindowFlags_NoTitleBar |
				ImGuiWindowFlags_NoCollapse |
				ImGuiWindowFlags_NoResize |
				ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_NoBringToFrontOnFocus |
				ImGuiWindowFlags_NoNavFocus |
				ImGuiWindowFlags_NoBackground;

			ImGui::Begin("DockSpace", nullptr, windowFlags);

			ImGui::PopStyleVar(3);

			const ImGuiIO& io = ImGui::GetIO();
			if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
				const ImGuiID dockSpaceId = ImGui::GetID("DockSpace");
				ImGui::DockSpace(
					dockSpaceId, ImVec2(0.0f, 0.0f), dockSpaceFlags
				);
			}

			ImGui::End();
		}

		ImGui::ShowDemoWindow();
	}

	void UEditorRuntime::BuildUi(const float deltaTime) {
		mViewportSizeChangedThisFrame = false;

		if (mPresentMode == EDITOR_PRESENT_MODE::VIEWPORT_PANEL) {
			DrawViewport(deltaTime);
			const auto input = ServiceLocator::Get<UInputSystem>();
			mEditorWorld.SetEditorCameraLookEnabled(mViewportLookActive);
			if (input) {
				if (mViewportLookActive) {
					if (const Window* window = mWindowManager.FindWindowById(
						mWindowManager.GetMainWindowId()
					)) {
						POINT clientPoint = {
							static_cast<LONG>(
								mViewportPosition.x + mViewportSize.x * 0.5f),
							static_cast<LONG>(
								mViewportPosition.y + mViewportSize.y * 0.5f)
						};
						ScreenToClient(window->GetHwnd(), &clientPoint);
						input->SetMouseCursorLockClientPosition(
							window->GetHwnd(),
							Vec2(
								static_cast<float>(clientPoint.x),
								static_cast<float>(clientPoint.y)
							)
						);
					}
				} else {
					input->ClearMouseCursorLockAnchor();
				}
			}

			DrawSceneHierarchy();
			DrawInspector();
		} else {
			const Render::SceneRenderRequest sceneRequest =
				BuildSceneRenderRequest();
			const bool requestChanged =
				sceneRequest.mode != mLastSceneRenderRequest.mode ||
				sceneRequest.viewportPanelWidth !=
				mLastSceneRenderRequest.viewportPanelWidth ||
				sceneRequest.viewportPanelHeight !=
				mLastSceneRenderRequest.viewportPanelHeight ||
				sceneRequest.preferRealtimeResize !=
				mLastSceneRenderRequest.preferRealtimeResize ||
				sceneRequest.presentToSwapChain !=
				mLastSceneRenderRequest.presentToSwapChain ||
				sceneRequest.clearSwapChainWhenNotPresenting !=
				mLastSceneRenderRequest.clearSwapChainWhenNotPresenting;
			if (requestChanged) {
				mLastSceneRenderRequest = sceneRequest;
			}
			mRenderModule.SetSceneRenderRequest(sceneRequest);

			const auto input = ServiceLocator::Get<UInputSystem>();
			mEditorWorld.SetEditorCameraLookEnabled(
				input && input->IsHeld("ed_look")
			);
			if (input) {
				if (const Window* window = mWindowManager.FindWindowById(
					mWindowManager.GetMainWindowId()
				)) {
					input->SetMouseCursorLockClientPosition(
						window->GetHwnd(),
						{
							static_cast<float>(sceneRequest.viewportPanelWidth)
							*
							0.5f,
							static_cast<float>(sceneRequest.viewportPanelHeight)
							*
							0.5f
						}
					);
				}
			}
			mViewportLookActive = false;
		}

		DrawProfilerWindow();

		mNotification->Update(deltaTime);
	}

	Render::SceneRenderRequest UEditorRuntime::GetSceneRenderRequest() const {
		return BuildSceneRenderRequest();
	}

	void UEditorRuntime::TogglePresentMode() {
		mPresentMode = mPresentMode == EDITOR_PRESENT_MODE::VIEWPORT_PANEL ?
			               EDITOR_PRESENT_MODE::FULLSCREEN_SWAP_CHAIN :
			               EDITOR_PRESENT_MODE::VIEWPORT_PANEL;
	}

	EDITOR_PRESENT_MODE UEditorRuntime::GetPresentMode() const {
		return mPresentMode;
	}

	void UEditorRuntime::SetSceneOutput(
		const Render::SceneOutputView& sceneOutput, const Vec2 size
	) {
		mSceneTextureId   = sceneOutput.textureId;
		mSceneSrvCpu      = sceneOutput.srvCpu;
		mSceneSrvRevision = sceneOutput.srvRevision;
		mSceneSize        = size;
	}

	void UEditorRuntime::LoadImGuizmoSettings() {
		auto guizmoConfig = ServiceLocator::Get<ConsoleSystem>()->GetConVarAs<
			UnnamedConVar<std::string>>("im_guizmoconfigpath");

		auto Vec4ToImVec4 = [](const Vec4& vec) {
			return ImVec4(vec.x, vec.y, vec.z, vec.w);
		};

		if (guizmoConfig) {
			JsonReader reader(guizmoConfig->GetValue());
			auto&      style  = ImGuizmo::GetStyle();
			auto&      colors = style.Colors;

			// Style
			{
				const auto rs                  = reader["style"];
				style.TranslationLineThickness =
					rs["translationLineThickness"].GetFloat();
				style.TranslationLineArrowSize =
					rs["translationLineArrowSize"].GetFloat();
				style.RotationLineThickness =
					rs["rotationLineThickness"].GetFloat();
				style.RotationOuterLineThickness =
					rs["rotationOuterLineThickness"].GetFloat();
				style.ScaleLineThickness =
					rs["scaleLineThickness"].GetFloat();
				style.ScaleLineCircleSize =
					rs["scaleLineCircleSize"].GetFloat();
				style.HatchedAxisLineThickness =
					rs["hatchedAxisLineThickness"].GetFloat();
				style.CenterCircleSize =
					rs["centerCircleSize"].GetFloat();
			}

			// Color
			{
				using namespace ImGuizmo;
				auto       rc = reader["color"];
				const auto dx = rc["directionX"].GetArray();
				colors[DIRECTION_X] = Vec4ToImVec4(dx.GetVec4());
				const auto dy = rc["directionY"].GetArray();
				colors[DIRECTION_Y] = Vec4ToImVec4(dy.GetVec4());
				const auto dz = rc["directionZ"].GetArray();
				colors[DIRECTION_Z] = Vec4ToImVec4(dz.GetVec4());
				const auto px = rc["planeX"].GetArray();
				colors[PLANE_X] = Vec4ToImVec4(px.GetVec4());
				const auto py = rc["planeY"].GetArray();
				colors[PLANE_Y] = Vec4ToImVec4(py.GetVec4());
				const auto pz = rc["planeZ"].GetArray();
				colors[PLANE_Z] = Vec4ToImVec4(pz.GetVec4());
				const auto sel = rc["selection"].GetArray();
				colors[SELECTION] = Vec4ToImVec4(sel.GetVec4());
				const auto inact = rc["inactive"].GetArray();
				colors[INACTIVE] = Vec4ToImVec4(inact.GetVec4());
				const auto tline = rc["translationLine"].GetArray();
				colors[TRANSLATION_LINE] = Vec4ToImVec4(tline.GetVec4());
				const auto sline = rc["scaleLine"].GetArray();
				colors[SCALE_LINE] = Vec4ToImVec4(sline.GetVec4());
				const auto rborder = rc["rotationUsingBorder"].GetArray();
				colors[ROTATION_USING_BORDER] = Vec4ToImVec4(rborder.GetVec4());
				const auto rfill = rc["rotationUsingFill"].GetArray();
				colors[ROTATION_USING_FILL] = Vec4ToImVec4(rfill.GetVec4());
				const auto hatch = rc["hatchedAxisLines"].GetArray();
				colors[HATCHED_AXIS_LINES] = Vec4ToImVec4(hatch.GetVec4());
				const auto text = rc["text"].GetArray();
				colors[TEXT] = Vec4ToImVec4(text.GetVec4());
				const auto textShadow = rc["textShadow"].GetArray();
				colors[TEXT_SHADOW] = Vec4ToImVec4(textShadow.GetVec4());
			}

			// どっちが正方向かわからんくなるので禁止
			ImGuizmo::AllowAxisFlip(false);
		}
	}

	Render::SceneRenderRequest UEditorRuntime::BuildSceneRenderRequest() const {
		Render::SceneRenderRequest sceneRequest = {};
		sceneRequest.preferRealtimeResize       = true;
		sceneRequest.presentToSwapChain         =
			mPresentMode == EDITOR_PRESENT_MODE::FULLSCREEN_SWAP_CHAIN;
		sceneRequest.clearSwapChainWhenNotPresenting =
			!sceneRequest.presentToSwapChain;

		float width  = mViewportPanelWidth;
		float height = mViewportPanelHeight;
		if (mPresentMode == EDITOR_PRESENT_MODE::FULLSCREEN_SWAP_CHAIN) {
			if (
				const Window* window = mWindowManager.FindWindowById(
					mWindowManager.GetMainWindowId()
				)
			) {
				const WindowDesc desc = window->GetDesc();
				width = static_cast<float>(std::max(1, desc.width));
				height = static_cast<float>(std::max(1, desc.height));
			}
		}

		sceneRequest.viewportPanelWidth  = static_cast<uint32_t>(width);
		sceneRequest.viewportPanelHeight = static_cast<uint32_t>(height);

		switch (mViewportRenderMode) {
			case EDITOR_VIEWPORT_RENDER_MODE::FIT_VIEWPORT
			: sceneRequest.mode = Render::SCENE_RENDER_MODE::FIT_VIEWPORT;
				break;
			case EDITOR_VIEWPORT_RENDER_MODE::FIXED_ASPECT_16_9
			: sceneRequest.mode = Render::SCENE_RENDER_MODE::FIXED_ASPECT_16X9;
				break;
			case EDITOR_VIEWPORT_RENDER_MODE::HD720
			: sceneRequest.mode = Render::SCENE_RENDER_MODE::HD_720P;
				break;
			case EDITOR_VIEWPORT_RENDER_MODE::FHD1080
			: sceneRequest.mode = Render::SCENE_RENDER_MODE::FHD_1080P;
				break;
			default: break;
		}

		const bool dynamicMode = sceneRequest.mode ==
		                         Render::SCENE_RENDER_MODE::FIT_VIEWPORT ||
		                         sceneRequest.mode ==
		                         Render::SCENE_RENDER_MODE::FIXED_ASPECT_16X9;
		if (dynamicMode) {
			sceneRequest.viewportPanelWidth = std::max(
				8u, sceneRequest.viewportPanelWidth / 8u * 8u
			);
			sceneRequest.viewportPanelHeight = std::max(
				8u, sceneRequest.viewportPanelHeight / 8u * 8u
			);
		}

		return sceneRequest;
	}

	UScene* UEditorRuntime::GetHierarchyScene() {
		return mEditorWorld.IsPlaying() ?
			       mEditorWorld.GetActiveScene() :
			       mEditorWorld.GetEditableScene();
	}

	const UScene* UEditorRuntime::GetHierarchyScene() const {
		return mEditorWorld.IsPlaying() ?
			       mEditorWorld.GetActiveScene() :
			       mEditorWorld.GetEditableScene();
	}

	void UEditorRuntime::DrawViewportGizmo(
		const Render::SceneRenderRequest& sceneRequest,
		const Vec2&                       imagePos,
		const float                       drawWidth,
		const float                       drawHeight
	) {
		UEntity* entity = GetSelectedEntity();
		if (!entity) {
			return;
		}

		auto* transform = entity->GetComponent<TransformComponent>();
		if (!transform) {
			return;
		}
		Mat4 world = transform->WorldMat();

		Mat4 view = Mat4::identity;
		Mat4 proj = Mat4::identity;
		if (!mEditorWorld.BuildEditorCameraMatrices(sceneRequest, view, proj)) {
			return;
		}

		static ImGuizmo::OPERATION sOperation = ImGuizmo::TRANSLATE;
		static ImGuizmo::MODE      sMode      = ImGuizmo::WORLD;

		if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
			if (ImGui::IsKeyPressed(ImGuiKey_W)) {
				sOperation = ImGuizmo::TRANSLATE;
			}
			if (ImGui::IsKeyPressed(ImGuiKey_E)) {
				sOperation = ImGuizmo::ROTATE;
			}
			if (ImGui::IsKeyPressed(ImGuiKey_R)) {
				sOperation = ImGuizmo::SCALE;
			}
		}

		ImGuizmo::SetOrthographic(false);
		ImGuizmo::SetDrawlist();
		ImGuizmo::SetRect(imagePos.x, imagePos.y, drawWidth, drawHeight);

		const bool  useSnap = sOperation != ImGuizmo::SCALE;
		const float snap[3] = {
			sOperation == ImGuizmo::ROTATE ? mAngleSnapDegree : mGridSnap,
			sOperation == ImGuizmo::ROTATE ? mAngleSnapDegree : mGridSnap,
			sOperation == ImGuizmo::ROTATE ? mAngleSnapDegree : mGridSnap
		};

		if (
			ImGuizmo::Manipulate(
				*view.m,
				*proj.m,
				sOperation,
				sMode,
				*world.m,
				nullptr,
				useSnap ? snap : nullptr
			)
		) {
			const Mat4 editedWorld = world;
			Mat4       localMat    = editedWorld;

			const Vec3 localScale  = localMat.GetScale();
			const Mat4 rotationMat = localMat;

			transform->SetPosition(localMat.GetTranslate());
			transform->SetRotation(rotationMat.ToQuaternion());
			transform->SetScale(localScale);
		}
	}

	void UEditorRuntime::DrawMainMenu() {
		// メニューバーを少し高くする

		// const float titlebarHeight =
		// 	ImGui::GetFontSize() + 1.5f * ImGui::GetStyle().FramePadding.y;

		ImGui::PushStyleVar(
			ImGuiStyleVar_FramePadding,
			ImVec2(12, 12)
		);

		if (ImGui::BeginMainMenuBar()) {
			ImGui::PushStyleVar(
				ImGuiStyleVar_WindowPadding,
				ImVec2(kPopupPadding, kPopupPadding)
			);

			// ロゴメニュー は色を変える
			ImGui::PushStyleColor(
				ImGuiCol_Text,
				ImVec4(0.13f, 0.5f, 1.0f, 1.0f)
			);

			const std::string iconText =
				" " + StrUtil::ConvertToUtf8(kIconArrowBack) + " ";

			if (ImGuiWidgets::BeginMainMenu(iconText.c_str())) {
				ImGui::PopStyleColor();
				if (ImGuiWidgets::MenuItemWithIcon(
					"About Unnamed", kIconInfo
				)) {}
				ImGui::EndMenu();
			} else {
				ImGui::PopStyleColor();
			}

			if (ImGuiWidgets::BeginMainMenu("File")) {
				const auto currentPath =
					std::string(mEditorWorld.GetLoadedScenePath());

				if (
					ImGuiWidgets::MenuItemWithIcon("Save", kIconSave)
				) {
					const std::string savePath = currentPath.empty() ?
						                             "./content/core/scenes/sandbox.json" :
						                             currentPath;
					SaveSceneAs(savePath);
				}

				if (
					ImGuiWidgets::MenuItemWithIcon(
						"Save As (sandbox.json)", kIconSaveAs
					)
				) {
					SaveSceneAs("./content/core/scenes/sandbox.json");
				}

				ImGui::Separator();

				if (ImGuiWidgets::MenuItemWithIcon("Import", kIconDownload)) {}
				if (ImGuiWidgets::MenuItemWithIcon("Export", kIconUpload)) {}

				ImGui::Separator();

				if (ImGuiWidgets::MenuItemWithIcon("Exit", kIconPower)) {
					ServiceLocator::Get<ConsoleSystem>()->
						ExecuteCommand("quit");
				}
				ImGui::EndMenu();
			}

			if (ImGuiWidgets::BeginMainMenu("Edit")) {
				if (
					ImGuiWidgets::MenuItemWithIcon("Settings", kIconSettings)
				) {}

				ImGui::EndMenu();
			}

			if (ImGuiWidgets::BeginMainMenu("Window")) {
				if (ImGuiWidgets::MenuItemWithIcon(
					"Profiler", kIconAvgTime, nullptr, mShowProfilerWindow
				)) {
					mShowProfilerWindow = !mShowProfilerWindow;
				}

				ImGui::EndMenu();
			}

			float availableHeight = ImGui::GetContentRegionAvail().y;

			// プレイコントロール
			{
				ImGui::PushStyleVar(
					ImGuiStyleVar_FrameBorderSize, 1.0f
				);

				const float windowPaddingY = ImGui::GetStyle().WindowPadding.y;

				ImGui::SetCursorPosY(windowPaddingY * 0.25f);

				if (!mEditorWorld.IsPlaying()) {
					ImGui::PushStyleColor(
						ImGuiCol_Text,
						ImVec4(0.34f, 0.59f, 0.36f, 1.0f)
					);
					if (
						ImGuiWidgets::IconButton(
							kIconPlay,
							nullptr,
							{availableHeight, availableHeight},
							3.0f
						)
					) {
						mEditorWorld.StartPlayInEditor();
					}
					ImGui::PopStyleColor();
				} else {
					ImGui::PushStyleColor(
						ImGuiCol_Text,
						ImVec4(0.79f, 0.31f, 0.31f, 1.0f)
					);
					if (ImGuiWidgets::IconButton(
						kIconStop, nullptr, {availableHeight, availableHeight},
						3.0f
					)) {
						mEditorWorld.StopPlayInEditor();
					}
					ImGui::PopStyleColor();
				}

				ImGui::PopStyleVar();
			}

			ImGui::PopStyleVar();

			ImGui::EndMainMenuBar();
		}

		ImGui::PopStyleVar();
	}

	void UEditorRuntime::DrawViewportTopBar() {
		const float toolbarHeight            = ImGui::GetFontSize() * 2.0f;
		const float toolbarHeightWithPadding =
			toolbarHeight +
			ImGui::GetStyle().WindowPadding.y * 2.0f;

		auto currentCursorPos = ImGui::GetCursorScreenPos();

		// ツールバーの背景をImGuiに描いてもらう
		ImGui::GetWindowDrawList()->AddRectFilled(
			currentCursorPos,
			ImVec2(
				currentCursorPos.x + ImGui::GetContentRegionAvail().x,
				currentCursorPos.y + ImGui::GetContentRegionAvail().y
			),
			ImGui::GetColorU32(ImGuiCol_ChildBg),
			ImGui::GetStyle().ChildRounding,
			ImDrawFlags_RoundCornersAll
		);

		// ツールバーのアイコンを描くためにカーソルを少し内側に移動
		ImGui::SetCursorScreenPos(
			ImVec2(
				currentCursorPos.x + ImGui::GetStyle().WindowPadding.x,
				currentCursorPos.y + ImGui::GetStyle().WindowPadding.y
			)
		);

		const auto toolbarIconSize = ImVec2(
			toolbarHeight * 2.75f,
			toolbarHeight
		);

		// 選択
		{
			constexpr float iconScale = 1.0f;

			if (
				ImGuiWidgets::IconButton(
					kIconVertex,
					"Vertices",
					toolbarIconSize,
					iconScale,
					ImGuiDir_Right
				)
			) {
				mNotification->PushNotification(
					"未実装",
					"頂点選択はまだ実装されていません。",
					NOTIFY_TYPE::WARNING,
					10.0f
				);
			}

			ImGui::SameLine();

			ImGuiWidgets::IconButton(
				kIconEdge,
				"Edges",
				toolbarIconSize,
				iconScale,
				ImGuiDir_Right
			);

			ImGui::SameLine();

			ImGuiWidgets::IconButton(
				kIconFace,
				"Faces",
				toolbarIconSize,
				iconScale,
				ImGuiDir_Right
			);

			ImGui::SameLine();

			ImGuiWidgets::IconButton(
				kIconMesh,
				"Meshes",
				toolbarIconSize,
				iconScale,
				ImGuiDir_Right
			);

			ImGui::SameLine();

			ImGuiWidgets::IconButton(
				kIconObject,
				"Object",
				toolbarIconSize,
				iconScale,
				ImGuiDir_Right
			);

			ImGui::SameLine();

			ImGuiWidgets::IconButton(
				kIconGroup,
				"Group",
				toolbarIconSize,
				iconScale,
				ImGuiDir_Right
			);
		}

		ImGui::SameLine();

		// 画面モード切替
		{
			const char* viewportModeLabel = "Fit";
			switch (mViewportRenderMode) {
				case EDITOR_VIEWPORT_RENDER_MODE::FIT_VIEWPORT
				: viewportModeLabel = "Fit";
					break;
				case EDITOR_VIEWPORT_RENDER_MODE::FIXED_ASPECT_16_9
				: viewportModeLabel = "16:9";
					break;
				case EDITOR_VIEWPORT_RENDER_MODE::HD720
				: viewportModeLabel = "HD";
					break;
				case EDITOR_VIEWPORT_RENDER_MODE::FHD1080
				: viewportModeLabel = "FHD";
					break;
				default: break;
			}
			ImGui::SetNextItemWidth(140.0f);
			if (ImGui::BeginCombo("RenderMode", viewportModeLabel)) {
				const bool fitSelected =
					mViewportRenderMode ==
					EDITOR_VIEWPORT_RENDER_MODE::FIT_VIEWPORT;
				if (ImGui::Selectable("Fit", fitSelected)) {
					mViewportRenderMode =
						EDITOR_VIEWPORT_RENDER_MODE::FIT_VIEWPORT;
				}
				const bool fixedSelected =
					mViewportRenderMode ==
					EDITOR_VIEWPORT_RENDER_MODE::FIXED_ASPECT_16_9;
				if (ImGui::Selectable("Fixed 16:9", fixedSelected)) {
					mViewportRenderMode =
						EDITOR_VIEWPORT_RENDER_MODE::FIXED_ASPECT_16_9;
				}
				const bool hdSelected =
					mViewportRenderMode ==
					EDITOR_VIEWPORT_RENDER_MODE::HD720;
				if (ImGui::Selectable("HD (1280x720)", hdSelected)) {
					mViewportRenderMode =
						EDITOR_VIEWPORT_RENDER_MODE::HD720;
				}
				const bool fhdSelected =
					mViewportRenderMode ==
					EDITOR_VIEWPORT_RENDER_MODE::FHD1080;
				if (ImGui::Selectable("FHD (1920x1080)", fhdSelected)) {
					mViewportRenderMode =
						EDITOR_VIEWPORT_RENDER_MODE::FHD1080;
				}
				ImGui::EndCombo();
			}

			ImGui::SameLine();
		}

		// ビューポートのためにツールバー分カーソルを下げる
		ImGui::SetCursorScreenPos(
			{
				currentCursorPos.x,
				currentCursorPos.y + toolbarHeightWithPadding
			}
		);
	}

	void UEditorRuntime::DrawSideBar() {
		constexpr ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoTitleBar |           // タイトルバーなし
			ImGuiWindowFlags_NoScrollbar |          // スクロールバーなし
			ImGuiWindowFlags_NoCollapse |           // 折りたたみなし
			ImGuiWindowFlags_NoResize |             // サイズ変更なし
			ImGuiWindowFlags_NoSavedSettings |      // 設定保存なし
			ImGuiWindowFlags_NoFocusOnAppearing |   // 表示時にフォーカスなし
			ImGuiWindowFlags_NoBringToFrontOnFocus; // フォーカス時に前面に出ない

		// TODO: サイドバーのアイコンサイズをConVarで調整できるようにする
		constexpr float iconSize = 40.0f; // アイコンのサイズを設定
		// TODO: サイドバーのアイコンスケールをConVarで調整できるようにする
		constexpr float iconScale = 0.75f;

		constexpr auto toolbarIconSize = ImVec2(iconSize, iconSize);

		const auto windowPadding = ImGui::GetStyle().WindowPadding;

		ImGui::SetNextWindowBgAlpha(1.0f);
		if (
			ImGui::BeginViewportSideBar(
				"##LeftToolbar", ImGui::GetMainViewport(), ImGuiDir_Left,
				toolbarIconSize.x + windowPadding.x * 2.0f, // アイコン幅 + 左右パディング
				flags
			)
		) {
			// ツールバー
			{
				ImGuiWidgets::IconButton(
					kIconSelect,
					"",
					toolbarIconSize,
					iconScale,
					ImGuiDir_None
				);

				ImGuiWidgets::IconButton(
					kIconMove,
					"",
					toolbarIconSize,
					iconScale,
					ImGuiDir_None
				);

				ImGuiWidgets::IconButton(
					kIconRotate,
					"",
					toolbarIconSize,
					iconScale,
					ImGuiDir_None
				);

				ImGuiWidgets::IconButton(
					kIconScale,
					"",
					toolbarIconSize,
					iconScale,
					ImGuiDir_None
				);

				ImGuiWidgets::IconButton(
					kIconPivot,
					"",
					toolbarIconSize,
					iconScale,
					ImGuiDir_None
				);

				ImGuiWidgets::IconButton(
					kIconObject,
					"",
					toolbarIconSize,
					iconScale,
					ImGuiDir_None
				);

				ImGuiWidgets::IconButton(
					kIconBox,
					"",
					toolbarIconSize,
					iconScale,
					ImGuiDir_None
				);

				ImGuiWidgets::IconButton(
					kIconTexture,
					"",
					toolbarIconSize,
					iconScale,
					ImGuiDir_None
				);
			}
			ImGui::End();
		}
	}

	void UEditorRuntime::DrawStatusBar() {
		constexpr ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoTitleBar |           // タイトルバーなし
			ImGuiWindowFlags_NoScrollbar |          // スクロールバーなし
			ImGuiWindowFlags_NoCollapse |           // 折りたたみなし
			ImGuiWindowFlags_NoResize |             // サイズ変更なし
			ImGuiWindowFlags_NoSavedSettings |      // 設定保存なし
			ImGuiWindowFlags_NoFocusOnAppearing |   // 表示時にフォーカスなし
			ImGuiWindowFlags_NoBringToFrontOnFocus; // フォーカス時に前面に出ない

		if (
			ImGui::BeginViewportSideBar(
				"##MainStatusBar",
				ImGui::GetMainViewport(),
				ImGuiDir_Down,
				32.0f,
				flags
			)
		) {
			const float statusBarWidth = ImGui::GetWindowWidth();

			// アングルスナップ
			{
				// 5.625° 360°の64分割
				// 11.25°は360°の32分割
				// 22.5°は360°の16分割
				constexpr std::array items = {
					"0.25°", "0.5°", "1°",
					"5°", "5.625°", "11.25°",
					"15°", "22.5°", "30°",
					"45°", "90°"
				};
				static uint32_t itemCurrentIndex = 6u; // デフォルトは15°
				const char*     comboLabel       = items[itemCurrentIndex];

				ImGui::Text("Angle: ");

				ImGui::SameLine();

				// コンボボックスの幅をステータスバーの幅に合わせて調整
				ImGui::PushItemWidth(statusBarWidth * 0.2f);
				ImGui::PushID("AngleSnapCombo");
				if (ImGui::BeginCombo("##angleSnap", comboLabel)) {
					for (int n = 0; n < items.size(); ++n) {
						const bool isSelected =
							static_cast<int>(itemCurrentIndex) == n;
						if (ImGui::Selectable(items[n], isSelected)) {
							itemCurrentIndex = n;
						}
						if (isSelected) {
							ImGui::SetItemDefaultFocus();
						}
					}
					ImGui::EndCombo();
				}
				constexpr auto size = static_cast<uint32_t>(items.size());
				ImGuiWidgets::HandleHoveredComboMenuMouseWheelScroll(
					itemCurrentIndex, size
				);
				// 選択された文字列を浮動小数点数に変換してangleSnapに設定
				mAngleSnapDegree = std::stof(items[itemCurrentIndex]);
				ImGui::PopID();
				ImGui::PopItemWidth();
			}

			ImGui::SameLine();

			// グリッドスナップ
			{
				constexpr std::array items = {
					"0.125", "0.25", "0.5",
					"1", "2", "4", "8",
					"16", "32", "64", "128",
					"256", "512", "1024"
				};
				static uint32_t itemCurrentIndex = 9u; // デフォルトは64
				const char*     comboLabel       = items[itemCurrentIndex];

				ImGui::Text("Grid: ");

				ImGui::SameLine();

				// コンボボックスの幅をステータスバーの幅に合わせて調整
				ImGui::PushItemWidth(statusBarWidth * 0.2f);
				ImGui::PushID("GridSnapCombo");
				if (ImGui::BeginCombo("##gridSnap", comboLabel)) {
					for (int n = 0; n < items.size(); ++n) {
						const bool isSelected =
							static_cast<int>(itemCurrentIndex) == n;
						if (ImGui::Selectable(items[n], isSelected)) {
							itemCurrentIndex = n;
						}
						if (isSelected) {
							ImGui::SetItemDefaultFocus();
						}
					}
					ImGui::EndCombo();
				}
				constexpr auto size = static_cast<uint32_t>(items.size());
				ImGuiWidgets::HandleHoveredComboMenuMouseWheelScroll(
					itemCurrentIndex, size

				);
				// 選択された文字列を浮動小数点数に変換してgridSnapに設定
				mGridSnap = std::stof(items[itemCurrentIndex]);
				ImGui::PopID();
				ImGui::PopItemWidth();
			}

			ImGui::End();
		}
	}

	void UEditorRuntime::DrawSceneHierarchy() {
		UScene* scene = GetHierarchyScene();
		if (!scene) {
			return;
		}

		if (!ImGui::Begin("Outliner")) {
			ImGui::End();
			return;
		}

		static uint64_t              renameEntityId = 0;
		static std::string           renameFolderPath;
		static std::array<char, 256> renameBuffer = {};

		auto openRenameEntity = [&](UEntity& entity) {
			renameEntityId = entity.GetGuid();
			renameFolderPath.clear();
			std::fill(renameBuffer.begin(), renameBuffer.end(), '\0');
			const std::string name(entity.GetName());
			memcpy(
				renameBuffer.data(),
				name.c_str(),
				std::min(name.size(), renameBuffer.size() - 1)
			);
			ImGui::OpenPopup("OutlinerRenamePopup");
		};

		auto openRenameFolder = [&](std::string_view folderPath) {
			renameEntityId   = 0;
			renameFolderPath = std::string(folderPath);
			std::fill(renameBuffer.begin(), renameBuffer.end(), '\0');
			const std::string leafName = GetFolderLeafName(folderPath);
			memcpy(
				renameBuffer.data(),
				leafName.c_str(),
				std::min(leafName.size(), renameBuffer.size() - 1)
			);
			ImGui::OpenPopup("OutlinerRenamePopup");
		};

		constexpr ImGuiTableFlags tableFlags =
			ImGuiTableFlags_ScrollY |
			ImGuiTableFlags_BordersV |
			ImGuiTableFlags_BordersOuterH |
			ImGuiTableFlags_RowBg |
			ImGuiTableFlags_NoBordersInBodyUntilResize;

		auto createEntity = [&](std::string_view folderPath) {
			UEntity& entity = scene->CreateEntity(
				"Entity", scene->AllocateEntityId(), false
			);
			entity.SetFolderPath(folderPath);
			scene->AddFolder(folderPath);
			entity.SetVisible(true);
			[[maybe_unused]] auto* addedTransform =
				entity.AddComponent<TransformComponent>();
			mSelectedEntityId = entity.GetGuid();
		};
		auto createFolder = [&](std::string_view folderPath) {
			scene->AddFolder(MakeUniqueFolderPath(*scene, folderPath));
		};

		if (ImGui::BeginPopupContextWindow(
			"OutlinerWindowContext",
			ImGuiPopupFlags_MouseButtonRight |
			ImGuiPopupFlags_NoOpenOverItems
		)) {
			if (ImGui::MenuItem("Add Entity")) {
				createEntity("");
			}
			if (ImGui::MenuItem("Add Folder")) {
				createFolder("");
			}
			ImGui::EndPopup();
		}

		float iconScale = 1.5f;

		auto addButtonSize = ImVec2(
			ImGui::GetFontSize() * iconScale,
			ImGui::GetFontSize() * iconScale
		);

		if (
			ImGuiWidgets::IconButton(
				kIconAdd,
				nullptr,
				addButtonSize,
				iconScale
			)
		) {
			ImGui::OpenPopup("OutlinerAddPopup");
		}
		if (ImGui::BeginPopup("OutlinerAddPopup")) {
			if (ImGui::MenuItem("Add Entity")) {
				createEntity("");
			}
			if (ImGui::MenuItem("Add Folder")) {
				createFolder("");
			}
			ImGui::EndPopup();
		}

		if (ImGui::BeginTable("OutlinerTable", 3, tableFlags)) {
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoHide);
			ImGui::TableSetupColumn(
				"Visible", ImGuiTableColumnFlags_WidthFixed
			);
			ImGui::TableSetupColumn(
				"Active", ImGuiTableColumnFlags_WidthFixed
			);
			ImGui::TableHeadersRow();

			HierarchyFolderNode root = {};
			BuildHierarchyTree(root, *scene);
			uint64_t    pendingDeleteEntityId = 0;
			std::string pendingDeleteFolderPath;
			bool        pendingCreateEntity = false;
			bool        pendingCreateFolder = false;
			std::string pendingCreateFolderPath;
			uint64_t    pendingParentChildEntityId  = 0;
			uint64_t    pendingParentTargetEntityId = 0;
			uint64_t    pendingMoveEntityId         = 0;
			std::string pendingMoveEntityFolderPath;
			std::string pendingMoveFolderSourcePath;
			std::string pendingMoveFolderTargetPath;
			std::unordered_map<uint64_t, std::vector<UEntity*>>
				childEntitiesByParent;
			for (const auto& entityPtr : scene->GetEntities()) {
				if (!entityPtr) {
					continue;
				}
				UEntity*    entity    = entityPtr.get();
				const auto* transform = entity->GetComponent<
					TransformComponent>();
				if (!transform || !transform->Parent() || !transform->Parent()->
				    GetOwner()) {
					continue;
				}
				childEntitiesByParent[transform->Parent()->GetOwner()->
				                                 GetGuid()]
					.emplace_back(entity);
			}

			std::function<void(UEntity*)> drawEntityNode;
			drawEntityNode = [&](UEntity* entity) {
				if (!entity) {
					return;
				}
				std::vector<UEntity*> childrenInSameFolder;
				if (const auto it = childEntitiesByParent.
						find(entity->GetGuid());
					it != childEntitiesByParent.end()) {
					for (UEntity* child : it->second) {
						if (
							child &&
							std::string(child->GetFolderPath()) ==
							std::string(entity->GetFolderPath())
						) {
							childrenInSameFolder.emplace_back(child);
						}
					}
				}

				const bool isSelected = entity->GetGuid() == mSelectedEntityId;
				ImGui::PushID(static_cast<int>(entity->GetGuid()));
				ImGui::TableNextRow();
				ImGui::TableNextColumn();

				const bool hasChildren = !childrenInSameFolder.empty();
				const ImGuiTreeNodeFlags nodeFlags =
					ImGuiTreeNodeFlags_SpanFullWidth |
					(hasChildren ?
						 ImGuiTreeNodeFlags_DefaultOpen :
						 ImGuiTreeNodeFlags_Leaf |
						 ImGuiTreeNodeFlags_NoTreePushOnOpen) |
					(isSelected ? ImGuiTreeNodeFlags_Selected : 0);
				const bool opened = ImGui::TreeNodeEx(
					reinterpret_cast<void*>(
						entity->GetGuid()
					),
					nodeFlags,
					"%s",
					entity->GetName().data()
				);
				if (ImGui::IsItemClicked()) {
					mSelectedEntityId = entity->GetGuid();
				}
				if (ImGui::BeginDragDropSource()) {
					const uint64_t guid = entity->GetGuid();
					ImGui::SetDragDropPayload(
						"OUTLINER_ENTITY", &guid, sizeof(guid)
					);
					ImGui::TextUnformatted(entity->GetName().data());
					ImGui::EndDragDropSource();
				}
				if (ImGui::BeginDragDropTarget()) {
					if (const ImGuiPayload* payload =
						ImGui::AcceptDragDropPayload(
							"OUTLINER_ENTITY"
						)) {
						const uint64_t draggedGuid = *static_cast<const uint64_t
							*>(
							payload->Data
						);
						if (draggedGuid != entity->GetGuid()) {
							pendingParentChildEntityId  = draggedGuid;
							pendingParentTargetEntityId = entity->GetGuid();
						}
					}
					if (const ImGuiPayload* payload =
						ImGui::AcceptDragDropPayload(
							"OUTLINER_FOLDER"
						)) {
						const char* sourcePath = static_cast<const char*>(
							payload->Data
						);
						if (sourcePath) {
							pendingMoveFolderSourcePath = sourcePath;
							pendingMoveFolderTargetPath = std::string(
								entity->GetFolderPath()
							);
						}
					}
					ImGui::EndDragDropTarget();
				}
				if (ImGui::BeginPopupContextItem("EntityContext")) {
					if (ImGui::MenuItem("Add Entity")) {
						pendingCreateEntity     = true;
						pendingCreateFolderPath = std::string(
							entity->GetFolderPath()
						);
					}
					if (ImGui::MenuItem("Add Folder")) {
						pendingCreateFolder     = true;
						pendingCreateFolderPath = std::string(
							entity->GetFolderPath()
						);
					}
					if (ImGui::MenuItem("Rename")) {
						openRenameEntity(*entity);
					}
					if (ImGui::MenuItem("Delete")) {
						pendingDeleteEntityId = entity->GetGuid();
					}
					ImGui::EndPopup();
				}

				auto fontSize = ImVec2(
					ImGui::GetFontSize(), ImGui::GetFontSize()
				);

				ImGui::TableNextColumn();
				const bool visible = entity->IsVisible();
				if (
					ImGui::Button(
						StrUtil::ConvertToUtf8(
							visible ? kIconVisibility : kIconVisibilityOff
						).c_str(),
						fontSize
					)
				) {
					entity->SetVisible(!visible);
				}

				ImGui::TableNextColumn();
				const bool active = entity->IsActive();
				if (
					ImGui::Button(
						StrUtil::ConvertToUtf8(
							active ? kIconCheckBoxOn : kIconCheckBoxOff
						).c_str(),
						fontSize
					)
				) {
					entity->SetActive(!active);
				}

				if (opened && hasChildren) {
					for (UEntity* child : childrenInSameFolder) {
						drawEntityNode(child);
					}
					ImGui::TreePop();
				}

				ImGui::PopID();
			};

			std::function<void(const HierarchyFolderNode&, const std::string&)>
				drawFolder;
			drawFolder = [&](
				const HierarchyFolderNode& node,
				const std::string&         folderPath
			) {
					if (!folderPath.empty()) {
						const std::string displayName = GetFolderLeafName(
							folderPath
						);
						ImGui::TableNextRow();
						ImGui::TableNextColumn();
						bool opened = ImGui::TreeNodeEx(
							folderPath.c_str(),
							ImGuiTreeNodeFlags_SpanFullWidth |
							ImGuiTreeNodeFlags_DefaultOpen,
							"%s",
							displayName.c_str()
						);
						if (ImGui::BeginDragDropSource()) {
							ImGui::SetDragDropPayload(
								"OUTLINER_FOLDER",
								folderPath.c_str(),
								folderPath.size() + 1
							);
							ImGui::TextUnformatted(displayName.c_str());
							ImGui::EndDragDropSource();
						}
						if (ImGui::BeginDragDropTarget()) {
							if (const ImGuiPayload* payload =
								ImGui::AcceptDragDropPayload(
									"OUTLINER_ENTITY"
								)) {
								const uint64_t draggedGuid = *static_cast<
									const uint64_t*>(payload->Data);
								pendingMoveEntityId         = draggedGuid;
								pendingMoveEntityFolderPath = folderPath;
							}
							if (const ImGuiPayload* payload =
								ImGui::AcceptDragDropPayload(
									"OUTLINER_FOLDER"
								)) {
								const char* sourcePath = static_cast<const char
									*>(
									payload->Data
								);
								if (
									sourcePath &&
									folderPath != sourcePath &&
									!IsFolderEqualOrDescendant(
										folderPath, sourcePath
									)
								) {
									pendingMoveFolderSourcePath = sourcePath;
									pendingMoveFolderTargetPath = folderPath;
								}
							}
							ImGui::EndDragDropTarget();
						}
						if (ImGui::BeginPopupContextItem("FolderContext")) {
							if (ImGui::MenuItem("Add Entity")) {
								pendingCreateEntity     = true;
								pendingCreateFolderPath = folderPath;
							}
							if (ImGui::MenuItem("Add Folder")) {
								pendingCreateFolder     = true;
								pendingCreateFolderPath = folderPath;
							}
							if (ImGui::MenuItem("Rename")) {
								openRenameFolder(folderPath);
							}
							if (ImGui::MenuItem("Delete")) {
								pendingDeleteFolderPath = folderPath;
							}
							ImGui::EndPopup();
						}
						ImGui::TableNextColumn();
						ImGui::TableNextColumn();
						if (!opened) {
							return;
						}
					}

					for (const auto& [childName, childNode] : node.children) {
						const std::string childPath = folderPath.empty() ?
							childName :
							folderPath + "/" + childName;
						drawFolder(childNode, childPath);
					}

					for (UEntity* entity : node.entities) {
						const auto* transform = entity ?
							                        entity->GetComponent<
								                        TransformComponent>() :
							                        nullptr;
						const auto* parentTransform = transform ?
							transform->Parent() :
							nullptr;
						const UEntity* parentEntity = parentTransform ?
							parentTransform->GetOwner() :
							nullptr;
						if (
							parentEntity &&
							std::string(parentEntity->GetFolderPath()) ==
							std::string(entity->GetFolderPath())
						) {
							continue;
						}
						drawEntityNode(entity);
					}

					if (!folderPath.empty()) {
						ImGui::TreePop();
					}
				};

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TreeNodeEx(
				"__outliner_root__",
				ImGuiTreeNodeFlags_Leaf |
				ImGuiTreeNodeFlags_NoTreePushOnOpen |
				ImGuiTreeNodeFlags_SpanFullWidth,
				"Root"
			);
			if (ImGui::BeginDragDropTarget()) {
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(
					"OUTLINER_FOLDER"
				)) {
					const char* sourcePath = static_cast<const char*>(payload->
						Data);
					if (sourcePath) {
						pendingMoveFolderSourcePath = sourcePath;
						pendingMoveFolderTargetPath.clear();
					}
				}
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(
					"OUTLINER_ENTITY"
				)) {
					const uint64_t draggedGuid = *static_cast<const uint64_t*>(
						payload->Data
					);
					pendingMoveEntityId = draggedGuid;
					pendingMoveEntityFolderPath.clear();
				}
				ImGui::EndDragDropTarget();
			}
			if (ImGui::BeginPopupContextItem("RootContext")) {
				if (ImGui::MenuItem("Add Entity")) {
					pendingCreateEntity = true;
				}
				if (ImGui::MenuItem("Add Folder")) {
					pendingCreateFolder = true;
				}
				ImGui::EndPopup();
			}
			ImGui::TableNextColumn();
			ImGui::TableNextColumn();

			drawFolder(root, "");

			ImGui::EndTable();

			if (pendingCreateEntity) {
				createEntity(pendingCreateFolderPath);
			}
			if (pendingCreateFolder) {
				createFolder(pendingCreateFolderPath);
			}
			if (pendingMoveEntityId != 0) {
				if (UEntity* entity = scene->FindEntity(pendingMoveEntityId)) {
					entity->SetFolderPath(pendingMoveEntityFolderPath);
					scene->AddFolder(pendingMoveEntityFolderPath);
				}
			}
			if (!pendingMoveFolderSourcePath.empty()) {
				scene->MoveFolderSubtree(
					pendingMoveFolderSourcePath, pendingMoveFolderTargetPath
				);
			}
			if (
				pendingParentChildEntityId != 0 &&
				pendingParentTargetEntityId != 0
			) {
				UEntity* childEntity = scene->FindEntity(
					pendingParentChildEntityId
				);
				UEntity* parentEntity = scene->FindEntity(
					pendingParentTargetEntityId
				);
				if (childEntity && parentEntity) {
					auto* childTransform = childEntity->GetComponent<
						TransformComponent>();
					auto* parentTransform = parentEntity->GetComponent<
						TransformComponent>();
					if (
						childTransform &&
						parentTransform &&
						childTransform != parentTransform &&
						!IsTransformAncestor(childTransform, parentTransform)
					) {
						childTransform->SetParent(parentTransform);
						childEntity->SetFolderPath(
							parentEntity->GetFolderPath()
						);
						scene->AddFolder(parentEntity->GetFolderPath());
					}
				}
			}
			if (pendingDeleteEntityId != 0) {
				if (mSelectedEntityId == pendingDeleteEntityId) {
					mSelectedEntityId = 0;
				}
				scene->DestroyEntity(pendingDeleteEntityId);
			}
			if (!pendingDeleteFolderPath.empty()) {
				scene->DeleteFolderSubtree(pendingDeleteFolderPath);
			}
		}

		if (ImGui::BeginPopup("OutlinerRenamePopup")) {
			ImGui::InputText(
				"##Rename", renameBuffer.data(), renameBuffer.size()
			);
			if (ImGui::Button("Apply")) {
				if (renameEntityId != 0) {
					if (UEntity* entity = scene->FindEntity(renameEntityId)) {
						entity->SetName(renameBuffer.data());
					}
				} else if (!renameFolderPath.empty()) {
					scene->RenameFolderSubtree(
						renameFolderPath, renameBuffer.data()
					);
				}
				renameEntityId = 0;
				renameFolderPath.clear();
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) {
				renameEntityId = 0;
				renameFolderPath.clear();
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		ImGui::End();
	}

	void UEditorRuntime::DrawInspector() {
		if (!ImGui::Begin("Inspector")) {
			ImGui::End();
			return;
		}

		UEntity* entity = GetSelectedEntity();
		if (!entity) {
			ImGui::TextUnformatted("No selected entity.");
			ImGui::End();
			return;
		}

		std::array<char, 256> nameBuffer = {};
		const std::string     name(entity->GetName());
		memcpy(
			nameBuffer.data(), name.c_str(), std::min(
				name.size(), nameBuffer.size() - 1
			)
		);
		if (ImGui::InputText("Name", nameBuffer.data(), nameBuffer.size())) {
			entity->SetName(nameBuffer.data());
		}

		std::array<char, 256> folderBuffer = {};
		const std::string     folder(entity->GetFolderPath());
		memcpy(
			folderBuffer.data(), folder.c_str(), std::min(
				folder.size(), folderBuffer.size() - 1
			)
		);
		if (ImGui::InputText(
			"FolderPath", folderBuffer.data(), folderBuffer.size()
		)) {
			entity->SetFolderPath(folderBuffer.data());
			if (UScene* hierarchyScene = GetHierarchyScene()) {
				hierarchyScene->AddFolder(folderBuffer.data());
			}
		}

		bool active = entity->IsActive();
		if (ImGui::Checkbox("Active", &active)) {
			entity->SetActive(active);
		}

		if (!entity->GetComponent<TransformComponent>()) {
			if (ImGui::Button("Add Transform")) {
				[[maybe_unused]] auto* addedTransform =
					entity->AddComponent<TransformComponent>();
			}
		}

		if (!entity->GetComponent<StaticMeshRendererComponent>()) {
			if (ImGui::Button("Add StaticMeshRenderer")) {
				[[maybe_unused]] auto* addedMesh =
					entity->AddComponent<StaticMeshRendererComponent>();
			}
		}

		if (!entity->GetComponent<SkeletalMeshRendererComponent>()) {
			if (ImGui::Button("Add SkeletalMeshRenderer")) {
				[[maybe_unused]] auto* addedSkel =
					entity->AddComponent<SkeletalMeshRendererComponent>();
			}
		}

		if (!entity->GetComponent<PortalComponent>()) {
			if (ImGui::Button("Add Portal")) {
				[[maybe_unused]] auto* addedPortal =
					entity->AddComponent<PortalComponent>();
			}
		}

		// すべてのコンポーネントのインスペクタUIを描画
		entity->ForEachComponent(
			[&](UBaseComponent& component) {
				const std::string label = std::string(
					                          component.GetComponentName()
				                          ) +
				                          "###" + std::to_string(
					                          component.GetGuid()
				                          );
				bool       componentActive = component.IsActive();
				const bool open = ImGuiUtil::CollapsingHeaderWithCheckbox(
					label.c_str(),
					&componentActive,
					ImGuiTreeNodeFlags_DefaultOpen
				);
				if (componentActive != component.IsActive()) {
					component.SetActive(componentActive);
				}
				if (open) {
					component.DrawInspectorImGui();
				}
			}
		);

		ImGui::End();
	}

	void UEditorRuntime::DrawProfilerWindow() {
		if (!mShowProfilerWindow) {
			return;
		}

		const bool bOpen = ImGui::Begin("Profiler", &mShowProfilerWindow);

		if (bOpen) {
			const UProfiler* profiler = ServiceLocator::Get<UProfiler>();
			if (!profiler) {
				ImGui::TextUnformatted("Profiler is unavailable.");
				ImGui::End();
				return;
			}

			ImGui::Text(
				"Frames: %llu  History: %u",
				static_cast<unsigned long long>(profiler->GetFrameCount()),
				profiler->GetHistorySize()
			);
			ImGui::Separator();

			const auto& samples   = profiler->GetSamples();
			float       globalMax = 1.0f;
			for (const auto& sample : samples) {
				if (!sample.history || sample.history->empty()) {
					continue;
				}
				globalMax = std::max(globalMax, sample.maxMs);
			}

			const ImVec2 canvasSize(
				ImGui::GetContentRegionAvail().x,
				180.0f
			);
			ImGui::InvisibleButton("##ProfilerCanvas", canvasSize);
			const ImVec2 canvasMin = ImGui::GetItemRectMin();
			const ImVec2 canvasMax = ImGui::GetItemRectMax();
			ImDrawList*  drawList  = ImGui::GetWindowDrawList();

			drawList->AddRectFilled(
				canvasMin,
				canvasMax,
				ImGui::GetColorU32(ImGuiCol_FrameBg),
				4.0f
			);
			drawList->AddRect(
				canvasMin,
				canvasMax,
				ImGui::GetColorU32(ImGuiCol_Border),
				4.0f
			);

			const float paddedMinX  = canvasMin.x + 8.0f;
			const float paddedMaxX  = canvasMax.x - 8.0f;
			const float paddedMinY  = canvasMin.y + 8.0f;
			const float paddedMaxY  = canvasMax.y - 8.0f;
			const float graphWidth  = std::max(1.0f, paddedMaxX - paddedMinX);
			const float graphHeight = std::max(1.0f, paddedMaxY - paddedMinY);

			for (int grid = 1; grid < 4; ++grid) {
				const float t = static_cast<float>(grid) / 4.0f;
				const float y = paddedMaxY - graphHeight * t;
				drawList->AddLine(
					ImVec2(paddedMinX, y),
					ImVec2(paddedMaxX, y),
					ImGui::GetColorU32(ImVec4(1, 1, 1, 0.08f))
				);
			}

			const size_t colorCount = std::max<size_t>(1, samples.size());
			for (const auto& sample : samples) {
				if (!sample.history || sample.history->size() < 2) {
					continue;
				}

				const ImU32 color = ImColor::HSV(
					static_cast<float>(sample.colorIndex % colorCount) /
					static_cast<float>(colorCount),
					0.75f,
					0.95f
				);

				ImVec2 prev    = {};
				bool   hasPrev = false;
				for (size_t i = 0; i < sample.history->size(); ++i) {
					const size_t historyIndex =
						(sample.historyWriteIndex + i) % sample.history->size();
					const float value = (*sample.history)[historyIndex];
					const float x     = paddedMinX +
					                    static_cast<float>(i) /
					                    static_cast<float>(
						                    sample.history->size() -
						                    1) *
					                    graphWidth;
					const float y = paddedMaxY -
					                value / (globalMax * 1.1f) * graphHeight;
					const ImVec2 point(
						x, std::clamp(y, paddedMinY, paddedMaxY)
					);
					if (hasPrev) {
						drawList->AddLine(prev, point, color, 1.5f);
					}
					prev    = point;
					hasPrev = true;
				}
			}

			for (const auto& sample : samples) {
				if (!sample.history || sample.history->empty()) {
					continue;
				}

				const ImVec4 color = ImColor::HSV(
					static_cast<float>(sample.colorIndex % colorCount) /
					static_cast<float>(colorCount),
					0.75f,
					0.95f
				);

				ImGui::PushStyleColor(ImGuiCol_Text, color);
				ImGui::BulletText(
					"%.*s  %.3f ms  avg %.3f ms  max %.3f ms",
					static_cast<int>(sample.name.size()),
					sample.name.data(),
					sample.currentMs,
					sample.averageMs,
					sample.maxMs
				);
				ImGui::PopStyleColor();
			}
		}
		ImGui::End();
	}

	void UEditorRuntime::DrawViewport(const float deltaTime) {
		if (!ImGui::Begin("Viewport")) {
			ImGui::End();
			return;
		}

		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);

		DrawViewportTopBar();

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

		const ImVec2 avail            = ImGui::GetContentRegionAvail();
		const float  width            = std::max(1.0f, avail.x);
		const float  height           = std::max(1.0f, avail.y);
		mViewportSizeChangedThisFrame =
			std::abs(width - mLastViewportSize.x) > 0.5f ||
			std::abs(height - mLastViewportSize.y) > 0.5f;
		mViewportPanelWidth  = width;
		mViewportPanelHeight = height;
		mLastViewportSize    = Vec2(width, height);

		const Render::SceneRenderRequest sceneRequest =
			BuildSceneRenderRequest();

		const bool requestChanged =
			sceneRequest.mode != mLastSceneRenderRequest.mode ||
			sceneRequest.viewportPanelWidth !=
			mLastSceneRenderRequest.viewportPanelWidth ||
			sceneRequest.viewportPanelHeight !=
			mLastSceneRenderRequest.viewportPanelHeight ||
			sceneRequest.preferRealtimeResize !=
			mLastSceneRenderRequest.preferRealtimeResize ||
			sceneRequest.presentToSwapChain !=
			mLastSceneRenderRequest.presentToSwapChain ||
			sceneRequest.clearSwapChainWhenNotPresenting !=
			mLastSceneRenderRequest.clearSwapChainWhenNotPresenting;
		if (requestChanged) {
			mLastSceneRenderRequest = sceneRequest;
		}

		mRenderModule.SetSceneRenderRequest(sceneRequest);

		if (mSceneSrvCpu.ptr != 0 && mSceneTextureId != 0) {
			float drawWidth  = width;
			float drawHeight = height;
			if (mViewportRenderMode !=
			    EDITOR_VIEWPORT_RENDER_MODE::FIT_VIEWPORT) {
				constexpr float targetAspect = 16.0f / 9.0f;
				if (drawWidth / drawHeight > targetAspect) {
					drawWidth = drawHeight * targetAspect;
				} else {
					drawHeight = drawWidth / targetAspect;
				}

				const ImVec2 cursor = ImGui::GetCursorPos();
				ImGui::SetCursorPos(
					ImVec2(
						cursor.x + (width - drawWidth) * 0.5f,
						cursor.y + (height - drawHeight) * 0.5f
					)
				);
			}

			const ImTextureID tex = mImGuiLayer.GetOrCreateTextureId(
				mSceneTextureId, mSceneSrvRevision, mSceneSrvCpu
			);
			const ImVec2 imagePos = ImGui::GetCursorScreenPos();
			ImGuiWidgets::ImageWithRounding(
				tex, ImVec2(drawWidth, drawHeight), 8.0f,
				ImDrawFlags_RoundCornersBottom
			);

			const bool imageHovered = ImGui::IsItemHovered();
			if (imageHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
				mViewportLookActive = true;
			}
			if (!ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
				mViewportLookActive = false;
			}
			DrawViewportGizmo(
				sceneRequest,
				Vec2(imagePos.x, imagePos.y),
				drawWidth,
				drawHeight
			);

			mViewportPosition = {imagePos.x, imagePos.y};
			mViewportSize     = Vec2(drawWidth, drawHeight);

			DrawViewportOverlay(deltaTime);
		} else {
			mViewportLookActive = false;
			ImGui::TextUnformatted("Scene output is not ready.");
		}

		ImGui::PopStyleVar(2);

		ImGui::End();
	}

	void UEditorRuntime::DrawViewportOverlay(const float deltaTime) {
		{
			const EditorCameraComponent* camera = mEditorWorld.
				GetEditorCamera();
			if (!camera || !camera->IsMoveSpeedPopupVisible()) {
				return;
			}

			constexpr float windowMinWidth = 256.0f;

			// 表示するテキストを作成
			const std::string text =
				StrUtil::ConvertToUtf8(kIconSpeed) +
				std::format(" {:.2f}", camera->GetMoveSpeed());

			// 先にテキストサイズを計算
			const auto textSize = ImGui::CalcTextSize(text.c_str());

			// ウィンドウのパディングを取得
			const auto windowPadding = ImGui::GetStyle().WindowPadding;

			// ウィンドウサイズを計算
			const ImVec2 windowSize(
				// 通常は最小幅を使うが、テキストが反逆してきたら媚びへつらう
				std::max(textSize.x, windowMinWidth) + windowPadding.x,
				textSize.y + windowPadding.y
			);

			// ウィンドウの位置をビューポートの下中央に設定
			ImVec2 windowPos(
				mViewportPosition.x + mViewportSize.x * 0.5f,
				mViewportPosition.y + mViewportSize.y * 0.9f
			);
			windowPos.x -= windowSize.x * 0.5f;
			windowPos.y -= windowSize.y - 8.0f;

			ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
			ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);

			// camera->GetMoveSpeedPopupTimer() は 0.0f から 1.0f までの値が返る
			// 0.75から1.0の間はフェードアウトする
			constexpr float fadeOutStart       = 0.5f;
			constexpr float invFadeOutDuration = 1.0f / (1.0f - fadeOutStart);

			const float alphaMultiplier =
				camera->GetMoveSpeedPopupTimer() < fadeOutStart ?
					1.0f :
					1.0f - (camera->GetMoveSpeedPopupTimer() - fadeOutStart) *
					invFadeOutDuration;

			// フェードもリッチに行こうズ!
			const float alpha = Math::CubicBezier(
				alphaMultiplier,
				0.2f, 0.0f, 0.0f, 1.0f
			);

			ImGui::SetNextWindowBgAlpha(alpha * 0.9f);

			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
			ImGui::PushStyleVar(
				ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f)
			);

			ImGui::Begin(
				"##editorCameraMoveSpeedPopup", nullptr,
				ImGuiWindowFlags_NoTitleBar |
				ImGuiWindowFlags_NoResize |
				ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_NoSavedSettings |
				ImGuiWindowFlags_NoFocusOnAppearing |
				ImGuiWindowFlags_NoScrollbar |
				ImGuiWindowFlags_NoInputs
			);

			// テキストをウィンドウの中央に配置
			ImGui::SetCursorPos(
				ImVec2(
					(windowSize.x - textSize.x) * 0.5f,
					(windowSize.y - textSize.y) * 0.25f
				)
			);
			ImGui::TextUnformatted(text.c_str());

			// 速度は0.125fから65536.0f までの範囲。
			// 0.125fのときはバーが空、65536.0fのときはバーが満タンになるようにする。
			static float currentProgress = 0.0f;
			float        progress;
			if (camera->GetMoveSpeed() <= 0.125f) {
				progress = 0.0f;
			} else if (
				camera->GetMoveSpeed() >= 65536.0f) {
				progress = 1.0f;
			} else {
				const float logMin   = std::log2(0.125f);
				const float logMax   = std::log2(65536.0f);
				const float logValue = std::log2(camera->GetMoveSpeed());
				progress             = (logValue - logMin) / (logMax - logMin);
			}

			float t = Math::CubicBezier(
				20.0f * deltaTime, 0.2f, 0.0f, 0.0f, 1.0f
			);

			currentProgress = std::lerp(currentProgress, progress, t);

			ImGui::SetCursorPosY(
				ImGui::GetCursorPosY() + ImGui::GetContentRegionAvail().y - 4.0f
			);

			ImGui::ProgressBar(
				currentProgress, ImVec2(
					ImGui::GetContentRegionAvail().x,
					4.0f
				),
				""
			);

			ImGui::End();
			ImGui::PopStyleVar(2);
		}
	}

	UEntity* UEditorRuntime::GetSelectedEntity() const {
		const UScene* scene = GetHierarchyScene();
		if (!scene || mSelectedEntityId == 0) {
			return nullptr;
		}
		return const_cast<UScene*>(scene)->FindEntity(mSelectedEntityId);
	}

	bool UEditorRuntime::SaveSceneAs(const std::string& path) {
		const UScene* scene = mEditorWorld.GetEditableScene();
		if (!scene) {
			return false;
		}
		if (!USceneSerializer::SaveToFile(*scene, path)) {
			return false;
		}
		mEditorWorld.SetLoadedScenePath(path);
		return true;
	}
}

#endif
