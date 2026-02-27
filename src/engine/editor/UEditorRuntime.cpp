#ifdef _DEBUG
#include "UEditorRuntime.h"

#include <algorithm>
#include <array>

#include <imgui.h>

#include "engine/platform/Window.h"
#include "engine/platform/WindowManager.h"
#include "engine/render/RenderModule.h"
#include "engine/scene/USceneSerializer.h"
#include "engine/ui/UImGuiLayer.h"
#include "engine/unnamed/framework/components/SkeletalMeshRendererComponent.h"
#include "engine/unnamed/framework/components/StaticMeshRendererComponent.h"
#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/entity/UEntity.h"
#include "engine/world/UEditorWorld.h"

namespace Unnamed {
	UEditorRuntime::UEditorRuntime(
		UEditorWorld&         editorWorld,
		WindowManager&        windowManager,
		Render::RenderModule& renderModule,
		UImGuiLayer&          imguiLayer
	) : mEditorWorld(editorWorld),
	    mWindowManager(windowManager),
	    mRenderModule(renderModule),
	    mImGuiLayer(imguiLayer) {}

	void UEditorRuntime::SetSceneOutput(
		const uint32_t                    textureId,
		const D3D12_CPU_DESCRIPTOR_HANDLE srvCpu,
		const Vec2                        size
	) {
		mSceneTextureId = textureId;
		mSceneSrvCpu    = srvCpu;
		mSceneSize      = size;
	}

	void UEditorRuntime::BuildUi() {
		ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

		DrawMainMenu();
		DrawToolbar();
		DrawViewport();

		if (mPresentMode == EditorPresentMode::ViewportPanel) {
			DrawSceneHierarchy();
			DrawInspector();
		}
	}

	void UEditorRuntime::DrawMainMenu() {
		if (!ImGui::BeginMainMenuBar()) { return; }

		if (ImGui::BeginMenu("File")) {
			const std::string currentPath =
				std::string(mEditorWorld.GetLoadedScenePath());
			if (ImGui::MenuItem("Save")) {
				const std::string savePath = currentPath.empty() ?
					                             "./content/core/scenes/sandbox.json" :
					                             currentPath;
				SaveSceneAs(savePath);
			}

			if (ImGui::MenuItem("Save As (sandbox.json)")) {
				SaveSceneAs("./content/core/scenes/sandbox.json");
			}

			ImGui::EndMenu();
		}

		ImGui::EndMainMenuBar();
	}

	void UEditorRuntime::DrawToolbar() {
		if (!ImGui::Begin("Toolbar")) {
			ImGui::End();
			return;
		}

		if (!mEditorWorld.IsPlaying()) {
			if (ImGui::Button("Play")) { mEditorWorld.StartPlayInEditor(); }
		} else if (ImGui::Button("Stop")) { mEditorWorld.StopPlayInEditor(); }

		ImGui::SameLine();
		const char* modeLabel = mPresentMode ==
		                        EditorPresentMode::ViewportPanel ?
			                        "Fullscreen" :
			                        "Viewport";
		if (ImGui::Button(modeLabel)) {
			if (
				Window* window = mWindowManager.FindWindowById(
					mWindowManager.GetMainWindowId()
				)
			) {
				window->ToggleFullscreen();
				mPresentMode = mPresentMode ==
				               EditorPresentMode::ViewportPanel ?
					               EditorPresentMode::FullscreenSwapChain :
					               EditorPresentMode::ViewportPanel;
			}
		}

		ImGui::SameLine();
		const char* viewportModeLabel = "Fit";
		switch (mViewportRenderMode) {
			case EditorViewportRenderMode::FitViewport
			: viewportModeLabel = "Fit";
				break;
			case EditorViewportRenderMode::FixedAspect16x9
			: viewportModeLabel = "16:9";
				break;
			case EditorViewportRenderMode::HD720: viewportModeLabel = "HD";
				break;
			case EditorViewportRenderMode::FHD1080: viewportModeLabel = "FHD";
				break;
			default: break;
		}
		ImGui::SetNextItemWidth(140.0f);
		if (ImGui::BeginCombo("RenderMode", viewportModeLabel)) {
			const bool fitSelected =
				mViewportRenderMode == EditorViewportRenderMode::FitViewport;
			if (ImGui::Selectable("Fit", fitSelected)) {
				mViewportRenderMode = EditorViewportRenderMode::FitViewport;
			}
			const bool fixedSelected =
				mViewportRenderMode ==
				EditorViewportRenderMode::FixedAspect16x9;
			if (ImGui::Selectable("Fixed 16:9", fixedSelected)) {
				mViewportRenderMode = EditorViewportRenderMode::FixedAspect16x9;
			}
			const bool hdSelected =
				mViewportRenderMode == EditorViewportRenderMode::HD720;
			if (ImGui::Selectable("HD (1280x720)", hdSelected)) {
				mViewportRenderMode = EditorViewportRenderMode::HD720;
			}
			const bool fhdSelected =
				mViewportRenderMode == EditorViewportRenderMode::FHD1080;
			if (ImGui::Selectable("FHD (1920x1080)", fhdSelected)) {
				mViewportRenderMode = EditorViewportRenderMode::FHD1080;
			}
			ImGui::EndCombo();
		}

		ImGui::End();
	}

	void UEditorRuntime::DrawSceneHierarchy() {
		UScene* scene = mEditorWorld.GetEditableScene();
		if (!scene) { return; }

		if (!ImGui::Begin("SceneHierarchy")) {
			ImGui::End();
			return;
		}

		if (ImGui::Button("Add Entity")) {
			UEntity& e = scene->CreateEntity(
				"Entity", scene->AllocateEntityId(), false
			);
			[[maybe_unused]] auto* addedTransform =
				e.AddComponent<TransformComponent>();
			mSelectedEntityId = e.GetGuid();
		}

		for (const auto& ePtr : scene->GetEntities()) {
			if (!ePtr) { continue; }
			const UEntity& entity     = *ePtr;
			const bool     isSelected = entity.GetGuid() == mSelectedEntityId;
			if (ImGui::Selectable(entity.GetName().data(), isSelected)) {
				mSelectedEntityId = entity.GetGuid();
			}
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

		bool active = entity->IsActive();
		if (ImGui::Checkbox("Active", &active)) { entity->SetActive(active); }

		auto* transform = entity->GetComponent<TransformComponent>();
		if (!transform) {
			if (ImGui::Button("Add Transform")) {
				[[maybe_unused]] auto* addedTransform =
					entity->AddComponent<TransformComponent>();
				transform = entity->GetComponent<TransformComponent>();
			}
		}

		if (transform) {
			const Vec3 pos  = transform->Position();
			float      p[3] = {pos.x, pos.y, pos.z};
			if (ImGui::DragFloat3("Position", p, 0.05f)) {
				transform->SetPosition(Vec3(p[0], p[1], p[2]));
			}

			const Quaternion rot  = transform->Rotation();
			float            r[4] = {rot.x, rot.y, rot.z, rot.w};
			if (ImGui::DragFloat4("Rotation(xyzw)", r, 0.01f)) {
				transform->SetRotation(Quaternion(r[0], r[1], r[2], r[3]));
			}

			const Vec3 scale = transform->Scale();
			float      s[3]  = {scale.x, scale.y, scale.z};
			if (ImGui::DragFloat3("Scale", s, 0.05f, 0.001f, 1000.0f)) {
				transform->SetScale(Vec3(s[0], s[1], s[2]));
			}
		}

		auto* meshRenderer = entity->GetComponent<
			StaticMeshRendererComponent>();
		if (!meshRenderer) {
			if (ImGui::Button("Add StaticMeshRenderer")) {
				[[maybe_unused]] auto* addedMesh =
					entity->AddComponent<StaticMeshRendererComponent>();
				meshRenderer = entity->GetComponent<
					StaticMeshRendererComponent>();
			}
		}

		if (meshRenderer) {
			std::array<char, 512> meshPath = {};
			std::array<char, 512> matPath = {};
			const std::string     mesh = meshRenderer->GetMeshPath();
			const std::string     mat = meshRenderer->GetMaterialInstancePath();
			memcpy(
				meshPath.data(), mesh.c_str(), std::min(
					mesh.size(), meshPath.size() - 1
				)
			);
			memcpy(
				matPath.data(), mat.c_str(), std::min(
					mat.size(), matPath.size() - 1
				)
			);

			if (ImGui::InputText(
				"MeshPath", meshPath.data(), meshPath.size()
			)) { meshRenderer->SetMeshPath(meshPath.data()); }
			if (ImGui::InputText(
				"MaterialInstancePath", matPath.data(), matPath.size()
			)) { meshRenderer->SetMaterialInstancePath(matPath.data()); }
		}

		auto* skeletalRenderer = entity->GetComponent<
			SkeletalMeshRendererComponent>();
		if (!skeletalRenderer) {
			if (ImGui::Button("Add SkeletalMeshRenderer")) {
				[[maybe_unused]] auto* addedSkel =
					entity->AddComponent<SkeletalMeshRendererComponent>();
				skeletalRenderer = entity->GetComponent<
					SkeletalMeshRendererComponent>();
			}
		}

		if (skeletalRenderer) {
			std::array<char, 512> skelMeshPath = {};
			std::array<char, 512> skelMatPath = {};
			const std::string     skelMesh = skeletalRenderer->GetMeshPath();
			const std::string     skelMat = skeletalRenderer->
				GetMaterialInstancePath();
			memcpy(
				skelMeshPath.data(),
				skelMesh.c_str(),
				std::min(skelMesh.size(), skelMeshPath.size() - 1)
			);
			memcpy(
				skelMatPath.data(),
				skelMat.c_str(),
				std::min(skelMat.size(), skelMatPath.size() - 1)
			);

			if (ImGui::InputText(
				"SkeletalMeshPath", skelMeshPath.data(), skelMeshPath.size()
			)) { skeletalRenderer->SetMeshPath(skelMeshPath.data()); }
			if (ImGui::InputText(
				"SkeletalMaterialPath", skelMatPath.data(), skelMatPath.size()
			)) {
				skeletalRenderer->SetMaterialInstancePath(skelMatPath.data());
			}
		}

		ImGui::End();
	}

	void UEditorRuntime::DrawViewport() {
		ImGuiWindowFlags flags = 0;
		if (mPresentMode == EditorPresentMode::FullscreenSwapChain) {
			const ImGuiViewport* mainVp = ImGui::GetMainViewport();
			ImGui::SetNextWindowPos(mainVp->Pos);
			ImGui::SetNextWindowSize(mainVp->Size);
			flags = ImGuiWindowFlags_NoDecoration |
			        ImGuiWindowFlags_NoMove |
			        ImGuiWindowFlags_NoSavedSettings;
		}

		if (!ImGui::Begin("Viewport", nullptr, flags)) {
			ImGui::End();
			return;
		}

		const ImVec2 avail   = ImGui::GetContentRegionAvail();
		const float  width   = std::max(1.0f, avail.x);
		const float  height  = std::max(1.0f, avail.y);
		mViewportPanelWidth  = width;
		mViewportPanelHeight = height;

		Render::SceneRenderRequest sceneRequest = {};
		uint32_t requestWidth = static_cast<uint32_t>(width);
		uint32_t requestHeight = static_cast<uint32_t>(height);
		sceneRequest.preferRealtimeResize = true;
		switch (mViewportRenderMode) {
			case EditorViewportRenderMode::FitViewport
			: sceneRequest.mode = Render::SCENE_RENDER_MODE::FIT_VIEWPORT;
				break;
			case EditorViewportRenderMode::FixedAspect16x9
			: sceneRequest.mode = Render::SCENE_RENDER_MODE::FIXED_ASPECT_16X9;
				break;
			case EditorViewportRenderMode::HD720
			: sceneRequest.mode = Render::SCENE_RENDER_MODE::HD_720P;
				break;
			case EditorViewportRenderMode::FHD1080
			: sceneRequest.mode = Render::SCENE_RENDER_MODE::FHD_1080P;
				break;
			default: break;
		}
		const bool dynamicMode = sceneRequest.mode ==
		                         Render::SCENE_RENDER_MODE::FIT_VIEWPORT ||
		                         sceneRequest.mode ==
		                         Render::SCENE_RENDER_MODE::FIXED_ASPECT_16X9;
		if (dynamicMode) {
			requestWidth  = std::max(8u, requestWidth / 8u * 8u);
			requestHeight = std::max(8u, requestHeight / 8u * 8u);
		}
		sceneRequest.viewportPanelWidth  = requestWidth;
		sceneRequest.viewportPanelHeight = requestHeight;

		const bool requestChanged =
			sceneRequest.mode != mLastSceneRenderRequest.mode ||
			(dynamicMode &&
			 (sceneRequest.viewportPanelWidth !=
			  mLastSceneRenderRequest.viewportPanelWidth ||
			  sceneRequest.viewportPanelHeight !=
			  mLastSceneRenderRequest.viewportPanelHeight ||
			  sceneRequest.preferRealtimeResize !=
			  mLastSceneRenderRequest.preferRealtimeResize));
		if (requestChanged) {
			mSkipViewportImageThisFrame = true;
			mLastSceneRenderRequest     = sceneRequest;
		}

		mRenderModule.SetSceneRenderRequest(sceneRequest);

		if (mSkipViewportImageThisFrame) {
			ImGui::TextUnformatted("Viewport resizing... (next frame updates)");
			mSkipViewportImageThisFrame = false;
		} else if (mSceneSrvCpu.ptr != 0 && mSceneTextureId != 0) {
			float drawWidth  = width;
			float drawHeight = height;
			if (mViewportRenderMode != EditorViewportRenderMode::FitViewport) {
				constexpr float targetAspect = 16.0f / 9.0f;
				if (drawWidth / drawHeight > targetAspect) {
					drawWidth = drawHeight * targetAspect;
				} else { drawHeight = drawWidth / targetAspect; }

				const ImVec2 cursor = ImGui::GetCursorPos();
				ImGui::SetCursorPos(
					ImVec2(
						cursor.x + (width - drawWidth) * 0.5f,
						cursor.y + (height - drawHeight) * 0.5f
					)
				);
			}

			const ImTextureID tex = mImGuiLayer.GetOrCreateTextureId(
				mSceneTextureId, mSceneSrvCpu
			);
			ImGui::Image(tex, ImVec2(drawWidth, drawHeight));
		} else { ImGui::TextUnformatted("Scene output is not ready."); }

		ImGui::End();
	}

	UEntity* UEditorRuntime::GetSelectedEntity() const {
		const UScene* scene = mEditorWorld.GetEditableScene();
		if (!scene || mSelectedEntityId == 0) { return nullptr; }
		return const_cast<UScene*>(scene)->FindEntity(mSelectedEntityId);
	}

	bool UEditorRuntime::SaveSceneAs(const std::string& path) {
		const UScene* scene = mEditorWorld.GetEditableScene();
		if (!scene) { return false; }
		if (!USceneSerializer::SaveToFile(*scene, path)) { return false; }
		mEditorWorld.SetLoadedScenePath(path);
		return true;
	}
}

#endif
