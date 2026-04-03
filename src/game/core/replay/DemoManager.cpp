#include "DemoManager.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>
#include <sstream>

#include "DemoBinaryReader.h"
#include "DemoBinaryWriter.h"

#include "engine/scene/Scene.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/unnamed/subsystem/console/ConsoleSystem.h"
#include "engine/unnamed/subsystem/console/Log.h"
// ReSharper disable once CppUnusedIncludeDirective
#include "engine/unnamed/subsystem/console/concommand/ConVar.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"
#include "engine/world/World.h"

namespace Unnamed {
	namespace {
		constexpr std::string_view kChannel = "Demo";

		std::string ToLowerAscii(std::string_view text) {
			std::string lowered(text);
			std::ranges::transform(
				lowered,
				lowered.begin(),
				[](const unsigned char ch) {
					return static_cast<char>(std::tolower(ch));
				}
			);
			return lowered;
		}

		[[nodiscard]] std::string NormalizePathForCompare(
			const std::string_view pathText
		) {
			if (pathText.empty()) {
				return {};
			}
			std::string normalized = std::filesystem::path(pathText)
			                         .lexically_normal()
			                         .generic_string();
			if (normalized.rfind("./", 0) == 0) {
				normalized.erase(0, 2);
			}
			return normalized;
		}
	}

	bool DemoManager::StartRecording(std::string path) {
		if (path.empty()) {
			path = "./content/parkour/replay/demo_recorded_v2.udemo";
		}
		if (std::filesystem::path(ToLowerAscii(path)).extension() == ".json") {
			Error(
				kChannel,
				"JSON demo recording is unsupported in DemoFileV2. Please use .udemo."
			);
			return false;
		}

		if (mMode == MODE::RECORDING) {
			(void)Stop();
		} else {
			Reset();
		}

		mMode                = MODE::RECORDING;
		mCurrentPath         = std::move(path);
		mFile                = {};
		mFile.version        = kDemoFileVersion;
		mActiveTickRate      = ResolveConfiguredTickRate();
		mFile.tickRate       = mActiveTickRate;
		mFile.startTick      = 0;
		mFile.meta["schema"] = "DemoFileV2";
		mFile.meta["note"]   = "binary-only v2, player-focused";
		Msg(kChannel, "Recording started: {}", mCurrentPath);
		return true;
	}

	bool DemoManager::StartPlayback(std::string path) {
		if (path.empty()) {
			Error(kChannel, "demo_play requires a path.");
			return false;
		}

		if (mMode == MODE::RECORDING) {
			(void)Stop();
		} else {
			Reset();
		}

		if (!LoadPlaybackFile(path)) {
			return false;
		}

		mMode           = MODE::PLAYBACK;
		mCurrentPath    = std::move(path);
		mActiveTickRate = mFile.tickRate;
		mPlaybackInitialAppliedEntities.clear();
		mPlaybackInitialSetApplied = false;
		Msg(
			kChannel,
			"Playback started: {} (commands={}, snapshots={})",
			mCurrentPath,
			mFile.commands.size(),
			mFile.snapshots.size()
		);
		return true;
	}

	bool DemoManager::Stop() {
		bool ok = true;
		if (mMode == MODE::RECORDING) {
			ok = SaveRecordingToFile();
			if (ok) {
				Msg(kChannel, "Recording stopped: {}", mCurrentPath);
			}
		} else if (mMode == MODE::PLAYBACK) {
			Msg(kChannel, "Playback stopped: {}", mCurrentPath);
		}

		Reset();
		return ok;
	}

	DemoManager::MODE DemoManager::GetMode() const {
		return mMode;
	}

	bool DemoManager::IsRecording() const {
		return mMode == MODE::RECORDING;
	}

	bool DemoManager::IsPlayback() const {
		return mMode == MODE::PLAYBACK;
	}

	bool DemoManager::IsPlaybackFinished() const {
		return mMode == MODE::PLAYBACK &&
		       mPlaybackCommandCursor >= mFile.commands.size();
	}

	uint64_t DemoManager::GetPlaybackStartTick() const {
		return mMode == MODE::PLAYBACK ? mFile.startTick : 0ull;
	}

	std::string_view DemoManager::GetCurrentPath() const {
		return mCurrentPath;
	}

	uint32_t DemoManager::GetSimulationTickRate() const {
		if (mMode == MODE::PLAYBACK || mMode == MODE::RECORDING) {
			return SanitizeTickRate(mActiveTickRate);
		}
		return ResolveConfiguredTickRate();
	}

	float DemoManager::GetSimulationStepSeconds() const {
		return TickStepSecondsFromRate(GetSimulationTickRate());
	}

	uint32_t DemoManager::ResolveConfiguredTickRate() {
		ConsoleSystem* console = ServiceLocator::Get<ConsoleSystem>();
		if (!console) {
			return kDefaultDemoTickRate;
		}
		const auto tickrate = console->GetConVarValueOr(
			"sv_tickrate",
			static_cast<int>(kDefaultDemoTickRate)
		);
		return SanitizeTickRate(tickrate);
	}

	float DemoManager::TickStepSecondsFromRate(const uint32_t tickRate) {
		const uint32_t rate = SanitizeTickRate(tickRate);
		return 1.0f / static_cast<float>(rate);
	}

	void DemoManager::SubmitLiveCommand(const DemoTickCommand& command) {
		if (mMode != MODE::RECORDING) {
			return;
		}
		if (mFile.commands.empty()) {
			mFile.startTick = command.tick;
		}
		mFile.commands.emplace_back(command);
	}

	bool DemoManager::ConsumePlaybackCommand(
		const uint64_t   tick,
		const uint64_t   subjectEntityGuid,
		DemoTickCommand& outCommand
	) {
		if (mMode != MODE::PLAYBACK) {
			return false;
		}

		while (mPlaybackCommandCursor < mFile.commands.size() &&
		       mFile.commands[mPlaybackCommandCursor].tick < tick) {
			++mPlaybackCommandCursor;
		}

		if (mPlaybackCommandCursor >= mFile.commands.size()) {
			return false;
		}

		const DemoTickCommand& command = mFile.commands[mPlaybackCommandCursor];
		if (command.tick != tick) {
			return false;
		}

		if (subjectEntityGuid != 0 && command.subjectEntityGuid != 0 &&
		    command.subjectEntityGuid != subjectEntityGuid) {
			return false;
		}

		outCommand = command;
		++mPlaybackCommandCursor;
		return true;
	}

	void DemoManager::CaptureInitialSnapshotIfNeeded(
		const Entity& subjectEntity
	) {
		if (mMode != MODE::RECORDING) {
			return;
		}

		const auto captureEntity = [this](const Entity& entity) {
			if (!entity.IsActive()) {
				return;
			}
			const uint64_t entityGuid = entity.GetGuid();
			for (const EntitySnapshotRecord& initial : mFile.initialEntities) {
				if (initial.entityGuid == entityGuid) {
					return;
				}
			}

			nlohmann::json state = nlohmann::json::object();
			uint64_t       hash  = 0;
			if (!mSerializerRegistry.SerializeEntity(entity, state, hash)) {
				return;
			}

			EntitySnapshotRecord initialRecord = {};
			initialRecord.entityGuid           = entityGuid;
			initialRecord.entityType           = "entity.generic.v1";
			initialRecord.entityTypeId         = 0;
			initialRecord.state                = std::move(state);
			initialRecord.hash                 = hash;
			mFile.initialEntities.emplace_back(std::move(initialRecord));
		};

		const World* world = subjectEntity.GetWorld();
		const Scene* scene = world ? world->GetScenePtr() : nullptr;
		if (!scene) {
			captureEntity(subjectEntity);
			return;
		}

		std::vector<const Entity*> orderedEntities = {};
		orderedEntities.reserve(scene->GetEntities().size());
		for (const auto& entityPtr : scene->GetEntities()) {
			if (!entityPtr) {
				continue;
			}
			orderedEntities.emplace_back(entityPtr.get());
		}
		std::ranges::sort(
			orderedEntities,
			[](const Entity* lhs, const Entity* rhs) {
				return lhs->GetGuid() < rhs->GetGuid();
			}
		);
		for (const Entity* entity : orderedEntities) {
			if (!entity) {
				continue;
			}
			captureEntity(*entity);
		}
	}

	bool DemoManager::ApplyInitialSnapshotIfNeeded(Entity& subjectEntity) {
		if (mMode != MODE::PLAYBACK) {
			return true;
		}

		if (!mFile.mapPath.empty()) {
			const World* world = subjectEntity.GetWorld();
			if (world) {
				const std::string expectedMap = NormalizePathForCompare(
					mFile.mapPath
				);
				const std::string currentMap = NormalizePathForCompare(
					world->GetLoadedScenePath()
				);
				if (!expectedMap.empty() && !currentMap.empty() &&
				    expectedMap != currentMap) {
					Error(
						kChannel,
						"Map mismatch for demo playback. expected='{}' actual='{}'",
						mFile.mapPath,
						world->GetLoadedScenePath()
					);
					return false;
				}
			}
		}

		const uint64_t entityGuid = subjectEntity.GetGuid();
		if (!mPlaybackInitialSetApplied && !mFile.initialEntities.empty()) {
			Scene* scene = nullptr;
			if (World* world = subjectEntity.GetWorld()) {
				scene = world->GetScenePtr();
			}

			for (const EntitySnapshotRecord& initial : mFile.initialEntities) {
				Entity* targetEntity = scene ?
					                       scene->FindEntity(
						                       initial.entityGuid
					                       ) :
					                       nullptr;
				if (!targetEntity) {
					continue;
				}
				mSerializerRegistry.DeserializeEntity(
					*targetEntity, initial.state
				);
				mPlaybackInitialAppliedEntities.insert(initial.entityGuid);
			}
			mPlaybackInitialSetApplied = true;
		}

		if (mPlaybackInitialAppliedEntities.contains(entityGuid)) {
			return true;
		}

		const EntitySnapshotRecord* record = nullptr;
		for (const EntitySnapshotRecord& initial : mFile.initialEntities) {
			if (initial.entityGuid == entityGuid) {
				record = &initial;
				break;
			}
		}

		if (!record && mFile.initialEntities.size() == 1) {
			record = &mFile.initialEntities.front();
			Warning(
				kChannel,
				"Initial snapshot for entity {} not found. Using only available initial entity {}.",
				entityGuid,
				record->entityGuid
			);
		}

		if (!record) {
			Warning(
				kChannel,
				"Initial snapshot is missing for entity {}. Falling back to current runtime state.",
				entityGuid
			);
			mPlaybackInitialAppliedEntities.insert(entityGuid);
			return true;
		}

		mSerializerRegistry.DeserializeEntity(subjectEntity, record->state);
		mPlaybackInitialAppliedEntities.insert(entityGuid);
		return true;
	}

	void DemoManager::RecordOrVerifySnapshot(
		const uint64_t tick, Entity& subjectEntity
	) {
		nlohmann::json state = nlohmann::json::object();
		uint64_t       hash  = 0;
		if (!mSerializerRegistry.SerializeEntity(subjectEntity, state, hash)) {
			return;
		}

		EntitySnapshotRecord currentSnapshotRecord = {};
		currentSnapshotRecord.entityGuid           = subjectEntity.GetGuid();
		currentSnapshotRecord.entityType           = "entity.player.v1";
		currentSnapshotRecord.entityTypeId         = 0;
		currentSnapshotRecord.hash                 = hash;

		FrameSnapshot snapshot = {};
		snapshot.tick          = tick;
		snapshot.entities.emplace_back(std::move(currentSnapshotRecord));

		if (mMode == MODE::RECORDING) {
			if (mFile.mapPath.empty()) {
				if (const World* world = subjectEntity.GetWorld()) {
					mFile.mapPath = std::string(world->GetLoadedScenePath());
				}
			}
			mFile.snapshots.emplace_back(std::move(snapshot));
			return;
		}

		if (mMode != MODE::PLAYBACK) {
			return;
		}

		while (mPlaybackSnapshotCursor < mFile.snapshots.size() &&
		       mFile.snapshots[mPlaybackSnapshotCursor].tick < tick) {
			++mPlaybackSnapshotCursor;
		}

		if (mPlaybackSnapshotCursor >= mFile.snapshots.size()) {
			return;
		}

		const FrameSnapshot& expected = mFile.snapshots[
			mPlaybackSnapshotCursor];
		if (expected.tick != tick) {
			return;
		}
		++mPlaybackSnapshotCursor;

		if (expected.entities.empty() || snapshot.entities.empty()) {
			return;
		}
		const EntitySnapshotRecord& currentRecord  = snapshot.entities.front();
		const EntitySnapshotRecord* expectedRecord = nullptr;
		for (const EntitySnapshotRecord& expectedEntityRecord : expected.
		     entities) {
			if (expectedEntityRecord.entityGuid == currentRecord.entityGuid) {
				expectedRecord = &expectedEntityRecord;
				break;
			}
		}
		if (expectedRecord == nullptr) {
			return;
		}

		if (expectedRecord->hash != currentRecord.hash) {
			++mSnapshotMismatchCount;
			if (mSnapshotMismatchCount == 1) {
				mFirstMismatchTick = tick;
			}

			const MISMATCH_POLICY policy = ResolveMismatchPolicy();
			const uint64_t logInterval = ResolveMismatchLogInterval();
			const bool shouldLogMismatch = mSnapshotMismatchCount <= 16ull ||
			                               (mSnapshotMismatchCount %
			                                logInterval) ==
			                               0ull;
			if (shouldLogMismatch) {
				Warning(
					kChannel,
					"Snapshot hash mismatch at tick {}: expected={}, actual={}, entity={}, policy={}",
					tick,
					expectedRecord->hash,
					currentRecord.hash,
					currentRecord.entityGuid,
					MismatchPolicyToString(policy)
				);
			}

			switch (policy) {
				case MISMATCH_POLICY::STOP: (void)Stop();
					break;
				case MISMATCH_POLICY::CONTINUE:
				default: break;
			}
		}
	}

	void DemoManager::Reset() {
		mMode = MODE::IDLE;
		mCurrentPath.clear();
		mFile                   = {};
		mPlaybackCommandCursor  = 0;
		mPlaybackSnapshotCursor = 0;
		mSnapshotMismatchCount  = 0;
		mFirstMismatchTick      = std::numeric_limits<uint64_t>::max();
		mPlaybackInitialAppliedEntities.clear();
		mPlaybackInitialSetApplied = false;
		mActiveTickRate            = ResolveConfiguredTickRate();
	}

	std::string DemoManager::BuildStatusString() const {
		std::string modeText = "Idle";
		if (mMode == MODE::RECORDING) {
			modeText = "Recording";
		} else if (mMode == MODE::PLAYBACK) {
			modeText = "Playback";
		}
		std::ostringstream stream;
		stream
			<< "mode=" << modeText
			<< ", path='" << mCurrentPath << "'"
			<< ", commands=" << mFile.commands.size()
			<< ", snapshots=" << mFile.snapshots.size()
			<< ", playbackCommandCursor=" << mPlaybackCommandCursor
			<< ", playbackSnapshotCursor=" << mPlaybackSnapshotCursor
			<< ", mismatch=" << mSnapshotMismatchCount
			<< ", tickRate=" << GetSimulationTickRate()
			<< ", firstMismatchTick="
			<< (mFirstMismatchTick != std::numeric_limits<uint64_t>::max() ?
				    std::to_string(mFirstMismatchTick) :
				    "none")
			<< ", mismatchPolicy=" << MismatchPolicyToString(
				ResolveMismatchPolicy()
			);
		return stream.str();
	}

	DemoManager::MISMATCH_POLICY DemoManager::ResolveMismatchPolicy() {
		ConsoleSystem* console = ServiceLocator::Get<ConsoleSystem>();
		if (!console) {
			return MISMATCH_POLICY::CONTINUE;
		}

		const std::string raw = ToLowerAscii(
			console->GetConVarValueString("demo_mismatch_policy")
		);
		if (raw == "stop") {
			return MISMATCH_POLICY::STOP;
		}
		if (raw == "resync") {
			static bool sWarnedResyncFallback = false;
			if (!sWarnedResyncFallback) {
				Warning(
					kChannel,
					"demo_mismatch_policy=resync is unsupported in DemoFileV2. Falling back to continue."
				);
				sWarnedResyncFallback = true;
			}
		}
		return MISMATCH_POLICY::CONTINUE;
	}

	uint64_t DemoManager::ResolveMismatchLogInterval() {
		ConsoleSystem* console = ServiceLocator::Get<ConsoleSystem>();
		if (!console) {
			return 120ull;
		}
		const std::string raw = console->GetConVarValueString(
			"demo_mismatch_log_interval"
		);
		if (raw.empty()) {
			return 120ull;
		}
		try {
			const int value = std::stoi(raw);
			return static_cast<uint64_t>(std::max(1, value));
		} catch (...) {
			return 120ull;
		}
	}

	uint32_t DemoManager::SanitizeTickRate(const uint32_t tickRate) {
		return std::clamp(tickRate, 1u, 1000u);
	}

	std::string_view DemoManager::MismatchPolicyToString(
		const MISMATCH_POLICY policy
	) {
		switch (policy) {
			case MISMATCH_POLICY::STOP: return "stop";
			case MISMATCH_POLICY::CONTINUE:
			default: return "continue";
		}
	}

	bool DemoManager::SaveRecordingToFile() const {
		if (mCurrentPath.empty()) {
			Error(kChannel, "Cannot save recording: path is empty.");
			return false;
		}

		const std::filesystem::path outPath(mCurrentPath);
		if (outPath.has_parent_path()) {
			std::filesystem::create_directories(outPath.parent_path());
		}

		DemoBinaryWriter writer = {};
		std::string      error;
		if (!writer.WriteFile(mCurrentPath, mFile, &error)) {
			Error(
				kChannel,
				"Failed to write demo binary '{}': {}",
				mCurrentPath,
				error
			);
			return false;
		}
		return true;
	}

	bool DemoManager::LoadPlaybackFile(const std::string& path) {
		const std::string loweredPath = ToLowerAscii(path);
		if (std::filesystem::path(loweredPath).extension() == ".json") {
			Error(
				kChannel,
				"JSON demo playback is unsupported in DemoFileV2. Please record or convert to .udemo."
			);
			return false;
		}

		DemoFileV2       loaded = {};
		DemoBinaryReader reader = {};
		std::string      error;
		if (!reader.ReadFile(path, loaded, &error)) {
			Error(kChannel, "Failed to read demo binary '{}': {}", path, error);
			return false;
		}

		if (loaded.version != kDemoFileVersion) {
			Error(
				kChannel,
				"Unsupported demo version: {} (expected {})",
				loaded.version,
				kDemoFileVersion
			);
			return false;
		}
		if (loaded.tickRate == 0) {
			Error(
				kChannel,
				"Unsupported demo tickRate: {}",
				loaded.tickRate,
				0
			);
			return false;
		}
		loaded.tickRate = SanitizeTickRate(loaded.tickRate);

		mFile                   = std::move(loaded);
		mPlaybackCommandCursor  = 0;
		mPlaybackSnapshotCursor = 0;
		mSnapshotMismatchCount  = 0;
		mFirstMismatchTick      = std::numeric_limits<uint64_t>::max();
		mActiveTickRate         = mFile.tickRate;
		return true;
	}
}
