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
				if (key.frame != frame) { continue; }
				key.value         = value;
				key.interpolation = SEQUENCE_INTERPOLATION_MODE::MODE_LINEAR;
				return;
			}

			ioCurve.keys.emplace_back(
				SequenceFloatKeyAssetData{
					.keyId         = AllocateStableId(),
					.frame         = frame,
					.value         = value,
					.arriveTangent = 0.0f,
					.leaveTangent  = 0.0f,
					.interpolation = SEQUENCE_INTERPOLATION_MODE::MODE_LINEAR,
				}
			);
			std::ranges::sort(
				ioCurve.keys,
				[](
				const SequenceFloatKeyAssetData& lhs,
				const SequenceFloatKeyAssetData& rhs
			) {
					return lhs.frame < rhs.frame;
				}
			);
		}

		[[nodiscard]] const char*
		TrackTypeLabel(const SEQUENCE_TRACK_TYPE type) {
			switch (type) {
				case SEQUENCE_TRACK_TYPE::TRANSFORM: return "Transform";
				case SEQUENCE_TRACK_TYPE::SKELETAL_CONTROL: return "Skeletal";
				case SEQUENCE_TRACK_TYPE::CAMERA_CUT: return "CameraCut";
				case SEQUENCE_TRACK_TYPE::EVENT: return "Event";
				case SEQUENCE_TRACK_TYPE::VISIBILITY: return "Visibility";
				case SEQUENCE_TRACK_TYPE::ACTIVATION: return "Activation";
				case SEQUENCE_TRACK_TYPE::PROPERTY_FLOAT: return
						"PropertyFloat";
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
				case SEQUENCE_TRACK_TYPE::EVENT: for (
						const SequenceEventKeyAssetData& key : section.
						eventKeys) { outFrames.emplace_back(key.frame); }
					break;
				case SEQUENCE_TRACK_TYPE::CAMERA_CUT: for (
						const SequenceCameraCutKeyAssetData& key : section.
						cameraCutKeys) { outFrames.emplace_back(key.frame); }
					break;
				case SEQUENCE_TRACK_TYPE::PROPERTY_BOOL:
				case SEQUENCE_TRACK_TYPE::VISIBILITY:
				case SEQUENCE_TRACK_TYPE::ACTIVATION: for (
						const SequenceBoolKeyAssetData& key : section.
						boolKeys) { outFrames.emplace_back(key.frame); }
					break;
				case SEQUENCE_TRACK_TYPE::PROPERTY_FLOAT: AppendCurveFrames(
						section.floatCurve, outFrames
					);
					break;
				case SEQUENCE_TRACK_TYPE::PROPERTY_VEC3: AppendCurveFrames(
						section.vec3XCurve, outFrames
					);
					AppendCurveFrames(section.vec3YCurve, outFrames);
					AppendCurveFrames(section.vec3ZCurve, outFrames);
					break;
				case SEQUENCE_TRACK_TYPE::TRANSFORM: AppendCurveFrames(
						section.transformPosX, outFrames
					);
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
				case SEQUENCE_TRACK_TYPE::SKELETAL_CONTROL: AppendCurveFrames(
						section.skeletal.weightCurve, outFrames
					);
					AppendCurveFrames(
						section.skeletal.playbackCurve, outFrames
					);
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
			frames.erase(
				std::unique(frames.begin(), frames.end()), frames.end()
			);
			return frames;
		}

		[[nodiscard]] std::string FormatTimecode(
			const float   frame,
			const int32_t displayRate
		) {
			const int64_t safeRate = std::max<int64_t>(1, displayRate);
			const int64_t frameInt = std::max<int64_t>(
				0, static_cast<int64_t>(std::llround(frame))
			);
			const int64_t totalSec   = frameInt / safeRate;
			const int64_t minutePart = totalSec / 60;
			const int64_t secondPart = totalSec % 60;
			const int64_t framePart  = frameInt % safeRate;
			return std::format(
				"{:02}:{:02}:{:02}", minutePart, secondPart, framePart
			);
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
				case SEQUENCE_TRACK_TYPE::TRANSFORM
				: ioSelection.floatChannel =
				  SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_POS_X;
					break;
				case SEQUENCE_TRACK_TYPE::PROPERTY_FLOAT
				: ioSelection.floatChannel =
				  SEQUENCE_EDITOR_FLOAT_CHANNEL::FLOAT;
					break;
				case SEQUENCE_TRACK_TYPE::PROPERTY_VEC3
				: ioSelection.floatChannel =
				  SEQUENCE_EDITOR_FLOAT_CHANNEL::VEC3_X;
					break;
				case SEQUENCE_TRACK_TYPE::SKELETAL_CONTROL
				: ioSelection.floatChannel =
				  SEQUENCE_EDITOR_FLOAT_CHANNEL::SKELETAL_WEIGHT;
					break;
				default: ioSelection.floatChannel =
				         SEQUENCE_EDITOR_FLOAT_CHANNEL::NONE;
					break;
			}
		}

		[[nodiscard]] int64_t QuantizeFrame(const float frame) {
			return static_cast<int64_t>(std::llround(std::max(0.0f, frame)));
		}

		[[nodiscard]] bool AddKeyAtPlayhead(
			SequenceEditorController& controller
		) {
			SequenceEditorSelection       selection = controller.GetSelection();
			const SequenceEditorDocument* document  = controller.
				GetActiveDocument();
			if (!document) { return false; }

			const SequenceAuthoringData& data = document->GetAuthoringData();
			if (
				selection.trackIndex < 0 ||
				selection.trackIndex >= static_cast<int32_t>(data.tracks.size())
			) { return false; }

			const SequenceTrackAssetData& track = data.tracks[selection.
				trackIndex];
			const SEQUENCE_TRACK_TYPE selectedTrackType = track.trackType;
			if (track.sections.empty()) { return false; }
			if (
				selection.sectionIndex < 0 ||
				selection.sectionIndex >= static_cast<int32_t>(track.sections.
					size())
			) { selection.sectionIndex = 0; }

			const int64_t frame = QuantizeFrame(controller.GetPlayheadFrame());
			const bool    modified = controller.ModifyActiveDocument(
				[selection, frame](SequenceAuthoringData& ioData) {
					if (
						selection.trackIndex < 0 ||
						selection.trackIndex >= static_cast<int32_t>(ioData.
							tracks.size())
					) { return; }
					SequenceTrackAssetData& localTrack = ioData.tracks[selection
						.trackIndex];
					if (
						selection.sectionIndex < 0 ||
						selection.sectionIndex >= static_cast<int32_t>(
							localTrack.sections.size())
					) { return; }
					SequenceSectionAssetData& section = localTrack.sections[
						selection.sectionIndex];

					switch (localTrack.trackType) {
						case SEQUENCE_TRACK_TYPE::EVENT: section.eventKeys.
								emplace_back(
									SequenceEventKeyAssetData{
										.keyId            = AllocateStableId(),
										.frame            = frame,
										.cueId            = "sequence.event",
										.sourceEntityGuid = 0,
										.cueValue         = 1.0f,
										.cueValue2        = 0.0f,
										.consoleCommand   = "",
									}
								);
							std::ranges::sort(
								section.eventKeys,
								[](
								const SequenceEventKeyAssetData& lhs,
								const SequenceEventKeyAssetData& rhs
							) {
									return lhs.frame < rhs.frame;
								}
							);
							break;
						case SEQUENCE_TRACK_TYPE::PROPERTY_FLOAT
						: AddOrUpdateFloatKey(section.floatCurve, frame, 0.0f);
							break;
						case SEQUENCE_TRACK_TYPE::PROPERTY_VEC3
						: AddOrUpdateFloatKey(section.vec3XCurve, frame, 0.0f);
							AddOrUpdateFloatKey(
								section.vec3YCurve, frame, 0.0f
							);
							AddOrUpdateFloatKey(
								section.vec3ZCurve, frame, 0.0f
							);
							break;
						case SEQUENCE_TRACK_TYPE::TRANSFORM
						: AddOrUpdateFloatKey(
								section.transformPosX, frame, 0.0f
							);
							AddOrUpdateFloatKey(
								section.transformPosY, frame, 0.0f
							);
							AddOrUpdateFloatKey(
								section.transformPosZ, frame, 0.0f
							);
							break;
						case SEQUENCE_TRACK_TYPE::PROPERTY_BOOL:
						case SEQUENCE_TRACK_TYPE::VISIBILITY:
						case SEQUENCE_TRACK_TYPE::ACTIVATION: section.boolKeys.
								emplace_back(
									SequenceBoolKeyAssetData{
										.keyId = AllocateStableId(),
										.frame = frame,
										.value = true,
									}
								);
							std::ranges::sort(
								section.boolKeys,
								[](
								const SequenceBoolKeyAssetData& lhs,
								const SequenceBoolKeyAssetData& rhs
							) {
									return lhs.frame < rhs.frame;
								}
							);
							break;
						default: break;
					}

					ioData.lengthFrames = std::max(ioData.lengthFrames, frame);
				}
			);
			if (!modified) { return false; }

			SequenceEditorSelection& activeSelection = controller.
				GetSelection();
			EnsureDefaultChannelForTrack(activeSelection, selectedTrackType);
			activeSelection.keyIndex = -1;
			return true;
		}
	}

	void SequenceTimelinePanel::Draw(SequenceEditorController& controller) {
		(void)controller;
	}
}

#endif
