#ifdef _DEBUG
#include "SequenceTimelinePanel.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <imgui.h>
#include <string>
#include <vector>

#include "core/guidgenerator/GuidGenerator.h"

#include "SequenceEditorController.h"

namespace Unnamed {
	namespace {
		struct TimelineRow final {
			int32_t     trackIndex   = -1;
			int32_t     sectionIndex = -1;
			std::string label        = {};
		};

		[[nodiscard]] uint64_t AllocateStableId() {
			static GuidGenerator generator;
			return generator.Alloc();
		}

		void EnsureCurveChannelId(SequenceRichCurveAssetData& ioCurve) {
			if (ioCurve.channelId == 0) {
				ioCurve.channelId = AllocateStableId();
			}
		}

		void AddOrUpdateFloatKey(
			SequenceRichCurveAssetData& ioCurve,
			const int64_t               frame,
			const float                 value
		) {
			EnsureCurveChannelId(ioCurve);
			for (SequenceFloatKeyAssetData& key : ioCurve.keys) {
				if (key.frame != frame) {
					continue;
				}
				key.value = value;
				key.interpolation = SEQUENCE_INTERPOLATION_MODE::MODE_LINEAR;
				return;
			}

			ioCurve.keys.emplace_back(
				SequenceFloatKeyAssetData{
					.keyId = AllocateStableId(),
					.frame = frame,
					.value = value,
					.arriveTangent = 0.0f,
					.leaveTangent = 0.0f,
					.interpolation = SEQUENCE_INTERPOLATION_MODE::MODE_LINEAR,
				}
			);
			std::ranges::sort(
				ioCurve.keys,
				[](const SequenceFloatKeyAssetData& lhs, const SequenceFloatKeyAssetData& rhs) {
					return lhs.frame < rhs.frame;
				}
			);
		}

		[[nodiscard]] const char* TrackTypeLabel(const SEQUENCE_TRACK_TYPE type) {
			switch (type) {
				case SEQUENCE_TRACK_TYPE::TRANSFORM: return "Transform";
				case SEQUENCE_TRACK_TYPE::SKELETAL_CONTROL: return "Skeletal";
				case SEQUENCE_TRACK_TYPE::CAMERA_CUT: return "CameraCut";
				case SEQUENCE_TRACK_TYPE::EVENT: return "Event";
				case SEQUENCE_TRACK_TYPE::VISIBILITY: return "Visibility";
				case SEQUENCE_TRACK_TYPE::ACTIVATION: return "Activation";
				case SEQUENCE_TRACK_TYPE::PROPERTY_FLOAT: return "PropertyFloat";
				case SEQUENCE_TRACK_TYPE::PROPERTY_BOOL: return "PropertyBool";
				case SEQUENCE_TRACK_TYPE::PROPERTY_VEC3: return "PropertyVec3";
				default: return "Unknown";
			}
		}

		void AppendCurveFrames(
			const SequenceRichCurveAssetData& curve,
			std::vector<int64_t>&             outFrames
		) {
			for (const SequenceFloatKeyAssetData& key : curve.keys) {
				outFrames.emplace_back(key.frame);
			}
		}

		void AppendSectionFrames(
			const SequenceTrackAssetData&   track,
			const SequenceSectionAssetData& section,
			std::vector<int64_t>&           outFrames
		) {
			switch (track.trackType) {
				case SEQUENCE_TRACK_TYPE::EVENT:
					for (const SequenceEventKeyAssetData& key : section.eventKeys) {
						outFrames.emplace_back(key.frame);
					}
					break;
				case SEQUENCE_TRACK_TYPE::CAMERA_CUT:
					for (const SequenceCameraCutKeyAssetData& key : section.cameraCutKeys) {
						outFrames.emplace_back(key.frame);
					}
					break;
				case SEQUENCE_TRACK_TYPE::PROPERTY_BOOL:
				case SEQUENCE_TRACK_TYPE::VISIBILITY:
				case SEQUENCE_TRACK_TYPE::ACTIVATION:
					for (const SequenceBoolKeyAssetData& key : section.boolKeys) {
						outFrames.emplace_back(key.frame);
					}
					break;
				case SEQUENCE_TRACK_TYPE::PROPERTY_FLOAT:
					AppendCurveFrames(section.floatCurve, outFrames);
					break;
				case SEQUENCE_TRACK_TYPE::PROPERTY_VEC3:
					AppendCurveFrames(section.vec3XCurve, outFrames);
					AppendCurveFrames(section.vec3YCurve, outFrames);
					AppendCurveFrames(section.vec3ZCurve, outFrames);
					break;
				case SEQUENCE_TRACK_TYPE::TRANSFORM:
					AppendCurveFrames(section.transformPosX, outFrames);
					AppendCurveFrames(section.transformPosY, outFrames);
					AppendCurveFrames(section.transformPosZ, outFrames);
					AppendCurveFrames(section.transformRotX, outFrames);
					AppendCurveFrames(section.transformRotY, outFrames);
					AppendCurveFrames(section.transformRotZ, outFrames);
					AppendCurveFrames(section.transformRotW, outFrames);
					AppendCurveFrames(section.transformScaleX, outFrames);
					AppendCurveFrames(section.transformScaleY, outFrames);
					AppendCurveFrames(section.transformScaleZ, outFrames);
					break;
				case SEQUENCE_TRACK_TYPE::SKELETAL_CONTROL:
					AppendCurveFrames(section.skeletal.weightCurve, outFrames);
					AppendCurveFrames(section.skeletal.playbackCurve, outFrames);
					AppendCurveFrames(section.skeletal.speedCurve, outFrames);
					break;
			}
		}

		[[nodiscard]] std::vector<int64_t> CollectSectionKeyFrames(
			const SequenceTrackAssetData&   track,
			const SequenceSectionAssetData& section
		) {
			std::vector<int64_t> frames = {};
			AppendSectionFrames(track, section, frames);
			std::ranges::sort(frames);
			frames.erase(std::unique(frames.begin(), frames.end()), frames.end());
			return frames;
		}

		[[nodiscard]] std::string FormatTimecode(
			const float   frame,
			const int32_t displayRate
		) {
			const int64_t safeRate   = std::max<int64_t>(1, displayRate);
			const int64_t frameInt   = std::max<int64_t>(0, static_cast<int64_t>(std::llround(frame)));
			const int64_t totalSec   = frameInt / safeRate;
			const int64_t minutePart = totalSec / 60;
			const int64_t secondPart = totalSec % 60;
			const int64_t framePart  = frameInt % safeRate;
			return std::format("{:02}:{:02}:{:02}", minutePart, secondPart, framePart);
		}

		[[nodiscard]] int64_t ComputeMajorTickStep(const float pixelsPerFrame) {
			static constexpr std::array<int64_t, 16> kSteps = {
				1, 2, 5,
				10, 20, 50,
				100, 200, 500,
				1000, 2000, 5000,
				10000, 20000, 50000,
				100000,
			};

			for (const int64_t step : kSteps) {
				if (static_cast<float>(step) * pixelsPerFrame >= 72.0f) {
					return step;
				}
			}
			return kSteps.back();
		}

		void EnsureDefaultChannelForTrack(
			SequenceEditorSelection&  ioSelection,
			const SEQUENCE_TRACK_TYPE trackType
		) {
			switch (trackType) {
				case SEQUENCE_TRACK_TYPE::TRANSFORM:
					ioSelection.floatChannel = SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_POS_X;
					break;
				case SEQUENCE_TRACK_TYPE::PROPERTY_FLOAT:
					ioSelection.floatChannel = SEQUENCE_EDITOR_FLOAT_CHANNEL::FLOAT;
					break;
				case SEQUENCE_TRACK_TYPE::PROPERTY_VEC3:
					ioSelection.floatChannel = SEQUENCE_EDITOR_FLOAT_CHANNEL::VEC3_X;
					break;
				case SEQUENCE_TRACK_TYPE::SKELETAL_CONTROL:
					ioSelection.floatChannel = SEQUENCE_EDITOR_FLOAT_CHANNEL::SKELETAL_WEIGHT;
					break;
				default:
					ioSelection.floatChannel = SEQUENCE_EDITOR_FLOAT_CHANNEL::NONE;
					break;
			}
		}

		[[nodiscard]] int64_t QuantizeFrame(const float frame) {
			return static_cast<int64_t>(std::llround(std::max(0.0f, frame)));
		}

		[[nodiscard]] bool AddKeyAtPlayhead(SequenceEditorController& controller) {
			SequenceEditorSelection selection = controller.GetSelection();
			const SequenceEditorDocument* document = controller.GetActiveDocument();
			if (!document) {
				return false;
			}

			const SequenceAuthoringData& data = document->GetAuthoringData();
			if (
				selection.trackIndex < 0 ||
				selection.trackIndex >= static_cast<int32_t>(data.tracks.size())
			) {
				return false;
			}

		const SequenceTrackAssetData& track = data.tracks[selection.trackIndex];
		const SEQUENCE_TRACK_TYPE selectedTrackType = track.trackType;
		if (track.sections.empty()) {
			return false;
		}
			if (
				selection.sectionIndex < 0 ||
				selection.sectionIndex >= static_cast<int32_t>(track.sections.size())
			) {
				selection.sectionIndex = 0;
			}

			const int64_t frame = QuantizeFrame(controller.GetPlayheadFrame());
			const bool modified = controller.ModifyActiveDocument(
				[selection, frame](SequenceAuthoringData& ioData) {
					if (
						selection.trackIndex < 0 ||
						selection.trackIndex >= static_cast<int32_t>(ioData.tracks.size())
					) {
						return;
					}
					SequenceTrackAssetData& localTrack = ioData.tracks[selection.trackIndex];
					if (
						selection.sectionIndex < 0 ||
						selection.sectionIndex >= static_cast<int32_t>(localTrack.sections.size())
					) {
						return;
					}
					SequenceSectionAssetData& section = localTrack.sections[selection.sectionIndex];

					switch (localTrack.trackType) {
						case SEQUENCE_TRACK_TYPE::EVENT:
							section.eventKeys.emplace_back(
								SequenceEventKeyAssetData{
									.keyId = AllocateStableId(),
									.frame = frame,
									.cueId = "sequence.event",
									.sourceEntityGuid = 0,
									.cueValue = 1.0f,
									.cueValue2 = 0.0f,
									.consoleCommand = "",
								}
							);
							std::ranges::sort(
								section.eventKeys,
								[](const SequenceEventKeyAssetData& lhs, const SequenceEventKeyAssetData& rhs) {
									return lhs.frame < rhs.frame;
								}
							);
							break;
						case SEQUENCE_TRACK_TYPE::PROPERTY_FLOAT:
							AddOrUpdateFloatKey(section.floatCurve, frame, 0.0f);
							break;
						case SEQUENCE_TRACK_TYPE::PROPERTY_VEC3:
							AddOrUpdateFloatKey(section.vec3XCurve, frame, 0.0f);
							AddOrUpdateFloatKey(section.vec3YCurve, frame, 0.0f);
							AddOrUpdateFloatKey(section.vec3ZCurve, frame, 0.0f);
							break;
						case SEQUENCE_TRACK_TYPE::TRANSFORM:
							AddOrUpdateFloatKey(section.transformPosX, frame, 0.0f);
							AddOrUpdateFloatKey(section.transformPosY, frame, 0.0f);
							AddOrUpdateFloatKey(section.transformPosZ, frame, 0.0f);
							break;
						case SEQUENCE_TRACK_TYPE::PROPERTY_BOOL:
						case SEQUENCE_TRACK_TYPE::VISIBILITY:
						case SEQUENCE_TRACK_TYPE::ACTIVATION:
							section.boolKeys.emplace_back(
								SequenceBoolKeyAssetData{
									.keyId = AllocateStableId(),
									.frame = frame,
									.value = true,
								}
							);
							std::ranges::sort(
								section.boolKeys,
								[](const SequenceBoolKeyAssetData& lhs, const SequenceBoolKeyAssetData& rhs) {
									return lhs.frame < rhs.frame;
								}
							);
							break;
						default:
							break;
					}

					ioData.lengthFrames = std::max(ioData.lengthFrames, frame);
				}
			);
			if (!modified) {
				return false;
			}

		SequenceEditorSelection& activeSelection = controller.GetSelection();
		EnsureDefaultChannelForTrack(activeSelection, selectedTrackType);
		activeSelection.keyIndex = -1;
		return true;
	}
	}

	void SequenceTimelinePanel::Draw(SequenceEditorController& controller) {
		if (!ImGui::Begin("Sequence Timeline")) {
			ImGui::End();
			return;
		}

		const auto& documents = controller.GetDocuments();
		if (documents.empty()) {
			ImGui::TextUnformatted(
				"No sequence opened. Double click a .sequence.json in Content Browser."
			);
			ImGui::End();
			return;
		}

		int32_t activeDocumentIndex = controller.GetActiveDocumentIndex();
		ImGui::TextUnformatted("Document");
		ImGui::SameLine();
		if (ImGui::BeginCombo(
			"##SequenceDoc",
			activeDocumentIndex >= 0 &&
			activeDocumentIndex < static_cast<int32_t>(documents.size()) &&
			documents[activeDocumentIndex] ?
				documents[activeDocumentIndex]->GetDisplayName().c_str() :
				"<none>"
		)) {
			for (int32_t i = 0; i < static_cast<int32_t>(documents.size()); ++i) {
				if (!documents[i]) {
					continue;
				}
				const bool selected = i == activeDocumentIndex;
				if (ImGui::Selectable(documents[i]->GetDisplayName().c_str(), selected)) {
					controller.SetActiveDocumentIndex(i);
					activeDocumentIndex = i;
				}
				if (selected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		SequenceEditorDocument* document = controller.GetActiveDocument();
		if (!document) {
			ImGui::End();
			return;
		}

		if (document->HasExternalConflict()) {
			ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 180, 90, 255));
			ImGui::TextUnformatted("External update detected. Resolve conflict:");
			ImGui::PopStyleColor();
			if (ImGui::Button("Keep Local")) {
				document->ResolveConflictKeepLocal();
			}
			ImGui::SameLine();
			if (ImGui::Button("Reload Disk")) {
				if (document->ResolveConflictReloadDisk()) {
					controller.SetActiveDocumentIndex(activeDocumentIndex);
				}
			}
			ImGui::Separator();
		}

		SequenceAuthoringData& data = document->GetAuthoringData();
		const float maxFrame = static_cast<float>(std::max<int64_t>(1, data.lengthFrames));

		if (ImGui::Button("Save")) {
			(void)controller.SaveActiveDocument();
		}
		ImGui::SameLine();
		if (ImGui::Button("Undo")) {
			(void)controller.UndoActiveDocument();
		}
		ImGui::SameLine();
		if (ImGui::Button("Redo")) {
			(void)controller.RedoActiveDocument();
		}

		ImGui::Separator();

		if (ImGui::Button("|<")) {
			controller.SetPlayheadFrame(0.0f, true);
		}
		ImGui::SameLine();
		if (ImGui::Button("<<")) {
			controller.PlayPreviewBackward();
		}
		ImGui::SameLine();
		if (ImGui::Button("||")) {
			controller.PausePreview();
		}
		ImGui::SameLine();
		if (ImGui::Button(">")) {
			controller.PlayPreview();
		}
		ImGui::SameLine();
		if (ImGui::Button("[]")) {
			controller.StopPreview();
		}

		const float playheadFrame = controller.GetPlayheadFrame();
		const std::string currentTimecode = FormatTimecode(playheadFrame, data.displayRate);
		const std::string totalTimecode = FormatTimecode(maxFrame, data.displayRate);
		ImGui::SameLine();
		ImGui::Text(
			"Time %s / %s  Frame %.0f / %lld",
			currentTimecode.c_str(),
			totalTimecode.c_str(),
			playheadFrame,
			data.lengthFrames
		);

		float frameInput = playheadFrame;
		if (ImGui::DragFloat("Frame", &frameInput, 1.0f, 0.0f, maxFrame, "%.0f")) {
			controller.SetPlayheadFrame(frameInput, true);
		}

		bool autoKey = controller.IsAutoKeyEnabled();
		if (ImGui::Checkbox("AutoKey", &autoKey)) {
			controller.SetAutoKeyEnabled(autoKey);
		}
		ImGui::SameLine();
		bool scrubFireEvents = controller.IsScrubFireEventsEnabled();
		if (ImGui::Checkbox("Scrub Fire Events", &scrubFireEvents)) {
			controller.SetScrubFireEventsEnabled(scrubFireEvents);
		}

		static float sPixelsPerFrame = 0.12f;
		ImGui::SameLine();
		ImGui::SetNextItemWidth(180.0f);
		(void)ImGui::SliderFloat("Zoom", &sPixelsPerFrame, 0.02f, 8.0f, "%.2f px/f");

		if (ImGui::Button("Add Transform Track")) {
			(void)controller.ModifyActiveDocument(
				[](SequenceAuthoringData& ioData) {
					SequenceTrackAssetData track = {};
					track.trackId = AllocateStableId();
					track.name = "Transform";
					track.trackType = SEQUENCE_TRACK_TYPE::TRANSFORM;
					track.blendMode = SEQUENCE_BLEND_MODE::MODE_ABSOLUTE;
					track.priority = 0;

					SequenceSectionAssetData section = {};
					section.sectionId = AllocateStableId();
					section.startFrame = 0;
					section.endFrame = std::max<int64_t>(ioData.lengthFrames, 60);
					section.transformPosX.channelId = AllocateStableId();
					section.transformPosY.channelId = AllocateStableId();
					section.transformPosZ.channelId = AllocateStableId();
					section.transformRotX.channelId = AllocateStableId();
					section.transformRotY.channelId = AllocateStableId();
					section.transformRotZ.channelId = AllocateStableId();
					section.transformRotW.channelId = AllocateStableId();
					section.transformScaleX.channelId = AllocateStableId();
					section.transformScaleY.channelId = AllocateStableId();
					section.transformScaleZ.channelId = AllocateStableId();
					track.sections.emplace_back(std::move(section));
					ioData.tracks.emplace_back(std::move(track));
				}
			);
		}
		ImGui::SameLine();
		if (ImGui::Button("Add Event Track")) {
			(void)controller.ModifyActiveDocument(
				[](SequenceAuthoringData& ioData) {
					SequenceTrackAssetData track = {};
					track.trackId = AllocateStableId();
					track.name = "Event";
					track.trackType = SEQUENCE_TRACK_TYPE::EVENT;
					track.blendMode = SEQUENCE_BLEND_MODE::MODE_ABSOLUTE;
					track.priority = 0;
					SequenceSectionAssetData section = {};
					section.sectionId = AllocateStableId();
					section.startFrame = 0;
					section.endFrame = std::max<int64_t>(ioData.lengthFrames, 60);
					track.sections.emplace_back(std::move(section));
					ioData.tracks.emplace_back(std::move(track));
				}
			);
		}
		ImGui::SameLine();
		if (ImGui::Button("Add Float Track")) {
			(void)controller.ModifyActiveDocument(
				[](SequenceAuthoringData& ioData) {
					SequenceTrackAssetData track = {};
					track.trackId = AllocateStableId();
					track.name = "PropertyFloat";
					track.trackType = SEQUENCE_TRACK_TYPE::PROPERTY_FLOAT;
					track.blendMode = SEQUENCE_BLEND_MODE::MODE_ABSOLUTE;
					track.priority = 0;
					SequenceSectionAssetData section = {};
					section.sectionId = AllocateStableId();
					section.startFrame = 0;
					section.endFrame = std::max<int64_t>(ioData.lengthFrames, 60);
					section.floatCurve.channelId = AllocateStableId();
					track.sections.emplace_back(std::move(section));
					ioData.tracks.emplace_back(std::move(track));
				}
			);
		}
		ImGui::SameLine();
		if (ImGui::Button("Add Key @ Playhead")) {
			(void)AddKeyAtPlayhead(controller);
		}

		std::vector<TimelineRow> rows = {};
		rows.reserve(data.tracks.size());
		for (int32_t trackIndex = 0; trackIndex < static_cast<int32_t>(data.tracks.size()); ++trackIndex) {
			const SequenceTrackAssetData& track = data.tracks[trackIndex];
			const int32_t sectionIndex = track.sections.empty() ? -1 : 0;
			rows.emplace_back(
				TimelineRow{
					.trackIndex = trackIndex,
					.sectionIndex = sectionIndex,
					.label = std::format("{} [{}]", track.name, TrackTypeLabel(track.trackType)),
				}
			);
		}

		ImGui::Separator();

		const float timelinePaneHeight = std::max(180.0f, ImGui::GetContentRegionAvail().y);
		const float leftPaneWidth = 260.0f;
		const float rulerHeight = 28.0f;
		const float rowHeight = 24.0f;
		static float sSynchronizedScrollY = 0.0f;
		static bool  sScrubbing = false;

		ImGui::BeginChild(
			"SequenceTimelineLeftPane",
			ImVec2(leftPaneWidth, timelinePaneHeight),
			true,
			0
		);
		ImGui::SetScrollY(sSynchronizedScrollY);
		ImGui::Dummy(ImVec2(1.0f, rulerHeight));
		for (const TimelineRow& row : rows) {
			const SequenceEditorSelection& selection = controller.GetSelection();
			const bool selected =
				selection.trackIndex == row.trackIndex &&
				selection.sectionIndex == row.sectionIndex;
			if (ImGui::Selectable(row.label.c_str(), selected, 0, ImVec2(-1.0f, rowHeight))) {
				SequenceEditorSelection& mutableSelection = controller.GetSelection();
				mutableSelection.trackIndex = row.trackIndex;
				mutableSelection.sectionIndex = row.sectionIndex;
				mutableSelection.keyIndex = -1;
				if (row.trackIndex >= 0 && row.trackIndex < static_cast<int32_t>(data.tracks.size())) {
					EnsureDefaultChannelForTrack(
						mutableSelection,
						data.tracks[row.trackIndex].trackType
					);
				}
			}
		}
		if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) {
			sSynchronizedScrollY = ImGui::GetScrollY();
		}
		ImGui::EndChild();

		ImGui::SameLine();
		ImGui::BeginChild(
			"SequenceTimelineRightPane",
			ImVec2(0.0f, timelinePaneHeight),
			true,
			ImGuiWindowFlags_HorizontalScrollbar
		);
		ImGui::SetScrollY(sSynchronizedScrollY);

		const float timelineFrames = std::max(1.0f, maxFrame);
		const float visibleWidth = std::max(120.0f, ImGui::GetContentRegionAvail().x);
		const float canvasWidth = std::max(visibleWidth, timelineFrames * sPixelsPerFrame + 64.0f);
		const float logicalCanvasHeight = rulerHeight + rowHeight * static_cast<float>(rows.size());
		const float canvasHeight = std::max(logicalCanvasHeight + 8.0f, timelinePaneHeight - 8.0f);

		ImGui::InvisibleButton("##SequenceTimelineCanvas", ImVec2(canvasWidth, canvasHeight));
		const bool canvasHovered = ImGui::IsItemHovered();
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		const ImVec2 itemMin = ImGui::GetItemRectMin();
		const ImVec2 itemMax = ImGui::GetItemRectMax();

		const float timelineOriginX = itemMin.x + 8.0f;
		const auto FrameToX = [&](const float frame) {
			return timelineOriginX + frame * sPixelsPerFrame;
		};

		drawList->AddRectFilled(itemMin, itemMax, IM_COL32(28, 30, 34, 255));
		drawList->AddRectFilled(
			itemMin,
			ImVec2(itemMax.x, itemMin.y + rulerHeight),
			IM_COL32(36, 40, 46, 255)
		);

		for (int32_t rowIndex = 0; rowIndex < static_cast<int32_t>(rows.size()); ++rowIndex) {
			const float y0 = itemMin.y + rulerHeight + rowHeight * static_cast<float>(rowIndex);
			const float y1 = y0 + rowHeight;
			const bool  isEven = (rowIndex % 2) == 0;
			drawList->AddRectFilled(
				ImVec2(itemMin.x, y0),
				ImVec2(itemMax.x, y1),
				isEven ? IM_COL32(33, 35, 40, 255) : IM_COL32(29, 31, 36, 255)
			);

			const SequenceEditorSelection& selection = controller.GetSelection();
			if (
				selection.trackIndex == rows[rowIndex].trackIndex &&
				selection.sectionIndex == rows[rowIndex].sectionIndex
			) {
				drawList->AddRectFilled(
					ImVec2(itemMin.x, y0),
					ImVec2(itemMax.x, y1),
					IM_COL32(90, 130, 210, 50)
				);
			}
		}

		const int64_t majorStep = ComputeMajorTickStep(sPixelsPerFrame);
		for (int64_t frame = 0; frame <= static_cast<int64_t>(timelineFrames) + majorStep; frame += majorStep) {
			const float x = FrameToX(static_cast<float>(frame));
			drawList->AddLine(
				ImVec2(x, itemMin.y),
				ImVec2(x, itemMin.y + logicalCanvasHeight),
				IM_COL32(120, 125, 135, 120),
				1.0f
			);
			drawList->AddText(
				ImVec2(x + 3.0f, itemMin.y + 5.0f),
				IM_COL32(220, 220, 220, 255),
				std::format("{}", frame).c_str()
			);
		}

		for (int32_t rowIndex = 0; rowIndex < static_cast<int32_t>(rows.size()); ++rowIndex) {
			const TimelineRow& row = rows[rowIndex];
			if (
				row.trackIndex < 0 ||
				row.trackIndex >= static_cast<int32_t>(data.tracks.size())
			) {
				continue;
			}
			const SequenceTrackAssetData& track = data.tracks[row.trackIndex];
			if (
				row.sectionIndex < 0 ||
				row.sectionIndex >= static_cast<int32_t>(track.sections.size())
			) {
				continue;
			}

			const SequenceSectionAssetData& section = track.sections[row.sectionIndex];
			const float y0 = itemMin.y + rulerHeight + rowHeight * static_cast<float>(rowIndex);
			const float y1 = y0 + rowHeight;
			const float sectionX0 = FrameToX(static_cast<float>(section.startFrame));
			const float sectionX1 = FrameToX(static_cast<float>(section.endFrame));

			// セクション範囲を可視化して、左の項目行と時間軸を対応させます。
			drawList->AddRectFilled(
				ImVec2(sectionX0, y0 + 3.0f),
				ImVec2(sectionX1, y1 - 3.0f),
				IM_COL32(70, 85, 115, 95),
				3.0f
			);

			const std::vector<int64_t> keyFrames = CollectSectionKeyFrames(track, section);
			for (const int64_t keyFrame : keyFrames) {
				const float x = FrameToX(static_cast<float>(keyFrame));
				const float cy = (y0 + y1) * 0.5f;
				const float half = 4.5f;
				drawList->AddQuadFilled(
					ImVec2(x, cy - half),
					ImVec2(x + half, cy),
					ImVec2(x, cy + half),
					ImVec2(x - half, cy),
					IM_COL32(255, 210, 80, 255)
				);
			}
		}

		if (canvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
			const ImVec2 mousePos = ImGui::GetIO().MousePos;
			const float localY = mousePos.y - itemMin.y;
			if (localY >= rulerHeight) {
				const int32_t rowIndex = static_cast<int32_t>((localY - rulerHeight) / rowHeight);
				if (rowIndex >= 0 && rowIndex < static_cast<int32_t>(rows.size())) {
					SequenceEditorSelection& selection = controller.GetSelection();
					selection.trackIndex = rows[rowIndex].trackIndex;
					selection.sectionIndex = rows[rowIndex].sectionIndex;
					selection.keyIndex = -1;
					if (
						selection.trackIndex >= 0 &&
						selection.trackIndex < static_cast<int32_t>(data.tracks.size())
					) {
						EnsureDefaultChannelForTrack(
							selection,
							data.tracks[selection.trackIndex].trackType
						);
					}
				}
			}
			sScrubbing = true;
		}
		if (sScrubbing) {
			if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
				const float scrubFrame = (ImGui::GetIO().MousePos.x - timelineOriginX) /
				                         std::max(0.0001f, sPixelsPerFrame);
				controller.SetPlayheadFrame(scrubFrame, true);
			} else {
				sScrubbing = false;
			}
		}

		const float playheadX = FrameToX(controller.GetPlayheadFrame());
		drawList->AddLine(
			ImVec2(playheadX, itemMin.y),
			ImVec2(playheadX, itemMin.y + logicalCanvasHeight),
			IM_COL32(255, 110, 90, 255),
			1.75f
		);
		drawList->AddTriangleFilled(
			ImVec2(playheadX - 5.0f, itemMin.y + 1.0f),
			ImVec2(playheadX + 5.0f, itemMin.y + 1.0f),
			ImVec2(playheadX, itemMin.y + 10.0f),
			IM_COL32(255, 110, 90, 255)
		);

		if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) {
			sSynchronizedScrollY = ImGui::GetScrollY();
		}
		ImGui::EndChild();

		ImGui::End();
	}
}

#endif
