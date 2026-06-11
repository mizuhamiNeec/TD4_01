#ifdef _DEBUG

#include "SequenceEditorTool.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <format>
#include <iterator>
#include <imgui.h>
#include <imgui_internal.h>

#include "core/assets/AssetManager.h"
#include "core/guidgenerator/GuidGenerator.h"
#include "core/math/Quaternion.h"

#include "engine/ImGui/Icons.h"
#include "engine/ImGui/ImGuiWidgets.h"
#include "engine/scene/Scene.h"
#include "engine/sequence/SequenceCurveEvaluation.h"
#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/world/World.h"

#include "SequenceEditorController.h"
#include "SequenceEditorDocument.h"

namespace Unnamed {
	namespace {
		static constexpr float kLeftPaneWidth = 280.0f;
		static constexpr float kRightPaneWidth = 320.0f;
		static constexpr float kRulerHeight = 30.0f;
		static constexpr float kRowHeight = 26.0f;
		static constexpr float kKeyRadius = 5.0f;
		static constexpr float kTimelinePaddingFrames = 30.0f;
		static constexpr int64_t kDefaultAddSectionLength = 120;

		enum class TimelineGroupKind : uint8_t {
			None,
			TransformPosition,
			TransformRotation,
			TransformScale,
		};

		struct TimelineLayout final {
			ImVec2 origin = {};
			float leftWidth = kLeftPaneWidth;
			float timelineWidth = 0.0f;
			float rightWidth = kRightPaneWidth;
			float pixelsPerFrame = 12.0f;
			float contentWidth = 0.0f;
			float contentHeight = 0.0f;
		};

		struct TimelineRow final {
			int32_t trackIndex = -1;
			int32_t sectionIndex = -1;
			SEQUENCE_EDITOR_FLOAT_CHANNEL floatChannel =
				SEQUENCE_EDITOR_FLOAT_CHANNEL::NONE;
			SequenceTimelineKeyKind kind = SequenceTimelineKeyKind::Float;
			std::string label = {};
			int32_t depth = 0;
			bool isGroup = false;
			TimelineGroupKind groupKind = TimelineGroupKind::None;
		};

		struct DrawnKey final {
			SequenceTimelineKeySelection selection = {};
			ImVec2 center = {};
		};

		[[nodiscard]] uint64_t AllocateStableId() {
			static GuidGenerator generator;
			return generator.Alloc();
		}

		[[nodiscard]] bool SameSelection(
			const SequenceTimelineKeySelection& lhs,
			const SequenceTimelineKeySelection& rhs
		) {
			return lhs.trackIndex == rhs.trackIndex &&
			       lhs.sectionIndex == rhs.sectionIndex &&
			       lhs.floatChannel == rhs.floatChannel &&
			       lhs.kind == rhs.kind &&
			       lhs.keyId == rhs.keyId;
		}

		[[nodiscard]] bool IsSelected(
			const std::vector<SequenceTimelineKeySelection>& selections,
			const SequenceTimelineKeySelection& selection
		) {
			return std::ranges::any_of(
				selections,
				[&](const SequenceTimelineKeySelection& current) {
					return SameSelection(current, selection);
				}
			);
		}

		void ToggleSelection(
			std::vector<SequenceTimelineKeySelection>& selections,
			const SequenceTimelineKeySelection& selection,
			const bool additive
		) {
			if (!additive) {
				selections.clear();
				selections.emplace_back(selection);
				return;
			}

			const auto it = std::ranges::find_if(
				selections,
				[&](const SequenceTimelineKeySelection& current) {
					return SameSelection(current, selection);
				}
			);
			if (it != selections.end()) {
				selections.erase(it);
				return;
			}
			selections.emplace_back(selection);
		}

		[[nodiscard]] std::string FormatTimecode(
			const float frame,
			const int32_t displayRate
		) {
			const int32_t rate = std::max(1, displayRate);
			const int64_t wholeFrame = static_cast<int64_t>(
				std::llround(std::max(0.0f, frame))
			);
			const int64_t totalSeconds = wholeFrame / rate;
			const int64_t frames = wholeFrame % rate;
			const int64_t seconds = totalSeconds % 60;
			const int64_t minutes = (totalSeconds / 60) % 60;
			const int64_t hours = totalSeconds / 3600;
			return std::format(
				"{:02}:{:02}:{:02}.{:02}",
				hours,
				minutes,
				seconds,
				frames
			);
		}

		[[nodiscard]] const char* ToTrackTypeLabel(
			const SEQUENCE_TRACK_TYPE type
		) {
			switch (type) {
				case SEQUENCE_TRACK_TYPE::TRANSFORM: return "Transform";
				case SEQUENCE_TRACK_TYPE::SKELETAL_CONTROL: return "Skeletal";
				case SEQUENCE_TRACK_TYPE::CAMERA_CUT: return "Camera Cut";
				case SEQUENCE_TRACK_TYPE::EVENT: return "Event";
				case SEQUENCE_TRACK_TYPE::VISIBILITY: return "Visibility";
				case SEQUENCE_TRACK_TYPE::ACTIVATION: return "Activation";
				case SEQUENCE_TRACK_TYPE::PROPERTY_BOOL: return "Bool";
				case SEQUENCE_TRACK_TYPE::PROPERTY_VEC3: return "Vec3";
				case SEQUENCE_TRACK_TYPE::PROPERTY_FLOAT:
				default: return "Float";
			}
		}

		[[nodiscard]] const std::array<SEQUENCE_TRACK_TYPE, 9>&
		GetAddableTrackTypes() {
			static constexpr std::array types = {
				SEQUENCE_TRACK_TYPE::TRANSFORM,
				SEQUENCE_TRACK_TYPE::CAMERA_CUT,
				SEQUENCE_TRACK_TYPE::EVENT,
				SEQUENCE_TRACK_TYPE::VISIBILITY,
				SEQUENCE_TRACK_TYPE::ACTIVATION,
				SEQUENCE_TRACK_TYPE::PROPERTY_FLOAT,
				SEQUENCE_TRACK_TYPE::PROPERTY_BOOL,
				SEQUENCE_TRACK_TYPE::PROPERTY_VEC3,
				SEQUENCE_TRACK_TYPE::SKELETAL_CONTROL,
			};
			return types;
		}

		[[nodiscard]] const char* ToInterpolationLabel(
			const SEQUENCE_INTERPOLATION_MODE mode
		) {
			switch (mode) {
				case SEQUENCE_INTERPOLATION_MODE::MODE_STEP: return "Step";
				case SEQUENCE_INTERPOLATION_MODE::MODE_CUBIC: return "Cubic";
				case SEQUENCE_INTERPOLATION_MODE::MODE_SPLINE: return "Spline";
				case SEQUENCE_INTERPOLATION_MODE::MODE_LINEAR:
				default: return "Linear";
			}
		}

		[[nodiscard]] const std::array<SEQUENCE_INTERPOLATION_MODE, 4>&
		GetInterpolationModes() {
			static constexpr std::array modes = {
				SEQUENCE_INTERPOLATION_MODE::MODE_STEP,
				SEQUENCE_INTERPOLATION_MODE::MODE_LINEAR,
				SEQUENCE_INTERPOLATION_MODE::MODE_CUBIC,
				SEQUENCE_INTERPOLATION_MODE::MODE_SPLINE,
			};
			return modes;
		}

		[[nodiscard]] SequenceRichCurveAssetData* GetFloatCurve(
			SequenceSectionAssetData& section,
			const SEQUENCE_EDITOR_FLOAT_CHANNEL channel
		) {
			switch (channel) {
				case SEQUENCE_EDITOR_FLOAT_CHANNEL::FLOAT:
					return &section.floatCurve;
				case SEQUENCE_EDITOR_FLOAT_CHANNEL::VEC3_X:
					return &section.vec3XCurve;
				case SEQUENCE_EDITOR_FLOAT_CHANNEL::VEC3_Y:
					return &section.vec3YCurve;
				case SEQUENCE_EDITOR_FLOAT_CHANNEL::VEC3_Z:
					return &section.vec3ZCurve;
				case SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_POS_X:
					return &section.transformPosX;
				case SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_POS_Y:
					return &section.transformPosY;
				case SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_POS_Z:
					return &section.transformPosZ;
				case SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_ROT_X:
					return &section.transformRotX;
				case SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_ROT_Y:
					return &section.transformRotY;
				case SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_ROT_Z:
					return &section.transformRotZ;
				case SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_ROT_W:
					return &section.transformRotW;
				case SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_SCALE_X:
					return &section.transformScaleX;
				case SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_SCALE_Y:
					return &section.transformScaleY;
				case SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_SCALE_Z:
					return &section.transformScaleZ;
				case SEQUENCE_EDITOR_FLOAT_CHANNEL::SKELETAL_WEIGHT:
					return &section.skeletal.weightCurve;
				case SEQUENCE_EDITOR_FLOAT_CHANNEL::SKELETAL_PLAYBACK:
					return &section.skeletal.playbackCurve;
				case SEQUENCE_EDITOR_FLOAT_CHANNEL::SKELETAL_SPEED:
					return &section.skeletal.speedCurve;
				case SEQUENCE_EDITOR_FLOAT_CHANNEL::NONE:
				default:
					return nullptr;
			}
		}

		[[nodiscard]] bool IsTransformRotationChannel(
			const SEQUENCE_EDITOR_FLOAT_CHANNEL channel
		) {
			return
				channel == SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_ROT_X ||
				channel == SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_ROT_Y ||
				channel == SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_ROT_Z ||
				channel == SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_ROT_W;
		}

		[[nodiscard]] const SequenceRichCurveAssetData* GetFloatCurve(
			const SequenceSectionAssetData& section,
			const SEQUENCE_EDITOR_FLOAT_CHANNEL channel
		) {
			return GetFloatCurve(
				const_cast<SequenceSectionAssetData&>(section),
				channel
			);
		}

		SequenceFloatKeyAssetData& AddOrUpdateFloatKeyAtFrame(
			SequenceRichCurveAssetData& curve,
			const int64_t frame,
			const float value
		) {
			if (curve.channelId == 0) {
				curve.channelId = AllocateStableId();
			}
			for (SequenceFloatKeyAssetData& key : curve.keys) {
				if (key.frame == frame) {
					key.value = value;
					return key;
				}
			}

			curve.keys.emplace_back(
				SequenceFloatKeyAssetData{
					.keyId = AllocateStableId(),
					.frame = frame,
					.value = value,
					.arriveTangent = 0.0f,
					.leaveTangent = 0.0f,
					.interpolation =
						SEQUENCE_INTERPOLATION_MODE::MODE_LINEAR,
				}
			);
			return curve.keys.back();
		}

		void AppendRotationKeyFrames(
			std::vector<int64_t>& outFrames,
			const SequenceRichCurveAssetData& curve
		) {
			for (const SequenceFloatKeyAssetData& key : curve.keys) {
				outFrames.emplace_back(key.frame);
			}
		}

		[[nodiscard]] SEQUENCE_INTERPOLATION_MODE FindRotationInterpolation(
			const SequenceSectionAssetData& section,
			const int64_t frame
		) {
			for (const SequenceRichCurveAssetData* curve : {
				     &section.transformRotX,
				     &section.transformRotY,
				     &section.transformRotZ,
				     &section.transformRotW
			     }) {
				for (const SequenceFloatKeyAssetData& key : curve->keys) {
					if (key.frame == frame) {
						return key.interpolation;
					}
				}
			}
			return SEQUENCE_INTERPOLATION_MODE::MODE_LINEAR;
		}

		[[nodiscard]] Quaternion EvaluateRotationKeyFrame(
			const SequenceSectionAssetData& section,
			const int64_t frame
		) {
			const float sampleFrame = static_cast<float>(frame);
			return Quaternion(
				EvaluateSequenceRichCurve(
					section.transformRotX, sampleFrame, 0.0f
				),
				EvaluateSequenceRichCurve(
					section.transformRotY, sampleFrame, 0.0f
				),
				EvaluateSequenceRichCurve(
					section.transformRotZ, sampleFrame, 0.0f
				),
				EvaluateSequenceRichCurve(
					section.transformRotW, sampleFrame, 1.0f
				)
			).Normalized();
		}

		[[nodiscard]] Quaternion SlerpShortest(
			const Quaternion& from,
			const Quaternion& to,
			const float t
		) {
			Quaternion adjustedTo = to;
			const float dot =
				from.x * to.x + from.y * to.y + from.z * to.z + from.w * to.w;
			if (dot < 0.0f) {
				adjustedTo.x = -adjustedTo.x;
				adjustedTo.y = -adjustedTo.y;
				adjustedTo.z = -adjustedTo.z;
				adjustedTo.w = -adjustedTo.w;
			}
			return Quaternion::Slerp(from, adjustedTo, t);
		}

		[[nodiscard]] Quaternion EvaluateRotationQuaternion(
			const SequenceSectionAssetData& section,
			const float frame
		) {
			std::vector<int64_t> frames = {};
			AppendRotationKeyFrames(frames, section.transformRotX);
			AppendRotationKeyFrames(frames, section.transformRotY);
			AppendRotationKeyFrames(frames, section.transformRotZ);
			AppendRotationKeyFrames(frames, section.transformRotW);
			if (frames.empty()) {
				return Quaternion::identity;
			}

			std::ranges::sort(frames);
			frames.erase(std::ranges::unique(frames).begin(), frames.end());
			if (
				frames.size() == 1 ||
				frame <= static_cast<float>(frames.front())
			) {
				return EvaluateRotationKeyFrame(section, frames.front());
			}
			if (frame >= static_cast<float>(frames.back())) {
				return EvaluateRotationKeyFrame(section, frames.back());
			}

			const auto rhsIt = std::ranges::upper_bound(frames, frame);
			if (rhsIt == frames.begin() || rhsIt == frames.end()) {
				return EvaluateRotationKeyFrame(section, frames.back());
			}
			const auto lhsIt = std::prev(rhsIt);
			const int64_t lhsFrame = *lhsIt;
			const int64_t rhsFrame = *rhsIt;
			const Quaternion lhs = EvaluateRotationKeyFrame(section, lhsFrame);
			const Quaternion rhs = EvaluateRotationKeyFrame(section, rhsFrame);
			if (
				FindRotationInterpolation(section, lhsFrame) ==
				SEQUENCE_INTERPOLATION_MODE::MODE_STEP
			) {
				return lhs;
			}

			const float segmentFrames = std::max(
				1.0f,
				static_cast<float>(rhsFrame - lhsFrame)
			);
			const float t = std::clamp(
				(frame - static_cast<float>(lhsFrame)) / segmentFrames,
				0.0f,
				1.0f
			);
			return SlerpShortest(lhs, rhs, t);
		}

		void SetRotationEulerDegreesAtFrame(
			SequenceSectionAssetData& section,
			const int64_t frame,
			const Vec3& eulerDegrees
		) {
			const Quaternion rotation = Quaternion::EulerDegrees(
				eulerDegrees
			).Normalized();
			AddOrUpdateFloatKeyAtFrame(section.transformRotX, frame, rotation.x);
			AddOrUpdateFloatKeyAtFrame(section.transformRotY, frame, rotation.y);
			AddOrUpdateFloatKeyAtFrame(section.transformRotZ, frame, rotation.z);
			AddOrUpdateFloatKeyAtFrame(section.transformRotW, frame, rotation.w);
		}

		[[nodiscard]] SequenceSectionAssetData* ResolveSection(
			SequenceAuthoringData& data,
			const int32_t trackIndex,
			const int32_t sectionIndex
		) {
			if (
				trackIndex < 0 ||
				trackIndex >= static_cast<int32_t>(data.tracks.size())
			) {
				return nullptr;
			}
			SequenceTrackAssetData& track = data.tracks[trackIndex];
			if (
				sectionIndex < 0 ||
				sectionIndex >= static_cast<int32_t>(track.sections.size())
			) {
				return nullptr;
			}
			return &track.sections[sectionIndex];
		}

		[[nodiscard]] const SequenceSectionAssetData* ResolveSection(
			const SequenceAuthoringData& data,
			const int32_t trackIndex,
			const int32_t sectionIndex
		) {
			return ResolveSection(
				const_cast<SequenceAuthoringData&>(data),
				trackIndex,
				sectionIndex
			);
		}

		void SortSectionKeys(SequenceSectionAssetData& section) {
			const auto sortFloat = [](SequenceRichCurveAssetData& curve) {
				std::ranges::sort(
					curve.keys,
					[](const SequenceFloatKeyAssetData& lhs,
					   const SequenceFloatKeyAssetData& rhs) {
						return lhs.frame < rhs.frame;
					}
				);
			};
			sortFloat(section.floatCurve);
			sortFloat(section.vec3XCurve);
			sortFloat(section.vec3YCurve);
			sortFloat(section.vec3ZCurve);
			sortFloat(section.transformPosX);
			sortFloat(section.transformPosY);
			sortFloat(section.transformPosZ);
			sortFloat(section.transformRotX);
			sortFloat(section.transformRotY);
			sortFloat(section.transformRotZ);
			sortFloat(section.transformRotW);
			sortFloat(section.transformScaleX);
			sortFloat(section.transformScaleY);
			sortFloat(section.transformScaleZ);
			sortFloat(section.skeletal.weightCurve);
			sortFloat(section.skeletal.playbackCurve);
			sortFloat(section.skeletal.speedCurve);

			std::ranges::sort(
				section.boolKeys,
				[](const SequenceBoolKeyAssetData& lhs,
				   const SequenceBoolKeyAssetData& rhs) {
					return lhs.frame < rhs.frame;
				}
			);
			std::ranges::sort(
				section.cameraCutKeys,
				[](const SequenceCameraCutKeyAssetData& lhs,
				   const SequenceCameraCutKeyAssetData& rhs) {
					return lhs.frame < rhs.frame;
				}
			);
			std::ranges::sort(
				section.eventKeys,
				[](const SequenceEventKeyAssetData& lhs,
				   const SequenceEventKeyAssetData& rhs) {
					return lhs.frame < rhs.frame;
				}
			);
		}

		[[nodiscard]] float FrameToX(
			const TimelineLayout& layout,
			const float frame
		) {
			return layout.origin.x + layout.leftWidth + frame *
			       layout.pixelsPerFrame;
		}

		[[nodiscard]] float XToFrame(
			const TimelineLayout& layout,
			const float x
		) {
			return std::max(
				0.0f,
				(x - layout.origin.x - layout.leftWidth) /
				layout.pixelsPerFrame
			);
		}

		[[nodiscard]] int64_t QuantizeFrame(const float frame) {
			return static_cast<int64_t>(std::llround(std::max(0.0f, frame)));
		}

		[[nodiscard]] int64_t QuantizeSignedFrameDelta(
			const float frameDelta
		) {
			return static_cast<int64_t>(std::llround(frameDelta));
		}

		void EnsureSectionSpan(
			SequenceSectionAssetData& section,
			const int64_t frame
		) {
			if (section.sectionId == 0) {
				section.sectionId = AllocateStableId();
			}
			if (section.endFrame <= section.startFrame) {
				section.endFrame = section.startFrame + kDefaultAddSectionLength;
			}
			section.startFrame = std::min(section.startFrame, frame);
			section.endFrame = std::max(section.endFrame, frame);
		}

		void InitializeDefaultSectionChannels(
			SequenceSectionAssetData& section,
			const SEQUENCE_TRACK_TYPE trackType
		) {
			switch (trackType) {
				case SEQUENCE_TRACK_TYPE::TRANSFORM:
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
					break;
				case SEQUENCE_TRACK_TYPE::PROPERTY_FLOAT:
					section.floatCurve.channelId = AllocateStableId();
					break;
				case SEQUENCE_TRACK_TYPE::PROPERTY_VEC3:
					section.vec3XCurve.channelId = AllocateStableId();
					section.vec3YCurve.channelId = AllocateStableId();
					section.vec3ZCurve.channelId = AllocateStableId();
					break;
				case SEQUENCE_TRACK_TYPE::SKELETAL_CONTROL:
					section.skeletal.weightCurve.channelId = AllocateStableId();
					section.skeletal.playbackCurve.channelId = AllocateStableId();
					section.skeletal.speedCurve.channelId = AllocateStableId();
					break;
				case SEQUENCE_TRACK_TYPE::CAMERA_CUT:
				case SEQUENCE_TRACK_TYPE::EVENT:
				case SEQUENCE_TRACK_TYPE::VISIBILITY:
				case SEQUENCE_TRACK_TYPE::ACTIVATION:
				case SEQUENCE_TRACK_TYPE::PROPERTY_BOOL:
				default:
					break;
			}
		}

		[[nodiscard]] uint64_t ResolveOrCreateBinding(
			SequenceAuthoringData& data,
			const SEQUENCE_TRACK_TYPE trackType,
			const uint64_t entityGuid
		) {
			if (
				trackType != SEQUENCE_TRACK_TYPE::TRANSFORM ||
				entityGuid == 0
			) {
				return 0;
			}

			for (SequenceBindingAssetData& binding : data.bindings) {
				if (
					binding.entityGuid == entityGuid &&
					binding.componentStableName == "engine.Transform"
				) {
					if (binding.bindingId == 0) {
						binding.bindingId = AllocateStableId();
					}
					return binding.bindingId;
				}
			}

			SequenceBindingAssetData binding = {};
			binding.bindingId = AllocateStableId();
			binding.entityGuid = entityGuid;
			binding.componentStableName = "engine.Transform";
			data.bindings.emplace_back(std::move(binding));
			return data.bindings.back().bindingId;
		}

		void AddTrackToSequence(
			SequenceAuthoringData& data,
			const SEQUENCE_TRACK_TYPE trackType,
			const std::string& requestedName,
			const uint64_t entityGuid
		) {
			SequenceTrackAssetData track = {};
			track.trackId = AllocateStableId();
			track.trackType = trackType;
			track.name = requestedName.empty() ?
				             ToTrackTypeLabel(trackType) :
				             requestedName;
			track.blendMode = SEQUENCE_BLEND_MODE::MODE_ABSOLUTE;
			track.priority = 0;
			track.bindingId = ResolveOrCreateBinding(
				data,
				trackType,
				entityGuid
			);

			SequenceSectionAssetData section = {};
			section.sectionId = AllocateStableId();
			section.startFrame = 0;
			section.endFrame = std::max<int64_t>(data.lengthFrames, 1);
			InitializeDefaultSectionChannels(section, trackType);
			track.sections.emplace_back(std::move(section));
			data.tracks.emplace_back(std::move(track));
			data.lengthFrames = std::max<int64_t>(data.lengthFrames, 1);
		}

		void RemoveUnusedBindings(SequenceAuthoringData& data) {
			std::erase_if(
				data.bindings,
				[&](const SequenceBindingAssetData& binding) {
					if (binding.bindingId == 0) {
						return false;
					}
					return std::ranges::none_of(
						data.tracks,
						[&](const SequenceTrackAssetData& track) {
							return track.bindingId == binding.bindingId;
						}
					);
				}
			);
		}

		void DeleteTrackFromSequence(
			SequenceAuthoringData& data,
			const int32_t trackIndex
		) {
			if (
				trackIndex < 0 ||
				trackIndex >= static_cast<int32_t>(data.tracks.size())
			) {
				return;
			}
			data.tracks.erase(data.tracks.begin() + trackIndex);
			RemoveUnusedBindings(data);
		}

		void SetSequenceLengthFrames(
			SequenceAuthoringData& data,
			const int64_t lengthFrames
		) {
			data.lengthFrames = std::max<int64_t>(0, lengthFrames);
			for (SequenceTrackAssetData& track : data.tracks) {
				for (SequenceSectionAssetData& section : track.sections) {
					section.startFrame = std::min(
						section.startFrame,
						data.lengthFrames
					);
					section.endFrame = std::clamp(
						section.endFrame,
						section.startFrame,
						data.lengthFrames
					);
				}
			}
		}

		void AddKeyToRow(
			SequenceAuthoringData& data,
			const TimelineRow& row,
			const int64_t frame
		) {
			SequenceSectionAssetData* section = ResolveSection(
				data,
				row.trackIndex,
				row.sectionIndex
			);
			if (!section) {
				return;
			}

			EnsureSectionSpan(*section, frame);
			switch (row.kind) {
				case SequenceTimelineKeyKind::Float: {
					if (IsTransformRotationChannel(row.floatChannel)) {
						const Vec3 eulerDegrees =
							EvaluateRotationQuaternion(
								*section,
								static_cast<float>(frame)
							).ToEulerDegrees();
						SetRotationEulerDegreesAtFrame(
							*section,
							frame,
							eulerDegrees
						);
						break;
					}

					SequenceRichCurveAssetData* curve = GetFloatCurve(
						*section,
						row.floatChannel
					);
					if (!curve) {
						return;
					}
					if (curve->channelId == 0) {
						curve->channelId = AllocateStableId();
					}
					curve->keys.emplace_back(
						SequenceFloatKeyAssetData{
							.keyId = AllocateStableId(),
							.frame = frame,
							.value = 0.0f,
							.arriveTangent = 0.0f,
							.leaveTangent = 0.0f,
							.interpolation =
								SEQUENCE_INTERPOLATION_MODE::MODE_LINEAR,
						}
					);
					break;
				}
				case SequenceTimelineKeyKind::Bool:
					section->boolKeys.emplace_back(
						SequenceBoolKeyAssetData{
							.keyId = AllocateStableId(),
							.frame = frame,
							.value = false,
						}
					);
					break;
				case SequenceTimelineKeyKind::CameraCut:
					section->cameraCutKeys.emplace_back(
						SequenceCameraCutKeyAssetData{
							.keyId = AllocateStableId(),
							.frame = frame,
							.cameraEntityGuid = 0,
						}
					);
					break;
				case SequenceTimelineKeyKind::Event:
					section->eventKeys.emplace_back(
						SequenceEventKeyAssetData{
							.keyId = AllocateStableId(),
							.frame = frame,
						}
					);
					break;
			}
			SortSectionKeys(*section);
			data.lengthFrames = std::max(data.lengthFrames, frame);
		}

		[[nodiscard]] bool GetKeyFrame(
			const SequenceAuthoringData& data,
			const SequenceTimelineKeySelection& selection,
			int64_t& outFrame
		) {
			const SequenceSectionAssetData* section = ResolveSection(
				data,
				selection.trackIndex,
				selection.sectionIndex
			);
			if (!section) {
				return false;
			}

			switch (selection.kind) {
				case SequenceTimelineKeyKind::Float: {
					const SequenceRichCurveAssetData* curve = GetFloatCurve(
						*section,
						selection.floatChannel
					);
					if (!curve) {
						return false;
					}
					for (const SequenceFloatKeyAssetData& key : curve->keys) {
						if (key.keyId == selection.keyId) {
							outFrame = key.frame;
							return true;
						}
					}
					break;
				}
				case SequenceTimelineKeyKind::Bool:
					for (const SequenceBoolKeyAssetData& key : section->boolKeys) {
						if (key.keyId == selection.keyId) {
							outFrame = key.frame;
							return true;
						}
					}
					break;
				case SequenceTimelineKeyKind::CameraCut:
					for (const SequenceCameraCutKeyAssetData& key :
					     section->cameraCutKeys) {
						if (key.keyId == selection.keyId) {
							outFrame = key.frame;
							return true;
						}
					}
					break;
				case SequenceTimelineKeyKind::Event:
					for (const SequenceEventKeyAssetData& key : section->eventKeys) {
						if (key.keyId == selection.keyId) {
							outFrame = key.frame;
							return true;
						}
					}
					break;
			}
			return false;
		}

		void MoveKeyToFrame(
			SequenceAuthoringData& data,
			const SequenceTimelineKeySelection& selection,
			const int64_t frame
		) {
			SequenceSectionAssetData* section = ResolveSection(
				data,
				selection.trackIndex,
				selection.sectionIndex
			);
			if (!section) {
				return;
			}

			switch (selection.kind) {
				case SequenceTimelineKeyKind::Float: {
					SequenceRichCurveAssetData* curve = GetFloatCurve(
						*section,
						selection.floatChannel
					);
					if (!curve) {
						return;
					}
					for (SequenceFloatKeyAssetData& key : curve->keys) {
						if (key.keyId == selection.keyId) {
							key.frame = frame;
							break;
						}
					}
					break;
				}
				case SequenceTimelineKeyKind::Bool:
					for (SequenceBoolKeyAssetData& key : section->boolKeys) {
						if (key.keyId == selection.keyId) {
							key.frame = frame;
							break;
						}
					}
					break;
				case SequenceTimelineKeyKind::CameraCut:
					for (SequenceCameraCutKeyAssetData& key :
					     section->cameraCutKeys) {
						if (key.keyId == selection.keyId) {
							key.frame = frame;
							break;
						}
					}
					break;
				case SequenceTimelineKeyKind::Event:
					for (SequenceEventKeyAssetData& key : section->eventKeys) {
						if (key.keyId == selection.keyId) {
							key.frame = frame;
							break;
						}
					}
					break;
			}
			EnsureSectionSpan(*section, frame);
			SortSectionKeys(*section);
			data.lengthFrames = std::max(data.lengthFrames, frame);
		}

		void DeleteKey(
			SequenceAuthoringData& data,
			const SequenceTimelineKeySelection& selection
		) {
			SequenceSectionAssetData* section = ResolveSection(
				data,
				selection.trackIndex,
				selection.sectionIndex
			);
			if (!section) {
				return;
			}

			switch (selection.kind) {
				case SequenceTimelineKeyKind::Float: {
					SequenceRichCurveAssetData* curve = GetFloatCurve(
						*section,
						selection.floatChannel
					);
					if (curve) {
						std::erase_if(
							curve->keys,
							[&](const SequenceFloatKeyAssetData& key) {
								return key.keyId == selection.keyId;
							}
						);
					}
					break;
				}
				case SequenceTimelineKeyKind::Bool:
					std::erase_if(
						section->boolKeys,
						[&](const SequenceBoolKeyAssetData& key) {
							return key.keyId == selection.keyId;
						}
					);
					break;
				case SequenceTimelineKeyKind::CameraCut:
					std::erase_if(
						section->cameraCutKeys,
						[&](const SequenceCameraCutKeyAssetData& key) {
							return key.keyId == selection.keyId;
						}
					);
					break;
				case SequenceTimelineKeyKind::Event:
					std::erase_if(
						section->eventKeys,
						[&](const SequenceEventKeyAssetData& key) {
							return key.keyId == selection.keyId;
						}
					);
					break;
			}
		}

		void SetFloatInterpolation(
			SequenceAuthoringData& data,
			const SequenceTimelineKeySelection& selection,
			const SEQUENCE_INTERPOLATION_MODE mode
		) {
			if (selection.kind != SequenceTimelineKeyKind::Float) {
				return;
			}
			SequenceSectionAssetData* section = ResolveSection(
				data,
				selection.trackIndex,
				selection.sectionIndex
			);
			if (!section) {
				return;
			}
			SequenceRichCurveAssetData* curve = GetFloatCurve(
				*section,
				selection.floatChannel
			);
			if (!curve) {
				return;
			}
			for (SequenceFloatKeyAssetData& key : curve->keys) {
				if (key.keyId == selection.keyId) {
					key.interpolation = mode;
				}
			}
		}

		[[nodiscard]] bool InputTextStdString(
			const char* label,
			std::string& value
		) {
			constexpr size_t kMinCapacity = 256;
			const size_t capacity = std::max(value.size() + 1, kMinCapacity);
			std::vector<char> buf(capacity, '\0');
			if (!value.empty()) {
				const size_t copyLen = std::min(value.size(), capacity - 1);
				std::memcpy(buf.data(), value.data(), copyLen);
				buf[copyLen] = '\0';
			}
			const bool changed = ImGui::InputText(
				label,
				buf.data(),
				buf.size()
			);
			if (changed) {
				value = buf.data();
			}
			return changed;
		}

		void AddFloatRow(
			std::vector<TimelineRow>& rows,
			const int32_t trackIndex,
			const int32_t sectionIndex,
			const SEQUENCE_EDITOR_FLOAT_CHANNEL channel,
			std::string label,
			const int32_t depth
		) {
			rows.emplace_back(
				TimelineRow{
					.trackIndex = trackIndex,
					.sectionIndex = sectionIndex,
					.floatChannel = channel,
					.kind = SequenceTimelineKeyKind::Float,
					.label = std::move(label),
					.depth = depth,
					.isGroup = false,
				}
			);
		}

		void BuildTimelineRows(
			const SequenceAuthoringData& data,
			std::vector<TimelineRow>& rows,
			const bool positionExpanded,
			const bool rotationExpanded,
			const bool scaleExpanded
		) {
			rows.clear();
			for (int32_t trackIndex = 0;
			     trackIndex < static_cast<int32_t>(data.tracks.size());
			     ++trackIndex) {
				const SequenceTrackAssetData& track = data.tracks[trackIndex];
				rows.emplace_back(
					TimelineRow{
						.trackIndex = trackIndex,
						.label = track.name.empty() ?
							         ToTrackTypeLabel(track.trackType) :
							         track.name,
						.depth = 0,
						.isGroup = true,
					}
				);

				for (int32_t sectionIndex = 0;
				     sectionIndex <
				     static_cast<int32_t>(track.sections.size());
				     ++sectionIndex) {
					rows.emplace_back(
						TimelineRow{
							.trackIndex = trackIndex,
							.sectionIndex = sectionIndex,
							.label = std::format("Section {}", sectionIndex),
							.depth = 1,
							.isGroup = true,
						}
					);

					switch (track.trackType) {
						case SEQUENCE_TRACK_TYPE::TRANSFORM:
							rows.emplace_back(
								TimelineRow{
									.trackIndex = trackIndex,
									.sectionIndex = sectionIndex,
									.label = "Position",
									.depth = 2,
									.isGroup = true,
									.groupKind =
										TimelineGroupKind::TransformPosition,
								}
							);
							if (positionExpanded) {
								AddFloatRow(
									rows,
									trackIndex,
									sectionIndex,
									SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_POS_X,
									"X",
									3
								);
								AddFloatRow(
									rows,
									trackIndex,
									sectionIndex,
									SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_POS_Y,
									"Y",
									3
								);
								AddFloatRow(
									rows,
									trackIndex,
									sectionIndex,
									SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_POS_Z,
									"Z",
									3
								);
							}
							rows.emplace_back(
								TimelineRow{
									.trackIndex = trackIndex,
									.sectionIndex = sectionIndex,
									.label = "Rotation",
									.depth = 2,
									.isGroup = true,
									.groupKind =
										TimelineGroupKind::TransformRotation,
								}
							);
							if (rotationExpanded) {
								AddFloatRow(
									rows,
									trackIndex,
									sectionIndex,
									SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_ROT_X,
									"X",
									3
								);
								AddFloatRow(
									rows,
									trackIndex,
									sectionIndex,
									SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_ROT_Y,
									"Y",
									3
								);
								AddFloatRow(
									rows,
									trackIndex,
									sectionIndex,
									SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_ROT_Z,
									"Z",
									3
								);
							}
							rows.emplace_back(
								TimelineRow{
									.trackIndex = trackIndex,
									.sectionIndex = sectionIndex,
									.label = "Scale",
									.depth = 2,
									.isGroup = true,
									.groupKind =
										TimelineGroupKind::TransformScale,
								}
							);
							if (scaleExpanded) {
								AddFloatRow(
									rows,
									trackIndex,
									sectionIndex,
									SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_SCALE_X,
									"X",
									3
								);
								AddFloatRow(
									rows,
									trackIndex,
									sectionIndex,
									SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_SCALE_Y,
									"Y",
									3
								);
								AddFloatRow(
									rows,
									trackIndex,
									sectionIndex,
									SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_SCALE_Z,
									"Z",
									3
								);
							}
							break;
						case SEQUENCE_TRACK_TYPE::PROPERTY_VEC3:
							rows.emplace_back(
								TimelineRow{
									.trackIndex = trackIndex,
									.sectionIndex = sectionIndex,
									.label = "Value",
									.depth = 2,
									.isGroup = true,
								}
							);
							AddFloatRow(
								rows,
								trackIndex,
								sectionIndex,
								SEQUENCE_EDITOR_FLOAT_CHANNEL::VEC3_X,
								"X",
								3
							);
							AddFloatRow(
								rows,
								trackIndex,
								sectionIndex,
								SEQUENCE_EDITOR_FLOAT_CHANNEL::VEC3_Y,
								"Y",
								3
							);
							AddFloatRow(
								rows,
								trackIndex,
								sectionIndex,
								SEQUENCE_EDITOR_FLOAT_CHANNEL::VEC3_Z,
								"Z",
								3
							);
							break;
						case SEQUENCE_TRACK_TYPE::SKELETAL_CONTROL:
							AddFloatRow(
								rows,
								trackIndex,
								sectionIndex,
								SEQUENCE_EDITOR_FLOAT_CHANNEL::SKELETAL_WEIGHT,
								"Weight",
								2
							);
							AddFloatRow(
								rows,
								trackIndex,
								sectionIndex,
								SEQUENCE_EDITOR_FLOAT_CHANNEL::SKELETAL_PLAYBACK,
								"Playback",
								2
							);
							AddFloatRow(
								rows,
								trackIndex,
								sectionIndex,
								SEQUENCE_EDITOR_FLOAT_CHANNEL::SKELETAL_SPEED,
								"Speed",
								2
							);
							break;
						case SEQUENCE_TRACK_TYPE::PROPERTY_BOOL:
						case SEQUENCE_TRACK_TYPE::VISIBILITY:
						case SEQUENCE_TRACK_TYPE::ACTIVATION:
							rows.emplace_back(
								TimelineRow{
									.trackIndex = trackIndex,
									.sectionIndex = sectionIndex,
									.kind = SequenceTimelineKeyKind::Bool,
									.label = "Value",
									.depth = 2,
								}
							);
							break;
						case SEQUENCE_TRACK_TYPE::CAMERA_CUT:
							rows.emplace_back(
								TimelineRow{
									.trackIndex = trackIndex,
									.sectionIndex = sectionIndex,
									.kind = SequenceTimelineKeyKind::CameraCut,
									.label = "Camera",
									.depth = 2,
								}
							);
							break;
						case SEQUENCE_TRACK_TYPE::EVENT:
							rows.emplace_back(
								TimelineRow{
									.trackIndex = trackIndex,
									.sectionIndex = sectionIndex,
									.kind = SequenceTimelineKeyKind::Event,
									.label = "Cue",
									.depth = 2,
								}
							);
							break;
						case SEQUENCE_TRACK_TYPE::PROPERTY_FLOAT:
						default:
							AddFloatRow(
								rows,
								trackIndex,
								sectionIndex,
								SEQUENCE_EDITOR_FLOAT_CHANNEL::FLOAT,
								"Value",
								2
							);
							break;
					}
				}
			}
		}

		void DrawKeyShape(
			ImDrawList& drawList,
			const ImVec2 center,
			const SequenceTimelineKeyKind kind,
			const SEQUENCE_INTERPOLATION_MODE interpolation,
			const ImU32 color,
			const ImU32 outlineColor
		) {
			if (kind == SequenceTimelineKeyKind::Float) {
				switch (interpolation) {
					case SEQUENCE_INTERPOLATION_MODE::MODE_STEP:
						drawList.AddRectFilled(
							{center.x - kKeyRadius, center.y - kKeyRadius},
							{center.x + kKeyRadius, center.y + kKeyRadius},
							color,
							1.0f
						);
						drawList.AddRect(
							{center.x - kKeyRadius, center.y - kKeyRadius},
							{center.x + kKeyRadius, center.y + kKeyRadius},
							outlineColor,
							1.0f,
							0,
							1.5f
						);
						return;
					case SEQUENCE_INTERPOLATION_MODE::MODE_CUBIC:
						drawList.AddCircleFilled(center, kKeyRadius, color, 16);
						drawList.AddCircle(center, kKeyRadius, outlineColor, 16, 1.5f);
						return;
					case SEQUENCE_INTERPOLATION_MODE::MODE_SPLINE:
						drawList.AddNgonFilled(center, kKeyRadius + 1.0f, color, 4);
						drawList.AddNgon(center, kKeyRadius + 1.0f, outlineColor, 4, 1.5f);
						return;
					case SEQUENCE_INTERPOLATION_MODE::MODE_LINEAR:
					default:
						drawList.AddQuadFilled(
							{center.x, center.y - kKeyRadius - 1.0f},
							{center.x + kKeyRadius + 1.0f, center.y},
							{center.x, center.y + kKeyRadius + 1.0f},
							{center.x - kKeyRadius - 1.0f, center.y},
							color
						);
						drawList.AddQuad(
							{center.x, center.y - kKeyRadius - 1.0f},
							{center.x + kKeyRadius + 1.0f, center.y},
							{center.x, center.y + kKeyRadius + 1.0f},
							{center.x - kKeyRadius - 1.0f, center.y},
							outlineColor,
							1.5f
						);
						return;
				}
			}

			drawList.AddRectFilled(
				{center.x - kKeyRadius, center.y - kKeyRadius},
				{center.x + kKeyRadius, center.y + kKeyRadius},
				color,
				kind == SequenceTimelineKeyKind::Event ? kKeyRadius : 1.0f
			);
			drawList.AddRect(
				{center.x - kKeyRadius, center.y - kKeyRadius},
				{center.x + kKeyRadius, center.y + kKeyRadius},
				outlineColor,
				kind == SequenceTimelineKeyKind::Event ? kKeyRadius : 1.0f,
				0,
				1.5f
			);
		}

		void DrawRuler(
			ImDrawList& drawList,
			const TimelineLayout& layout,
			const SequenceAuthoringData& data
		) {
			const float startX = layout.origin.x + layout.leftWidth;
			const float endX = startX + layout.timelineWidth;
			const float y0 = layout.origin.y;
			const float y1 = y0 + kRulerHeight;
			drawList.AddRectFilled(
				{startX, y0},
				{endX, y1},
				ImGui::GetColorU32(ImGuiCol_Header)
			);

			const int32_t displayRate = std::max(1, data.displayRate);
			const float minLabelPixels = 72.0f;
			int32_t step = displayRate;
			while (static_cast<float>(step) * layout.pixelsPerFrame <
			       minLabelPixels) {
				step *= 2;
			}
			const int64_t maxFrame = static_cast<int64_t>(
				std::ceil(layout.timelineWidth / layout.pixelsPerFrame)
			);
			for (int64_t frame = 0; frame <= maxFrame; frame += step) {
				const float x = FrameToX(layout, static_cast<float>(frame));
				drawList.AddLine(
					{x, y0},
					{x, y1},
					ImGui::GetColorU32(ImGuiCol_Border)
				);
				const std::string label = std::format(
					"{:.2f}s",
					static_cast<double>(frame) / displayRate
				);
				drawList.AddText(
					{x + 4.0f, y0 + 6.0f},
					ImGui::GetColorU32(ImGuiCol_Text),
					label.c_str()
				);
			}
		}

		[[nodiscard]] bool IsPointInRect(
			const ImVec2 point,
			const ImVec2 a,
			const ImVec2 b
		) {
			const float minX = std::min(a.x, b.x);
			const float maxX = std::max(a.x, b.x);
			const float minY = std::min(a.y, b.y);
			const float maxY = std::max(a.y, b.y);
			return point.x >= minX && point.x <= maxX &&
			       point.y >= minY && point.y <= maxY;
		}

		[[nodiscard]] const SequenceBindingAssetData* FindBinding(
			const SequenceAuthoringData& data,
			const uint64_t bindingId
		) {
			for (const SequenceBindingAssetData& binding : data.bindings) {
				if (binding.bindingId == bindingId) {
					return &binding;
				}
			}
			return nullptr;
		}

		void AppendUniqueKeyFrames(
			std::vector<int64_t>& outFrames,
			const SequenceRichCurveAssetData& curve
		) {
			for (const SequenceFloatKeyAssetData& key : curve.keys) {
				outFrames.emplace_back(key.frame);
			}
		}

		void DrawTransformPathDebug(
			World& world,
			const SequenceAuthoringData& data
		) {
			Scene* const scene = world.GetScenePtr();
			if (!scene) {
				return;
			}

			static constexpr Vec4 kPathColor(0.1f, 0.75f, 1.0f, 1.0f);
			static constexpr Vec4 kKeyColor(1.0f, 0.85f, 0.15f, 1.0f);
			static constexpr Vec3 kKeyBoxSize(0.25f, 0.25f, 0.25f);

			for (const SequenceTrackAssetData& track : data.tracks) {
				if (track.trackType != SEQUENCE_TRACK_TYPE::TRANSFORM) {
					continue;
				}

				const SequenceBindingAssetData* binding = FindBinding(
					data,
					track.bindingId
				);
				if (!binding || binding->entityGuid == 0) {
					continue;
				}

				Entity* const entity = scene->FindEntity(binding->entityGuid);
				if (!entity) {
					continue;
				}
				const TransformComponent* const transform =
					entity->GetComponent<TransformComponent>();
				if (!transform) {
					continue;
				}

				for (const SequenceSectionAssetData& section : track.sections) {
					std::vector<int64_t> frames = {};
					AppendUniqueKeyFrames(frames, section.transformPosX);
					AppendUniqueKeyFrames(frames, section.transformPosY);
					AppendUniqueKeyFrames(frames, section.transformPosZ);
					if (frames.empty()) {
						continue;
					}

					std::ranges::sort(frames);
					frames.erase(
						std::ranges::unique(frames).begin(),
						frames.end()
					);

					const Vec3 fallback = transform->GetPosition();
					Vec3 previous = Vec3::zero;
					bool hasPrevious = false;
					for (const int64_t frame : frames) {
						const float sampleFrame = static_cast<float>(frame);
						const Vec3 position(
							EvaluateSequenceRichCurve(
								section.transformPosX,
								sampleFrame,
								fallback.x
							),
							EvaluateSequenceRichCurve(
								section.transformPosY,
								sampleFrame,
								fallback.y
							),
							EvaluateSequenceRichCurve(
								section.transformPosZ,
								sampleFrame,
								fallback.z
							)
						);

						if (hasPrevious) {
							world.GetDebugDraw().DrawLine(
								previous,
								position,
								kPathColor
							);
						}
						world.GetDebugDraw().DrawBox(
							position,
							Quaternion::identity,
							kKeyBoxSize,
							kKeyColor
						);
						previous = position;
						hasPrevious = true;
					}
				}
			}
		}
	}

	SequenceEditorTool::SequenceEditorTool() = default;

	SequenceEditorTool::~SequenceEditorTool() = default;

	std::string_view SequenceEditorTool::GetToolId() const {
		return "tool.sequencer";
	}

	std::string_view SequenceEditorTool::GetDisplayName() const {
		return "Sequence Editor";
	}

	void SequenceEditorTool::SetController(
		SequenceEditorController* controller
	) {
		mController = controller;
	}

	void SequenceEditorTool::SetRuntimeWorld(World* world) {
		mRuntimeWorld = world;
	}

	void SequenceEditorTool::Initialize(const EditorToolServices& services) {
		mAssetManager = services.assetManager;
	}

	void SequenceEditorTool::Shutdown() {
		mAssetManager = nullptr;
		mRuntimeWorld = nullptr;
		mController = nullptr;
		mSelectedKeys.clear();
		mKeyDragStart.clear();
		mEditingSequenceId = kInvalidAssetID;
	}

	void SequenceEditorTool::Tick(const EditorToolFrameContext& frameContext) {
		(void)frameContext;
		if (
			!mController ||
			!mRuntimeWorld ||
			mController->IsPreviewPlaying()
		) {
			return;
		}

		if (const SequenceEditorDocument* document =
			mController->GetActiveDocument()) {
			DrawTransformPathDebug(
				*mRuntimeWorld,
				document->GetAuthoringData()
			);
		}
	}

	void SequenceEditorTool::BuildUi(
		const EditorToolFrameContext& frameContext
	) {
		(void)frameContext;

		if (!mOpen) {
			return;
		}
		if (!mController) {
			return;
		}

		if (!ImGui::Begin("Sequence Editor", &mOpen)) {
			ImGui::End();
			return;
		}

		if (ImGuiWidgets::AssetPathPicker(
			"Sequence",
			mSequenceAssetPath,
			ImGuiWidgets::AssetTypeToMask(ASSET_TYPE::SEQUENCE)
		)) {
			if (!mSequenceAssetPath.empty() && mController->OpenDocument(mSequenceAssetPath)) {
				if (SequenceEditorDocument* document = mController->GetActiveDocument()) {
					mEditingSequenceId = document->GetSourceAssetId();
				}
				mSelectedKeys.clear();
			}
		}

		const auto& documents = mController->GetDocuments();
		if (!documents.empty() && ImGui::BeginTabBar("SequenceDocuments")) {
			for (int32_t index = 0;
			     index < static_cast<int32_t>(documents.size());
			     ++index) {
				const auto& document = documents[index];
				if (!document) {
					continue;
				}
				const std::string label = document->GetDisplayName() +
				                          (document->IsDirty() ? "*" : "");
				bool openTab = true;
				if (ImGui::BeginTabItem(label.c_str(), &openTab)) {
					mController->SetActiveDocumentIndex(index);
					ImGui::EndTabItem();
				}
			}
			ImGui::EndTabBar();
		}

		SequenceEditorDocument* document = mController->GetActiveDocument();
		if (!document) {
			ImGui::TextUnformatted("No sequence document is open.");
			ImGui::End();
			return;
		}

		SequenceAuthoringData& data = document->GetAuthoringData();
		if (document->HasExternalConflict()) {
			ImGui::TextColored(
				ImVec4(1.0f, 0.65f, 0.2f, 1.0f),
				"External change detected."
			);
			ImGui::SameLine();
			if (ImGui::Button("Keep Local")) {
				document->ResolveConflictKeepLocal();
			}
			ImGui::SameLine();
			if (ImGui::Button("Reload Disk")) {
				(void)document->ResolveConflictReloadDisk();
				mSelectedKeys.clear();
			}
		}

		const float toolbarHeight = ImGui::GetFrameHeight() +
		                            ImGui::GetStyle().ItemSpacing.y;
		const ImVec2 toolbarStart = ImGui::GetCursorScreenPos();
		ImGui::GetWindowDrawList()->AddRectFilled(
			toolbarStart,
			{
				toolbarStart.x + ImGui::GetContentRegionAvail().x,
				toolbarStart.y + toolbarHeight
			},
			ImGui::GetColorU32(ImGuiCol_Header),
			ImGui::GetStyle().FrameRounding
		);

		const float playheadFrame = mController->GetPlayheadFrame();
		const std::string timecode = FormatTimecode(
			playheadFrame,
			data.displayRate
		);
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(timecode.c_str());
		ImGui::SameLine();

		const ImVec2 buttonSize(
			ImGui::GetFrameHeight(),
			ImGui::GetFrameHeight()
		);
		if (ImGuiWidgets::IconButton(kIconSkipPrevious, nullptr, buttonSize)) {
			mController->SetPlayheadFrame(0.0f, true);
		}
		ImGui::SameLine();
		if (ImGuiWidgets::IconButton(kIconArrowBack2, nullptr, buttonSize)) {
			mController->PlayPreviewBackward();
		}
		ImGui::SameLine();
		if (ImGuiWidgets::IconButton(
			mController->IsPreviewPlaying() ? kIconStop : kIconPlay,
			nullptr,
			buttonSize
		)) {
			if (mController->IsPreviewPlaying()) {
				mController->StopPreview();
			} else {
				mController->PlayPreview();
			}
		}
		ImGui::SameLine();
		if (ImGuiWidgets::IconButton(kIconSkipNext, nullptr, buttonSize)) {
			mController->SetPlayheadFrame(
				static_cast<float>(std::max<int64_t>(0, data.lengthFrames)),
				true
			);
		}
		ImGui::SameLine();
		if (ImGuiWidgets::IconButton(kIconSave, nullptr, buttonSize)) {
			(void)mController->SaveActiveDocument();
		}
		ImGui::SameLine();
		if (ImGui::Button("Undo")) {
			(void)mController->UndoActiveDocument();
		}
		ImGui::SameLine();
		if (ImGui::Button("Redo")) {
			(void)mController->RedoActiveDocument();
		}
		ImGui::SameLine();
		int64_t lengthFrames = data.lengthFrames;
		ImGui::SetNextItemWidth(120.0f);
		if (ImGui::InputScalar(
			"Length",
			ImGuiDataType_S64,
			&lengthFrames
		)) {
			lengthFrames = std::max<int64_t>(0, lengthFrames);
			(void)mController->ModifyActiveDocument(
				[lengthFrames](SequenceAuthoringData& ioData) {
					SetSequenceLengthFrames(ioData, lengthFrames);
				}
			);
			mController->SetPlayheadFrame(
				std::min(
					mController->GetPlayheadFrame(),
					static_cast<float>(lengthFrames)
				),
				true
			);
		}
		ImGui::SameLine();
		ImGui::SetNextItemWidth(130.0f);
		if (ImGui::BeginCombo(
			"Track",
			ToTrackTypeLabel(mAddTrackType)
		)) {
			for (const SEQUENCE_TRACK_TYPE type : GetAddableTrackTypes()) {
				if (ImGui::Selectable(
					ToTrackTypeLabel(type),
					mAddTrackType == type
				)) {
					mAddTrackType = type;
				}
			}
			ImGui::EndCombo();
		}
		ImGui::SameLine();
		ImGui::SetNextItemWidth(160.0f);
		(void)InputTextStdString("Track Name", mAddTrackName);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(150.0f);
		(void)ImGui::InputScalar(
			"Entity",
			ImGuiDataType_U64,
			&mAddTrackEntityGuid
		);
		ImGui::SameLine();
		if (ImGui::Button("Add Track")) {
			const SEQUENCE_TRACK_TYPE addType = mAddTrackType;
			const std::string addName = mAddTrackName;
			const uint64_t addEntityGuid = mAddTrackEntityGuid;
			(void)mController->ModifyActiveDocument(
				[addType, addName, addEntityGuid](
					SequenceAuthoringData& ioData
				) {
					AddTrackToSequence(
						ioData,
						addType,
						addName,
						addEntityGuid
					);
				}
			);
			mAddTrackName.clear();
		}
		ImGui::SameLine();
		bool autoKey = mController->IsAutoKeyEnabled();
		if (ImGui::Checkbox("Auto Key", &autoKey)) {
			mController->SetAutoKeyEnabled(autoKey);
		}
		ImGui::SameLine();
		bool scrubEvents = mController->IsScrubFireEventsEnabled();
		if (ImGui::Checkbox("Scrub Events", &scrubEvents)) {
			mController->SetScrubFireEventsEnabled(scrubEvents);
		}
		ImGui::SameLine();
		ImGui::SetNextItemWidth(120.0f);
		ImGui::SliderFloat("Zoom", &mPixelsPerFrame, 2.0f, 48.0f, "%.1f");

		const ImGuiIO& io = ImGui::GetIO();
		if (
			ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
			ImGui::IsKeyPressed(ImGuiKey_Space, false) &&
			!io.WantTextInput &&
			!mDraggingKeys &&
			!mDraggingPlayhead &&
			!mBoxSelecting
		) {
			if (mController->IsPreviewPlaying()) {
				mController->StopPreview();
			} else {
				mController->PlayPreview();
			}
		}

		auto drawSelectionPane = [&]() {
			ImGui::TextUnformatted("Selection");
			ImGui::Separator();
			if (mSelectedKeys.empty()) {
				ImGui::TextUnformatted("No key selected.");
				return;
			}

			if (mSelectedKeys.size() > 1) {
				ImGui::Text("%zu keys selected", mSelectedKeys.size());
				int delta = 0;
				ImGui::SetNextItemWidth(120.0f);
				if (ImGui::DragInt("Frame Delta", &delta, 1.0f, -10000, 10000)) {
					const auto selectedKeys = mSelectedKeys;
					(void)mController->ModifyActiveDocument(
						[selectedKeys, delta](SequenceAuthoringData& ioData) {
							for (const auto& selection : selectedKeys) {
								int64_t frame = 0;
								if (GetKeyFrame(ioData, selection, frame)) {
									MoveKeyToFrame(
										ioData,
										selection,
										std::max<int64_t>(0, frame + delta)
									);
								}
							}
						}
					);
				}
				if (ImGui::Button("Delete Selected")) {
					const auto selectedKeys = mSelectedKeys;
					(void)mController->ModifyActiveDocument(
						[selectedKeys](SequenceAuthoringData& ioData) {
							for (const auto& selection : selectedKeys) {
								DeleteKey(ioData, selection);
							}
						}
					);
					mSelectedKeys.clear();
				}
				return;
			}

			const SequenceTimelineKeySelection selection = mSelectedKeys.front();
			SequenceSectionAssetData* section = ResolveSection(
				data,
				selection.trackIndex,
				selection.sectionIndex
			);
			int64_t frame = 0;
			if (!section || !GetKeyFrame(data, selection, frame)) {
				ImGui::TextUnformatted("Selected key no longer exists.");
				return;
			}

			int64_t editFrame = frame;
			ImGui::SetNextItemWidth(140.0f);
			if (ImGui::InputScalar("Frame", ImGuiDataType_S64, &editFrame)) {
				(void)mController->ModifyActiveDocument(
					[selection, editFrame](SequenceAuthoringData& ioData) {
						MoveKeyToFrame(
							ioData,
							selection,
							std::max<int64_t>(0, editFrame)
						);
					}
				);
			}

			if (selection.kind == SequenceTimelineKeyKind::Float) {
				SequenceRichCurveAssetData* curve = GetFloatCurve(
					*section,
					selection.floatChannel
				);
				if (!curve) {
					return;
				}
				if (IsTransformRotationChannel(selection.floatChannel)) {
					Vec3 eulerDegrees =
						EvaluateRotationQuaternion(
							*section,
							static_cast<float>(frame)
						).ToEulerDegrees();
					float eulerValues[3] = {
						eulerDegrees.x,
						eulerDegrees.y,
						eulerDegrees.z
					};
					ImGui::SetNextItemWidth(220.0f);
					if (ImGui::DragFloat3(
						"Euler Degrees",
						eulerValues,
						0.1f
					)) {
						const Vec3 nextEuler(
							eulerValues[0],
							eulerValues[1],
							eulerValues[2]
						);
						(void)mController->ModifyActiveDocument(
							[selection, frame, nextEuler](
								SequenceAuthoringData& ioData
							) {
								SequenceSectionAssetData* ioSection =
									ResolveSection(
										ioData,
										selection.trackIndex,
										selection.sectionIndex
									);
								if (!ioSection) {
									return;
								}
								SetRotationEulerDegreesAtFrame(
									*ioSection,
									frame,
									nextEuler
								);
								SortSectionKeys(*ioSection);
							}
						);
					}

					for (const SequenceFloatKeyAssetData& key : curve->keys) {
						if (key.keyId != selection.keyId) {
							continue;
						}
						if (ImGui::BeginCombo(
							"Interpolation",
							ToInterpolationLabel(key.interpolation)
						)) {
							for (
								const SEQUENCE_INTERPOLATION_MODE mode :
								GetInterpolationModes()
							) {
								if (ImGui::Selectable(
									ToInterpolationLabel(mode),
									key.interpolation == mode
								)) {
									(void)mController->ModifyActiveDocument(
										[selection, mode](
											SequenceAuthoringData& ioData
										) {
											SetFloatInterpolation(
												ioData,
												selection,
												mode
											);
										}
									);
								}
							}
							ImGui::EndCombo();
						}
						return;
					}
				}
				for (SequenceFloatKeyAssetData& key : curve->keys) {
					if (key.keyId != selection.keyId) {
						continue;
					}
					float value = key.value;
					ImGui::SetNextItemWidth(140.0f);
					if (ImGui::DragFloat("Value", &value, 0.01f)) {
						(void)mController->ModifyActiveDocument(
							[selection, value](SequenceAuthoringData& ioData) {
								SequenceSectionAssetData* ioSection =
									ResolveSection(
										ioData,
										selection.trackIndex,
										selection.sectionIndex
									);
								if (!ioSection) {
									return;
								}
								SequenceRichCurveAssetData* ioCurve =
									GetFloatCurve(
										*ioSection,
										selection.floatChannel
									);
								if (!ioCurve) {
									return;
								}
								for (SequenceFloatKeyAssetData& ioKey :
								     ioCurve->keys) {
									if (ioKey.keyId == selection.keyId) {
										ioKey.value = value;
									}
								}
							}
						);
					}
					if (ImGui::BeginCombo(
						"Interpolation",
						ToInterpolationLabel(key.interpolation)
					)) {
						for (const SEQUENCE_INTERPOLATION_MODE mode : GetInterpolationModes()) {

							if (ImGui::Selectable(
								ToInterpolationLabel(mode),
								key.interpolation == mode
							)) {
								(void)mController->ModifyActiveDocument(
									[selection, mode](SequenceAuthoringData& ioData) {
										SetFloatInterpolation(ioData, selection, mode);
									}
								);
							}
						}
						ImGui::EndCombo();
					}
					return;
				}
			}

			if (selection.kind == SequenceTimelineKeyKind::Bool) {
				for (SequenceBoolKeyAssetData& key : section->boolKeys) {
					if (key.keyId != selection.keyId) {
						continue;
					}
					bool value = key.value;
					if (ImGui::Checkbox("Value", &value)) {
						(void)mController->ModifyActiveDocument(
							[selection, value](SequenceAuthoringData& ioData) {
								SequenceSectionAssetData* ioSection =
									ResolveSection(
										ioData,
										selection.trackIndex,
										selection.sectionIndex
									);
								if (!ioSection) {
									return;
								}
								for (SequenceBoolKeyAssetData& ioKey :
								     ioSection->boolKeys) {
									if (ioKey.keyId == selection.keyId) {
										ioKey.value = value;
									}
								}
							}
						);
					}
					return;
				}
			}

			if (selection.kind == SequenceTimelineKeyKind::CameraCut) {
				for (SequenceCameraCutKeyAssetData& key :
				     section->cameraCutKeys) {
					if (key.keyId != selection.keyId) {
						continue;
					}
					uint64_t cameraGuid = key.cameraEntityGuid;
					ImGui::SetNextItemWidth(180.0f);
					if (ImGui::InputScalar(
						"Camera Entity",
						ImGuiDataType_U64,
						&cameraGuid
					)) {
						(void)mController->ModifyActiveDocument(
							[selection, cameraGuid](SequenceAuthoringData& ioData) {
								SequenceSectionAssetData* ioSection =
									ResolveSection(
										ioData,
										selection.trackIndex,
										selection.sectionIndex
									);
								if (!ioSection) {
									return;
								}
								for (SequenceCameraCutKeyAssetData& ioKey :
								     ioSection->cameraCutKeys) {
									if (ioKey.keyId == selection.keyId) {
										ioKey.cameraEntityGuid = cameraGuid;
									}
								}
							}
						);
					}
					return;
				}
			}

			if (selection.kind == SequenceTimelineKeyKind::Event) {
				for (SequenceEventKeyAssetData& key : section->eventKeys) {
					if (key.keyId != selection.keyId) {
						continue;
					}
					std::string cueId = key.cueId;
					if (InputTextStdString("Cue", cueId)) {
						(void)mController->ModifyActiveDocument(
							[selection, cueId](SequenceAuthoringData& ioData) {
								SequenceSectionAssetData* ioSection =
									ResolveSection(
										ioData,
										selection.trackIndex,
										selection.sectionIndex
									);
								if (!ioSection) {
									return;
								}
								for (SequenceEventKeyAssetData& ioKey :
								     ioSection->eventKeys) {
									if (ioKey.keyId == selection.keyId) {
										ioKey.cueId = cueId;
									}
								}
							}
						);
					}
					float cueValue = key.cueValue;
					ImGui::SetNextItemWidth(140.0f);
					if (ImGui::DragFloat("Cue Value", &cueValue, 0.01f)) {
						(void)mController->ModifyActiveDocument(
							[selection, cueValue](SequenceAuthoringData& ioData) {
								SequenceSectionAssetData* ioSection =
									ResolveSection(
										ioData,
										selection.trackIndex,
										selection.sectionIndex
									);
								if (!ioSection) {
									return;
								}
								for (SequenceEventKeyAssetData& ioKey :
								     ioSection->eventKeys) {
									if (ioKey.keyId == selection.keyId) {
										ioKey.cueValue = cueValue;
									}
								}
							}
						);
					}
					return;
				}
			}
		};

		std::vector<TimelineRow> splitRows = {};
		BuildTimelineRows(
			data,
			splitRows,
			mTransformPositionExpanded,
			mTransformRotationExpanded,
			mTransformScaleExpanded
		);
		auto isTransformGroupExpanded = [&](const TimelineGroupKind kind) {
			switch (kind) {
				case TimelineGroupKind::TransformPosition:
					return mTransformPositionExpanded;
				case TimelineGroupKind::TransformRotation:
					return mTransformRotationExpanded;
				case TimelineGroupKind::TransformScale:
					return mTransformScaleExpanded;
				case TimelineGroupKind::None:
				default:
					return false;
			}
		};
		auto toggleTransformGroup = [&](const TimelineGroupKind kind) {
			switch (kind) {
				case TimelineGroupKind::TransformPosition:
					mTransformPositionExpanded = !mTransformPositionExpanded;
					break;
				case TimelineGroupKind::TransformRotation:
					mTransformRotationExpanded = !mTransformRotationExpanded;
					break;
				case TimelineGroupKind::TransformScale:
					mTransformScaleExpanded = !mTransformScaleExpanded;
					break;
				case TimelineGroupKind::None:
				default:
					break;
			}
		};
		const float splitLengthFrames = static_cast<float>(
			std::max<int64_t>(data.lengthFrames, 300)
		);
		TimelineLayout splitLayout = {};
		splitLayout.leftWidth = 0.0f;
		splitLayout.pixelsPerFrame = mPixelsPerFrame;
		splitLayout.timelineWidth =
			(splitLengthFrames + kTimelinePaddingFrames) * mPixelsPerFrame;
		splitLayout.contentHeight = kRulerHeight +
			static_cast<float>(std::max<size_t>(splitRows.size(), 1)) *
			kRowHeight + 40.0f;

		if (ImGui::BeginTable(
			"SequenceEditorSplit",
			3,
			ImGuiTableFlags_BordersInnerV |
			ImGuiTableFlags_Resizable |
			ImGuiTableFlags_SizingStretchProp
		)) {
			ImGui::TableSetupColumn(
				"Tracks",
				ImGuiTableColumnFlags_WidthFixed,
				kLeftPaneWidth
			);
			ImGui::TableSetupColumn("Timeline", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn(
				"Selection",
				ImGuiTableColumnFlags_WidthFixed,
				kRightPaneWidth
			);
			ImGui::TableNextRow();

			std::vector<DrawnKey> drawnKeys = {};
			bool timelinePaneHovered = false;
			ImGui::TableSetColumnIndex(1);
			if (ImGui::BeginChild(
				"TimelineScroll",
				ImVec2(0, 0),
				false,
				ImGuiWindowFlags_AlwaysHorizontalScrollbar |
				ImGuiWindowFlags_HorizontalScrollbar |
				ImGuiWindowFlags_AlwaysVerticalScrollbar
			)) {
				timelinePaneHovered = ImGui::IsWindowHovered(
					ImGuiHoveredFlags_AllowWhenBlockedByActiveItem
				);
				mTimelineScrollY = ImGui::GetScrollY();
				splitLayout.origin = ImGui::GetCursorScreenPos();
				splitLayout.timelineWidth = std::max(
					ImGui::GetContentRegionAvail().x,
					splitLayout.timelineWidth
				);
				splitLayout.contentWidth = splitLayout.timelineWidth;

				ImDrawList* drawList = ImGui::GetWindowDrawList();
				const ImU32 rowBg = ImGui::GetColorU32(ImGuiCol_WindowBg);
				const ImU32 altRowBg = ImGui::GetColorU32(ImGuiCol_TableRowBgAlt);
				const ImU32 groupBg = ImGui::GetColorU32(ImGuiCol_Header);
				const ImU32 border = ImGui::GetColorU32(ImGuiCol_Border);
				const ImU32 selectedColor = ImGui::GetColorU32(ImGuiCol_PlotHistogram);
				const ImU32 keyColor = ImGui::GetColorU32(ImGuiCol_TextSelectedBg);

				drawList->AddRectFilled(
					splitLayout.origin,
					{
						splitLayout.origin.x + splitLayout.contentWidth,
						splitLayout.origin.y + splitLayout.contentHeight
					},
					rowBg
				);
				DrawRuler(*drawList, splitLayout, data);

				ImGui::SetCursorScreenPos(splitLayout.origin);
				ImGui::InvisibleButton(
					"##SequenceRuler",
					ImVec2(splitLayout.timelineWidth, kRulerHeight),
					ImGuiButtonFlags_MouseButtonLeft
				);
				if (ImGui::IsItemActive() &&
				    ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
					mDraggingPlayhead = true;
					mController->SetPlayheadFrame(
						XToFrame(splitLayout, ImGui::GetMousePos().x),
						true
					);
				}
				if (
					mDraggingPlayhead &&
					ImGui::IsMouseReleased(ImGuiMouseButton_Left)
				) {
					mDraggingPlayhead = false;
				}

				auto beginKeyDrag = [&](const SequenceTimelineKeySelection& selection) {
					if (mDraggingKeys) {
						return;
					}
					if (!IsSelected(mSelectedKeys, selection)) {
						mSelectedKeys.clear();
						mSelectedKeys.emplace_back(selection);
					}
					mKeyDragStart.clear();
					for (const SequenceTimelineKeySelection& selectedKey :
					     mSelectedKeys) {
						int64_t startFrame = 0;
						if (GetKeyFrame(data, selectedKey, startFrame)) {
							mKeyDragStart.emplace_back(selectedKey, startFrame);
						}
					}
					mDraggingKeys = true;
				};

				auto drawKeyControl = [&](
					const SequenceTimelineKeySelection& selection,
					const ImVec2 center,
					const SEQUENCE_INTERPOLATION_MODE interpolation
				) {
					const bool selected = IsSelected(mSelectedKeys, selection);
					DrawKeyShape(
						*drawList,
						center,
						selection.kind,
						interpolation,
						selected ? selectedColor : keyColor,
						border
					);
					drawnKeys.push_back(DrawnKey{selection, center});

					ImGui::SetCursorScreenPos(
						{
							center.x - kKeyRadius - 3.0f,
							center.y - kKeyRadius - 3.0f
						}
					);
					ImGui::PushID(static_cast<int>(selection.keyId & 0x7fffffff));
					ImGui::InvisibleButton(
						"##key",
						ImVec2(
							(kKeyRadius + 3.0f) * 2.0f,
							(kKeyRadius + 3.0f) * 2.0f
						),
						ImGuiButtonFlags_MouseButtonLeft |
						ImGuiButtonFlags_MouseButtonRight
					);
					if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
						ToggleSelection(
							mSelectedKeys,
							selection,
							ImGui::GetIO().KeyShift
						);
					}
					if (ImGui::IsItemActive() &&
					    ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
						beginKeyDrag(selection);
						mKeyDragFrameDelta = static_cast<float>(
							QuantizeSignedFrameDelta(
								ImGui::GetMouseDragDelta(
									ImGuiMouseButton_Left
								).x / splitLayout.pixelsPerFrame
							)
						);
					}
					if (ImGui::BeginPopupContextItem("KeyContext")) {
						if (ImGui::MenuItem("Delete Key")) {
							auto selectedKeys = mSelectedKeys;
							if (!IsSelected(selectedKeys, selection)) {
								selectedKeys = {selection};
							}
							(void)mController->ModifyActiveDocument(
								[selectedKeys](SequenceAuthoringData& ioData) {
									for (const auto& selectedKey : selectedKeys) {
										DeleteKey(ioData, selectedKey);
									}
								}
							);
							mSelectedKeys.clear();
						}
						if (
							selection.kind == SequenceTimelineKeyKind::Float &&
							ImGui::BeginMenu("Interpolation")
						) {
							for (
								const SEQUENCE_INTERPOLATION_MODE mode :
								GetInterpolationModes()
							) {
								if (ImGui::MenuItem(ToInterpolationLabel(mode))) {
									auto selectedKeys = mSelectedKeys;
									if (!IsSelected(selectedKeys, selection)) {
										selectedKeys = {selection};
									}
									(void)mController->ModifyActiveDocument(
										[selectedKeys, mode](
											SequenceAuthoringData& ioData
										) {
											for (const auto& selectedKey :
											     selectedKeys) {
												SetFloatInterpolation(
													ioData,
													selectedKey,
													mode
												);
											}
										}
									);
								}
							}
							ImGui::EndMenu();
						}
						ImGui::EndPopup();
					}
					ImGui::PopID();
				};

				for (size_t rowIndex = 0; rowIndex < splitRows.size(); ++rowIndex) {
					const TimelineRow& row = splitRows[rowIndex];
					const float y0 = splitLayout.origin.y + kRulerHeight +
						static_cast<float>(rowIndex) * kRowHeight;
					const float y1 = y0 + kRowHeight;
					drawList->AddRectFilled(
						{splitLayout.origin.x, y0},
						{splitLayout.origin.x + splitLayout.contentWidth, y1},
						row.isGroup ? groupBg :
							(rowIndex % 2 == 0 ? rowBg : altRowBg)
					);
					drawList->AddLine(
						{splitLayout.origin.x, y1},
						{splitLayout.origin.x + splitLayout.contentWidth, y1},
						border
					);
					if (row.isGroup) {
						continue;
					}

					const SequenceSectionAssetData* section = ResolveSection(
						data,
						row.trackIndex,
						row.sectionIndex
					);
					if (!section) {
						continue;
					}
					const float keyY = y0 + kRowHeight * 0.5f;
					if (row.kind == SequenceTimelineKeyKind::Float) {
						const SequenceRichCurveAssetData* curve = GetFloatCurve(
							*section,
							row.floatChannel
						);
						if (!curve) {
							continue;
						}
						for (const SequenceFloatKeyAssetData& key : curve->keys) {
							const SequenceTimelineKeySelection selection{
								.trackIndex = row.trackIndex,
								.sectionIndex = row.sectionIndex,
								.floatChannel = row.floatChannel,
								.kind = row.kind,
								.keyId = key.keyId,
							};
							const float keyFrame = static_cast<float>(key.frame) +
								(mDraggingKeys &&
								 IsSelected(mSelectedKeys, selection) ?
									mKeyDragFrameDelta :
									0.0f);
							drawKeyControl(
								selection,
								{FrameToX(splitLayout, keyFrame), keyY},
								key.interpolation
							);
						}
					} else {
						auto drawNonFloatKey =
							[&](const uint64_t keyId, const int64_t frame) {
								const SequenceTimelineKeySelection selection{
									.trackIndex = row.trackIndex,
									.sectionIndex = row.sectionIndex,
									.floatChannel = row.floatChannel,
									.kind = row.kind,
									.keyId = keyId,
								};
								const float keyFrame = static_cast<float>(frame) +
									(mDraggingKeys &&
									 IsSelected(mSelectedKeys, selection) ?
										mKeyDragFrameDelta :
										0.0f);
								drawKeyControl(
									selection,
									{FrameToX(splitLayout, keyFrame), keyY},
									SEQUENCE_INTERPOLATION_MODE::MODE_STEP
								);
							};
						if (row.kind == SequenceTimelineKeyKind::Bool) {
							for (const SequenceBoolKeyAssetData& key :
							     section->boolKeys) {
								drawNonFloatKey(key.keyId, key.frame);
							}
						} else if (row.kind == SequenceTimelineKeyKind::CameraCut) {
							for (const SequenceCameraCutKeyAssetData& key :
							     section->cameraCutKeys) {
								drawNonFloatKey(key.keyId, key.frame);
							}
						} else if (row.kind == SequenceTimelineKeyKind::Event) {
							for (const SequenceEventKeyAssetData& key :
							     section->eventKeys) {
								drawNonFloatKey(key.keyId, key.frame);
							}
						}
					}
				}

				if (mDraggingKeys && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
					const int64_t frameDelta = QuantizeSignedFrameDelta(
						mKeyDragFrameDelta
					);
					const auto dragStart = mKeyDragStart;
					if (frameDelta != 0 && !dragStart.empty()) {
						(void)mController->ModifyActiveDocument(
							[dragStart, frameDelta](
								SequenceAuthoringData& ioData
							) {
								for (const SequenceTimelineKeyDragStart& start :
								     dragStart) {
									MoveKeyToFrame(
										ioData,
										start.selection,
										std::max<int64_t>(
											0,
											start.frame + frameDelta
										)
									);
								}
							}
						);
					}
					mDraggingKeys = false;
					mKeyDragFrameDelta = 0.0f;
					mKeyDragStart.clear();
				}

				const float playheadX = FrameToX(
					splitLayout,
					mController->GetPlayheadFrame()
				);
				drawList->AddTriangleFilled(
					{playheadX - 7.0f, splitLayout.origin.y + 2.0f},
					{playheadX + 7.0f, splitLayout.origin.y + 2.0f},
					{playheadX, splitLayout.origin.y + kRulerHeight - 2.0f},
					ImGui::GetColorU32(ImGuiCol_PlotLinesHovered)
				);
				drawList->AddLine(
					{playheadX, splitLayout.origin.y + kRulerHeight},
					{playheadX, splitLayout.origin.y + splitLayout.contentHeight},
					ImGui::GetColorU32(ImGuiCol_PlotLinesHovered),
					1.5f
				);

				const ImVec2 mousePos = ImGui::GetMousePos();
				const bool mouseInTimeline =
					timelinePaneHovered &&
					mousePos.x >= splitLayout.origin.x &&
					mousePos.x <= splitLayout.origin.x +
						splitLayout.timelineWidth &&
					mousePos.y >= splitLayout.origin.y + kRulerHeight &&
					mousePos.y <= splitLayout.origin.y +
						splitLayout.contentHeight;
				bool keyHovered = false;
				for (const DrawnKey& drawnKey : drawnKeys) {
					if (
						std::abs(mousePos.x - drawnKey.center.x) <=
							kKeyRadius + 3.0f &&
						std::abs(mousePos.y - drawnKey.center.y) <=
							kKeyRadius + 3.0f
					) {
						keyHovered = true;
						break;
					}
				}
				if (
					mouseInTimeline &&
					!keyHovered &&
					ImGui::IsMouseClicked(ImGuiMouseButton_Left)
				) {
					mBoxSelecting = true;
					mBoxSelectStart = {mousePos.x, mousePos.y};
					mBoxSelectEnd = mBoxSelectStart;
					if (!ImGui::GetIO().KeyShift) {
						mSelectedKeys.clear();
					}
				}
				if (mBoxSelecting && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
					mBoxSelectEnd = {mousePos.x, mousePos.y};
					drawList->AddRectFilled(
						{mBoxSelectStart.x, mBoxSelectStart.y},
						{mBoxSelectEnd.x, mBoxSelectEnd.y},
						IM_COL32(80, 140, 220, 48)
					);
					drawList->AddRect(
						{mBoxSelectStart.x, mBoxSelectStart.y},
						{mBoxSelectEnd.x, mBoxSelectEnd.y},
						IM_COL32(120, 180, 255, 180)
					);
				}
				if (
					mBoxSelecting &&
					ImGui::IsMouseReleased(ImGuiMouseButton_Left)
				) {
					for (const DrawnKey& drawnKey : drawnKeys) {
						if (IsPointInRect(
							drawnKey.center,
							{mBoxSelectStart.x, mBoxSelectStart.y},
							{mBoxSelectEnd.x, mBoxSelectEnd.y}
						)) {
							if (!IsSelected(mSelectedKeys, drawnKey.selection)) {
								mSelectedKeys.emplace_back(drawnKey.selection);
							}
						}
					}
					mBoxSelecting = false;
				}
				if (
					mouseInTimeline &&
					!keyHovered &&
					ImGui::IsMouseClicked(ImGuiMouseButton_Right)
				) {
					const int32_t rowIndex = static_cast<int32_t>(
						(mousePos.y - splitLayout.origin.y - kRulerHeight) /
						kRowHeight
					);
					if (
						rowIndex >= 0 &&
						rowIndex < static_cast<int32_t>(splitRows.size())
					) {
						const TimelineRow& row = splitRows[rowIndex];
						mContextFrame = QuantizeFrame(
							XToFrame(splitLayout, mousePos.x)
						);
						mContextTrackIndex = row.trackIndex;
						mContextSectionIndex = row.sectionIndex;
						mContextFloatChannel = row.floatChannel;
						mContextKeyKind = row.kind;
						ImGui::OpenPopup("SequenceTimelineContext");
					}
				}
				if (ImGui::BeginPopup("SequenceTimelineContext")) {
					TimelineRow contextRow = {};
					bool hasRow = false;
					for (const TimelineRow& row : splitRows) {
						if (
							row.trackIndex == mContextTrackIndex &&
							row.sectionIndex == mContextSectionIndex &&
							row.floatChannel == mContextFloatChannel &&
							row.kind == mContextKeyKind &&
							!row.isGroup
						) {
							contextRow = row;
							hasRow = true;
							break;
						}
					}
					if (
						mContextTrackIndex >= 0 &&
						mContextTrackIndex <
							static_cast<int32_t>(data.tracks.size())
					) {
						const std::string deleteLabel = std::format(
							"Delete Track '{}'",
							data.tracks[mContextTrackIndex].name.empty() ?
								ToTrackTypeLabel(data.tracks[mContextTrackIndex].trackType) :
								data.tracks[mContextTrackIndex].name
						);
						if (ImGui::MenuItem(deleteLabel.c_str())) {
							const int32_t trackIndex = mContextTrackIndex;
							(void)mController->ModifyActiveDocument(
								[trackIndex](SequenceAuthoringData& ioData) {
									DeleteTrackFromSequence(ioData, trackIndex);
								}
							);
							mSelectedKeys.clear();
							ImGui::CloseCurrentPopup();
						}
						ImGui::Separator();
					}
					if (hasRow && ImGui::MenuItem("Add Key")) {
						(void)mController->ModifyActiveDocument(
							[contextRow, frame = mContextFrame](
								SequenceAuthoringData& ioData
							) {
								AddKeyToRow(ioData, contextRow, frame);
							}
						);
					}
					if (
						!mSelectedKeys.empty() &&
						ImGui::MenuItem("Delete Selected")
					) {
						const auto selectedKeys = mSelectedKeys;
						(void)mController->ModifyActiveDocument(
							[selectedKeys](SequenceAuthoringData& ioData) {
								for (const auto& selection : selectedKeys) {
									DeleteKey(ioData, selection);
								}
							}
						);
						mSelectedKeys.clear();
					}
					if (
						!mSelectedKeys.empty() &&
						ImGui::BeginMenu("Interpolation")
					) {
						for (const SEQUENCE_INTERPOLATION_MODE mode : GetInterpolationModes()) {

							if (ImGui::MenuItem(ToInterpolationLabel(mode))) {
								const auto selectedKeys = mSelectedKeys;
								(void)mController->ModifyActiveDocument(
									[selectedKeys, mode](
										SequenceAuthoringData& ioData
									) {
										for (const auto& selection :
										     selectedKeys) {
											SetFloatInterpolation(
												ioData,
												selection,
												mode
											);
										}
									}
								);
							}
						}
						ImGui::EndMenu();
					}
					ImGui::EndPopup();
				}

				ImGui::SetCursorScreenPos(splitLayout.origin);
				ImGui::Dummy(
					ImVec2(splitLayout.contentWidth, splitLayout.contentHeight)
				);
			}
			ImGui::EndChild();

			ImGui::TableSetColumnIndex(0);
			if (ImGui::BeginChild(
				"TrackNames",
				ImVec2(0, 0),
				false,
				ImGuiWindowFlags_NoScrollbar |
				ImGuiWindowFlags_NoScrollWithMouse
			)) {
				const ImVec2 labelOrigin = ImGui::GetCursorScreenPos();
				ImDrawList* drawList = ImGui::GetWindowDrawList();
				const ImU32 rowBg = ImGui::GetColorU32(ImGuiCol_FrameBg);
				const ImU32 altRowBg = ImGui::GetColorU32(ImGuiCol_TableRowBgAlt);
				const ImU32 groupBg = ImGui::GetColorU32(ImGuiCol_Header);
				const ImU32 border = ImGui::GetColorU32(ImGuiCol_Border);
				const ImU32 textColor = ImGui::GetColorU32(ImGuiCol_Text);
				drawList->AddRectFilled(
					labelOrigin,
					{
						labelOrigin.x + kLeftPaneWidth,
						labelOrigin.y + splitLayout.contentHeight
					},
					rowBg
				);
				for (size_t rowIndex = 0; rowIndex < splitRows.size(); ++rowIndex) {
					const TimelineRow& row = splitRows[rowIndex];
					const float y0 = labelOrigin.y + kRulerHeight +
						static_cast<float>(rowIndex) * kRowHeight -
						mTimelineScrollY;
					const float y1 = y0 + kRowHeight;
					drawList->AddRectFilled(
						{labelOrigin.x, y0},
						{labelOrigin.x + kLeftPaneWidth, y1},
						row.isGroup ? groupBg :
							(rowIndex % 2 == 0 ? rowBg : altRowBg)
					);
					drawList->AddLine(
						{labelOrigin.x, y1},
						{labelOrigin.x + kLeftPaneWidth, y1},
						border
					);
					std::string label = row.label;
					if (row.groupKind != TimelineGroupKind::None) {
						label = std::string(
							isTransformGroupExpanded(row.groupKind) ? "v " : "> "
						) + label;
					}
					drawList->AddText(
						{
							labelOrigin.x + 8.0f +
							static_cast<float>(row.depth) * 18.0f,
							y0 + 5.0f
						},
						textColor,
						label.c_str()
					);
					if (row.groupKind != TimelineGroupKind::None) {
						ImGui::SetCursorScreenPos({labelOrigin.x, y0});
						ImGui::PushID(static_cast<int>(rowIndex));
						ImGui::InvisibleButton(
							"##toggleTransformGroup",
							ImVec2(kLeftPaneWidth, kRowHeight)
						);
						if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
							toggleTransformGroup(row.groupKind);
						}
						ImGui::PopID();
					}
				}
				ImGui::Dummy(ImVec2(kLeftPaneWidth, splitLayout.contentHeight));
			}
			ImGui::EndChild();

			ImGui::TableSetColumnIndex(2);
			if (ImGui::BeginChild("SelectionPane", ImVec2(0, 0), false)) {
				drawSelectionPane();
			}
			ImGui::EndChild();

			ImGui::EndTable();
		}

		ImGui::End();
		return;

	}

	void SequenceEditorTool::CollectRenderViews(
		Render::RenderFrameInputs& inputs
	) {
		(void)inputs;
	}

	void SequenceEditorTool::EnumerateViewKeys(
		std::vector<std::string>& outViewKeys
	) const {
		(void)outViewKeys;
	}

	void SequenceEditorTool::SetViewOutput(
		const std::string_view viewKey,
		const Render::SceneOutputView& output,
		const Vec2 size
	) {
		(void)viewKey;
		(void)output;
		(void)size;
	}

	bool SequenceEditorTool::IsOpen() const {
		return mOpen;
	}

	void SequenceEditorTool::SetOpen(const bool open) {
		mOpen = open;
	}
}

#endif
