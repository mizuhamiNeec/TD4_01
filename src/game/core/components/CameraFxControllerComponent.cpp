#include "CameraFxControllerComponent.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>
#include <cstring>

#ifdef _DEBUG
#include <imgui.h>
#endif

#include "core/ComponentRegistry.h"
#include "core/json/JsonReader.h"
#include "core/json/JsonWriter.h"
#include "core/math/Math.h"

#include "engine/ImGui/Icons.h"
#include "engine/scene/Scene.h"
#include "engine/tween/TweenEase.h"
#include "engine/unnamed/framework/components/CameraComponent.h"
#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/world/World.h"

namespace Unnamed {
	namespace {
		struct EaseNamePair {
			EASE_TYPE        type = EASE_TYPE::LINEAR;
			std::string_view name = "LINEAR";
		};

		constexpr EaseNamePair kEaseNames[] = {
			{EASE_TYPE::LINEAR, "LINEAR"},
			{EASE_TYPE::IN_SINE, "IN_SINE"},
			{EASE_TYPE::OUT_SINE, "OUT_SINE"},
			{EASE_TYPE::IN_OUT_SINE, "IN_OUT_SINE"},
			{EASE_TYPE::IN_QUAD, "IN_QUAD"},
			{EASE_TYPE::OUT_QUAD, "OUT_QUAD"},
			{EASE_TYPE::IN_OUT_QUAD, "IN_OUT_QUAD"},
			{EASE_TYPE::IN_CUBIC, "IN_CUBIC"},
			{EASE_TYPE::OUT_CUBIC, "OUT_CUBIC"},
			{EASE_TYPE::IN_OUT_CUBIC, "IN_OUT_CUBIC"},
			{EASE_TYPE::IN_QUART, "IN_QUART"},
			{EASE_TYPE::OUT_QUART, "OUT_QUART"},
			{EASE_TYPE::IN_OUT_QUART, "IN_OUT_QUART"},
			{EASE_TYPE::IN_QUINT, "IN_QUINT"},
			{EASE_TYPE::OUT_QUINT, "OUT_QUINT"},
			{EASE_TYPE::IN_OUT_QUINT, "IN_OUT_QUINT"},
			{EASE_TYPE::IN_EXPO, "IN_EXPO"},
			{EASE_TYPE::OUT_EXPO, "OUT_EXPO"},
			{EASE_TYPE::IN_OUT_EXPO, "IN_OUT_EXPO"},
			{EASE_TYPE::IN_CIRC, "IN_CIRC"},
			{EASE_TYPE::OUT_CIRC, "OUT_CIRC"},
			{EASE_TYPE::IN_OUT_CIRC, "IN_OUT_CIRC"},
			{EASE_TYPE::IN_BACK, "IN_BACK"},
			{EASE_TYPE::OUT_BACK, "OUT_BACK"},
			{EASE_TYPE::IN_OUT_BACK, "IN_OUT_BACK"},
			{EASE_TYPE::IN_ELASTIC, "IN_ELASTIC"},
			{EASE_TYPE::OUT_ELASTIC, "OUT_ELASTIC"},
			{EASE_TYPE::IN_OUT_ELASTIC, "IN_OUT_ELASTIC"},
			{EASE_TYPE::IN_BOUNCE, "IN_BOUNCE"},
			{EASE_TYPE::OUT_BOUNCE, "OUT_BOUNCE"},
			{EASE_TYPE::IN_OUT_BOUNCE, "IN_OUT_BOUNCE"},
		};

#ifdef _DEBUG
		bool EditStringField(
			const char* label, std::string& value, const size_t capacity = 128
		) {
			std::vector<char> buffer(capacity, '\0');
			const size_t copyLength = std::min(value.size(), capacity - 1);
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
	}

	void CameraFxControllerComponent::OnAttached() {
		mCamera         = ResolveCamera();
		mShakeTransform = ResolveShakeTransform();
		if (mCamera) {
			mBaseFovYDegrees = mCamera->GetFovYDegrees();
		}
	}

	void CameraFxControllerComponent::OnDetached() {
		ResetOutputs();
		mActiveShakes.clear();
		mActiveFovAnim = {};
	}

	void CameraFxControllerComponent::OnTick(const float deltaTime) {
		if (deltaTime <= 0.0f) {
			return;
		}

		mCamera         = ResolveCamera();
		mShakeTransform = ResolveShakeTransform();

		Vec2 shakeOffset = Vec2::zero;
		for (size_t i = 0; i < mActiveShakes.size();) {
			ActiveShake& shake = mActiveShakes[i];
			shake.elapsedSec += deltaTime;
			if (shake.elapsedSec >= shake.durationSec) {
				mActiveShakes.erase(
					mActiveShakes.begin() + static_cast<std::ptrdiff_t>(i)
				);
				continue;
			}

			const float normalizedTime = std::clamp(
				shake.elapsedSec / std::max(shake.durationSec, 1.0e-4f),
				0.0f,
				1.0f
			);
			const float envelope = std::pow(
				1.0f - normalizedTime, std::max(0.01f, shake.decay)
			);
			const float phaseStep = Math::pi * 2.0f * shake.frequencyHz *
			                        deltaTime;
			shake.phasePitch += phaseStep;
			shake.phaseYaw += phaseStep * 0.93f;
			shakeOffset.x += std::sin(shake.phasePitch) * shake.amplitudeDeg.x *
			                 envelope;
			shakeOffset.y += std::sin(shake.phaseYaw) * shake.amplitudeDeg.y *
			                 envelope;
			++i;
		}

		float fovOffset = mCurrentFovOffset;
		if (mActiveFovAnim.active) {
			mActiveFovAnim.elapsedSec += deltaTime;
			const float totalDuration = std::max(
				0.0f, mActiveFovAnim.inSec
			) + std::max(0.0f, mActiveFovAnim.outSec);
			if (mActiveFovAnim.elapsedSec >= totalDuration) {
				mActiveFovAnim.active = false;
				fovOffset             = 0.0f;
			} else if (mActiveFovAnim.elapsedSec <= mActiveFovAnim.inSec) {
				const float t = mActiveFovAnim.inSec > 1.0e-4f ?
					                mActiveFovAnim.elapsedSec / mActiveFovAnim.
					                inSec :
					                1.0f;
				fovOffset = mActiveFovAnim.targetDeltaDeg *
				            TweenEase::Evaluate(
					            mActiveFovAnim.ease, std::clamp(t, 0.0f, 1.0f)
				            );
			} else {
				const float outElapsed = mActiveFovAnim.elapsedSec -
				                         mActiveFovAnim.inSec;
				const float outT = mActiveFovAnim.outSec > 1.0e-4f ?
					                   outElapsed / mActiveFovAnim.outSec :
					                   1.0f;
				const float easedOut = TweenEase::Evaluate(
					mActiveFovAnim.ease, std::clamp(outT, 0.0f, 1.0f)
				);
				fovOffset = mActiveFovAnim.targetDeltaDeg * (1.0f - easedOut);
			}
		} else {
			fovOffset = 0.0f;
		}

		if (mCamera && !mActiveFovAnim.active && std::abs(mCurrentFovOffset) <=
		    1.0e-6f) {
			mBaseFovYDegrees = mCamera->GetFovYDegrees();
		}

		mCurrentLookOffset = shakeOffset;
		mCurrentFovOffset  = fovOffset;
		ApplyOutputs(mCurrentLookOffset, mCurrentFovOffset);
	}

	BaseComponent::TICK_GROUP CameraFxControllerComponent::GetTickGroup() const {
		return TICK_GROUP::LATE;
	}

	void CameraFxControllerComponent::TriggerShake(
		const std::string_view presetId, const float intensityScale
	) {
		const ShakePreset* preset = FindShakePreset(presetId);
		if (!preset || intensityScale <= 0.0f) {
			return;
		}

		ActiveShake shake = {};
		shake.amplitudeDeg = preset->ampPitchYawDeg * intensityScale;
		shake.frequencyHz  = std::max(0.01f, preset->freqHz);
		shake.durationSec  = std::max(0.01f, preset->durationSec);
		shake.decay        = std::max(0.01f, preset->decay);
		shake.phasePitch   = static_cast<float>(mActiveShakes.size()) * 0.73f;
		shake.phaseYaw     = static_cast<float>(mActiveShakes.size()) * 1.37f +
		                 Math::pi * 0.5f;
		mActiveShakes.emplace_back(shake);
	}

	void CameraFxControllerComponent::TriggerFov(
		const std::string_view presetId, const float intensityScale
	) {
		const FovPreset* preset = FindFovPreset(presetId);
		if (!preset || intensityScale <= 0.0f) {
			return;
		}

		mCamera = ResolveCamera();
		if (mCamera) {
			mBaseFovYDegrees = mCamera->GetFovYDegrees() - mCurrentFovOffset;
		}

		mActiveFovAnim.active         = true;
		mActiveFovAnim.elapsedSec     = 0.0f;
		mActiveFovAnim.targetDeltaDeg = preset->deltaDeg * intensityScale;
		mActiveFovAnim.inSec          = std::max(0.0f, preset->inSec);
		mActiveFovAnim.outSec         = std::max(0.0f, preset->outSec);
		mActiveFovAnim.ease           = preset->ease;
	}

	bool CameraFxControllerComponent::HasShakePreset(
		const std::string_view presetId
	) const {
		return FindShakePreset(presetId) != nullptr;
	}

	bool CameraFxControllerComponent::HasFovPreset(
		const std::string_view presetId
	) const {
		return FindFovPreset(presetId) != nullptr;
	}

	bool CameraFxControllerComponent::GetFovPresetInSec(
		const std::string_view presetId, float& outInSec
	) const {
		const FovPreset* preset = FindFovPreset(presetId);
		if (!preset) {
			return false;
		}
		outInSec = std::max(0.0f, preset->inSec);
		return true;
	}

	std::string_view CameraFxControllerComponent::GetStableName() const {
		return "game.CameraFxController";
	}

	std::string_view CameraFxControllerComponent::GetComponentName() const {
		return "CameraFxController";
	}

	uint32_t CameraFxControllerComponent::GetIcon() const {
		return kIconVideoCam;
	}

#ifdef _DEBUG
	void CameraFxControllerComponent::DrawInspectorImGui() {
		mCamera = ResolveCamera();
		mShakeTransform = ResolveShakeTransform();

		Entity* owner = GetOwner();
		ImGui::Text(
			"Owner GUID: %llu",
			static_cast<unsigned long long>(owner ? owner->GetGuid() : 0)
		);
		if (EditEntityGuidField("Camera Entity GUID", mCameraEntityGuid)) {
			mCamera = ResolveCamera();
		}
		if (EditEntityGuidField("Shake Entity GUID", mShakeEntityGuid)) {
			ResetOutputs();
			mShakeTransform = ResolveShakeTransform();
		}
		ImGui::TextUnformatted("GUID=0 means owner entity.");
		if (ImGui::Button("Use Owner As Targets")) {
			mCameraEntityGuid = 0;
			mShakeEntityGuid = 0;
			ResetOutputs();
			mCamera = ResolveCamera();
			mShakeTransform = ResolveShakeTransform();
		}

		ImGui::Text("Camera: %s", mCamera ? "Connected" : "Missing");
		ImGui::Text(
			"Shake Transform: %s", mShakeTransform ? "Connected" : "Missing"
		);
		ImGui::Text("Base FOV Y: %.3f", mBaseFovYDegrees);
		ImGui::Text("Current FOV Offset: %.3f", mCurrentFovOffset);
		ImGui::Text(
			"Current Look Offset: (%.3f, %.3f)",
			mCurrentLookOffset.x,
			mCurrentLookOffset.y
		);
		ImGui::Text(
			"Active Shakes: %d", static_cast<int>(mActiveShakes.size())
		);
		ImGui::Text("FOV Anim Active: %s", mActiveFovAnim.active ? "Yes" : "No");

		if (ImGui::Button("Capture Base FOV")) {
			if (mCamera) {
				mBaseFovYDegrees = mCamera->GetFovYDegrees() - mCurrentFovOffset;
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Reset FX Outputs")) {
			mActiveShakes.clear();
			mActiveFovAnim = {};
			ResetOutputs();
		}

		(void)ImGui::DragFloat(
			"Trigger Intensity", &mDebugTriggerIntensity, 0.01f, 0.0f, 8.0f, "%.2f"
		);

		ImGui::SeparatorText("Shake Presets");
		size_t removeShake = static_cast<size_t>(-1);
		for (size_t i = 0; i < mShakePresets.size(); ++i) {
			ShakePreset& preset = mShakePresets[i];
			ImGui::PushID(static_cast<int>(i));
			ImGui::SeparatorText(("ShakePreset " + std::to_string(i)).c_str());

			(void)EditStringField("ID", preset.id);
			(void)ImGui::DragFloat2(
				"Amp PitchYaw Deg",
				&preset.ampPitchYawDeg.x,
				0.01f,
				-180.0f,
				180.0f,
				"%.2f"
			);
			if (ImGui::DragFloat("Freq Hz", &preset.freqHz, 0.1f, 0.01f, 200.0f, "%.2f")) {
				preset.freqHz = std::max(0.01f, preset.freqHz);
			}
			if (ImGui::DragFloat(
				    "Duration Sec", &preset.durationSec, 0.01f, 0.01f, 20.0f, "%.2f"
			    )) {
				preset.durationSec = std::max(0.01f, preset.durationSec);
			}
			if (ImGui::DragFloat("Decay", &preset.decay, 0.01f, 0.01f, 8.0f, "%.2f")) {
				preset.decay = std::max(0.01f, preset.decay);
			}
			if (ImGui::Button("Trigger Shake")) {
				TriggerShake(preset.id, std::max(0.0f, mDebugTriggerIntensity));
			}
			ImGui::SameLine();
			if (ImGui::Button("Remove")) {
				removeShake = i;
			}
			ImGui::PopID();
		}
		if (removeShake != static_cast<size_t>(-1)) {
			mShakePresets.erase(
				mShakePresets.begin() + static_cast<std::ptrdiff_t>(removeShake)
			);
		}
		if (ImGui::Button("Add Shake Preset")) {
			ShakePreset preset = {};
			preset.id = "new_shake";
			mShakePresets.emplace_back(std::move(preset));
		}

		ImGui::SeparatorText("FOV Presets");
		size_t removeFov = static_cast<size_t>(-1);
		for (size_t i = 0; i < mFovPresets.size(); ++i) {
			FovPreset& preset = mFovPresets[i];
			ImGui::PushID(100000 + static_cast<int>(i));
			ImGui::SeparatorText(("FovPreset " + std::to_string(i)).c_str());

			(void)EditStringField("ID", preset.id);
			(void)ImGui::DragFloat("Delta Deg", &preset.deltaDeg, 0.05f, -120.0f, 120.0f, "%.2f");
			if (ImGui::DragFloat("In Sec", &preset.inSec, 0.01f, 0.0f, 20.0f, "%.2f")) {
				preset.inSec = std::max(0.0f, preset.inSec);
			}
			if (ImGui::DragFloat("Out Sec", &preset.outSec, 0.01f, 0.0f, 20.0f, "%.2f")) {
				preset.outSec = std::max(0.0f, preset.outSec);
			}

			int currentEaseIndex = 0;
			for (size_t easeIndex = 0; easeIndex < std::size(kEaseNames); ++easeIndex) {
				if (kEaseNames[easeIndex].type == preset.ease) {
					currentEaseIndex = static_cast<int>(easeIndex);
					break;
				}
			}
			std::array<const char*, std::size(kEaseNames)> easeNames = {};
			for (size_t easeIndex = 0; easeIndex < std::size(kEaseNames); ++easeIndex) {
				easeNames[easeIndex] = kEaseNames[easeIndex].name.data();
			}
			if (ImGui::Combo(
				    "Ease",
				    &currentEaseIndex,
				    easeNames.data(),
				    static_cast<int>(easeNames.size())
			    )) {
				preset.ease = kEaseNames[currentEaseIndex].type;
			}

			if (ImGui::Button("Trigger FOV")) {
				TriggerFov(preset.id, std::max(0.0f, mDebugTriggerIntensity));
			}
			ImGui::SameLine();
			if (ImGui::Button("Remove")) {
				removeFov = i;
			}
			ImGui::PopID();
		}
		if (removeFov != static_cast<size_t>(-1)) {
			mFovPresets.erase(
				mFovPresets.begin() + static_cast<std::ptrdiff_t>(removeFov)
			);
		}
		if (ImGui::Button("Add FOV Preset")) {
			FovPreset preset = {};
			preset.id = "new_fov";
			mFovPresets.emplace_back(std::move(preset));
		}
	}
#endif

	void CameraFxControllerComponent::Deserialize(const JsonReader& reader) {
		mShakePresets.clear();
		mFovPresets.clear();
		mCameraEntityGuid = 0;
		mShakeEntityGuid = 0;

		if (reader.Has("cameraEntityGuid")) {
			mCameraEntityGuid = reader["cameraEntityGuid"].GetUint64();
		}
		if (reader.Has("shakeEntityGuid")) {
			mShakeEntityGuid = reader["shakeEntityGuid"].GetUint64();
		} else if (reader.Has("rotatorEntityGuid")) {
			// Backward compatibility for older scene data.
			mShakeEntityGuid = reader["rotatorEntityGuid"].GetUint64();
		}

		const JsonReader shakePresets = reader["shakePresets"];
		if (shakePresets.Valid() && shakePresets.IsArray()) {
			for (size_t i = 0; i < shakePresets.Size(); ++i) {
				const JsonReader node = shakePresets[i];
				if (!node.Valid()) {
					continue;
				}

				ShakePreset preset = {};
				if (node.Has("id")) {
					preset.id = node["id"].GetString();
				}
				if (preset.id.empty()) {
					continue;
				}
				if (node.Has("ampPitchYawDeg")) {
					preset.ampPitchYawDeg = node["ampPitchYawDeg"].GetVec2(
						preset.ampPitchYawDeg
					);
				}
				if (node.Has("freqHz")) {
					preset.freqHz = node["freqHz"].GetFloat(preset.freqHz);
				}
				if (node.Has("durationSec")) {
					preset.durationSec = node["durationSec"].GetFloat(
						preset.durationSec
					);
				}
				if (node.Has("decay")) {
					preset.decay = node["decay"].GetFloat(preset.decay);
				}

				preset.freqHz      = std::max(0.01f, preset.freqHz);
				preset.durationSec = std::max(0.01f, preset.durationSec);
				preset.decay       = std::max(0.01f, preset.decay);
				mShakePresets.emplace_back(std::move(preset));
			}
		}

		const JsonReader fovPresets = reader["fovPresets"];
		if (fovPresets.Valid() && fovPresets.IsArray()) {
			for (size_t i = 0; i < fovPresets.Size(); ++i) {
				const JsonReader node = fovPresets[i];
				if (!node.Valid()) {
					continue;
				}

				FovPreset preset = {};
				if (node.Has("id")) {
					preset.id = node["id"].GetString();
				}
				if (preset.id.empty()) {
					continue;
				}
				if (node.Has("deltaDeg")) {
					preset.deltaDeg = node["deltaDeg"].GetFloat(preset.deltaDeg);
				}
				if (node.Has("inSec")) {
					preset.inSec = node["inSec"].GetFloat(preset.inSec);
				}
				if (node.Has("outSec")) {
					preset.outSec = node["outSec"].GetFloat(preset.outSec);
				}
				if (node.Has("ease")) {
					preset.ease = ParseEase(node["ease"]);
				}

				preset.inSec  = std::max(0.0f, preset.inSec);
				preset.outSec = std::max(0.0f, preset.outSec);
				mFovPresets.emplace_back(std::move(preset));
			}
		}
	}

	void CameraFxControllerComponent::Serialize(JsonWriter& writer) const {
		writer.Key("cameraEntityGuid");
		writer.Write(mCameraEntityGuid);
		writer.Key("shakeEntityGuid");
		writer.Write(mShakeEntityGuid);

		writer.Key("shakePresets");
		writer.BeginArray();
		for (const ShakePreset& preset : mShakePresets) {
			writer.BeginObject();
			writer.Key("id");
			writer.Write(preset.id);
			writer.Key("ampPitchYawDeg");
			writer.BeginArray();
			writer.Write(preset.ampPitchYawDeg.x);
			writer.Write(preset.ampPitchYawDeg.y);
			writer.EndArray();
			writer.Key("freqHz");
			writer.Write(preset.freqHz);
			writer.Key("durationSec");
			writer.Write(preset.durationSec);
			writer.Key("decay");
			writer.Write(preset.decay);
			writer.EndObject();
		}
		writer.EndArray();

		writer.Key("fovPresets");
		writer.BeginArray();
		for (const FovPreset& preset : mFovPresets) {
			writer.BeginObject();
			writer.Key("id");
			writer.Write(preset.id);
			writer.Key("deltaDeg");
			writer.Write(preset.deltaDeg);
			writer.Key("inSec");
			writer.Write(preset.inSec);
			writer.Key("outSec");
			writer.Write(preset.outSec);
			writer.Key("ease");
			writer.Write(std::string(EaseToString(preset.ease)));
			writer.EndObject();
		}
		writer.EndArray();
	}

	CameraComponent* CameraFxControllerComponent::ResolveCamera() {
		Entity* target = nullptr;
		if (mCameraEntityGuid != 0) {
			World* world = GetWorld();
			Scene* scene = world ? world->GetScenePtr() : nullptr;
			target = scene ? scene->FindEntity(mCameraEntityGuid) : nullptr;
		}
		if (!target) {
			target = GetOwner();
		}
		return target ? target->GetComponent<CameraComponent>() : nullptr;
	}

	TransformComponent* CameraFxControllerComponent::ResolveShakeTransform() {
		Entity* target = nullptr;
		if (mShakeEntityGuid != 0) {
			World* world = GetWorld();
			Scene* scene = world ? world->GetScenePtr() : nullptr;
			target = scene ? scene->FindEntity(mShakeEntityGuid) : nullptr;
		}
		if (!target) {
			target = GetOwner();
		}
		return target ? target->GetComponent<TransformComponent>() : nullptr;
	}

	const CameraFxControllerComponent::ShakePreset*
	CameraFxControllerComponent::FindShakePreset(
		const std::string_view id
	) const {
		for (const ShakePreset& preset : mShakePresets) {
			if (preset.id == id) {
				return &preset;
			}
		}
		return nullptr;
	}

	const CameraFxControllerComponent::FovPreset*
	CameraFxControllerComponent::FindFovPreset(const std::string_view id) const {
		for (const FovPreset& preset : mFovPresets) {
			if (preset.id == id) {
				return &preset;
			}
		}
		return nullptr;
	}

	EASE_TYPE CameraFxControllerComponent::ParseEase(const JsonReader& reader) {
		if (!reader.Valid()) {
			return EASE_TYPE::LINEAR;
		}

		const std::string text = reader.GetString();
		if (!text.empty()) {
			for (const EaseNamePair& pair : kEaseNames) {
				if (pair.name == text) {
					return pair.type;
				}
			}
		}

		const int idx = reader.GetInt();
		if (idx < 0 || idx >= static_cast<int>(std::size(kEaseNames))) {
			return EASE_TYPE::LINEAR;
		}
		return kEaseNames[idx].type;
	}

	std::string_view CameraFxControllerComponent::EaseToString(
		const EASE_TYPE ease
	) {
		for (const EaseNamePair& pair : kEaseNames) {
			if (pair.type == ease) {
				return pair.name;
			}
		}
		return "LINEAR";
	}

	void CameraFxControllerComponent::ApplyOutputs(
		const Vec2& lookOffsetDeg, const float fovOffsetDeg
	) {
		mShakeTransform = ResolveShakeTransform();
		if (mShakeTransform) {
			const Quaternion offsetRotation = Quaternion::EulerDegrees(
				Vec3(lookOffsetDeg.x, lookOffsetDeg.y, 0.0f)
			);
			const Quaternion deltaRotation =
				mLastAppliedShakeRotation.Inverse() * offsetRotation;
			mShakeTransform->SetRotation(
				mShakeTransform->Rotation() * deltaRotation
			);
			mLastAppliedShakeRotation = offsetRotation;
		} else {
			mLastAppliedShakeRotation = Quaternion::identity;
		}

		mCamera = ResolveCamera();
		if (mCamera) {
			mCamera->SetFovYDegrees(mBaseFovYDegrees + fovOffsetDeg);
		}
	}

	void CameraFxControllerComponent::ResetOutputs() {
		mCurrentLookOffset = Vec2::zero;
		mCurrentFovOffset  = 0.0f;
		ApplyOutputs(Vec2::zero, 0.0f);
	}

	REGISTER_COMPONENT(CameraFxControllerComponent);
}
