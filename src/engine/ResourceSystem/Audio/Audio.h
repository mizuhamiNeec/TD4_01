#pragma once
#include <string>
#include <vector>
#include <xaudio2.h>

/// @brief オーディオクラス
class Audio {
public:
	Audio();
	~Audio();

	bool LoadFromFile(IXAudio2* xAudio2, const char* filename);
	void Play(bool isLoop = false);
	void Stop();
	void Pause();
	void Resume();
	void SetVolume(float volume) const;
	void SetPitch(float pitch) const;

	void InvalidateVoice() noexcept;

private:
	// チャンクヘッダ
	struct ChunkHeader {
		char    id[4]; // チャンク毎のID
		int32_t size;  // チャンクサイズ
	};

	// RIFFヘッダチャンク
	struct RiffHeader {
		ChunkHeader chunk;   // "RIFF"
		char        type[4]; // "WAVE"
	};

	// FMTチャンク
	struct FormatChunk {
		ChunkHeader  chunk; // "fmt"
		WAVEFORMATEX fmt;   // 波形フォーマット
	};

	struct SoundData {
		WAVEFORMATEX wfex       = {};      // 波形フォーマット
		BYTE*        pBuffer    = nullptr; // バッファの先頭サイズ
		unsigned int bufferSize = 0;       // バッファのサイズ
	};

	static bool LoadWavFile(const std::string& filename, SoundData& outData);

	IXAudio2SourceVoice* mSourceVoice = nullptr;
	XAUDIO2_BUFFER       mAudioBuffer = {};
	SoundData            mAudioData;
	bool                 mIsPlaying = false;
	bool                 mVoiceValid = true;
};
