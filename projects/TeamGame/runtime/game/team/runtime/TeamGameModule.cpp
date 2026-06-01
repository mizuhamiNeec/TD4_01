#include "TeamGameModule.h"
#include "TeamGameComponentRegistration.h"
#include "MagVoiceBridge.h"
#include "VoiceShockWaveComponent.h"

#include "engine/game/IDemoService.h"
#include "engine/physics/core/Physics.h"
#include "engine/scene/Scene.h"
#include "engine/world/World.h"

namespace Unnamed {
	TeamGameModule::~TeamGameModule() {
		// 静的参照が破棄済みインスタンスを指さないよう、所有側の破棄前に切り離す。
		SetGlobalMagVoiceBridge(nullptr);
		voiceBridge_.reset();
	}

	void TeamGameModule::Initialize(EngineServices& services) {
		(void)services;

		// NOTE: ゲーム開始時に MagVoiceBridge を初期化・起動
		// これにより、すべてのシーンでマイクからの音声入力が有効になる
		InitializeMagVoiceBridge();
	}

	void TeamGameModule::InitializeMagVoiceBridge() {
		if (voiceBridge_) {
			return;
		}

		// NOTE: VoiceShockWaveComponent の静的メンバを初期化
		// VoiceShockWaveComponent が使用する _voiceBridgeInstance を作成
		auto voiceBridge = std::make_unique<MagVoiceBridge>();
		
		if (voiceBridge) {
			bool initSuccess = voiceBridge->Initialize();
			if (initSuccess) {
				// NOTE: 音声感度を大幅に調整（敏感に反応するように）
				voiceBridge->SetSmoothingFactor(0.2f);  // より反応的（0.4 → 0.2）
				voiceBridge->SetNoiseFloor(-80.0f);     // ノイズフロアを下げる（-50dB → -80dB）
				voiceBridge->SetVolumeRange(-80.0f, 0.0f);  // 音量範囲を拡大

				bool startSuccess = voiceBridge->Start();
				if (startSuccess) {
					#ifdef _DEBUG
					OutputDebugStringA("[TeamGameModule] ✓ MagVoiceBridge initialized and started successfully\n");
					OutputDebugStringA("[TeamGameModule] Audio sensitivity: HIGH (SmoothingFactor=0.2, NoiseFloor=-80dB)\n");
					OutputDebugStringA("[TeamGameModule] Audio capture is now active\n");
					#endif
					// NOTE: VoiceShockWaveComponent に MagVoiceBridge を設定
					SetGlobalMagVoiceBridge(voiceBridge.get());
					voiceBridge_ = std::move(voiceBridge);
				} else {
					#ifdef _DEBUG
					OutputDebugStringA("[TeamGameModule] ✗ ERROR: Failed to start MagVoiceBridge\n");
					#endif
				}
			} else {
				#ifdef _DEBUG
				OutputDebugStringA("[TeamGameModule] ✗ ERROR: Failed to initialize MagVoiceBridge\n");
				#endif
			}
		}
	}

	void TeamGameModule::SetGlobalMagVoiceBridge(MagVoiceBridge* bridge) {
		// NOTE: VoiceShockWaveComponent の静的メンバを設定
		MyGame::VoiceShockWaveComponent::SetVoiceBridgeInstance(bridge);
		
		#ifdef _DEBUG
		OutputDebugStringA("[TeamGameModule] MagVoiceBridge set to VoiceShockWaveComponent\n");
		#endif
	}

	std::unique_ptr<World> TeamGameModule::CreateRuntimeWorld(
		const WorldServices& services
	) {
		auto world = std::make_unique<World>();
		world->SetServices(services);
		return world;
	}

	std::unique_ptr<World> TeamGameModule::CreatePlayWorld(
		const WorldServices& services
	) {
		auto world = std::make_unique<World>();
		world->SetServices(services);
		return world;
	}

	std::unique_ptr<IDemoService> TeamGameModule::CreateDemoService() {
		return nullptr;
	}

	void TeamGameModule::RegisterGameComponents(
		ComponentRegistry& componentRegistry
	) {
		RegisterTeamGameComponents(componentRegistry);
	}

	GameModulePaths TeamGameModule::GetGameModulePaths() const {
		return {
			.gameName            = "TeamGame",
			.gameRoot            = "./projects/TeamGame",
			.contentRoot         = "./projects/TeamGame/content",
			.configRoot          = "./projects/TeamGame/config",
			.defaultStartupScene = "scenes/title.json",
		};
	}

	std::string TeamGameModule::GetDefaultStartupScenePath() const {
		return GetGameModulePaths().defaultStartupScene;
	}

	std::string TeamGameModule::GetDefaultUiDocumentPath() const {
		return {};
	}

	std::unique_ptr<IGameModule> CreateTeamGameModule() {
		return std::make_unique<TeamGameModule>();
	}
}
