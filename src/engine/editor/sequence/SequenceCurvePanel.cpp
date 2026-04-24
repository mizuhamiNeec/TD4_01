#ifdef _DEBUG
#include "SequenceCurvePanel.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <imgui.h>

#include "core/guidgenerator/GuidGenerator.h"

#include "SequenceEditorController.h"

namespace Unnamed {
	namespace {
		[[nodiscard]] uint64_t AllocateStableId() {
			static GuidGenerator generator;
			return generator.Alloc();
		}

		[[nodiscard]] float EvaluateCurveValue(
			const SequenceRichCurveAssetData& curve,
			const float                       frame
		) {
			if (curve.keys.empty()) {
				return 0.0f;
			}
			if (curve.keys.size() == 1) {
				return curve.keys.front().value;
			}

			if (frame <= static_cast<float>(curve.keys.front().frame)) {
				return curve.keys.front().value;
			}
			if (frame >= static_cast<float>(curve.keys.back().frame)) {
				return curve.keys.back().value;
			}

			for (size_t i = 0; i + 1 < curve.keys.size(); ++i) {
				const SequenceFloatKeyAssetData& lhs = curve.keys[i];
				const SequenceFloatKeyAssetData& rhs = curve.keys[i + 1];
				const float lhsFrame = static_cast<float>(lhs.frame);
				const float rhsFrame = static_cast<float>(rhs.frame);
				if (frame < lhsFrame || frame > rhsFrame) {
					continue;
				}

				const float segmentFrames = std::max(1.0f, rhsFrame - lhsFrame);
				const float t = std::clamp(
					(frame - lhsFrame) / segmentFrames,
					0.0f,
					1.0f
				);
				switch (lhs.interpolation) {
					case SEQUENCE_INTERPOLATION_MODE::MODE_STEP: return lhs.value;
					case SEQUENCE_INTERPOLATION_MODE::MODE_LINEAR: return
							lhs.value + (rhs.value - lhs.value) * t;
					case SEQUENCE_INTERPOLATION_MODE::MODE_CUBIC: {
						const float t2  = t * t;
						const float t3  = t2 * t;
						const float h00 = 2.0f * t3 - 3.0f * t2 + 1.0f;
						const float h10 = t3 - 2.0f * t2 + t;
						const float h01 = -2.0f * t3 + 3.0f * t2;
						const float h11 = t3 - t2;
						return
							h00 * lhs.value +
							h10 * lhs.leaveTangent * segmentFrames +
							h01 * rhs.value +
							h11 * rhs.arriveTangent * segmentFrames;
					}
				}
			}

			return curve.keys.back().value;
		}

		[[nodiscard]] bool IsCurveTrackType(const SEQUENCE_TRACK_TYPE trackType) {
			return
				trackType == SEQUENCE_TRACK_TYPE::TRANSFORM ||
				trackType == SEQUENCE_TRACK_TYPE::PROPERTY_FLOAT ||
				trackType == SEQUENCE_TRACK_TYPE::PROPERTY_VEC3 ||
				trackType == SEQUENCE_TRACK_TYPE::SKELETAL_CONTROL;
		}

		void EnsureDefaultChannel(
			SequenceEditorSelection& selection,
			const SEQUENCE_TRACK_TYPE trackType
		) {
			switch (trackType) {
				case SEQUENCE_TRACK_TYPE::TRANSFORM:
					if (
						selection.floatChannel < SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_POS_X ||
						selection.floatChannel > SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_SCALE_Z
					) {
						selection.floatChannel = SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_POS_X;
					}
					return;
				case SEQUENCE_TRACK_TYPE::PROPERTY_FLOAT:
					selection.floatChannel = SEQUENCE_EDITOR_FLOAT_CHANNEL::FLOAT;
					return;
				case SEQUENCE_TRACK_TYPE::PROPERTY_VEC3:
					if (
						selection.floatChannel != SEQUENCE_EDITOR_FLOAT_CHANNEL::VEC3_X &&
						selection.floatChannel != SEQUENCE_EDITOR_FLOAT_CHANNEL::VEC3_Y &&
						selection.floatChannel != SEQUENCE_EDITOR_FLOAT_CHANNEL::VEC3_Z
					) {
						selection.floatChannel = SEQUENCE_EDITOR_FLOAT_CHANNEL::VEC3_X;
					}
					return;
				case SEQUENCE_TRACK_TYPE::SKELETAL_CONTROL:
					if (
						selection.floatChannel < SEQUENCE_EDITOR_FLOAT_CHANNEL::SKELETAL_WEIGHT ||
						selection.floatChannel > SEQUENCE_EDITOR_FLOAT_CHANNEL::SKELETAL_SPEED
					) {
						selection.floatChannel = SEQUENCE_EDITOR_FLOAT_CHANNEL::SKELETAL_WEIGHT;
					}
					return;
				default: return;
			}
		}

		[[nodiscard]] SequenceRichCurveAssetData* ResolveFloatChannelCurve(
			SequenceSectionAssetData&         section,
			const SEQUENCE_TRACK_TYPE         trackType,
			const SEQUENCE_EDITOR_FLOAT_CHANNEL channel
		) {
			switch (trackType) {
				case SEQUENCE_TRACK_TYPE::PROPERTY_FLOAT: return &section.floatCurve;
				case SEQUENCE_TRACK_TYPE::PROPERTY_VEC3:
					switch (channel) {
						case SEQUENCE_EDITOR_FLOAT_CHANNEL::VEC3_X: return &section.vec3XCurve;
						case SEQUENCE_EDITOR_FLOAT_CHANNEL::VEC3_Y: return &section.vec3YCurve;
						case SEQUENCE_EDITOR_FLOAT_CHANNEL::VEC3_Z: return &section.vec3ZCurve;
						default: return &section.vec3XCurve;
					}
				case SEQUENCE_TRACK_TYPE::TRANSFORM:
					switch (channel) {
						case SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_POS_X: return &section.transformPosX;
						case SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_POS_Y: return &section.transformPosY;
						case SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_POS_Z: return &section.transformPosZ;
						case SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_ROT_X: return &section.transformRotX;
						case SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_ROT_Y: return &section.transformRotY;
						case SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_ROT_Z: return &section.transformRotZ;
						case SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_ROT_W: return &section.transformRotW;
						case SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_SCALE_X: return &section.transformScaleX;
						case SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_SCALE_Y: return &section.transformScaleY;
						case SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_SCALE_Z: return &section.transformScaleZ;
						default: return &section.transformPosX;
					}
				case SEQUENCE_TRACK_TYPE::SKELETAL_CONTROL:
					switch (channel) {
						case SEQUENCE_EDITOR_FLOAT_CHANNEL::SKELETAL_WEIGHT: return &section.skeletal.weightCurve;
						case SEQUENCE_EDITOR_FLOAT_CHANNEL::SKELETAL_PLAYBACK: return &section.skeletal.playbackCurve;
						case SEQUENCE_EDITOR_FLOAT_CHANNEL::SKELETAL_SPEED: return &section.skeletal.speedCurve;
						default: return &section.skeletal.weightCurve;
					}
				default: return nullptr;
			}
		}

		void DrawChannelSelector(
			SequenceEditorSelection& selection,
			const SEQUENCE_TRACK_TYPE trackType
		) {
			if (trackType == SEQUENCE_TRACK_TYPE::TRANSFORM) {
				int channelIndex = 0;
				switch (selection.floatChannel) {
					case SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_POS_Y: channelIndex = 1; break;
					case SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_POS_Z: channelIndex = 2; break;
					case SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_ROT_X: channelIndex = 3; break;
					case SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_ROT_Y: channelIndex = 4; break;
					case SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_ROT_Z: channelIndex = 5; break;
					case SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_ROT_W: channelIndex = 6; break;
					case SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_SCALE_X: channelIndex = 7; break;
					case SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_SCALE_Y: channelIndex = 8; break;
					case SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_SCALE_Z: channelIndex = 9; break;
					default: channelIndex = 0; break;
				}
				if (
					ImGui::Combo(
						"Channel",
						&channelIndex,
						"PosX\0PosY\0PosZ\0RotX\0RotY\0RotZ\0RotW\0ScaleX\0ScaleY\0ScaleZ\0"
					)
				) {
					static constexpr std::array<SEQUENCE_EDITOR_FLOAT_CHANNEL, 10> kMap = {
						SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_POS_X,
						SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_POS_Y,
						SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_POS_Z,
						SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_ROT_X,
						SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_ROT_Y,
						SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_ROT_Z,
						SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_ROT_W,
						SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_SCALE_X,
						SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_SCALE_Y,
						SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_SCALE_Z
					};
					selection.floatChannel = kMap[std::clamp(channelIndex, 0, 9)];
				}
				return;
			}

			if (trackType == SEQUENCE_TRACK_TYPE::PROPERTY_VEC3) {
				int channelIndex = selection.floatChannel == SEQUENCE_EDITOR_FLOAT_CHANNEL::VEC3_Y ? 1 :
				                   selection.floatChannel == SEQUENCE_EDITOR_FLOAT_CHANNEL::VEC3_Z ? 2 :
				                   0;
				if (ImGui::Combo("Channel", &channelIndex, "X\0Y\0Z\0")) {
					selection.floatChannel = channelIndex == 1 ?
						                         SEQUENCE_EDITOR_FLOAT_CHANNEL::VEC3_Y :
						                         (channelIndex == 2 ?
							                          SEQUENCE_EDITOR_FLOAT_CHANNEL::VEC3_Z :
							                          SEQUENCE_EDITOR_FLOAT_CHANNEL::VEC3_X);
				}
				return;
			}

			if (trackType == SEQUENCE_TRACK_TYPE::SKELETAL_CONTROL) {
				int channelIndex = selection.floatChannel == SEQUENCE_EDITOR_FLOAT_CHANNEL::SKELETAL_PLAYBACK ?
					                   1 :
					                   selection.floatChannel == SEQUENCE_EDITOR_FLOAT_CHANNEL::SKELETAL_SPEED ?
						                   2 :
						                   0;
				if (ImGui::Combo("Channel", &channelIndex, "Weight\0Playback\0Speed\0")) {
					selection.floatChannel = channelIndex == 1 ?
						                         SEQUENCE_EDITOR_FLOAT_CHANNEL::SKELETAL_PLAYBACK :
						                         (channelIndex == 2 ?
							                          SEQUENCE_EDITOR_FLOAT_CHANNEL::SKELETAL_SPEED :
							                          SEQUENCE_EDITOR_FLOAT_CHANNEL::SKELETAL_WEIGHT);
				}
				return;
			}

			selection.floatChannel = SEQUENCE_EDITOR_FLOAT_CHANNEL::FLOAT;
		}
	}

	void SequenceCurvePanel::Draw(SequenceEditorController& controller) {
		if (!ImGui::Begin("Sequence Curves")) {
			ImGui::End();
			return;
		}

		SequenceEditorDocument* document = controller.GetActiveDocument();
		if (!document) {
			ImGui::TextUnformatted("No sequence document selected.");
			ImGui::End();
			return;
		}

		SequenceAuthoringData& data      = document->GetAuthoringData();
		SequenceEditorSelection& selection = controller.GetSelection();
		if (
			selection.trackIndex < 0 ||
			selection.trackIndex >= static_cast<int32_t>(data.tracks.size())
		) {
			ImGui::TextUnformatted("Select a track in Sequence Timeline.");
			ImGui::End();
			return;
		}

		SequenceTrackAssetData& track = data.tracks[selection.trackIndex];
		if (
			selection.sectionIndex < 0 ||
			selection.sectionIndex >= static_cast<int32_t>(track.sections.size())
		) {
			ImGui::TextUnformatted("Select a section in Sequence Timeline.");
			ImGui::End();
			return;
		}
		SequenceSectionAssetData& section = track.sections[selection.sectionIndex];

		if (!IsCurveTrackType(track.trackType)) {
			ImGui::TextUnformatted("Selected track type has no editable float curve.");
			ImGui::End();
			return;
		}

		EnsureDefaultChannel(selection, track.trackType);
		DrawChannelSelector(selection, track.trackType);

		SequenceRichCurveAssetData* curve = ResolveFloatChannelCurve(
			section,
			track.trackType,
			selection.floatChannel
		);
		if (!curve) {
			ImGui::TextUnformatted("Failed to resolve curve channel.");
			ImGui::End();
			return;
		}

		const int32_t trackIndex = selection.trackIndex;
		const int32_t sectionIndex = selection.sectionIndex;
		const SEQUENCE_EDITOR_FLOAT_CHANNEL channel = selection.floatChannel;

		if (curve->channelId == 0) {
			(void)controller.ModifyActiveDocument(
				[=](SequenceAuthoringData& ioData) {
					if (
						trackIndex < 0 ||
						trackIndex >= static_cast<int32_t>(ioData.tracks.size())
					) {
						return;
					}
					auto& localTrack = ioData.tracks[trackIndex];
					if (
						sectionIndex < 0 ||
						sectionIndex >= static_cast<int32_t>(localTrack.sections.size())
					) {
						return;
					}
					auto* localCurve = ResolveFloatChannelCurve(
						localTrack.sections[sectionIndex],
						localTrack.trackType,
						channel
					);
					if (!localCurve || localCurve->channelId != 0) {
						return;
					}
					localCurve->channelId = AllocateStableId();
				}
			);
		}

		if (ImGui::Button("Insert Key At Playhead")) {
			const int64_t keyFrame = static_cast<int64_t>(std::llround(
				std::max(0.0f, controller.GetPlayheadFrame())
			));
			const float keyValue = EvaluateCurveValue(*curve, static_cast<float>(keyFrame));
			(void)controller.ModifyActiveDocument(
				[=](SequenceAuthoringData& ioData) {
					if (
						trackIndex < 0 ||
						trackIndex >= static_cast<int32_t>(ioData.tracks.size())
					) {
						return;
					}
					auto& localTrack = ioData.tracks[trackIndex];
					if (
						sectionIndex < 0 ||
						sectionIndex >= static_cast<int32_t>(localTrack.sections.size())
					) {
						return;
					}
					auto* localCurve = ResolveFloatChannelCurve(
						localTrack.sections[sectionIndex],
						localTrack.trackType,
						channel
					);
					if (!localCurve) {
						return;
					}
					if (localCurve->channelId == 0) {
						localCurve->channelId = AllocateStableId();
					}
					for (auto& key : localCurve->keys) {
						if (key.frame != keyFrame) {
							continue;
						}
						key.value = keyValue;
						return;
					}
					localCurve->keys.emplace_back(
						SequenceFloatKeyAssetData{
							.keyId = AllocateStableId(),
							.frame = keyFrame,
							.value = keyValue,
							.arriveTangent = 0.0f,
							.leaveTangent = 0.0f,
							.interpolation = SEQUENCE_INTERPOLATION_MODE::MODE_LINEAR
						}
					);
					std::ranges::sort(
						localCurve->keys,
						[](const SequenceFloatKeyAssetData& lhs, const SequenceFloatKeyAssetData& rhs) {
							return lhs.frame < rhs.frame;
						}
					);
				}
			);
		}
		ImGui::SameLine();
		if (
			ImGui::Button("Delete Selected Key") &&
			selection.keyIndex >= 0 &&
			selection.keyIndex < static_cast<int32_t>(curve->keys.size())
		) {
			const int32_t deleteIndex = selection.keyIndex;
			(void)controller.ModifyActiveDocument(
				[=](SequenceAuthoringData& ioData) {
					if (
						trackIndex < 0 ||
						trackIndex >= static_cast<int32_t>(ioData.tracks.size())
					) {
						return;
					}
					auto& localTrack = ioData.tracks[trackIndex];
					if (
						sectionIndex < 0 ||
						sectionIndex >= static_cast<int32_t>(localTrack.sections.size())
					) {
						return;
					}
					auto* localCurve = ResolveFloatChannelCurve(
						localTrack.sections[sectionIndex],
						localTrack.trackType,
						channel
					);
					if (
						!localCurve ||
						deleteIndex < 0 ||
						deleteIndex >= static_cast<int32_t>(localCurve->keys.size())
					) {
						return;
					}
					localCurve->keys.erase(localCurve->keys.begin() + deleteIndex);
				}
			);
			selection.keyIndex = -1;
		}

		ImGui::Separator();

		if (!curve->keys.empty()) {
			static constexpr int32_t kSampleCount = 128;
			std::array<float, kSampleCount> samples = {};
			const float minFrame = static_cast<float>(section.startFrame);
			const float maxFrame = static_cast<float>(std::max(section.startFrame + 1, section.endFrame));
			for (int32_t i = 0; i < kSampleCount; ++i) {
				const float t = static_cast<float>(i) / static_cast<float>(kSampleCount - 1);
				const float frame = minFrame + (maxFrame - minFrame) * t;
				samples[static_cast<size_t>(i)] = EvaluateCurveValue(*curve, frame);
			}
			ImGui::PlotLines(
				"##curvePreview",
				samples.data(),
				kSampleCount,
				0,
				nullptr,
				FLT_MAX,
				FLT_MAX,
				ImVec2(0.0f, 180.0f)
			);
		} else {
			ImGui::TextUnformatted("No keys in this curve.");
		}

		ImGui::Separator();
		ImGui::Text("Keys: %zu", curve->keys.size());
		for (int32_t keyIndex = 0; keyIndex < static_cast<int32_t>(curve->keys.size()); ++keyIndex) {
			const SequenceFloatKeyAssetData& key = curve->keys[static_cast<size_t>(keyIndex)];
			ImGui::PushID(keyIndex);

			const bool selected = selection.keyIndex == keyIndex;
			if (
				ImGui::Selectable(
					(std::string("Key ") + std::to_string(keyIndex) +
					 "  F:" + std::to_string(key.frame) +
					 "  V:" + std::to_string(key.value)).c_str(),
					selected
				)
			) {
				selection.keyIndex = keyIndex;
			}
			if (!selected) {
				ImGui::PopID();
				continue;
			}

			int frameValue = static_cast<int>(key.frame);
			if (ImGui::InputInt("Frame", &frameValue)) {
				const int32_t localKeyIndex = keyIndex;
				(void)controller.ModifyActiveDocument(
					[=](SequenceAuthoringData& ioData) {
						auto* localCurve = ResolveFloatChannelCurve(
							ioData.tracks[trackIndex].sections[sectionIndex],
							ioData.tracks[trackIndex].trackType,
							channel
						);
						if (
							!localCurve ||
							localKeyIndex < 0 ||
							localKeyIndex >= static_cast<int32_t>(localCurve->keys.size())
						) {
							return;
						}
						localCurve->keys[localKeyIndex].frame = std::max(0, frameValue);
						std::ranges::sort(
							localCurve->keys,
							[](const SequenceFloatKeyAssetData& lhs, const SequenceFloatKeyAssetData& rhs) {
								return lhs.frame < rhs.frame;
							}
						);
					}
				);
			}

			float value = key.value;
			if (ImGui::DragFloat("Value", &value, 0.05f)) {
				const int32_t localKeyIndex = keyIndex;
				(void)controller.ModifyActiveDocument(
					[=](SequenceAuthoringData& ioData) {
						auto* localCurve = ResolveFloatChannelCurve(
							ioData.tracks[trackIndex].sections[sectionIndex],
							ioData.tracks[trackIndex].trackType,
							channel
						);
						if (
							!localCurve ||
							localKeyIndex < 0 ||
							localKeyIndex >= static_cast<int32_t>(localCurve->keys.size())
						) {
							return;
						}
						localCurve->keys[localKeyIndex].value = value;
					}
				);
			}

			float arrive = key.arriveTangent;
			if (ImGui::DragFloat("Arrive", &arrive, 0.05f)) {
				const int32_t localKeyIndex = keyIndex;
				(void)controller.ModifyActiveDocument(
					[=](SequenceAuthoringData& ioData) {
						auto* localCurve = ResolveFloatChannelCurve(
							ioData.tracks[trackIndex].sections[sectionIndex],
							ioData.tracks[trackIndex].trackType,
							channel
						);
						if (
							!localCurve ||
							localKeyIndex < 0 ||
							localKeyIndex >= static_cast<int32_t>(localCurve->keys.size())
						) {
							return;
						}
						localCurve->keys[localKeyIndex].arriveTangent = arrive;
					}
				);
			}

			float leave = key.leaveTangent;
			if (ImGui::DragFloat("Leave", &leave, 0.05f)) {
				const int32_t localKeyIndex = keyIndex;
				(void)controller.ModifyActiveDocument(
					[=](SequenceAuthoringData& ioData) {
						auto* localCurve = ResolveFloatChannelCurve(
							ioData.tracks[trackIndex].sections[sectionIndex],
							ioData.tracks[trackIndex].trackType,
							channel
						);
						if (
							!localCurve ||
							localKeyIndex < 0 ||
							localKeyIndex >= static_cast<int32_t>(localCurve->keys.size())
						) {
							return;
						}
						localCurve->keys[localKeyIndex].leaveTangent = leave;
					}
				);
			}

			int interpolationIndex = key.interpolation == SEQUENCE_INTERPOLATION_MODE::MODE_STEP ? 0 :
			                         key.interpolation == SEQUENCE_INTERPOLATION_MODE::MODE_CUBIC ? 2 :
			                         1;
			if (ImGui::Combo("Interpolation", &interpolationIndex, "Step\0Linear\0Cubic\0")) {
				const int32_t localKeyIndex = keyIndex;
				const SEQUENCE_INTERPOLATION_MODE interpolation =
					interpolationIndex == 0 ? SEQUENCE_INTERPOLATION_MODE::MODE_STEP :
					interpolationIndex == 2 ? SEQUENCE_INTERPOLATION_MODE::MODE_CUBIC :
					                          SEQUENCE_INTERPOLATION_MODE::MODE_LINEAR;
				(void)controller.ModifyActiveDocument(
					[=](SequenceAuthoringData& ioData) {
						auto* localCurve = ResolveFloatChannelCurve(
							ioData.tracks[trackIndex].sections[sectionIndex],
							ioData.tracks[trackIndex].trackType,
							channel
						);
						if (
							!localCurve ||
							localKeyIndex < 0 ||
							localKeyIndex >= static_cast<int32_t>(localCurve->keys.size())
						) {
							return;
						}
						localCurve->keys[localKeyIndex].interpolation = interpolation;
					}
				);
			}

			ImGui::Separator();
			ImGui::PopID();
		}

		ImGui::End();
	}
}

#endif
