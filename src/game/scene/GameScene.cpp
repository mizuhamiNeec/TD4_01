#include "GameScene.h"

#include <array>

#include <engine/Engine.h>
#include <engine/EngineServices.h>
#include <engine/Camera/CameraManager.h>
#include <engine/Components/Camera/CameraComponent.h>
#include <engine/Components/ColliderComponent/MeshColliderComponent.h>
#include <engine/ImGui/ImGuiUtil.h>
#include <engine/Input/InputSystem.h>
#include <engine/OldConsole/ConVarManager.h>
#include <engine/ResourceSystem/Audio/AudioManager.h>
#include <engine/TextureManager/TexManager.h>
#include <engine/unnamed/subsystem/interface/ServiceLocator.h>

#include <game/components/RotateComponent.h>
#include <game/components/checkpoint/CheckpointManager.h>

#include "engine/CubeMap/CubeMap.h"
#include "engine/particle/ExplosionEffect.h"
#include "engine/particle/ParticleEmitter.h"
#include "engine/particle/ParticleManager.h"
#include "engine/particle/ParticleObject.h"
#include "engine/renderer/D3D12.h"
#include "engine/ResourceSystem/Audio/Audio.h"
#include "engine/Sprite/Sprite.h"
#include "engine/unnamed/subsystem/time/GameTime.h"
#include "engine/unnamed/subsystem/time/TimeSystem.h"

#include "game/effects/WindEffect.h"

namespace {
	constexpr char kScenePath[] =
		"./content/parkour/scenes/game_scene.json";
	constexpr char kPingTexturePath[] =
		"./content/parkour/textures/ping.png";
	constexpr char kArrowTexturePath[] =
		"./content/parkour/textures/arrow.png";
	constexpr char kWindParticleTexturePath[] =
		"./content/core/textures/circle.png";
	constexpr char kWorldMeshReloadPath[] =
		"./content/parkour/models/map/sp_city.obj";
	constexpr char kMeshReloadBindCommand[] = "bind f5 +f5";
	constexpr char kWaveTexturePath[]       =
		"./content/core/textures/wave.dds";
}

/// @brief コンストラクタ
GameScene::~GameScene() {
	mResourceManager = nullptr;
	mSpriteCommon    = nullptr;
	mParticleManager = nullptr;
	mObject3DCommon  = nullptr;
	mModelCommon     = nullptr;
	mUPhysicsEngine  = nullptr;
	mTimer           = nullptr;
}

/// @brief 初期化
void GameScene::Init() {
	// 各種マネージャーの取得
	const auto* engine = Unnamed::EngineServices::Get();
	UASSERT(engine && "Engine instance not registered");

	mAudioManager = engine ? engine->GetAudioManagerInstance() : nullptr;
	mRenderer = engine ? engine->GetRendererInstance() : nullptr;
	mResourceManager = engine ? engine->GetResourceManagerInstance() : nullptr;
	mSrvManager = engine ? engine->GetSrvManagerInstance() : nullptr;
	mTimer = ServiceLocator::Get<Unnamed::TimeSystem>()->GetGameTime();
	mSpriteCommon = engine ? engine->GetSpriteCommonInstance() : nullptr;

	// CheckpointManagerを初期化
	CheckpointManager::Initialize();
	// 各種初期化処理
	LoadCoreTextures();
	InitializeCubeMap();
	InitializeParticles();
	InitializePhysics();

	// 次のチェックポイント矢印スプライトの初期化
	mNextCheckpointSprite = std::make_unique<Sprite>();
	mNextCheckpointSprite->Init(
		mSpriteCommon,
		kPingTexturePath
	);
	mNextCheckpointSprite->SetAnchorPoint({0.5f, 0.5f});

	mNextCheckpointArrowSprite = std::make_unique<Sprite>();
	mNextCheckpointArrowSprite->Init(
		mSpriteCommon,
		kArrowTexturePath
	);
	mNextCheckpointArrowSprite->SetAnchorPoint({0.5f, 0.5f});

	const auto run = mAudioManager->GetAudio(
		"./content/parkour/sounds/bgm/Run.wav"
	);

	run->Play(true);
	run->SetVolume(0.125f);

	mWind = mAudioManager->GetAudio(
		"./content/parkour/sounds/amb/wind.wav"
	);
	mWind->Play(true);
	mWind->SetVolume(1.0f);

	// ゲームタイマー開始
	mTimer->StartGame();
}

/// @brief 更新
/// @param deltaTime 経過時間
void GameScene::Update(const float deltaTime) {
	UpdateEntities(deltaTime);

	mNextCheckpointSprite->Update();
	mNextCheckpointArrowSprite->Update();
}

/// @brief 描画
void GameScene::Render() {
	if (!mRenderer) { return; }

	auto* commandList = mRenderer->GetCommandList();

	for (const auto* entity : mEntities) {
		if (entity) { entity->Render(commandList); }
	}

	if (const auto* engine = Unnamed::EngineServices::Get()) {
		if (auto* particleManager = engine->GetParticleManagerInstance()) {
			particleManager->Render();
		}
	}

	if (mParticleObject) { mParticleObject->Draw(); }

	if (mWindEffect) { mWindEffect->Draw(); }

	if (mExplosionEffect) { mExplosionEffect->Draw(); }

	// if (mNextCheckpointArrowSprite) { mNextCheckpointArrowSprite->Draw(TODO); }
}

/// @brief コンソール変数の登録
void GameScene::RegisterConVars() {
	ConVarManager::RegisterConVar("sv_ducktime", 1000.0f, "ms");
	ConVarManager::RegisterConVar(
		"sv_jumptime", 510.0f,
		"ms approx - based on the 21 unit height jump"
	);
	ConVarManager::RegisterConVar("sv_jumpheight", 21.0f, "units");
	ConVarManager::RegisterConVar("sv_timetounduck", 0.2f * 1000.0f, "ms");
	ConVarManager::RegisterConVar(
		"sv_timetounduckinv",
		1000.0f - 0.2f * 1000.0f, "ms"
	);
}

/// @brief コアテクスチャの読み込み
void GameScene::LoadCoreTextures() const {
	const auto* engine     = Unnamed::EngineServices::Get();
	auto*       texManager = engine ? engine->GetTexManagerInstance() : nullptr;
	if (!texManager) { return; }

	struct TextureRequest {
		const char* path    = nullptr;
		bool        useSrgb = false;
	};

	constexpr std::array<TextureRequest, 3> requests{
		{

			{kWaveTexturePath, true},

			{kPingTexturePath, false},
			{kArrowTexturePath, false},
		}
	};

	for (const auto& request : requests) {
		if (!request.path) { continue; }

		if (request.useSrgb) {
			texManager->LoadTexture(request.path, true);
		} else { texManager->LoadTexture(request.path); }
	}
}


/// @brief キューブマップの初期化
void GameScene::InitializeCubeMap() {
	if (!mRenderer || !mSrvManager) { return; }
	const auto* engine     = Unnamed::EngineServices::Get();
	auto*       texManager = engine ? engine->GetTexManagerInstance() : nullptr;
	if (!texManager) { return; }

	mCubeMap = std::make_unique<CubeMap>(
		mRenderer->GetDevice(),
		mSrvManager,
		texManager,
		kWaveTexturePath
	);
}

/// @brief パーティクルの初期化
void GameScene::InitializeParticles() {
	const auto* engine          = Unnamed::EngineServices::Get();
	auto*       particleManager = engine ?
		                              engine->GetParticleManagerInstance() :
		                              nullptr;
	if (!particleManager) { return; }

	particleManager->CreateParticleGroup("wind", kWindParticleTexturePath);

	mParticleEmitter = std::make_unique<ParticleEmitter>();
	mParticleEmitter->Init(particleManager, "wind");

	mParticleObject = std::make_unique<ParticleObject>();
	mParticleObject->Init(particleManager, kWindParticleTexturePath);
}

/// @brief 物理エンジンの初期化
void GameScene::InitializePhysics() {
	mUPhysicsEngine = std::make_unique<UPhysics::Engine>();
	mUPhysicsEngine->Init();
}

/// @brief 入力の処理
void GameScene::HandleInput() {
	if (InputSystem::IsPressed("+reload")) {
		// 最後のチェックポイントにリスポーン
		CheckpointManager::RespawnAtLastCheckpoint();
	}
}

/// @brief 武器発射の処理
void GameScene::HandleWeaponFire(
	const std::shared_ptr<CameraComponent>& camera
) {
	if (!camera) { return; }

	Mat4       inverseView = camera->GetViewMat().Inverse();
	const Vec3 origin      = inverseView.GetTranslate();
	const Vec3 direction   = inverseView.GetForward();

	Unnamed::Ray ray{};
	ray.origin = origin;
	ray.dir    = direction;
	ray.tMin   = 0.0f;
	ray.tMax   = 100.0f;

	UPhysics::Hit hit{};
	if (!mUPhysicsEngine->RayCast(ray, &hit)) {}
}

/// @brief パーティクルとエフェクトの更新
/// @param deltaTime 経過時間
void GameScene::UpdateParticlesAndEffects(const float deltaTime) {
	if (const auto* engine = Unnamed::EngineServices::Get()) {
		if (auto* particleManager = engine->GetParticleManagerInstance()) {
			particleManager->Update(deltaTime);
		}
	}

	if (mParticleEmitter) { mParticleEmitter->Update(deltaTime); }

	if (mWindEffect) { mWindEffect->Update(deltaTime); }

	if (mExplosionEffect) { mExplosionEffect->Update(deltaTime); }
}

/// @brief エンティティの更新
/// @param deltaTime 経過時間
void GameScene::UpdateEntities(const float deltaTime) {
	// 物理更新前の処理
	for (const auto* entity : mEntities) {
		if (entity && !entity->GetParent()) { entity->PrePhysics(deltaTime); }
	}

	// 物理更新
	for (auto* entity : mEntities) {
		if (entity && !entity->GetParent()) { entity->Update(deltaTime); }
	}

	// 物理エンジンの更新
	if (mUPhysicsEngine) { mUPhysicsEngine->Update(deltaTime); }

	// 物理更新後の処理
	for (const auto* entity : mEntities) {
		if (entity && !entity->GetParent()) { entity->PostPhysics(deltaTime); }
	}
}

#ifdef _DEBUG
/// @brief デバッグHUDの描画
void GameScene::DrawDebugHud() const {
	// レティクルの描画
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	if (!viewport) { return; }

	const ImVec2 windowCenter(
		viewport->Pos.x + viewport->Size.x * 0.5f,
		viewport->Pos.y + viewport->Size.y * 0.5f
	);

	ImDrawList*      drawList = ImGui::GetBackgroundDrawList();
	constexpr ImVec4 reticleColor(1.0f, 1.0f, 1.0f, 0.8f);
	constexpr ImVec4 outlineColor(0.0f, 0.0f, 0.0f, 0.5f);
	constexpr float  lineLength       = 10.0f;
	constexpr float  gapSize          = 3.0f;
	constexpr float  lineThickness    = 1.5f;
	constexpr float  outlineThickness = 0.5f;

	drawList->AddLine(
		{windowCenter.x - lineLength - gapSize, windowCenter.y},
		{windowCenter.x - gapSize, windowCenter.y},
		ImGui::ColorConvertFloat4ToU32(reticleColor),
		lineThickness
	);
	drawList->AddLine(
		{windowCenter.x + gapSize, windowCenter.y},
		{windowCenter.x + lineLength + gapSize, windowCenter.y},
		ImGui::ColorConvertFloat4ToU32(reticleColor),
		lineThickness
	);
	drawList->AddLine(
		{windowCenter.x, windowCenter.y - lineLength - gapSize},
		{windowCenter.x, windowCenter.y - gapSize},
		ImGui::ColorConvertFloat4ToU32(reticleColor),
		lineThickness
	);
	drawList->AddLine(
		{windowCenter.x, windowCenter.y + gapSize},
		{windowCenter.x, windowCenter.y + lineLength + gapSize},
		ImGui::ColorConvertFloat4ToU32(reticleColor),
		lineThickness
	);

	drawList->AddCircle(
		windowCenter,
		2.0f,
		ImGui::ColorConvertFloat4ToU32(outlineColor),
		0,
		outlineThickness + lineThickness
	);

	drawList->AddCircleFilled(
		windowCenter,
		1.0f,
		ImGui::ColorConvertFloat4ToU32(reticleColor)
	);
}
#endif

/// @brief シャットダウン
void GameScene::Shutdown() {
	// CheckpointManagerをシャットダウン
	CheckpointManager::Shutdown();
}
