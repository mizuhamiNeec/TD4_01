#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include <wrl/client.h>

struct IXAudio2MasteringVoice;
struct IXAudio2;

namespace Unnamed {
	class AudioVoice;
	struct SoundAssetData;

	/// @brief 音量調整に使うサウンドカテゴリです。
	enum class AudioBus : uint8_t {
		Bgm,
		Sfx,
		Count,
	};

	class AudioSystem {
	public:
		AudioSystem();
		~AudioSystem();

		AudioSystem(const AudioSystem&)            = delete;
		AudioSystem& operator=(const AudioSystem&) = delete;

		bool Init();
		void Shutdown();

		[[nodiscard]] std::shared_ptr<AudioVoice> CreateVoice(
			const SoundAssetData& soundData
		);

		void StopAll();
		[[nodiscard]] bool IsReady() const noexcept;

		/// @brief 全体音量を設定します。
		void SetMasterVolume(float volume) noexcept;
		/// @brief 指定カテゴリの音量を設定します。
		void SetBusVolume(AudioBus bus, float volume) noexcept;
		/// @brief 指定カテゴリの音量を取得します。
		[[nodiscard]] float GetBusVolume(AudioBus bus) const noexcept;

	private:
		void CleanupExpiredVoices();
		void ApplyBusVolume(AudioBus bus);

		Microsoft::WRL::ComPtr<IXAudio2> mXAudio2;
		IXAudio2MasteringVoice*          mMasterVoice = nullptr;
		std::vector<std::weak_ptr<AudioVoice>> mVoices;
		float                                  mMasterVolume = 1.0f;
		std::array<float, static_cast<size_t>(AudioBus::Count)> mBusVolumes = {
			1.0f,
			1.0f
		};
	};
}
