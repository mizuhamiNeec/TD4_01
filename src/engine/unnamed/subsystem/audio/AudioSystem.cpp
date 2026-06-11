#include "engine/unnamed/subsystem/audio/AudioSystem.h"

#include <algorithm>

#include "engine/unnamed/subsystem/audio/Audio.h"

namespace Unnamed {
	AudioSystem::AudioSystem() = default;

	AudioSystem::~AudioSystem() {
		Shutdown();
	}

	bool AudioSystem::Init() {
		if (mXAudio2 && mMasterVoice) {
			return true;
		}

		HRESULT hr = XAudio2Create(
			mXAudio2.GetAddressOf(), 0, XAUDIO2_DEFAULT_PROCESSOR
		);
		if (FAILED(hr)) {
			return false;
		}

		hr = mXAudio2->CreateMasteringVoice(&mMasterVoice);
		if (FAILED(hr)) {
			mXAudio2.Reset();
			mMasterVoice = nullptr;
			return false;
		}
		mMasterVoice->SetVolume(mMasterVolume);

		return true;
	}

	void AudioSystem::Shutdown() {
		StopAll();
		mVoices.clear();

		if (mMasterVoice) {
			mMasterVoice->DestroyVoice();
			mMasterVoice = nullptr;
		}
		mXAudio2.Reset();
	}

	std::shared_ptr<AudioVoice> AudioSystem::CreateVoice(
		const SoundAssetData& soundData
	) {
		if (!mXAudio2 || !mMasterVoice) {
			return nullptr;
		}

		auto voice = std::make_shared<AudioVoice>();
		if (!voice->Init(mXAudio2.Get(), soundData)) {
			return nullptr;
		}
		voice->SetBusVolume(GetBusVolume(voice->GetBus()));

		CleanupExpiredVoices();
		mVoices.emplace_back(voice);
		return voice;
	}

	void AudioSystem::StopAll() {
		CleanupExpiredVoices();
		for (const auto& weak : mVoices) {
			if (auto voice = weak.lock()) {
				voice->Stop();
			}
		}
	}

	bool AudioSystem::IsReady() const noexcept {
		return mXAudio2 != nullptr && mMasterVoice != nullptr;
	}

	void AudioSystem::SetMasterVolume(float volume) noexcept {
		mMasterVolume = std::clamp(volume, 0.0f, 1.0f);
		if (mMasterVoice) {
			mMasterVoice->SetVolume(mMasterVolume);
		}
	}

	void AudioSystem::SetBusVolume(const AudioBus bus, float volume) noexcept {
		const auto index = static_cast<size_t>(bus);
		if (index >= mBusVolumes.size()) {
			return;
		}
		mBusVolumes[index] = std::clamp(volume, 0.0f, 1.0f);
		ApplyBusVolume(bus);
	}

	float AudioSystem::GetBusVolume(const AudioBus bus) const noexcept {
		const auto index = static_cast<size_t>(bus);
		if (index >= mBusVolumes.size()) {
			return 1.0f;
		}
		return mBusVolumes[index];
	}

	void AudioSystem::CleanupExpiredVoices() {
		std::erase_if(
			mVoices,
			[](const std::weak_ptr<AudioVoice>& weak) { return weak.expired(); }
		);
	}

	void AudioSystem::ApplyBusVolume(const AudioBus bus) {
		CleanupExpiredVoices();
		const float volume = GetBusVolume(bus);
		for (const auto& weak : mVoices) {
			if (auto voice = weak.lock(); voice && voice->GetBus() == bus) {
				voice->SetBusVolume(volume);
			}
		}
	}
}
