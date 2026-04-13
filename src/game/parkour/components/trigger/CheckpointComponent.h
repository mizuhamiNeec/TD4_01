#pragma once

#include "TriggerVolumeComponentBase.h"

namespace Unnamed {
	class JsonReader;
	class JsonWriter;

	class CheckpointComponent final : public TriggerVolumeComponentBase {
	public:
		[[nodiscard]] std::string_view GetStableName() const override {
			return "parkour.Checkpoint";
		}

		[[nodiscard]] std::string_view GetComponentName() const override {
			return "Checkpoint";
		}

#ifdef _DEBUG
		void DrawInspectorImGui() override;
#endif

		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;

		[[nodiscard]] int32_t GetIndex() const noexcept {
			return mIndex;
		}

		[[nodiscard]] Vec3 GetRespawnPosition() const noexcept {
			return mRespawnPosition;
		}

	private:
		int32_t mIndex           = 0;
		Vec3    mRespawnPosition = Vec3::zero;
	};
}
