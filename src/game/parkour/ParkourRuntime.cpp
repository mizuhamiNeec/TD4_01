#include "ParkourRuntime.h"

#include "components/ControllerComponents.h"
#include "components/ParticleComponents.h"

#include "core/assets/AssetManager.h"
#include "core/assets/types/MeshAssetData.h"

#include "engine/render/frame/RenderFrameInputs.h"
#include "engine/scene/UScene.h"
#include "engine/unnamed/framework/components/GameplayCameraComponent.h"
#include "engine/unnamed/framework/components/StaticMeshRendererComponent.h"
#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/entity/UEntity.h"
#include "engine/unnamed/subsystem/console/concommand/UnnamedConCommand.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"
#include "engine/world/UGameWorld.h"

namespace Unnamed {
	namespace {
		ParkourRuntime* gActiveRuntime = nullptr;

		UEntity* FindEntityByName(UScene& scene, const std::string_view name) {
			for (const auto& entity : scene.GetEntities()) {
				if (entity && entity->GetName() == name) {
					return entity.get();
				}
			}
			return nullptr;
		}

		ParkourRuntime* GetActiveRuntime() { return gActiveRuntime; }
	}

	ParkourRuntime::ParkourRuntime(UGameWorld& world) : mWorld(world) {}

	void ParkourRuntime::Initialize() {
		mPhysics.Init();
		mServices.physics = &mPhysics;
		gActiveRuntime = this;

		static UnnamedConCommand parkourDebugExplosion(
			"parkour_debug_explosion",
			[](std::vector<std::string>) {
				ParkourRuntime* runtime = GetActiveRuntime();
				return runtime ? runtime->TriggerDebugExplosion() : false;
			},
			"Spawn a debug burst using the active parkour explosion emitter."
		);
		(void)parkourDebugExplosion;
	}

	void ParkourRuntime::OnSceneLoaded(UScene& scene) {
		mServices.physics = &mPhysics;
		ServiceLocator::Register<ParkourServices>(&mServices);
		CacheScene(scene);
		RegisterStaticMeshes(scene);
	}

	void ParkourRuntime::OnSceneUnloaded() {
		mGameplayCamera = nullptr;
		mPhysics.ClearStaticMeshes();
		mServices.physics = nullptr;
		ServiceLocator::Register<ParkourServices>(nullptr);
	}

	void ParkourRuntime::PreWorldTick(const float) {}

	void ParkourRuntime::PostWorldTick(const float) {}

	void ParkourRuntime::PrepareRender(const Render::SceneRenderRequest& request) {
		if (!mGameplayCamera) { return; }
		const float width = request.viewportPanelWidth != 0 ?
			                    static_cast<float>(request.viewportPanelWidth) :
			                    1280.0f;
		const float height = request.viewportPanelHeight != 0 ?
			                     static_cast<float>(request.viewportPanelHeight) :
			                     720.0f;
		mGameplayCamera->SetAspectRatio(width / std::max(1.0f, height));
	}

	void ParkourRuntime::BuildRenderContributions(
		Render::RenderFrameInputs& inputs,
		AssetManager&              assetManager
	) {
		UScene& scene = mWorld.GetScene();
		for (const auto& entity : scene.GetEntities()) {
			if (!entity || !entity->IsActive()) { continue; }

			if (auto* title = entity->GetComponent<TitleFlowComponent>()) {
				title->AppendOverlaySprites(inputs, assetManager);
			}
			if (auto* course = entity->GetComponent<CourseComponent>()) {
				course->AppendOverlaySprites(inputs, assetManager);
			}
			if (auto* emitter = entity->GetComponent<ParticleEmitterComponent>()) {
				emitter->AppendWorldBillboards(
					inputs.worldBillboards,
					assetManager
				);
			}
		}
	}

	bool ParkourRuntime::TriggerDebugExplosion() {
		if (!mServices.physics) { return false; }
		UScene& scene = mWorld.GetScene();
		UEntity* cameraEntity = FindEntityByName(scene, "Camera");
		UEntity* emitterEntity = FindEntityByName(scene, "DebugExplosionEmitter");
		if (!cameraEntity || !emitterEntity) { return false; }

		auto* cameraTransform = cameraEntity->GetComponent<TransformComponent>();
		auto* emitter = emitterEntity->GetComponent<ParticleEmitterComponent>();
		if (!cameraTransform || !emitter) { return false; }

		Mat4 cameraWorld = cameraTransform->WorldMat();
		const Vec3 origin = cameraWorld.GetTranslate();
		const Vec3 direction = cameraWorld.GetForward().Normalized();
		const Vec3 invDirection(
			direction.x != 0.0f ? 1.0f / direction.x : 0.0f,
			direction.y != 0.0f ? 1.0f / direction.y : 0.0f,
			direction.z != 0.0f ? 1.0f / direction.z : 0.0f
		);

		Unnamed::Ray ray = {};
		ray.origin = origin;
		ray.dir = direction;
		ray.invDir = invDirection;
		ray.tMin = 0.0f;
		ray.tMax = 100.0f;

		UPhysics::Hit hit = {};
		Vec3 burstPosition = origin + direction * 4.0f;
		if (mServices.physics->RayCast(ray, &hit)) {
			burstPosition = hit.pos + hit.normal * 0.1f;
		}
		emitter->EmitBurstAt(burstPosition);
		return true;
	}

	void ParkourRuntime::CacheScene(UScene& scene) {
		mGameplayCamera = nullptr;
		for (const auto& entity : scene.GetEntities()) {
			if (!entity || !entity->IsActive()) { continue; }
			if (auto* camera = entity->GetComponent<GameplayCameraComponent>()) {
				mGameplayCamera = camera;
				break;
			}
		}
	}

	void ParkourRuntime::RegisterStaticMeshes(UScene& scene) {
		mPhysics.ClearStaticMeshes();

		auto* assetManager = ServiceLocator::Get<AssetManager>();
		if (!assetManager) { return; }

		for (const auto& entity : scene.GetEntities()) {
			if (!entity || !entity->IsActive()) { continue; }
			if (auto* transform = entity->GetComponent<TransformComponent>()) {
				transform->OnTick(0.0f);
			}
		}

		for (const auto& entity : scene.GetEntities()) {
			if (!entity || !entity->IsActive()) { continue; }

			const auto* collider = entity->GetComponent<StaticMeshColliderComponent>();
			const auto* transform = entity->GetComponent<TransformComponent>();
			auto* renderer = entity->GetComponent<StaticMeshRendererComponent>();
			if (!collider || !collider->IsCollisionEnabled() || !transform || !renderer) {
				continue;
			}

			const AssetID meshAssetId = renderer->ResolveMeshAsset(*assetManager);
			const auto* mesh = assetManager->Get<MeshAssetData>(meshAssetId);
			if (!mesh) { continue; }

			mPhysics.RegisterStaticMesh(
				entity->GetGuid(),
				mesh->vertices,
				mesh->indices,
				transform->WorldMat()
			);
		}
	}
}
