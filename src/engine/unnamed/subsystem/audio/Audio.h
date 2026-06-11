#pragma once
#include <cstdint>
#include <vector>

#include <xaudio2.h>

#include "engine/unnamed/subsystem/audio/AudioSystem.h"

namespace Unnamed {
	struct SoundAssetData;

	/// @brief サウンド再生インスタンス（1ボイス）
	class AudioVoice final {
	public:
		AudioVoice();
		~AudioVoice();

		AudioVoice(const AudioVoice&)            = delete;
		AudioVoice& operator=(const AudioVoice&) = delete;

		bool Init(IXAudio2* xAudio2, const SoundAssetData& soundData);

		void Play(bool isLoop = false);
		void Stop();
		void Pause();
		void Resume();
		void SetVolume(float volume) const;
		void SetPitch(float pitch) const;
		/// @brief このボイスが属するサウンドカテゴリを設定します。
		void SetBus(AudioBus bus) noexcept;
		/// @brief カテゴリ音量を設定し、実効音量へ反映します。
		void SetBusVolume(float volume) noexcept;

		[[nodiscard]] bool IsPlaying() const;
		[[nodiscard]] bool IsPaused() const noexcept;
		/// @brief このボイスが属するサウンドカテゴリを取得します。
		[[nodiscard]] AudioBus GetBus() const noexcept;

	private:
		void DestroyVoice();
		/// @brief 個別音量とカテゴリ音量を SourceVoice に適用します。
		void ApplyEffectiveVolume() const;

		IXAudio2SourceVoice* mSourceVoice = nullptr;
		XAUDIO2_BUFFER       mAudioBuffer = {};
		std::vector<uint8_t> mOwnedPcmData;
		bool                 mIsPaused = false;
		mutable float        mVolume   = 1.0f;
		float                mBusVolume = 1.0f;
		AudioBus             mBus       = AudioBus::Sfx;
	};
}
