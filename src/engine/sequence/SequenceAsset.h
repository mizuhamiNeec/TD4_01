#pragma once

#include <memory>
#include <string>
#include <vector>

#include "SequenceBinder.h"

namespace Unnamed {
	class JsonReader;
	class JsonWriter;

	class SequenceAsset {
	public:
		[[nodiscard]] static std::shared_ptr<SequenceAsset> Load(
			const std::string& path
		);
		[[nodiscard]] bool Save(const std::string& path) const;

		void Clear();

		void SetName(const std::string& name);
		[[nodiscard]] const std::string& GetName() const;

		void SetLengthSeconds(float seconds);
		[[nodiscard]] float GetLengthSeconds() const;

		void SetLoop(bool loop);
		[[nodiscard]] bool IsLoop() const;

		void SetLoopType(LOOP_TYPE loopType);
		[[nodiscard]] LOOP_TYPE GetLoopType() const;

		std::vector<SequenceTrack>&       GetTracks();
		const std::vector<SequenceTrack>& GetTracks() const;

		void RecalculateLengthFromTracks();

		void Serialize(JsonWriter& writer) const;
		bool Deserialize(const JsonReader& reader);

	private:
		std::string               mName;
		float                     mLengthSeconds = 0.0f;
		bool                      mLoop          = false;
		LOOP_TYPE                 mLoopType      = LOOP_TYPE::RESTART;
		std::vector<SequenceTrack> mTracks;
	};
}
