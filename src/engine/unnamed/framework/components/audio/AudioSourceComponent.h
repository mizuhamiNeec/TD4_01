#pragma once

#include <memory>
#include <string>

#include "core/assets/AssetID.h"
#include "engine/unnamed/framework/components/base/BaseComponent.h"
#include "engine/unnamed/subsystem/audio/AudioSystem.h"

namespace Unnamed {
	class AssetManager;
	class AudioSystem;
	class AudioVoice;
	class JsonReader;
	class JsonWriter;

	class AudioSourceComponent final : public BaseComponent {
	public:
		[[nodiscard]] std::string_view GetStableName() const override;

		[[nodiscard]] std::string_view GetComponentName() const override;

		void OnAttached() override;
		void OnDetached() override;
		void OnTick(float deltaTime) override;

		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;

#ifdef _DEBUG
		void DrawInspectorImGui() override;
#endif

		[[nodiscard]] uint32_t GetIcon() const override;

		void                             SetSoundPath(const std::string& path);
		[[nodiscard]] const std::string& GetSoundPath() const noexcept;

		void               SetPlayOnStart(bool enabled) noexcept;
		[[nodiscard]] bool GetPlayOnStart() const noexcept;

		void               SetLoop(bool enabled) noexcept;
		[[nodiscard]] bool GetLoop() const noexcept;

		void                SetVolume(float volume) noexcept;
		[[nodiscard]] float GetVolume() const noexcept;

		/// @brief サウンドカテゴリを設定します。
		void SetBus(AudioBus bus) noexcept;
		/// @brief サウンドカテゴリを取得します。
		[[nodiscard]] AudioBus GetBus() const noexcept;

		void                SetPitch(float pitch) noexcept;
		[[nodiscard]] float GetPitch() const noexcept;

		void               Play();
		void               Stop() const;
		void               Pause() const;
		void               Resume() const;
		[[nodiscard]] bool IsPlaying() const;

	private:
		bool EnsureVoiceReady(bool preservePlayback);
		void InvalidateVoice();
		/// @brief 個別音量とカテゴリ音量を現在のボイスに反映します。
		void ApplyVoiceVolume() const noexcept;

		std::string mSoundPath;
		AssetID     mSoundAssetId       = kInvalidAssetID;
		uint64_t    mLoadedAssetVersion = 0;

		bool  mPlayOnStart      = true;
		bool  mLoop             = false;
		float mVolume           = 1.0f;
		float mPitch            = 1.0f;
		AudioBus mBus           = AudioBus::Sfx;
		bool  mAutoPlayConsumed = false;
		bool  mLoggedError      = false;

		std::shared_ptr<AudioVoice> mVoice;
	};
}
