#include "TeamGameComponentRegistration.h"

#include "collision/base/BaseKinematicCollisionResolver.h"
#include "core/ComponentRegistry.h"
#include "engine/unnamed/subsystem/console/Log.h"
#include "MicComponent.h"
#include "PlayerMoveComponent.h"
#include "PlayerControlComponent.h"
#include "GolfBallComponent.h"
#include "GolfBallLaunchCountdownComponent.h"
#include "GolfBallStartPosComponent.h"
#include "GolfBallEndPosComponent.h"
#include "FakeShadowComponent.h"
#include "TrashObjMoverComponent.h"
#include "PlayerHoleComponent.h"
#include "VoiceShockWaveComponent.h"
#include "VoiceUiComponent.h"
#include "GolfBallUiComponent.h"
#include "Player2BallRangeUIComponent.h"
#include "GameRuleSystemComponent.h"
#include "GameScoreComponent.h"

namespace Unnamed {
	namespace {
		template <typename T>
		void RegisterComponentIfMissing(
			ComponentRegistry &componentRegistry,
			const std::string_view stableName,
			const std::string_view displayName) {
			if (componentRegistry.IsRegistered(stableName)) {
				return;
			}

			const bool registered = componentRegistry.Register(
				stableName,
				[]() -> std::unique_ptr<BaseComponent> {
					return std::make_unique<T>();
				},
				displayName);
			if (!registered) {
				Warning(
					"TeamGameRuntime",
					"Failed to register game component '{}'.",
					stableName);
			}
		}
	}

	void RegisterTeamGameComponents(ComponentRegistry &componentRegistry) {
		RegisterComponentIfMissing<MyGame::MicComponent>(
			componentRegistry, "mygame.MicComponent", "Mic Component");
		RegisterComponentIfMissing<MyGame::PlayerMoveComponent>(
			componentRegistry, "mygame.PlayerMoveComponent", "Player Move Component");
		RegisterComponentIfMissing<MyGame::PlayerControlComponent>(
			componentRegistry, "mygame.PlayerControlComponent", "Player Control Component");
		RegisterComponentIfMissing<MyGame::GolfBallComponent>(
			componentRegistry, "mygame.GolfBall", "Golf Ball");
		RegisterComponentIfMissing<MyGame::GolfBallLaunchCountdownComponent>(
			componentRegistry, "mygame.GolfBallLaunchCountdownComponent",
			"Golf Ball Launch Countdown Component");
		RegisterComponentIfMissing<MyGame::GolfBallStartPosComponent>(
			componentRegistry, "mygame.GolfBallStartPos", "Golf Ball Start Position");
		RegisterComponentIfMissing<MyGame::GolfBallEndPosComponent>(
			componentRegistry, "mygame.GolfBallEndPos", "Golf Ball End Position");
		RegisterComponentIfMissing<MyGame::FakeShadowComponent>(
			componentRegistry, "mygame.FakeShadow", "Fake Shadow");
		RegisterComponentIfMissing<MyGame::TrashObjMoverComponent>(
			componentRegistry, "mygame.TrashObjMoverComponent", "Trash Obj");
		RegisterComponentIfMissing<MyGame::PlayerHoleComponent>(
			componentRegistry, "mygame.PlayerHoleComponent", "Player Hole Component");
		RegisterComponentIfMissing<MyGame::VoiceShockWaveComponent>(
			componentRegistry, "mygame.VoiceShockWave", "Voice Shock Wave");
		RegisterComponentIfMissing<MyGame::VoiceUiComponent>(
			componentRegistry, "mygame.VoiceUiComponent", "Voice Ui Component");
		RegisterComponentIfMissing<MyGame::GolfBallUiComponent>(
			componentRegistry, "mygame.GolfBallUiComponent", "Golf Ball Ui Component");
		RegisterComponentIfMissing<MyGame::Player2BallRangeUIComponent>(
			componentRegistry, "mygame.Player2BallRangeUIComponent",
			"Player To Ball Range UI Component");
		RegisterComponentIfMissing<MyGame::GameRuleSystemComponent>(
			componentRegistry, "mygame.GameRuleSystemComponent",
			"Game Rule System Component");
		RegisterComponentIfMissing<MyGame::GameScoreComponent>(
			componentRegistry, "mygame.GameScoreComponent",
			"Game Score Component");

	}
}
