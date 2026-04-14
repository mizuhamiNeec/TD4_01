#include "SequenceAsset.h"

#include <algorithm>
#include <iterator>

#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"

#include "engine/unnamed/subsystem/console/Log.h"

namespace Unnamed {
	namespace {
		static constexpr std::string_view kChannel = "SequenceAsset";

		struct EaseNamePair {
			std::string_view name = "LINEAR";
			EASE_TYPE        type = EASE_TYPE::LINEAR;
		};

		constexpr EaseNamePair kEaseNames[] = {
			{.name = "LINEAR", .type = EASE_TYPE::LINEAR},
			{.name = "IN_SINE", .type = EASE_TYPE::IN_SINE},
			{.name = "OUT_SINE", .type = EASE_TYPE::OUT_SINE},
			{.name = "IN_OUT_SINE", .type = EASE_TYPE::IN_OUT_SINE},
			{.name = "IN_QUAD", .type = EASE_TYPE::IN_QUAD},
			{.name = "OUT_QUAD", .type = EASE_TYPE::OUT_QUAD},
			{.name = "IN_OUT_QUAD", .type = EASE_TYPE::IN_OUT_QUAD},
			{.name = "IN_CUBIC", .type = EASE_TYPE::IN_CUBIC},
			{.name = "OUT_CUBIC", .type = EASE_TYPE::OUT_CUBIC},
			{.name = "IN_OUT_CUBIC", .type = EASE_TYPE::IN_OUT_CUBIC},
			{.name = "IN_QUART", .type = EASE_TYPE::IN_QUART},
			{.name = "OUT_QUART", .type = EASE_TYPE::OUT_QUART},
			{.name = "IN_OUT_QUART", .type = EASE_TYPE::IN_OUT_QUART},
			{.name = "IN_QUINT", .type = EASE_TYPE::IN_QUINT},
			{.name = "OUT_QUINT", .type = EASE_TYPE::OUT_QUINT},
			{.name = "IN_OUT_QUINT", .type = EASE_TYPE::IN_OUT_QUINT},
			{.name = "IN_EXPO", .type = EASE_TYPE::IN_EXPO},
			{.name = "OUT_EXPO", .type = EASE_TYPE::OUT_EXPO},
			{.name = "IN_OUT_EXPO", .type = EASE_TYPE::IN_OUT_EXPO},
			{.name = "IN_CIRC", .type = EASE_TYPE::IN_CIRC},
			{.name = "OUT_CIRC", .type = EASE_TYPE::OUT_CIRC},
			{.name = "IN_OUT_CIRC", .type = EASE_TYPE::IN_OUT_CIRC},
			{.name = "IN_BACK", .type = EASE_TYPE::IN_BACK},
			{.name = "OUT_BACK", .type = EASE_TYPE::OUT_BACK},
			{.name = "IN_OUT_BACK", .type = EASE_TYPE::IN_OUT_BACK},
			{.name = "IN_ELASTIC", .type = EASE_TYPE::IN_ELASTIC},
			{.name = "OUT_ELASTIC", .type = EASE_TYPE::OUT_ELASTIC},
			{.name = "IN_OUT_ELASTIC", .type = EASE_TYPE::IN_OUT_ELASTIC},
			{.name = "IN_BOUNCE", .type = EASE_TYPE::IN_BOUNCE},
			{.name = "OUT_BOUNCE", .type = EASE_TYPE::OUT_BOUNCE},
			{.name = "IN_OUT_BOUNCE", .type = EASE_TYPE::IN_OUT_BOUNCE},
		};

		std::string_view EaseToString(const EASE_TYPE ease) {
			for (const EaseNamePair& entry : kEaseNames) {
				if (entry.type == ease) {
					return entry.name;
				}
			}
			return "LINEAR";
		}

		EASE_TYPE EaseFromReader(const JsonReader& reader) {
			if (!reader.Valid()) {
				return EASE_TYPE::LINEAR;
			}

			const std::string      text  = reader.GetString();
			const std::string_view value = text;
			if (!value.empty()) {
				for (const EaseNamePair& entry : kEaseNames) {
					if (entry.name == value) {
						return entry.type;
					}
				}
			}

			const int index = reader.GetInt();
			if (index < 0 || index >= static_cast<int>(std::size(kEaseNames))) {
				return EASE_TYPE::LINEAR;
			}
			return kEaseNames[index].type;
		}

		std::string_view LoopTypeToString(const LOOP_TYPE loopType) {
			return loopType == LOOP_TYPE::YOYO ? "Yoyo" : "Restart";
		}

		LOOP_TYPE LoopTypeFromReader(const JsonReader& reader) {
			if (!reader.Valid()) {
				return LOOP_TYPE::RESTART;
			}

			const std::string text = reader.GetString();
			if (!text.empty()) {
				if (text == std::string("Yoyo")) {
					return LOOP_TYPE::YOYO;
				}
				if (text == std::string("Restart")) {
					return LOOP_TYPE::RESTART;
				}
			}

			const int raw = reader.GetInt();
			return raw == static_cast<int>(LOOP_TYPE::YOYO) ?
				       LOOP_TYPE::YOYO :
				       LOOP_TYPE::RESTART;
		}

		std::string_view BindingTargetToString(
			const SEQUENCE_BINDING_TARGET target
		) {
			return target == SEQUENCE_BINDING_TARGET::UI ? "UI" : "Entity";
		}

		SEQUENCE_BINDING_TARGET BindingTargetFromReader(
			const JsonReader& reader
		) {
			if (!reader.Valid()) {
				return SEQUENCE_BINDING_TARGET::ENTITY;
			}

			const std::string text = reader.GetString();
			if (!text.empty()) {
				if (text == std::string("UI")) {
					return SEQUENCE_BINDING_TARGET::UI;
				}
				if (text == std::string("Entity")) {
					return SEQUENCE_BINDING_TARGET::ENTITY;
				}
			}

			return reader.GetInt() == static_cast<int>(
				       SEQUENCE_BINDING_TARGET::UI) ?
				       SEQUENCE_BINDING_TARGET::UI :
				       SEQUENCE_BINDING_TARGET::ENTITY;
		}
	}

	std::shared_ptr<SequenceAsset>
	SequenceAsset::Load(const std::string& path) {
		const JsonReader reader(path);
		if (!reader.Valid()) {
			Error(kChannel, "Failed to open sequence asset '{}'.", path);
			return nullptr;
		}

		auto asset = std::make_shared<SequenceAsset>();
		if (!asset->Deserialize(reader)) {
			Error(kChannel, "Failed to parse sequence asset '{}'.", path);
			return nullptr;
		}
		return asset;
	}

	bool SequenceAsset::Save(const std::string& path) const {
		JsonWriter writer(path);
		Serialize(writer);
		return writer.Save();
	}

	void SequenceAsset::Clear() {
		mName.clear();
		mLengthSeconds = 0.0f;
		mLoop          = false;
		mLoopType      = LOOP_TYPE::RESTART;
		mTracks.clear();
	}

	void SequenceAsset::SetName(const std::string& name) {
		mName = name;
	}

	const std::string& SequenceAsset::GetName() const {
		return mName;
	}

	void SequenceAsset::SetLengthSeconds(const float seconds) {
		mLengthSeconds = std::max(0.0f, seconds);
	}

	float SequenceAsset::GetLengthSeconds() const {
		return mLengthSeconds;
	}

	void SequenceAsset::SetLoop(const bool loop) {
		mLoop = loop;
	}

	bool SequenceAsset::IsLoop() const {
		return mLoop;
	}

	void SequenceAsset::SetLoopType(const LOOP_TYPE loopType) {
		mLoopType = loopType;
	}

	LOOP_TYPE SequenceAsset::GetLoopType() const {
		return mLoopType;
	}

	std::vector<SequenceTrack>& SequenceAsset::GetTracks() {
		return mTracks;
	}

	const std::vector<SequenceTrack>& SequenceAsset::GetTracks() const {
		return mTracks;
	}

	void SequenceAsset::RecalculateLengthFromTracks() {
		float maxTime = 0.0f;
		for (const SequenceTrack& track : mTracks) {
			for (const SequenceKeyframe& key : track.keyframes) {
				maxTime = std::max(maxTime, key.time);
			}
		}
		mLengthSeconds = std::max(mLengthSeconds, maxTime);
	}

	void SequenceAsset::Serialize(JsonWriter& writer) const {
		writer.BeginObject();
		writer.Key("version");
		writer.Write(1);
		writer.Key("name");
		writer.Write(mName);
		writer.Key("length");
		writer.Write(mLengthSeconds);
		writer.Key("loop");
		writer.Write(mLoop);
		writer.Key("loopType");
		writer.Write(std::string(LoopTypeToString(mLoopType)));

		writer.Key("tracks");
		writer.BeginArray();
		for (const SequenceTrack& track : mTracks) {
			writer.BeginObject();
			writer.Key("name");
			writer.Write(track.name);

			writer.Key("binding");
			writer.BeginObject();
			writer.Key("target");
			writer.Write(
				std::string(BindingTargetToString(track.binding.target))
			);

			if (track.binding.target == SEQUENCE_BINDING_TARGET::ENTITY) {
				writer.Key("entityGuid");
				writer.Write(
					static_cast<int64_t>(track.binding.entity.entityGuid)
				);
				writer.Key("componentStableName");
				writer.Write(track.binding.entity.componentStableName);
				writer.Key("propertyPath");
				writer.Write(track.binding.entity.propertyPath);
			} else {
				writer.Key("canvasEntityGuid");
				writer.Write(
					static_cast<int64_t>(track.binding.ui.canvasEntityGuid)
				);
				writer.Key("widgetName");
				writer.Write(track.binding.ui.widgetName);
				writer.Key("componentType");
				writer.Write(track.binding.ui.componentType);
				writer.Key("propertyPath");
				writer.Write(track.binding.ui.propertyPath);
			}
			writer.EndObject();

			writer.Key("keys");
			writer.BeginArray();
			for (const SequenceKeyframe& keyframe : track.keyframes) {
				writer.BeginObject();
				writer.Key("time");
				writer.Write(keyframe.time);
				writer.Key("value");
				writer.Write(keyframe.value);
				writer.Key("ease");
				writer.Write(std::string(EaseToString(keyframe.ease)));
				writer.EndObject();
			}
			writer.EndArray();
			writer.EndObject();
		}
		writer.EndArray();

		writer.EndObject();
	}

	bool SequenceAsset::Deserialize(const JsonReader& reader) {
		if (!reader.Valid()) {
			return false;
		}

		Clear();

		const JsonReader versionNode = reader["version"];
		if (versionNode.Valid() && versionNode.GetInt() != 1) {
			Error(
				kChannel, "Unsupported sequence version '{}'.",
				versionNode.GetInt()
			);
			return false;
		}

		if (reader.Has("name")) {
			mName = reader["name"].GetString();
		}
		if (reader.Has("length")) {
			mLengthSeconds = std::max(0.0f, reader["length"].GetFloat());
		}
		if (reader.Has("loop")) {
			mLoop = reader["loop"].GetBool();
		}
		if (reader.Has("loopType")) {
			mLoopType = LoopTypeFromReader(reader["loopType"]);
		}

		if (reader.Has("tracks")) {
			const JsonReader tracks = reader["tracks"].GetArray();
			mTracks.reserve(tracks.Size());
			for (size_t i = 0; i < tracks.Size(); ++i) {
				const JsonReader trackNode = tracks[i];
				if (!trackNode.Valid()) {
					continue;
				}

				SequenceTrack track = {};
				if (trackNode.Has("name")) {
					track.name = trackNode["name"].GetString();
				}

				if (trackNode.Has("binding")) {
					const JsonReader bindingNode = trackNode["binding"];
					track.binding.target         = BindingTargetFromReader(
						bindingNode["target"]
					);
					if (track.binding.target ==
					    SEQUENCE_BINDING_TARGET::ENTITY) {
						if (bindingNode.Has("entityGuid")) {
							track.binding.entity.entityGuid =
								bindingNode["entityGuid"].GetUint64();
						}
						if (bindingNode.Has("componentStableName")) {
							track.binding.entity.componentStableName =
								bindingNode["componentStableName"].GetString();
						}
						if (bindingNode.Has("propertyPath")) {
							track.binding.entity.propertyPath =
								bindingNode["propertyPath"].GetString();
						}
					} else {
						if (bindingNode.Has("canvasEntityGuid")) {
							track.binding.ui.canvasEntityGuid =
								bindingNode["canvasEntityGuid"].GetUint64();
						}
						if (bindingNode.Has("widgetName")) {
							track.binding.ui.widgetName = bindingNode[
								"widgetName"].GetString();
						}
						if (bindingNode.Has("componentType")) {
							track.binding.ui.componentType =
								bindingNode["componentType"].GetString();
						}
						if (bindingNode.Has("propertyPath")) {
							track.binding.ui.propertyPath =
								bindingNode["propertyPath"].GetString();
						}
					}
				}

				if (trackNode.Has("keys")) {
					const JsonReader keys = trackNode["keys"].GetArray();
					track.keyframes.reserve(keys.Size());
					for (size_t k = 0; k < keys.Size(); ++k) {
						const JsonReader keyNode = keys[k];
						if (!keyNode.Valid()) {
							continue;
						}

						SequenceKeyframe keyframe = {};
						if (keyNode.Has("time")) {
							keyframe.time = std::max(
								0.0f, keyNode["time"].GetFloat()
							);
						}
						if (keyNode.Has("value")) {
							keyframe.value = keyNode["value"].GetFloat();
						}
						if (keyNode.Has("ease")) {
							keyframe.ease = EaseFromReader(keyNode["ease"]);
						}
						track.keyframes.emplace_back(keyframe);
					}
					std::ranges::sort(
						track.keyframes
						,
						[](
						const SequenceKeyframe& a, const SequenceKeyframe& b
					) {
							return a.time < b.time;
						}
					);
				}

				mTracks.emplace_back(std::move(track));
			}
		}

		RecalculateLengthFromTracks();
		return true;
	}
}
