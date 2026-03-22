#pragma once
#include "core/assets/AssetID.h"
#include "core/math/Mat4.h"
#include "core/math/Vec3.h"

#include "engine/unnamed/framework/components/base/BaseComponent.h"

namespace Unnamed {
	class JsonReader;
	class JsonWriter;
	class TransformComponent;

	class TriggerVolumeComponentBase : public BaseComponent {
	public:
		[[nodiscard]] Vec3 GetLocalCenter() const noexcept {
			return mLocalCenter;
		}

		[[nodiscard]] Vec3 GetExtentsHu() const noexcept {
			return mExtentsHu;
		}

		[[nodiscard]] Vec3 GetWorldCenter() const noexcept;
		[[nodiscard]] Vec3 GetWorldHalfExtentsMeters() const noexcept;

	protected:
		void DeserializeVolume(const JsonReader& reader);
		void SerializeVolume(JsonWriter& writer) const;
		[[nodiscard]] TransformComponent* GetTransform() const;

		Vec3 mLocalCenter = Vec3::zero;
		Vec3 mExtentsHu   = Vec3(32.0f, 32.0f, 32.0f);
	};

	class JumpPadComponent final : public TriggerVolumeComponentBase {
	public:
		[[nodiscard]] std::string_view GetStableName() const override {
			return "parkour.JumpPad";
		}

		[[nodiscard]] std::string_view GetComponentName() const override {
			return "JumpPad";
		}

		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;

		[[nodiscard]] float GetBoostVelocityHu() const noexcept {
			return mBoostVelocityHu;
		}

	private:
		float mBoostVelocityHu = 800.0f;
	};

	class SpeedBoostAreaComponent final : public TriggerVolumeComponentBase {
	public:
		[[nodiscard]] std::string_view GetStableName() const override {
			return "parkour.SpeedBoostArea";
		}

		[[nodiscard]] std::string_view GetComponentName() const override {
			return "SpeedBoostArea";
		}

		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;

		[[nodiscard]] float GetMultiplier() const noexcept {
			return mMultiplier;
		}

		[[nodiscard]] float GetDurationSec() const noexcept {
			return mDurationSec;
		}

	private:
		float mMultiplier  = 1.5f;
		float mDurationSec = 3.0f;
	};

	class CheckpointComponent final : public TriggerVolumeComponentBase {
	public:
		[[nodiscard]] std::string_view GetStableName() const override {
			return "parkour.Checkpoint";
		}

		[[nodiscard]] std::string_view GetComponentName() const override {
			return "Checkpoint";
		}

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

	class GoalComponent final : public TriggerVolumeComponentBase {
	public:
		[[nodiscard]] std::string_view GetStableName() const override {
			return "parkour.Goal";
		}

		[[nodiscard]] std::string_view GetComponentName() const override {
			return "Goal";
		}

		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;
	};
}
