#include "SequenceRegressionRunner.h"

#include <cmath>
#include <format>
#include <memory>
#include <string>
#include <vector>

#include "core/assets/AssetManager.h"
#include "core/assets/AssetType.h"
#include "core/assets/loader/SequenceFileIO.h"

#include "engine/scene/Scene.h"
#include "engine/sequence/SequencePlayer.h"
#include "engine/sequence/SequenceRuntime.h"
#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/world/GameplayCueBus.h"
#include "engine/world/World.h"

namespace Unnamed {
	namespace {
		static constexpr std::string_view kChannel = "SeqRegression";
		static constexpr uint64_t         kTestEntityGuid = 990000001ull;
		static constexpr float            kFloatEpsilon = 0.05f;

		struct RegressionCaseResult final {
			std::string name = {};
			bool        passed = false;
			std::string detail = {};
		};

		struct ScopedSequencePlayer final {
			SequenceRuntime&                runtime;
			std::shared_ptr<SequencePlayer> player;

			ScopedSequencePlayer(SequenceRuntime& inRuntime) :
				runtime(inRuntime),
				player(std::make_shared<SequencePlayer>()) {
				runtime.RegisterPlayer(player);
			}

			~ScopedSequencePlayer() {
				if (player) {
					runtime.UnregisterPlayer(player->GetPlayerId());
				}
			}
		};

		[[nodiscard]] AssetID LoadSequenceAsset(
			AssetManager&            assetManager,
			const std::string_view   path
		) {
			return assetManager.LoadFromFile(
				std::string(path),
				ASSET_TYPE::SEQUENCE,
				AssetManager::AssetLoadPolicy::ForceReload
			);
		}

		[[nodiscard]] bool EnsureRegressionEntity(
			World&   world,
			Entity*& outEntity,
			bool&    outCreated
		) {
			outEntity   = nullptr;
			outCreated  = false;
			Scene* scene = world.GetScenePtr();
			if (!scene) {
				return false;
			}

			Entity* entity = scene->FindEntity(kTestEntityGuid);
			if (!entity) {
				entity = &scene->CreateEntity(
					"SequenceRegressionEntity",
					kTestEntityGuid,
					false
				);
				outCreated = true;
			}
			if (!entity) {
				return false;
			}

			if (!entity->GetComponent<TransformComponent>()) {
				(void)entity->AddComponent<TransformComponent>();
			}
			entity->SetActive(true);
			entity->SetVisible(true);
			outEntity = entity;
			return entity->GetComponent<TransformComponent>() != nullptr;
		}

		void CleanupRegressionEntity(World& world, const bool created) {
			if (!created) {
				return;
			}
			Scene* scene = world.GetScenePtr();
			if (!scene) {
				return;
			}
			scene->DestroyEntity(kTestEntityGuid);
			scene->ProcessPendingEntityDestruction();
		}

		void AppendCase(
			std::vector<RegressionCaseResult>& ioResults,
			const std::string_view             name,
			const bool                         passed,
			std::string                        detail
		) {
			ioResults.emplace_back(
				RegressionCaseResult{
					.name = std::string(name),
					.passed = passed,
					.detail = std::move(detail),
				}
			);
		}

		[[nodiscard]] float ReadEntityPositionX(Entity& entity) {
			if (const auto* transform = entity.GetComponent<TransformComponent>()) {
				return transform->GetPosition().x;
			}
			return 0.0f;
		}

		void SetEntityPositionX(Entity& entity, const float x) {
			auto* transform = entity.GetComponent<TransformComponent>();
			if (!transform) {
				return;
			}
			Vec3 position = transform->GetPosition();
			position.x = x;
			transform->SetPosition(position);
		}
	}

	bool SequenceRegressionRunner::RunAll(
		World&        world,
		AssetManager& assetManager,
		std::string*  outReport
	) {
		std::vector<RegressionCaseResult> results = {};
		SequenceRuntime& runtime = world.GetSequenceRuntime();

		Entity* regressionEntity = nullptr;
		bool    createdEntity = false;
		if (!EnsureRegressionEntity(world, regressionEntity, createdEntity)) {
			if (outReport) {
				*outReport = "FAILED: regression entity initialization";
			}
			return false;
		}

		auto runTick = [&runtime](const float deltaSeconds) {
			runtime.EditorTick(deltaSeconds);
		};

		// seek抑制/許可の検証
		{
			const AssetID assetId = LoadSequenceAsset(
				assetManager,
				"./content/core/tests/sequence/seek_policy.sequence.json"
			);
			if (assetId == kInvalidAssetID) {
				AppendCase(
					results,
					"SeekPolicy",
					false,
					"sequence asset load failed"
				);
			} else {
				int cueCount = 0;
				const GameplayCueBus::Handle handle = world.GetGameplayCueBus().Subscribe(
					{
						.cueId = "seq.regression.seek",
						.sourceEntityGuid = kTestEntityGuid
					},
					[&cueCount](const GameplayCue&) {
						++cueCount;
					}
				);

				ScopedSequencePlayer player(runtime);
				player.player->SetAssetId(assetId);
				player.player->SetLoop(false);
				player.player->SetSeekEventPolicy(SEQUENCE_SEEK_EVENT_POLICY::SUPPRESS);

				player.player->SeekFrames(25.0f);
				runTick(0.0f);
				const bool suppressForwardOk = cueCount == 0;

				player.player->SeekFrames(0.0f);
				runTick(0.0f);
				const bool suppressBackwardOk = cueCount == 0;

				player.player->SetSeekEventPolicy(SEQUENCE_SEEK_EVENT_POLICY::FIRE_IN_RANGE);
				player.player->SeekFrames(25.0f);
				runTick(0.0f);
				const bool allowForwardOk = cueCount == 2;

				(void)world.GetGameplayCueBus().Unsubscribe(handle);
				const bool passed = suppressForwardOk && suppressBackwardOk &&
				                    allowForwardOk;
				AppendCase(
					results,
					"SeekPolicy",
					passed,
					std::format("cueCount={} expected=2", cueCount)
				);
			}
		}

		// 逆再生境界とループ跨ぎ発火回数の検証
		{
			const AssetID assetId = LoadSequenceAsset(
				assetManager,
				"./content/core/tests/sequence/reverse_loop.sequence.json"
			);
			if (assetId == kInvalidAssetID) {
				AppendCase(
					results,
					"ReverseLoop",
					false,
					"sequence asset load failed"
				);
			} else {
				int cueCount = 0;
				const GameplayCueBus::Handle handle = world.GetGameplayCueBus().Subscribe(
					{
						.cueId = "seq.regression.reverse_loop",
						.sourceEntityGuid = kTestEntityGuid
					},
					[&cueCount](const GameplayCue&) {
						++cueCount;
					}
				);

				ScopedSequencePlayer player(runtime);
				player.player->SetAssetId(assetId);
				player.player->SetSeekEventPolicy(SEQUENCE_SEEK_EVENT_POLICY::SUPPRESS);

				player.player->SetPlaybackDirection(SEQUENCE_PLAYBACK_DIRECTION::BACKWARD);
				player.player->SetLoop(false);
				player.player->Stop();
				player.player->SeekFrames(30.0f);
				runTick(0.0f);
				cueCount = 0;
				player.player->Play();
				runTick(1.0f);
				const int reverseCount = cueCount;

				player.player->SetPlaybackDirection(SEQUENCE_PLAYBACK_DIRECTION::FORWARD);
				player.player->SetLoop(true);
				player.player->Stop();
				player.player->SeekFrames(28.0f);
				runTick(0.0f);
				cueCount = 0;
				player.player->Play();
				runTick(3.0f);
				const int loopCount = cueCount;

				(void)world.GetGameplayCueBus().Unsubscribe(handle);
				const bool passed = reverseCount == 2 && loopCount == 6;
				AppendCase(
					results,
					"ReverseLoop",
					passed,
					std::format("reverse={} expected=2, loop={} expected=6", reverseCount, loopCount)
				);
			}
		}

		// Restore/Keep 完了モードの検証
		{
			const AssetID assetId = LoadSequenceAsset(
				assetManager,
				"./content/core/tests/sequence/restore_keep.sequence.json"
			);
			if (assetId == kInvalidAssetID || !regressionEntity) {
				AppendCase(
					results,
					"RestoreKeep",
					false,
					"sequence asset load failed"
				);
			} else {
				SetEntityPositionX(*regressionEntity, 1.0f);

				float restoreX = 0.0f;
				{
					ScopedSequencePlayer player(runtime);
					player.player->SetAssetId(assetId);
					player.player->SetCompletionMode(SEQUENCE_COMPLETION_MODE::RESTORE_STATE);
					player.player->SetLoop(false);
					player.player->SetPlayRate(1.0f);
					player.player->Stop();
					player.player->SeekFrames(0.0f);
					runTick(0.0f);
					player.player->Play();
					runTick(0.5f);
					runTick(0.6f);
					restoreX = ReadEntityPositionX(*regressionEntity);
				}
				runTick(0.0f);

				SetEntityPositionX(*regressionEntity, 1.0f);

				float keepX = 0.0f;
				{
					ScopedSequencePlayer player(runtime);
					player.player->SetAssetId(assetId);
					player.player->SetCompletionMode(SEQUENCE_COMPLETION_MODE::KEEP_STATE);
					player.player->SetLoop(false);
					player.player->SetPlayRate(1.0f);
					player.player->Stop();
					player.player->SeekFrames(0.0f);
					runTick(0.0f);
					player.player->Play();
					runTick(0.5f);
					runTick(0.6f);
					keepX = ReadEntityPositionX(*regressionEntity);
				}
				runTick(0.0f);
				SetEntityPositionX(*regressionEntity, 1.0f);

				const bool restoreOk = std::fabs(restoreX - 1.0f) <= kFloatEpsilon;
				const bool keepOk = keepX > 10.0f;
				AppendCase(
					results,
					"RestoreKeep",
					restoreOk && keepOk,
					std::format("restoreX={:.3f} keepX={:.3f}", restoreX, keepX)
				);
			}
		}

		// priority と absolute/additive 合成の検証
		{
			const AssetID lowAssetId = LoadSequenceAsset(
				assetManager,
				"./content/core/tests/sequence/priority_abs_low.sequence.json"
			);
			const AssetID highAssetId = LoadSequenceAsset(
				assetManager,
				"./content/core/tests/sequence/priority_abs_high.sequence.json"
			);
			const AssetID additiveAssetId = LoadSequenceAsset(
				assetManager,
				"./content/core/tests/sequence/priority_additive.sequence.json"
			);
			if (
				lowAssetId == kInvalidAssetID ||
				highAssetId == kInvalidAssetID ||
				additiveAssetId == kInvalidAssetID ||
				!regressionEntity
			) {
				AppendCase(
					results,
					"PriorityBlend",
					false,
					"one or more sequence assets failed to load"
				);
			} else {
				SetEntityPositionX(*regressionEntity, 10.0f);
				ScopedSequencePlayer lowPlayer(runtime);
				ScopedSequencePlayer highPlayer(runtime);

				lowPlayer.player->SetAssetId(lowAssetId);
				lowPlayer.player->SetCompletionMode(SEQUENCE_COMPLETION_MODE::KEEP_STATE);
				lowPlayer.player->Play();

				highPlayer.player->SetAssetId(highAssetId);
				highPlayer.player->SetCompletionMode(SEQUENCE_COMPLETION_MODE::KEEP_STATE);
				highPlayer.player->Play();

				runTick(0.0f);
				const float absoluteX = ReadEntityPositionX(*regressionEntity);

				float additiveX = 0.0f;
				{
					ScopedSequencePlayer additivePlayer(runtime);
					additivePlayer.player->SetAssetId(additiveAssetId);
					additivePlayer.player->SetCompletionMode(SEQUENCE_COMPLETION_MODE::KEEP_STATE);
					additivePlayer.player->Play();
					runTick(0.0f);
					additiveX = ReadEntityPositionX(*regressionEntity);
				}
				runTick(0.0f);
				SetEntityPositionX(*regressionEntity, 10.0f);

				const bool absoluteOk = std::fabs(absoluteX - 5.0f) <= kFloatEpsilon;
				const bool additiveOk = std::fabs(additiveX - 8.0f) <= kFloatEpsilon;
				AppendCase(
					results,
					"PriorityBlend",
					absoluteOk && additiveOk,
					std::format("absoluteX={:.3f} additiveX={:.3f}", absoluteX, additiveX)
				);
			}
		}

		// ホットリロード時のフレームクランプ検証
		{
			const std::string hotReloadPath =
				"./content/core/tests/sequence/hot_reload.sequence.json";
			const AssetID assetId = LoadSequenceAsset(assetManager, hotReloadPath);
			if (assetId == kInvalidAssetID) {
				AppendCase(
					results,
					"HotReloadClamp",
					false,
					"sequence asset load failed"
				);
			} else {
				SequenceFileLoadResult loadResult = {};
				if (!SequenceFileIO::LoadFromFile(hotReloadPath, loadResult)) {
					AppendCase(
						results,
						"HotReloadClamp",
						false,
						"failed to load fixture authoring"
					);
				} else {
					const SequenceAuthoringData originalAuthoring = loadResult.authoring;
					SequenceAuthoringData modifiedAuthoring = originalAuthoring;
					modifiedAuthoring.lengthFrames = 30;
					for (auto& track : modifiedAuthoring.tracks) {
						for (auto& section : track.sections) {
							section.endFrame = std::min<int64_t>(section.endFrame, 30);
						}
					}

					ScopedSequencePlayer player(runtime);
					player.player->SetAssetId(assetId);
					player.player->SetSeekEventPolicy(SEQUENCE_SEEK_EVENT_POLICY::SUPPRESS);
					player.player->SeekFrames(90.0f);
					runTick(0.0f);
					const float frameBeforeReload = player.player->GetCurrentFrame();

					const bool writeOk = SequenceFileIO::SaveToFile(
						hotReloadPath,
						modifiedAuthoring,
						nullptr
					);
					const bool reloadOk = writeOk && assetManager.ReloadWithDependents(assetId);
					runTick(0.0f);
					const float frameAfterReload = player.player->GetCurrentFrame();

					(void)SequenceFileIO::SaveToFile(
						hotReloadPath,
						originalAuthoring,
						nullptr
					);
					(void)assetManager.ReloadWithDependents(assetId);
					runTick(0.0f);

					const bool clampOk = frameBeforeReload >= 89.0f &&
					                     frameAfterReload <= 30.0f + kFloatEpsilon;
					AppendCase(
						results,
						"HotReloadClamp",
						reloadOk && clampOk,
						std::format(
							"before={:.3f} after={:.3f}",
							frameBeforeReload,
							frameAfterReload
						)
					);
				}
			}
		}

		CleanupRegressionEntity(world, createdEntity);

		int passCount = 0;
		std::string report = {};
		for (const RegressionCaseResult& result : results) {
			if (result.passed) {
				++passCount;
			}
			report += std::format(
				"[{}] {} - {}\n",
				result.passed ? "PASS" : "FAIL",
				result.name,
				result.detail
			);
		}
		report += std::format("Summary: {}/{} passed", passCount, results.size());

		if (outReport) {
			*outReport = report;
		}
		return passCount == static_cast<int>(results.size());
	}
}
