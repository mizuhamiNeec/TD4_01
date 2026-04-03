#include "CharacterPresentationComponent.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

#ifdef _DEBUG
#include <imgui.h>
#endif

#include "game/core/components/AudioFxControllerComponent.h"
#include "game/core/components/CameraFxControllerComponent.h"

#include "core/ComponentRegistry.h"
#include "core/assets/AssetManager.h"
#include "core/assets/AssetType.h"
#include "core/assets/types/PresentationProfileAssetData.h"
#include "core/string/StrUtil.h"

#include "engine/ImGui/Icons.h"
#ifdef _DEBUG
#include "engine/ImGui/ImGuiWidgets.h"
#endif
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"

#include "engine/scene/Scene.h"
#include "engine/unnamed/framework/components/mesh/SkeletalAnimationComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/unnamed/subsystem/console/Log.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"
#include "engine/world/World.h"

namespace Unnamed {
	namespace {
		constexpr std::string_view kChannel             = "CharPresentation";
		constexpr std::string_view kCueStateEnterPrefix =
			"movement.state.enter.";
		constexpr std::string_view kCueStateUpdatePrefix =
			"movement.state.update.";

#ifdef _DEBUG
		bool EditStringField(
			const char* label, std::string& value, const size_t capacity = 256
		) {
			std::vector<char> buffer(capacity, '\0');
			const size_t      copyLength = std::min(value.size(), capacity - 1);
			if (copyLength > 0) {
				std::memcpy(buffer.data(), value.data(), copyLength);
			}
			if (!ImGui::InputText(label, buffer.data(), buffer.size())) {
				return false;
			}
			value = buffer.data();
			return true;
		}

		bool EditEntityGuidField(const char* label, uint64_t& guid) {
			return ImGui::InputScalar(label, ImGuiDataType_U64, &guid);
		}
#endif

		std::string TrimAscii(std::string_view text) {
			size_t begin = 0;
			while (
				begin < text.size() &&
				std::isspace(static_cast<unsigned char>(text[begin])) != 0
			) {
				++begin;
			}
			size_t end = text.size();
			while (
				end > begin &&
				std::isspace(static_cast<unsigned char>(text[end - 1])) != 0
			) {
				--end;
			}
			return std::string(text.substr(begin, end - begin));
		}

		std::string ToLowerAscii(std::string text) {
			std::ranges::transform(
				text,
				text.begin(),
				[](const unsigned char c) {
					return static_cast<char>(std::tolower(c));
				}
			);
			return text;
		}

		bool TryExtractStateCueSuffix(
			const std::string_view cueId,
			const std::string_view prefix,
			std::string&           outSuffix
		) {
			if (!cueId.starts_with(prefix)) {
				return false;
			}
			const std::string suffix = ToLowerAscii(
				TrimAscii(cueId.substr(prefix.size()))
			);
			if (suffix.empty()) {
				return false;
			}
			outSuffix = suffix;
			return true;
		}
	}

	void CharacterPresentationComponent::OnAttached() {
		mAnimation = ResolveAnimation();
		mCameraFx  = ResolveCameraFx();
		mAudioFx   = ResolveAudioFx();
		(void)LoadProfile();
		SubscribeAll();
	}

	void CharacterPresentationComponent::OnDetached() {
		UnsubscribeAll();
		mAnimation = nullptr;
		mCameraFx  = nullptr;
		mAudioFx   = nullptr;
		mStateEnterTimeSeconds.clear();
	}

	void CharacterPresentationComponent::OnRenderTick(
		const float renderDeltaTime,
		const float interpolationAlpha
	) {
		(void)interpolationAlpha;
		mElapsedSeconds += std::max(0.0f, renderDeltaTime);
		if (!mAnimation) {
			mAnimation = ResolveAnimation();
			if (mAnimation) {
				ValidateResolvedRules();
			}
		}
		if (!mCameraFx) {
			mCameraFx = ResolveCameraFx();
			if (mCameraFx) {
				RebuildUpdateFovWaitMap();
				ValidateResolvedRules();
			}
		}
		if (!mAudioFx) {
			mAudioFx = ResolveAudioFx();
			if (mAudioFx) {
				ValidateResolvedRules();
			}
		}
		RefreshProfileIfNeeded();
	}

	std::string_view CharacterPresentationComponent::GetStableName() const {
		return "game.CharacterPresentation";
	}

	std::string_view CharacterPresentationComponent::GetComponentName() const {
		return "CharacterPresentation";
	}

	uint32_t CharacterPresentationComponent::GetIcon() const {
		return kIconAccessibility;
	}

#ifdef _DEBUG
	void CharacterPresentationComponent::DrawInspectorImGui() {
		Entity* owner            = GetOwner();
		World*  world            = GetWorld();
		bool    needsResubscribe = false;
		if (!mAnimation) {
			mAnimation = ResolveAnimation();
		}
		if (!mCameraFx) {
			mCameraFx = ResolveCameraFx();
		}
		if (!mAudioFx) {
			mAudioFx = ResolveAudioFx();
		}

		ImGui::Text(
			"Owner GUID: %llu",
			static_cast<unsigned long long>(owner ? owner->GetGuid() : 0)
		);
		ImGui::Text(
			"Owner Name: %s", owner ? owner->GetName().data() : "(null)"
		);
		ImGui::Text(
			"Cue Source GUID (effective): %llu",
			static_cast<unsigned long long>(ResolveCueSourceEntityGuid())
		);
		const uint64_t effectiveAnimGuid = mAnimationEntityGuid != 0 ?
			                                   mAnimationEntityGuid :
			                                   (owner ? owner->GetGuid() : 0);
		const uint64_t effectiveCameraFxGuid = mCameraFxEntityGuid != 0 ?
			                                       mCameraFxEntityGuid :
			                                       (owner ?
				                                        owner->GetGuid() :
				                                        0);
		ImGui::Text(
			"Animation Entity GUID (effective): %llu",
			static_cast<unsigned long long>(effectiveAnimGuid)
		);
		ImGui::Text(
			"CameraFx Entity GUID (effective): %llu",
			static_cast<unsigned long long>(effectiveCameraFxGuid)
		);
		ImGui::Text("Profile Path: %s", mProfilePath.c_str());
		ImGui::Text(
			"Profile Asset: %s", mProfileAssetId != kInvalidAssetID ?
				                     "Connected" :
				                     "Missing"
		);
		ImGui::Text("Rule Count: %d", static_cast<int>(mRules.size()));
		ImGui::Text("Subscriptions: %d", static_cast<int>(mCueHandles.size()));

		if (EditEntityGuidField(
			"Cue Source Entity GUID", mCueSourceEntityGuid
		)) {
			needsResubscribe = true;
		}
		if (EditEntityGuidField(
			"Animation Entity GUID", mAnimationEntityGuid
		)) {
			mAnimation = ResolveAnimation();
			ValidateResolvedRules();
		}
		if (EditEntityGuidField("CameraFx Entity GUID", mCameraFxEntityGuid)) {
			mCameraFx = ResolveCameraFx();
			mAudioFx  = ResolveAudioFx();
			RebuildUpdateFovWaitMap();
			ValidateResolvedRules();
		}
		ImGui::TextUnformatted("GUID=0 means owner entity.");
		if (ImGui::Button("Use Owner As Cue Source")) {
			mCueSourceEntityGuid = 0;
			needsResubscribe     = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("Use Owner As Targets")) {
			mAnimationEntityGuid = 0;
			mCameraFxEntityGuid  = 0;
			mAnimation           = ResolveAnimation();
			mCameraFx            = ResolveCameraFx();
			mAudioFx             = ResolveAudioFx();
			RebuildUpdateFovWaitMap();
			ValidateResolvedRules();
		}

		std::string profilePath = mProfilePath;
		if (
			ImGuiWidgets::AssetPathPicker(
				"Presentation Profile",
				profilePath,
				ImGuiWidgets::AssetTypeToMask(ASSET_TYPE::PRESENTATION_PROFILE)
			)
		) {
			SetProfilePath(profilePath);
			(void)LoadProfile();
			needsResubscribe = true;
		}
		if (ImGui::Button("Reload Profile")) {
			(void)LoadProfile();
			needsResubscribe = true;
		}

		ImGui::Text(
			"SkeletalAnimation: %s", mAnimation ? "Connected" : "Missing"
		);
		ImGui::Text(
			"CameraFxController: %s", mCameraFx ? "Connected" : "Missing"
		);
		ImGui::Text(
			"AudioFxController: %s", mAudioFx ? "Connected" : "Missing"
		);
		if (!world || !owner) {
			ImGui::TextUnformatted("World/Owner is missing.");
		}

		ImGui::SeparatorText("Debug Receive");
		ImGui::Text(
			"Handled Cues: %llu",
			static_cast<unsigned long long>(mDebugHandledCueCount)
		);
		ImGui::Text("Last Cue ID: %s", mDebugLastCueId.c_str());
		ImGui::Text(
			"Last Source GUID: %llu",
			static_cast<unsigned long long>(mDebugLastCueSourceGuid)
		);
		ImGui::Text(
			"Last Value: %.3f / %.3f", mDebugLastCueValue, mDebugLastCueValue2
		);
		ImGui::Text("Last Cue Time: %.3f sec", mDebugLastCueAt);
		ImGui::Text(
			"Last PlayState: %s (%s)",
			mDebugLastPlayStateId.c_str(),
			mDebugLastPlayStateOk ? "OK" : "FAILED"
		);
		ImGui::Text(
			"Last AudioPreset: %s (%s)",
			mDebugLastAudioPresetId.c_str(),
			mDebugLastAudioTriggerOk ? "OK" : "FAILED"
		);

		ImGui::SeparatorText("Debug Publish");
		(void)EditStringField("Publish Cue ID", mDebugPublishCueId);
		(void)ImGui::DragFloat(
			"Publish Value", &mDebugPublishValue, 0.01f, -10000.0f, 10000.0f,
			"%.3f"
		);
		(void)ImGui::DragFloat(
			"Publish Value2",
			&mDebugPublishValue2,
			0.01f,
			-10000.0f,
			10000.0f,
			"%.3f"
		);
		if (ImGui::Button("Publish Test Cue")) {
			if (world && !mDebugPublishCueId.empty()) {
				GameplayCue cue      = {};
				cue.id               = mDebugPublishCueId;
				cue.sourceEntityGuid = ResolveCueSourceEntityGuid();
				cue.value            = mDebugPublishValue;
				cue.value2           = mDebugPublishValue2;
				if (cue.sourceEntityGuid != 0) {
					world->GetGameplayCueBus().Publish(cue);
				}
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Rebind Subscriptions")) {
			needsResubscribe = true;
		}

		if (needsResubscribe) {
			SubscribeAll();
		}
	}
#endif

	void CharacterPresentationComponent::Deserialize(const JsonReader& reader) {
		mCueSourceEntityGuid = 0;
		mAnimationEntityGuid = 0;
		mCameraFxEntityGuid  = 0;
		mStateEnterTimeSeconds.clear();
		SetProfilePath("");

		if (reader.Has("profilePath")) {
			SetProfilePath(reader["profilePath"].GetString(""));
		}
		if (reader.Has("cueSourceEntityGuid")) {
			mCueSourceEntityGuid = reader["cueSourceEntityGuid"].GetUint64();
		}
		if (reader.Has("animationEntityGuid")) {
			mAnimationEntityGuid = reader["animationEntityGuid"].GetUint64();
		}
		if (reader.Has("cameraFxEntityGuid")) {
			mCameraFxEntityGuid = reader["cameraFxEntityGuid"].GetUint64();
		}

		mAnimation = ResolveAnimation();
		mCameraFx  = ResolveCameraFx();
		mAudioFx   = ResolveAudioFx();
		(void)LoadProfile();
		SubscribeAll();
	}

	void CharacterPresentationComponent::Serialize(JsonWriter& writer) const {
		writer.Key("profilePath");
		writer.Write(mProfilePath);
		writer.Key("cueSourceEntityGuid");
		writer.Write(mCueSourceEntityGuid);
		writer.Key("animationEntityGuid");
		writer.Write(mAnimationEntityGuid);
		writer.Key("cameraFxEntityGuid");
		writer.Write(mCameraFxEntityGuid);
	}

	void CharacterPresentationComponent::SubscribeAll() {
		UnsubscribeAll();

		World* world = GetWorld();
		if (!world) {
			return;
		}

		std::unordered_set<std::string> uniqueCueIds;
		uniqueCueIds.reserve(mRules.size());
		for (const RuntimeRule& rule : mRules) {
			if (!rule.cueId.empty()) {
				uniqueCueIds.emplace(rule.cueId);
			}
		}

		mCueHandles.reserve(uniqueCueIds.size());
		const uint64_t sourceEntityGuid = ResolveCueSourceEntityGuid();
		if (sourceEntityGuid == 0) {
			return;
		}
		for (const std::string& cueId : uniqueCueIds) {
			GameplayCueFilter filter = {};
			filter.cueId             = cueId;
			filter.sourceEntityGuid  = sourceEntityGuid;
			const auto handle        = world->GetGameplayCueBus().Subscribe(
				filter,
				[this](const GameplayCue& cue) {
					HandleCue(cue);
				}
			);
			if (handle != 0) {
				mCueHandles.emplace_back(handle);
			}
		}
	}

	void CharacterPresentationComponent::UnsubscribeAll() {
		if (World* world = GetWorld()) {
			for (const GameplayCueBus::Handle handle : mCueHandles) {
				(void)world->GetGameplayCueBus().Unsubscribe(handle);
			}
		}
		mCueHandles.clear();
	}

	void CharacterPresentationComponent::HandleCue(const GameplayCue& cue) {
		if (!mAnimation) {
			mAnimation = ResolveAnimation();
		}
		if (!mCameraFx) {
			mCameraFx = ResolveCameraFx();
		}
		if (!mAudioFx) {
			mAudioFx = ResolveAudioFx();
		}

#ifdef _DEBUG
		mDebugLastCueId         = cue.id;
		mDebugLastCueSourceGuid = cue.sourceEntityGuid;
		mDebugLastCueValue      = cue.value;
		mDebugLastCueValue2     = cue.value2;
		mDebugLastCueAt         = mElapsedSeconds;
		++mDebugHandledCueCount;
#endif

		std::string enteredStateSuffix;
		if (
			TryExtractStateCueSuffix(
				cue.id, kCueStateEnterPrefix, enteredStateSuffix
			)
		) {
			mStateEnterTimeSeconds[enteredStateSuffix] = mElapsedSeconds;
		}

		const float cueScale = cue.value > 0.0f ? cue.value : 1.0f;
		for (RuntimeRule& rule : mRules) {
			if (rule.cueId != cue.id) {
				continue;
			}
			if (cue.value < rule.minValue || cue.value > rule.maxValue) {
				continue;
			}
			if (
				rule.cooldownSec > 0.0f &&
				(mElapsedSeconds - rule.lastTriggerAt) < rule.cooldownSec
			) {
				continue;
			}

			for (const RuntimeOutput& output : rule.outputs) {
				switch (output.type) {
					case OutputType::AnimationState: if (mAnimation) {
							const bool played = mAnimation->PlayState(
								output.id
							);
#ifdef _DEBUG
							mDebugLastPlayStateId = output.id;
							mDebugLastPlayStateOk = played;
#else
							(void)played;
#endif
						}
						break;
					case OutputType::AnimationSpeed: if (mAnimation) {
							mAnimation->SetSpeed(
								std::max(0.0f, output.scale * cueScale)
							);
						}
						break;
					case OutputType::CameraShake: if (mCameraFx) {
							const float intensity = std::max(
								0.0f, output.scale * cueScale
							);
							if (intensity > 0.0f) {
								mCameraFx->TriggerShake(output.id, intensity);
							}
						}
						break;
					case OutputType::CameraFov: if (mCameraFx) {
							if (
								output.enterFovInSec > 0.0f &&
								!output.stateCueSuffix.empty()
							) {
								const auto enterIt = mStateEnterTimeSeconds.
									find(
										output.stateCueSuffix
									);
								if (
									enterIt == mStateEnterTimeSeconds.end() ||
									(mElapsedSeconds - enterIt->second) <
									output.enterFovInSec
								) {
									continue;
								}
							}
							const float intensity = std::max(
								0.0f, output.scale * cueScale
							);
							if (intensity > 0.0f) {
								mCameraFx->TriggerFov(output.id, intensity);
							}
						}
						break;
					case OutputType::CameraRotation: if (mCameraFx) {
							const float intensity = std::max(
								0.0f, output.scale * cueScale
							);
							if (intensity > 0.0f) {
								mCameraFx->TriggerRotation(
									output.id, intensity
								);
							}
						}
						break;
					case OutputType::AudioPlay: if (mAudioFx) {
							const float intensity = std::max(
								0.0f, output.scale * cueScale
							);
							const bool triggered = intensity > 0.0f &&
							                       mAudioFx->TriggerOneShot(
								                       output.id, intensity
							                       );
#ifdef _DEBUG
							mDebugLastAudioPresetId  = output.id;
							mDebugLastAudioTriggerOk = triggered;
#else
							(void)triggered;
#endif
						}
						break;
					case OutputType::Unknown:
					default: break;
				}
			}

			rule.lastTriggerAt = mElapsedSeconds;
		}
	}

	void CharacterPresentationComponent::SetProfilePath(
		const std::string& path
	) {
		const std::string normalized = path.empty() ?
			                               std::string() :
			                               StrUtil::NormalizePath(path);
		if (mProfilePath == normalized) {
			return;
		}
		mProfilePath          = normalized;
		mProfileAssetId       = kInvalidAssetID;
		mLoadedProfileVersion = 0;
		mRules.clear();
		mStateEnterTimeSeconds.clear();
	}

	bool CharacterPresentationComponent::LoadProfile() {
		auto parseOutputType = [](const std::string_view typeName) {
			const std::string normalized = ToLowerAscii(TrimAscii(typeName));
			if (normalized == "animation.state") {
				return OutputType::AnimationState;
			}
			if (normalized == "animation.speed") {
				return OutputType::AnimationSpeed;
			}
			if (normalized == "camera.shake") {
				return OutputType::CameraShake;
			}
			if (normalized == "camera.fov") {
				return OutputType::CameraFov;
			}
			if (normalized == "camera.rotation") {
				return OutputType::CameraRotation;
			}
			if (normalized == "audio.play") {
				return OutputType::AudioPlay;
			}
			return OutputType::Unknown;
		};

		mRules.clear();
		mProfileAssetId       = kInvalidAssetID;
		mLoadedProfileVersion = 0;

		if (mProfilePath.empty()) {
			return false;
		}

		AssetManager* assetManager = ServiceLocator::Get<AssetManager>();
		if (!assetManager) {
			return false;
		}

		const AssetID profileAssetId = assetManager->LoadFromFile(
			mProfilePath, ASSET_TYPE::PRESENTATION_PROFILE
		);
		if (profileAssetId == kInvalidAssetID) {
			return false;
		}

		const auto* profileData = assetManager->Get<
			PresentationProfileAssetData>(
			profileAssetId
		);
		if (!profileData) {
			return false;
		}

		mProfileAssetId       = profileAssetId;
		mLoadedProfileVersion = assetManager->Meta(profileAssetId).version;
		mRules.reserve(profileData->rules.size());

		for (const PresentationRuleAssetData& assetRule : profileData->rules) {
			RuntimeRule rule = {};
			rule.cueId       = TrimAscii(assetRule.cueId);
			if (rule.cueId.empty()) {
				continue;
			}
			rule.minValue    = assetRule.minValue;
			rule.maxValue    = assetRule.maxValue;
			rule.cooldownSec = std::max(0.0f, assetRule.cooldownSec);
			if (rule.maxValue < rule.minValue) {
				std::swap(rule.minValue, rule.maxValue);
			}

			for (const PresentationOutputAssetData& assetOutput : assetRule.
			     outputs) {
				RuntimeOutput output = {};
				output.typeName      = TrimAscii(assetOutput.type);
				output.type          = parseOutputType(output.typeName);
				output.id            = TrimAscii(assetOutput.id);
				output.scale         = assetOutput.scale;
				output.enterFovInSec = 0.0f;
				output.stateCueSuffix.clear();
				if (output.id.empty()) {
					continue;
				}
				if (
					output.type != OutputType::AnimationState &&
					output.type != OutputType::AnimationSpeed
				) {
					output.scale = std::max(0.0f, output.scale);
				}
				rule.outputs.emplace_back(std::move(output));
			}

			if (!rule.outputs.empty()) {
				mRules.emplace_back(std::move(rule));
			}
		}

		RebuildUpdateFovWaitMap();
		ValidateResolvedRules();
		return true;
	}

	void CharacterPresentationComponent::RefreshProfileIfNeeded() {
		if (mProfilePath.empty()) {
			if (mProfileAssetId != kInvalidAssetID || !mRules.empty()) {
				mRules.clear();
				mProfileAssetId       = kInvalidAssetID;
				mLoadedProfileVersion = 0;
				SubscribeAll();
			}
			return;
		}

		AssetManager* assetManager = ServiceLocator::Get<AssetManager>();
		if (!assetManager) {
			return;
		}

		bool needsReload;
		if (mProfileAssetId == kInvalidAssetID) {
			needsReload = true;
		} else {
			const auto& meta = assetManager->Meta(mProfileAssetId);
			needsReload = !meta.loaded || meta.version != mLoadedProfileVersion;
		}

		if (needsReload) {
			(void)LoadProfile();
			SubscribeAll();
		}
	}

	void CharacterPresentationComponent::RebuildUpdateFovWaitMap() {
		for (RuntimeRule& rule : mRules) {
			for (RuntimeOutput& output : rule.outputs) {
				output.enterFovInSec = 0.0f;
				output.stateCueSuffix.clear();
			}
		}
		if (!mCameraFx) {
			return;
		}

		std::unordered_map<std::string, float> enterFovWaitByState;
		enterFovWaitByState.reserve(mRules.size());
		for (const RuntimeRule& rule : mRules) {
			std::string stateSuffix;
			if (
				!TryExtractStateCueSuffix(
					rule.cueId, kCueStateEnterPrefix, stateSuffix
				)
			) {
				continue;
			}

			for (const RuntimeOutput& output : rule.outputs) {
				if (output.type != OutputType::CameraFov) {
					continue;
				}

				float enterInSec = 0.0f;
				if (!mCameraFx->GetFovPresetInSec(output.id, enterInSec)) {
					continue;
				}

				const auto it = enterFovWaitByState.find(stateSuffix);
				if (it == enterFovWaitByState.end()) {
					enterFovWaitByState.emplace(stateSuffix, enterInSec);
				} else {
					it->second = std::max(it->second, enterInSec);
				}
			}
		}

		for (RuntimeRule& rule : mRules) {
			std::string stateSuffix;
			if (
				!TryExtractStateCueSuffix(
					rule.cueId, kCueStateUpdatePrefix, stateSuffix
				)
			) {
				continue;
			}
			const auto enterWaitIt = enterFovWaitByState.find(stateSuffix);
			if (enterWaitIt == enterFovWaitByState.end()) {
				continue;
			}

			for (RuntimeOutput& output : rule.outputs) {
				if (output.type != OutputType::CameraFov) {
					continue;
				}
				output.stateCueSuffix = stateSuffix;
				output.enterFovInSec  = std::max(0.0f, enterWaitIt->second);
			}
		}
	}

	void CharacterPresentationComponent::ValidateResolvedRules() const {
		for (const RuntimeRule& rule : mRules) {
			for (const RuntimeOutput& output : rule.outputs) {
				switch (output.type) {
					case OutputType::AnimationState: if (
							mAnimation && !mAnimation->HasState(output.id)) {
							Warning(
								kChannel,
								"Missing animation state '{}' for cue '{}'.",
								output.id,
								rule.cueId
							);
						}
						break;
					case OutputType::AnimationSpeed: break;
					case OutputType::CameraShake: if (
							mCameraFx && !mCameraFx->HasShakePreset(
								output.id
							)) {
							Warning(
								kChannel,
								"Missing shake preset '{}' for cue '{}'.",
								output.id,
								rule.cueId
							);
						}
						break;
					case OutputType::CameraFov: if (
							mCameraFx && !mCameraFx->HasFovPreset(output.id)) {
							Warning(
								kChannel,
								"Missing fov preset '{}' for cue '{}'.",
								output.id,
								rule.cueId
							);
						}
						break;
					case OutputType::CameraRotation: if (
							mCameraFx && !mCameraFx->HasRotationPreset(
								output.id
							)) {
							Warning(
								kChannel,
								"Missing rotation preset '{}' for cue '{}'.",
								output.id,
								rule.cueId
							);
						}
						break;
					case OutputType::AudioPlay: if (
							mAudioFx && !mAudioFx->HasPreset(output.id)) {
							Warning(
								kChannel,
								"Missing audio preset '{}' for cue '{}'.",
								output.id,
								rule.cueId
							);
						}
						break;
					case OutputType::Unknown:
					default:
						Warning(
							kChannel,
							"Unknown presentation output type '{}' for cue '{}'.",
							output.typeName,
							rule.cueId
						);
						break;
				}
			}
		}
	}

	uint64_t
	CharacterPresentationComponent::ResolveCueSourceEntityGuid() const {
		if (mCueSourceEntityGuid != 0) {
			return mCueSourceEntityGuid;
		}
		const Entity* owner = GetOwner();
		return owner ? owner->GetGuid() : 0;
	}

	SkeletalAnimationComponent*
	CharacterPresentationComponent::ResolveAnimation(
	) {
		Entity* target = nullptr;
		if (mAnimationEntityGuid != 0) {
			World* world = GetWorld();
			Scene* scene = world ? world->GetScenePtr() : nullptr;
			target = scene ? scene->FindEntity(mAnimationEntityGuid) : nullptr;
		}
		if (!target) {
			target = GetOwner();
		}
		return target ?
			       target->GetComponent<SkeletalAnimationComponent>() :
			       nullptr;
	}

	CameraFxControllerComponent*
	CharacterPresentationComponent::ResolveCameraFx(
	) {
		Entity* target = nullptr;
		if (mCameraFxEntityGuid != 0) {
			World* world = GetWorld();
			Scene* scene = world ? world->GetScenePtr() : nullptr;
			target = scene ? scene->FindEntity(mCameraFxEntityGuid) : nullptr;
		}
		if (!target) {
			target = GetOwner();
		}
		return target ?
			       target->GetComponent<CameraFxControllerComponent>() :
			       nullptr;
	}

	AudioFxControllerComponent*
	CharacterPresentationComponent::ResolveAudioFx() {
		Entity* target = nullptr;
		if (mCameraFxEntityGuid != 0) {
			World* world = GetWorld();
			Scene* scene = world ? world->GetScenePtr() : nullptr;
			target = scene ? scene->FindEntity(mCameraFxEntityGuid) : nullptr;
		}
		if (!target) {
			target = GetOwner();
		}
		return target ?
			       target->GetComponent<AudioFxControllerComponent>() :
			       nullptr;
	}

	REGISTER_COMPONENT(CharacterPresentationComponent);
}
