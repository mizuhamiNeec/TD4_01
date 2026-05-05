#include "MicComponent.h"
#include "./core/ComponentRegistry.h"

#ifdef _DEBUG
#include "imgui.h"
#endif

namespace MyGame {

	// -----------------------------------------------------------------------
	// ライフサイクル
	// -----------------------------------------------------------------------

	void MicComponent::OnAttached() {
		// NOTE: コンポーネントがアタッチされたときに呼び出される
		// ここでは MagVoiceBridge のインスタンスを作成するだけに留める
		voiceBridge_ = std::make_unique<MagVoiceBridge>();
		InitializeMic();
	}

	// -----------------------------------------------------------------------
	// マイク制御
	// -----------------------------------------------------------------------

	bool MicComponent::InitializeMic() {
		if (bIsInitialized_) {
			return true; // 既に初期化済み
		}

		if (!voiceBridge_ || !voiceBridge_->Initialize()) {
			return false;
		}

		bIsInitialized_ = true;
		return true;
	}

	bool MicComponent::StartMic() {
		if (!bIsInitialized_) {
			if (!InitializeMic()) {
				return false;
			}
		}

		if (!voiceBridge_ || !voiceBridge_->Start()) {
			return false;
		}

		bIsCapturing_ = true;
		return true;
	}

	void MicComponent::StopMic() {
		if (bIsCapturing_ && voiceBridge_) {
			voiceBridge_->Stop();
			bIsCapturing_ = false;
		}
	}

	void MicComponent::ShutdownMic() {
		StopMic();

		if (bIsInitialized_ && voiceBridge_) {
			voiceBridge_->Shutdown();
			bIsInitialized_ = false;
		}
	}

	// -----------------------------------------------------------------------
	// 音声データ取得インターフェース
	// -----------------------------------------------------------------------

	float MicComponent::GetSmoothedVolume() const {
		return voiceBridge_ ? voiceBridge_->GetSmoothedVolume() : 0.0f;
	}

	float MicComponent::GetCurrentVolume() const {
		return voiceBridge_ ? voiceBridge_->GetCurrentVolume() : 0.0f;
	}

	float MicComponent::GetCurrentVolumeDB() const {
		return voiceBridge_ ? voiceBridge_->GetCurrentVolumeDB() : -80.0f;
	}

	float MicComponent::GetPeakVolume() const {
		return voiceBridge_ ? voiceBridge_->GetPeakVolume() : 0.0f;
	}

	float MicComponent::GetPeakVolumeDB() const {
		return voiceBridge_ ? voiceBridge_->GetPeakVolumeDB() : -80.0f;
	}

	float MicComponent::GetSmoothedVolumeDB() const {
		return voiceBridge_ ? voiceBridge_->GetSmoothedVolumeDB() : -80.0f;
	}

	float MicComponent::GetVolumePercentage() const {
		return voiceBridge_ ? voiceBridge_->GetVolumePercentage() : 0.0f;
	}

	bool MicComponent::IsVoiceDetected() const {
		return voiceBridge_ ? voiceBridge_->IsVoiceDetected() : false;
	}

	float MicComponent::CalculateZeroCrossingRate() const {
		return voiceBridge_ ? voiceBridge_->CalculateZeroCrossingRate() : 0.0f;
	}

	float MicComponent::GetVoiceCharacteristicScore() const {
		return voiceBridge_ ? voiceBridge_->GetVoiceCharacteristicScore() : 0.0f;
	}

	MagVoiceBridge::VolumeStats MicComponent::GetVolumeStats() const {
		if (voiceBridge_) {
			return voiceBridge_->GetVolumeStats();
		}
		return MagVoiceBridge::VolumeStats{};
	}

	std::vector<float> MicComponent::GetSamples() const {
		if (voiceBridge_) {
			return voiceBridge_->GetSamples();
		}
		return std::vector<float>{};
	}

	// -----------------------------------------------------------------------
	// パラメータ調整
	// -----------------------------------------------------------------------

	void MicComponent::SetSmoothingFactor(float factor) {
		if (voiceBridge_) {
			voiceBridge_->SetSmoothingFactor(factor);
		}
	}

	void MicComponent::SetVolumeRange(float minDb, float maxDb) {
		if (voiceBridge_) {
			voiceBridge_->SetVolumeRange(minDb, maxDb);
		}
	}

	void MicComponent::SetNoiseFloor(float noiseFloorDB) {
		if (voiceBridge_) {
			voiceBridge_->SetNoiseFloor(noiseFloorDB);
		}
	}

	void MicComponent::SetVoiceDetectionThresholds(float zcRateThreshold, float volumeThresholdDB) {
		if (voiceBridge_) {
			voiceBridge_->SetVoiceDetectionThresholds(zcRateThreshold, volumeThresholdDB);
		}
	}

	// -----------------------------------------------------------------------
	// BaseComponent override
	// -----------------------------------------------------------------------

	std::string_view MicComponent::GetStableName() const {
		return "mygame.MicComponent";
	}

	std::string_view MicComponent::GetComponentName() const {
		return "MicComponent";
	}

	void MicComponent::OnTick(float ) {
		// NOTE: BaseComponent から毎フレーム呼び出される
		if (bIsCapturing_ && voiceBridge_) {
			voiceBridge_->Update();
		}
	}

#ifdef _DEBUG
	void MicComponent::DrawInspectorImGui() {
		ImGui::Text("=== Mic Component ===");

		// 状態表示
		ImGui::Checkbox("Initialized", &bIsInitialized_);
		ImGui::Checkbox("Capturing", &bIsCapturing_);

		ImGui::Separator();

		// マイク制御ボタン
		if (!bIsCapturing_) {
			if (ImGui::Button("Start Mic", ImVec2(100, 0))) {
				StartMic();
			}
		} else {
			if (ImGui::Button("Stop Mic", ImVec2(100, 0))) {
				StopMic();
			}
		}

		ImGui::Separator();

		// 音量統計情報表示
		if (voiceBridge_) {
			auto stats = voiceBridge_->GetVolumeStats();

			// 基本情報
			ImGui::Text("Sample Rate: %u Hz", voiceBridge_->GetSampleRate());
			ImGui::Text("Channels: %u", voiceBridge_->GetChannelCount());

			ImGui::Separator();

			// 音量情報
			ImGui::Text("Current RMS: %.3f (%.1f dB)", stats.currentRMS, stats.currentRMSDB);
			ImGui::Text("Smoothed: %.3f (%.1f dB)", stats.smoothedRMS, stats.smoothedRMSDB);
			ImGui::Text("Peak: %.3f (%.1f dB)", stats.peakValue, stats.peakDB);
			ImGui::Text("Volume: %.1f%%", stats.percentage);

			ImGui::Separator();

			// 音声検出情報
			ImGui::Text("Voice Detected: %s", stats.isVoiceDetected ? "YES" : "NO");
			ImGui::Text("Voice Score: %.3f", stats.voiceScore);
			ImGui::Text("ZCR: %.3f", CalculateZeroCrossingRate());

			// プログレスバー表示
			ImGui::ProgressBar(stats.smoothedRMS, ImVec2(-1.0f, 0.0f));
		}
	}
	void MicComponent::Deserialize(const Unnamed::JsonReader &) {
	}
	void MicComponent::Serialize(Unnamed::JsonWriter &) const {
	}
#endif

	// 忘れると死ぬやつ
	REGISTER_COMPONENT(MicComponent);

} // namespace MyGame
