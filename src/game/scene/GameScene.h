#pragma once
#include <memory>

#include "engine/scene/BaseScene.h"

class Sprite;
class ExplosionEffect;
class WindEffect;
class ParticleObject;
class ParticleEmitter;

namespace UPhysics {
	class Engine;
}

class CubeMap;
class Audio;
class GameTime;
class D3D12;
class EnemyMovement;
class CameraRotator;
class CameraSystem;
class CameraComponent;
class IConVar;

/**
 * @brief メインゲームシーンクラス
 * @details プレイヤー、敵、武器などのゲームプレイ要素を管理します
 */
class GameScene : public BaseScene {
public:
	~GameScene() override;

	void Init() override;
	void Update(float deltaTime) override;
	void Render() override;
	void Shutdown() override;

private:
	void RegisterConVars();
	void LoadCoreTextures() const;
	void InitializeCubeMap();
	void InitializeParticles();
	void InitializePhysics();
	void HandleInput();
	void HandleWeaponFire(const std::shared_ptr<CameraComponent>& camera);
	void UpdateParticlesAndEffects(float deltaTime);
	void UpdateEntities(float deltaTime);

#ifdef _DEBUG
	void DrawDebugHud() const;
#endif

	D3D12*    mRenderer = nullptr;
	GameTime* mTimer    = nullptr;

	std::unique_ptr<CubeMap> mCubeMap;

	std::unique_ptr<UPhysics::Engine> mUPhysicsEngine;

	std::unique_ptr<ParticleEmitter> mParticleEmitter;

	std::unique_ptr<ParticleObject> mParticleObject;

	std::unique_ptr<WindEffect>      mWindEffect;
	std::unique_ptr<ExplosionEffect> mExplosionEffect;

	std::unique_ptr<Sprite> mNextCheckpointSprite;
	std::unique_ptr<Sprite> mNextCheckpointArrowSprite;

	std::shared_ptr<Audio> mWind;

	// 遅延読み込み用フラグ
	bool mPendingMeshReload = false;
	bool mMeshReloadArmed   = false;
};
