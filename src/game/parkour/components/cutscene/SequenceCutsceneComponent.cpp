#include "SequenceCutsceneComponent.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>

#ifdef _DEBUG
#include <imgui.h>
#endif

#include "core/ComponentRegistry.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"

#include "engine/ImGui/Icons.h"
#include "engine/scene/Scene.h"
#include "engine/sequence/EntitySequenceBinder.h"
#include "engine/sequence/SequenceAsset.h"
#include "engine/sequence/SequencePlayer.h"
#include "engine/sequence/UiSequenceBinder.h"
#include "engine/unnamed/framework/components/base/BaseComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/unnamed/subsystem/console/Log.h"
#include "engine/world/World.h"

namespace Unnamed {
	namespace {
		static constexpr std::string_view kChannel = "SequenceCutscene";

#ifdef _DEBUG
		template <size_t N>
		void DrawStringInput(const char* label, std::string& value) {
			std::array<char, N> buffer = {};
			const size_t copyLen = std::min(value.size(), buffer.size() - 1);
			if (copyLen > 0) {
				std::memcpy(buffer.data(), value.data(), copyLen);
			}
			if (ImGui::InputText(label, buffer.data(), buffer.size())) {
				value = buffer.data();
			}
		}
#endif
	}

	void SequenceCutsceneComponent::OnAttached() {
		mPlayRequested = mPlayOnAttach;
	}

	void SequenceCutsceneComponent::OnDetached() {
		StopPlayback(true);
		mSequenceAsset.reset();
		mSequencePlayer.reset();
		mEntityBinder.reset();
		mUiBinder.reset();
	}

	void SequenceCutsceneComponent::OnTick(const float deltaTime) {
		if (mPlayRequested && !mPlaying) {
			(void)StartPlayback();
		}
		if (mPlaying) {
			TickPlayback(deltaTime);
		}
	}

	void SequenceCutsceneComponent::OnEditorTick(const float deltaTime) {
		if (mPlaying) {
			TickPlayback(deltaTime);
		}
	}

	std::string_view SequenceCutsceneComponent::GetStableName() const {
		return "game.SequenceCutscene";
	}

	std::string_view SequenceCutsceneComponent::GetComponentName() const {
		return "SequenceCutscene";
	}

	uint32_t SequenceCutsceneComponent::GetIcon() const {
		return kIconVideoCam;
	}

#ifdef _DEBUG
	void SequenceCutsceneComponent::DrawInspectorImGui() {
		DrawStringInput<256>("Sequence Path", mSequencePath);
		ImGui::Checkbox("Play On Attach", &mPlayOnAttach);
		ImGui::Checkbox("Auto Stop When Completed", &mAutoStopWhenCompleted);
		ImGui::DragFloat("Play Rate", &mPlayRate, 0.05f, 0.0f, 8.0f);
		ImGui::Checkbox("Force Cutscene Camera", &mForceCutsceneCamera);
		ImGui::InputScalar(
			"Cutscene Camera Entity Guid",
			ImGuiDataType_U64,
			&mCutsceneCameraEntityGuid
		);
		ImGui::Checkbox("Restore Previous Camera", &mRestorePreviousCamera);
		ImGui::Checkbox("Apply Component Locks", &mApplyComponentLocks);

		if (ImGui::Button("Add Lock Target")) {
			mLockTargets.emplace_back();
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear Lock Targets")) {
			mLockTargets.clear();
		}
		for (size_t i = 0; i < mLockTargets.size(); ++i) {
			ImGui::PushID(static_cast<int>(i));
			LockTargetSpec& spec = mLockTargets[i];
			ImGui::InputScalar(
				"Component Guid",
				ImGuiDataType_U64,
				&spec.componentGuid
			);
			ImGui::InputScalar("Entity Guid", ImGuiDataType_U64, &spec.entityGuid);
			DrawStringInput<128>("Component StableName", spec.componentStableName);
			if (ImGui::Button("Remove")) {
				mLockTargets.erase(mLockTargets.begin() + static_cast<ptrdiff_t>(i));
				ImGui::PopID();
				break;
			}
			ImGui::Separator();
			ImGui::PopID();
		}

		ImGui::Text("Playing: %s", mPlaying ? "true" : "false");
		if (!mPlaying) {
			if (ImGui::Button("Start")) {
				mPlayRequested = true;
			}
		} else {
			if (ImGui::Button("Stop")) {
				StopPlayback(true);
			}
		}
	}
#endif

	void SequenceCutsceneComponent::Deserialize(const JsonReader& reader) {
		mSequencePath = reader["sequencePath"].GetString(mSequencePath);
		if (const JsonReader playOnAttach = reader["playOnAttach"]; playOnAttach.Valid()) {
			mPlayOnAttach = playOnAttach.GetBool(mPlayOnAttach);
		}
		if (const JsonReader autoStop = reader["autoStopWhenCompleted"];
			autoStop.Valid()) {
			mAutoStopWhenCompleted = autoStop.GetBool(mAutoStopWhenCompleted);
		}
		if (const JsonReader playRate = reader["playRate"]; playRate.Valid()) {
			mPlayRate = std::max(0.0f, playRate.GetFloat(mPlayRate));
		}
		if (const JsonReader forceCamera = reader["forceCutsceneCamera"];
			forceCamera.Valid()) {
			mForceCutsceneCamera = forceCamera.GetBool(mForceCutsceneCamera);
		}
		if (const JsonReader cameraGuid = reader["cutsceneCameraEntityGuid"];
			cameraGuid.Valid()) {
			mCutsceneCameraEntityGuid = cameraGuid.GetUint64();
		}
		if (const JsonReader restoreCamera = reader["restorePreviousCamera"];
			restoreCamera.Valid()) {
			mRestorePreviousCamera = restoreCamera.GetBool(mRestorePreviousCamera);
		}
		if (const JsonReader applyLocks = reader["applyComponentLocks"];
			applyLocks.Valid()) {
			mApplyComponentLocks = applyLocks.GetBool(mApplyComponentLocks);
		}

		mLockTargets.clear();
		if (const JsonReader lockTargets = reader["lockTargets"]; lockTargets.Valid()) {
			const JsonReader lockArray = lockTargets.GetArray();
			for (size_t i = 0; i < lockArray.Size(); ++i) {
				const JsonReader lockNode = lockArray[i];
				if (!lockNode.Valid()) {
					continue;
				}
				LockTargetSpec spec = {};
				if (const JsonReader componentGuid = lockNode["componentGuid"];
					componentGuid.Valid()) {
					spec.componentGuid = componentGuid.GetUint64();
				}
				if (const JsonReader entityGuid = lockNode["entityGuid"]; entityGuid.Valid()) {
					spec.entityGuid = entityGuid.GetUint64();
				}
				spec.componentStableName = lockNode["componentStableName"].GetString(
					spec.componentStableName
				);
				mLockTargets.emplace_back(std::move(spec));
			}
		}

		mPlayRequested = mPlayOnAttach;
	}

	void SequenceCutsceneComponent::Serialize(JsonWriter& writer) const {
		writer.Key("sequencePath");
		writer.Write(mSequencePath);
		writer.Key("playOnAttach");
		writer.Write(mPlayOnAttach);
		writer.Key("autoStopWhenCompleted");
		writer.Write(mAutoStopWhenCompleted);
		writer.Key("playRate");
		writer.Write(mPlayRate);
		writer.Key("forceCutsceneCamera");
		writer.Write(mForceCutsceneCamera);
		writer.Key("cutsceneCameraEntityGuid");
		writer.Write(mCutsceneCameraEntityGuid);
		writer.Key("restorePreviousCamera");
		writer.Write(mRestorePreviousCamera);
		writer.Key("applyComponentLocks");
		writer.Write(mApplyComponentLocks);

		writer.Key("lockTargets");
		writer.BeginArray();
		for (const LockTargetSpec& spec : mLockTargets) {
			SerializeLockTarget(writer, spec);
		}
		writer.EndArray();
	}

	bool SequenceCutsceneComponent::StartPlayback() {
		if (mPlaying) {
			return true;
		}
		if (!EnsureSequenceLoaded()) {
			return false;
		}

		RefreshBinders();
		if (!mSequencePlayer || !mSequenceAsset) {
			return false;
		}

		mSequencePlayer->SetAsset(mSequenceAsset);
		mSequencePlayer->SetPlayRate(std::max(0.0f, mPlayRate));
		mSequencePlayer->Play();
		if (!mSequencePlayer->IsPlaying()) {
			return false;
		}

		mSavedCameraValid      = false;
		mSavedCameraEntityGuid = 0;
		if (mRestorePreviousCamera) {
			if (World* world = GetWorld()) {
				const auto& cameraManager = world->GetCameraManager();
				mSavedCameraValid = true;
				mSavedCameraEntityGuid = cameraManager.HasCurrentCamera() ?
					                         cameraManager.GetCurrentCameraGuid() :
					                         0;
			}
		}

		ApplyLockTargets();
		ApplyCutsceneCamera();
		mPlaying       = true;
		mPlayRequested = false;
		return true;
	}

	void SequenceCutsceneComponent::StopPlayback(const bool restoreState) {
		if (mSequencePlayer) {
			mSequencePlayer->Stop();
		}
		if (restoreState) {
			RestoreLockTargets();

			if (mRestorePreviousCamera && mSavedCameraValid) {
				if (World* world = GetWorld()) {
					auto& cameraManager = world->GetCameraManager();
					if (mSavedCameraEntityGuid != 0) {
						if (!cameraManager.SetCurrentCamera(mSavedCameraEntityGuid)) {
							cameraManager.ClearCurrentCamera();
						}
					} else {
						cameraManager.ClearCurrentCamera();
					}
				}
			}
		}

		mSavedCameraValid      = false;
		mSavedCameraEntityGuid = 0;
		mPlaying               = false;
		mPlayRequested         = false;
	}

	void SequenceCutsceneComponent::TickPlayback(const float deltaTime) {
		if (!mSequencePlayer) {
			return;
		}

		mSequencePlayer->Tick(std::max(0.0f, deltaTime));
		ApplyCutsceneCamera();
		if (
			mAutoStopWhenCompleted &&
			mSequencePlayer->GetState() == SEQUENCE_PLAYER_STATE::COMPLETED
		) {
			StopPlayback(true);
		}
	}

	bool SequenceCutsceneComponent::EnsureSequenceLoaded() {
		if (mSequencePath.empty()) {
			return false;
		}
		if (mSequenceAsset && mLoadedSequencePath == mSequencePath) {
			return true;
		}

		mSequenceAsset = SequenceAsset::Load(mSequencePath);
		if (!mSequenceAsset) {
			if (!mLoggedLoadFailure) {
				Warning(
					kChannel,
					"Failed to load sequence '{}'.",
					mSequencePath
				);
				mLoggedLoadFailure = true;
			}
			return false;
		}

		mLoadedSequencePath = mSequencePath;
		mLoggedLoadFailure  = false;
		return true;
	}

	void SequenceCutsceneComponent::RefreshBinders() {
		if (!mSequencePlayer) {
			mSequencePlayer = std::make_unique<SequencePlayer>();
		}
		if (!mEntityBinder) {
			mEntityBinder = std::make_unique<EntitySequenceBinder>(GetScene());
		}
		if (!mUiBinder) {
			mUiBinder = std::make_unique<UiSequenceBinder>(GetScene());
		}

		mEntityBinder->SetScene(GetScene());
		mUiBinder->SetScene(GetScene());
		mSequencePlayer->ClearBinders();
		mSequencePlayer->AddBinder(mEntityBinder.get());
		mSequencePlayer->AddBinder(mUiBinder.get());
	}

	void SequenceCutsceneComponent::ApplyLockTargets() {
		RestoreLockTargets();
		if (!mApplyComponentLocks) {
			return;
		}

		std::vector<BaseComponent*> lockedComponents = {};
		lockedComponents.reserve(mLockTargets.size());
		for (const LockTargetSpec& spec : mLockTargets) {
			BaseComponent* target = ResolveLockTarget(spec);
			if (!target || target == this) {
				continue;
			}
			if (
				std::ranges::find(lockedComponents, target) !=
				lockedComponents.end()
			) {
				continue;
			}

			mActiveLocks.emplace_back(
				ActiveLockState{
					.component = target,
					.previousActive = target->IsActive(),
				}
			);
			lockedComponents.emplace_back(target);
			target->SetActive(false);
		}
	}

	void SequenceCutsceneComponent::RestoreLockTargets() {
		for (const ActiveLockState& lockState : mActiveLocks) {
			if (!lockState.component) {
				continue;
			}
			lockState.component->SetActive(lockState.previousActive);
		}
		mActiveLocks.clear();
	}

	BaseComponent* SequenceCutsceneComponent::ResolveLockTarget(
		const LockTargetSpec& spec
	) const {
		Scene* scene = GetScene();
		if (!scene) {
			return nullptr;
		}

		auto matchesSpec = [&](BaseComponent& component) {
			if (
				spec.componentGuid != 0 &&
				component.GetGuid() != spec.componentGuid
			) {
				return false;
			}
			if (
				!spec.componentStableName.empty() &&
				component.GetStableName() != spec.componentStableName
			) {
				return false;
			}
			return true;
		};

		auto findInEntity = [&](Entity& entity) -> BaseComponent* {
			BaseComponent* found = nullptr;
			entity.ForEachComponent(
				[&](BaseComponent& component) {
					if (!matchesSpec(component)) {
						return true;
					}
					found = &component;
					return false;
				}
			);
			return found;
		};

		if (spec.entityGuid != 0) {
			if (Entity* entity = scene->FindEntity(spec.entityGuid)) {
				return findInEntity(*entity);
			}
			return nullptr;
		}

		if (!spec.componentStableName.empty()) {
			if (Entity* owner = GetOwner()) {
				if (BaseComponent* found = findInEntity(*owner)) {
					return found;
				}
			}
		}

		for (const auto& entityPtr : scene->GetEntities()) {
			if (!entityPtr) {
				continue;
			}
			if (BaseComponent* found = findInEntity(*entityPtr)) {
				return found;
			}
		}

		return nullptr;
	}

	void SequenceCutsceneComponent::ApplyCutsceneCamera() const {
		if (!mForceCutsceneCamera || mCutsceneCameraEntityGuid == 0) {
			return;
		}
		if (World* world = GetWorld()) {
			(void)world->GetCameraManager().SetCurrentCamera(
				mCutsceneCameraEntityGuid
			);
		}
	}

	void SequenceCutsceneComponent::SerializeLockTarget(
		JsonWriter&           writer,
		const LockTargetSpec& spec
	) {
		writer.BeginObject();
		writer.Key("componentGuid");
		writer.Write(spec.componentGuid);
		writer.Key("entityGuid");
		writer.Write(spec.entityGuid);
		writer.Key("componentStableName");
		writer.Write(spec.componentStableName);
		writer.EndObject();
	}

	REGISTER_COMPONENT(SequenceCutsceneComponent);
}
