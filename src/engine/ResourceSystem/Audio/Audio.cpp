#include "engine/ResourceSystem/Audio/Audio.h"

#include <fstream>

#include "engine/OldConsole/Console.h"

/// @brief コンストラクタ
Audio::Audio() {
	mAudioBuffer = {};
}

/// @brief デストラクタ
Audio::~Audio() {
	if (mSourceVoice) {
		mSourceVoice->Stop();
		mSourceVoice->DestroyVoice();
		mSourceVoice = nullptr;
	}
}

/// @brief オーディオファイルの読み込み
/// @param xAudio2 XAudio2インスタンスへのポインタ
/// @param filename 読み込むWAVファイルのパス
/// @return 成功したらtrue、失敗したらfalse
bool Audio::LoadFromFile(IXAudio2* xAudio2, const char* filename) {
	SoundData soundData;
	if (!LoadWavFile(filename, soundData)) {
		return false;
	}

	// ソースボイスの作成前にエラーチェック
	if (!xAudio2) {
		Console::Print("[Audio] XAudio2インスタンスが無効です\n", kConTextColorError,
		               Channel::ResourceSystem);
		return false;
	}

	// WAVEFORMATEX のチェック
	if (soundData.wfex.wFormatTag != WAVE_FORMAT_PCM) {
		Console::Print("[Audio] 未対応の音声フォーマットです\n", kConTextColorError,
		               Channel::ResourceSystem);
		return false;
	}

	// ソースボイスの生成（詳細なエラーハンドリング）
	HRESULT hr = xAudio2->CreateSourceVoice(&mSourceVoice, &soundData.wfex);
	if (FAILED(hr)) {
		switch (hr) {
		case XAUDIO2_E_INVALID_CALL:
			Console::Print("[Audio] 無効な関数呼び出しです\n", kConTextColorError,
			               Channel::ResourceSystem);
			break;
		case XAUDIO2_E_XMA_DECODER_ERROR:
			Console::Print("[Audio] XMAデコーダーエラーです\n", kConTextColorError,
			               Channel::ResourceSystem);
			break;
		default:
			Console::Print(std::format("[Audio] ソースボイスの作成に失敗しました: {:#x}\n", hr),
			               kConTextColorError, Channel::ResourceSystem);
		}
		return false;
	}

	// バッファの設定
	mAudioBuffer.pAudioData = soundData.pBuffer;
	mAudioBuffer.AudioBytes = soundData.bufferSize;
	mAudioBuffer.Flags      = XAUDIO2_END_OF_STREAM;

	// データを保持
	mAudioData = soundData;

	return true;
}

/// @brief オーディオの再生
/// @param isLoop ループ再生するかどうか
void Audio::Play(const bool isLoop) {
	if (!mSourceVoice) {
		return;
	}

	Stop();
	mAudioBuffer.LoopCount = isLoop ? XAUDIO2_LOOP_INFINITE : 0;
	mSourceVoice->SubmitSourceBuffer(&mAudioBuffer);
	mSourceVoice->Start();
	mIsPlaying = true;
}

/// @brief オーディオの停止
void Audio::Stop() {
	if (!mSourceVoice) {
		return;
	}
	mSourceVoice->Stop();
	mSourceVoice->FlushSourceBuffers();
	mIsPlaying = false;
}

/// @brief オーディオの一時停止
void Audio::Pause() {
	if (!mSourceVoice || !mIsPlaying) {
		return;
	}
	mSourceVoice->Stop();
	mIsPlaying = false;
}

/// @brief オーディオの再開
void Audio::Resume() {
	if (!mSourceVoice || mIsPlaying) {
		return;
	}
	mSourceVoice->Start();
	mIsPlaying = true;
}

/// @brief ボリュームの設定
/// @param volume ボリューム (0.0f から 1.0f)
void Audio::SetVolume(float volume) const {
	if (!mSourceVoice) {
		return;
	}
	// 0.0f から 1.0f の範囲にクランプ
	volume = std::clamp(volume, 0.0f, 1.0f);
	mSourceVoice->SetVolume(volume);
}

/// @brief ピッチの設定
/// @param pitch ピッチ (1.0f が標準、2.0f が2倍速、0.5f が半分の速さ)
void Audio::SetPitch(float pitch) const {
	if (!mSourceVoice) {
		return;
	}
	mSourceVoice->SetFrequencyRatio(pitch);
}

/// @brief WAVファイルの読み込み
/// @param filename 読み込むWAVファイルのパス
/// @param outData 読み込んだ音声データの出力先
/// @return 成功したらtrue、失敗したらfalse
bool Audio::LoadWavFile(const std::string& filename, SoundData& outData) {
	// ファイルを開く
	std::ifstream file;
	file.open(filename, std::ios_base::binary);
	if (!file.is_open()) {
		Console::Print(std::format("[Audio] ファイルのオープンに失敗しました: {}\n", filename),
		               kConTextColorError, Channel::ResourceSystem);
		return false;
	}

	// RIFFヘッダーの読み込みと検証
	RiffHeader riff;
	file.read(reinterpret_cast<char*>(&riff), sizeof(riff));
	if (strncmp(riff.chunk.id, "RIFF", 4) != 0 || strncmp(riff.type, "WAVE", 4)
		!= 0) {
		Console::Print("[Audio] 無効なWAVEファイル形式です\n",
		               kConTextColorError, Channel::ResourceSystem);
		return false;
	}

	FormatChunk format = {};
	ChunkHeader chunkHeader;
	bool        foundFmt  = false;
	bool        foundData = false;

	// チャンクの解析
	while (file.read(reinterpret_cast<char*>(&chunkHeader),
	                 sizeof(ChunkHeader))) {
		if (strncmp(chunkHeader.id, "fmt ", 4) == 0) {
			format.chunk = chunkHeader;
			if (format.chunk.size <= sizeof(WAVEFORMATEX)) {
				file.read(reinterpret_cast<char*>(&format.fmt),
				          format.chunk.size);
			} else if (format.chunk.size <= sizeof(WAVEFORMATEXTENSIBLE)) {
				WAVEFORMATEXTENSIBLE wfext;
				file.read(reinterpret_cast<char*>(&wfext), format.chunk.size);
				format.fmt = wfext.Format;
			} else {
				Console::Print("[Audio] 未対応のフォーマットチャンクサイズです\n",
				               kConTextColorError, Channel::ResourceSystem);
				return false;
			}
			foundFmt = true;
		} else if (strncmp(chunkHeader.id, "data", 4) == 0) {
			foundData = true;
			break;
		} else {
			file.seekg(chunkHeader.size, std::ios_base::cur);
		}
	}

	if (!foundFmt || !foundData) {
		Console::Print("[Audio] 必要なチャンクが見つかりません\n",
		               kConTextColorError, Channel::ResourceSystem);
		return false;
	}

	// 音声データの読み込み
	auto pBuffer = new char[chunkHeader.size];
	file.read(pBuffer, chunkHeader.size);

	// フォーマット情報のログ出力
	Console::Print(std::format(
		               "[Audio] 読み込み完了:\n  フォーマット: {}\n  チャンネル数: {}\n  サンプリングレート: {} Hz\n  ビット深度: {}\n",
		               format.fmt.wFormatTag,
		               format.fmt.nChannels,
		               format.fmt.nSamplesPerSec,
		               format.fmt.wBitsPerSample),
	               kConTextColorCompleted, Channel::ResourceSystem);

	file.close();

	// 出力データの設定
	outData.wfex       = format.fmt;
	outData.pBuffer    = reinterpret_cast<BYTE*>(pBuffer);
	outData.bufferSize = chunkHeader.size;

	return true;
}
