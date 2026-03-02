#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Unnamed {
	class UProfiler {
	public:
		struct SampleView {
			std::string_view          name;
			const std::vector<float>* history           = nullptr;
			uint32_t                  historyWriteIndex = 0;
			float                     currentMs         = 0.0f;
			float                     averageMs         = 0.0f;
			float                     maxMs             = 0.0f;
			uint32_t                  colorIndex        = 0;
		};

		class ScopeTimer {
		public:
			ScopeTimer(UProfiler* profiler, std::string_view name);
			~ScopeTimer();

			ScopeTimer(const ScopeTimer&)            = delete;
			ScopeTimer& operator=(const ScopeTimer&) = delete;

		private:
			using Clock = std::chrono::steady_clock;

			UProfiler*        mProfiler = nullptr;
			std::string_view  mName;
			Clock::time_point mStart = {};
		};

		void BeginFrame();
		void EndFrame();
		void AddSample(std::string_view name, float milliseconds);

		[[nodiscard]] const std::vector<SampleView>& GetSamples() const;
		[[nodiscard]] uint32_t                       GetHistorySize() const;
		[[nodiscard]] uint64_t                       GetFrameCount() const;

	private:
		struct SampleData {
			std::string        name;
			std::vector<float> history;
			float              frameAccumulatedMs = 0.0f;
			float              currentMs          = 0.0f;
			float              averageMs          = 0.0f;
			float              maxMs              = 0.0f;
			uint32_t           colorIndex         = 0;
		};

		SampleData& GetOrCreateSample(std::string_view name);

		static constexpr uint32_t kHistorySize = 180;

		std::vector<SampleData>                 mSamples;
		std::unordered_map<std::string, size_t> mSampleIndices;
		mutable std::vector<SampleView>         mSampleViews;
		uint32_t                                mHistoryWriteIndex = 0;
		uint64_t                                mFrameCount        = 0;
	};
}
