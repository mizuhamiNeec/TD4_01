#include <pch.h>

#include <engine/state/EditorModeState.h>

#ifdef _DEBUG
#include <ImGuizmo.h>
#include <imgui_internal.h>
#endif

#include <engine/Engine.h>

namespace Unnamed {
	void EditorModeState::OnEnter(Engine& engine) { engine.CreateEditor(); }

	void EditorModeState::OnExit(Engine& engine) { engine.DestroyEditor(); }

	void EditorModeState::Update(Engine& engine, float deltaTime) {
#ifdef _DEBUG
		if (engine.GetEditor()) { engine.GetEditor()->DrawMenuBars(); }

		// DockSpace
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
				ImGuiDockNodeFlags_PassthruCentralNode;
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
				const ImGuiID dockSpaceId = ImGui::GetID("MyDockSpace");
				ImGui::DockSpace(
					dockSpaceId, ImVec2(0.0f, 0.0f), dockSpaceFlags
				);
			}

			ImGui::End();
		}

		static auto tint = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
		static auto bg   = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

		ImGuiWindowFlags windowFlags =
			ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoScrollWithMouse;

		if (ImGuizmo::IsUsing()) { windowFlags |= ImGuiWindowFlags_NoMove; }

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		ImGui::Begin("ViewPort", nullptr, windowFlags);
		ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());

		ImVec2     avail = ImGui::GetContentRegionAvail();
		const auto ptr   = engine.GetActivePingSrvGpuPtr();

		static int prevW = 0, prevH = 0;
		int        w     = static_cast<int>(avail.x);
		int        h     = static_cast<int>(avail.y);
		if ((w != prevW || h != prevH) && w > 0 && h > 0) {
			prevW = w;
			prevH = h;
		}

		if (ptr) {
			auto        desc      = engine.GetActivePingRtvDesc();
			const float texWidth  = static_cast<float>(desc.Width);
			const float texHeight = static_cast<float>(desc.Height);

			const float availAspect = avail.x / avail.y;
			const float texAspect   = texWidth / texHeight;

			ImVec2 drawSize = avail;
			if (availAspect > texAspect) {
				drawSize.x = avail.y * texAspect;
			} else { drawSize.y = avail.x / texAspect; }

			float titleBarHeight = ImGui::GetCurrentWindow()->TitleBarHeight;

			ImVec2 offset = {
				(avail.x - drawSize.x) * 0.5f,
				(avail.y - drawSize.y) * 0.5f + titleBarHeight
			};
			ImGui::SetCursorPos(offset);

			ImVec2 viewportScreenPos = ImGui::GetCursorScreenPos();

			ImGui::ImageWithBg(
				ptr,
				drawSize,
				ImVec2(0, 0), ImVec2(1, 1),
				bg, tint
			);

			engine.SetViewportFromEditor(
				viewportScreenPos.x, viewportScreenPos.y, drawSize.x, drawSize.y
			);
		}
		ImGui::End();
		ImGui::PopStyleVar();
#endif

		if (engine.GetEditor()) { engine.GetEditor()->Update(deltaTime); }

#ifdef _DEBUG
		ImGui::Begin("Post Process");
		for (int i = 0; i < static_cast<int>(engine.GetPostChainSize()); ++i) {
			if (i == 2) { continue; }
			if (auto* pp = engine.GetPostProcessAt(i)) {
				pp->Update(deltaTime);
			}
		}
		ImGui::End();
#endif
	}

	void EditorModeState::Render(Engine& engine) {
		// エディターの描画は任せる
		if (engine.GetEditor()) { engine.GetEditor()->Render(); }
	}
}
