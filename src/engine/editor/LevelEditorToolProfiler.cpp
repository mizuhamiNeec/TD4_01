#ifdef _DEBUG
#include "LevelEditorTool.h"

#include <algorithm>
#include <imgui.h>

#include "engine/profiler/Profiler.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"

namespace Unnamed {
	void LevelEditorTool::DrawProfilerWindow() {
		if (!mShowProfilerWindow) {
			return;
		}

		const bool bOpen = ImGui::Begin("Profiler", &mShowProfilerWindow);

		if (bOpen) {
			const Profiler* profiler = ServiceLocator::Get<Profiler>();
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
}

#endif

