#include "CylinderTriggerComponent.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <imgui.h>

#include "../TransformComponent.h"

#include "core/ComponentRegistry.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"
#include "core/math/Mat4.h"
#include "core/string/StrUtil.h"

#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/scene/Scene.h"
#include "engine/unnamed/subsystem/console/Log.h"

namespace Unnamed {
	namespace {
		[[nodiscard]] std::string NormalizeTag(std::string_view tag) {
			return StrUtil::TrimSpaces(std::string(tag));
		}

		void ReadGuidArrayUnique(
			const JsonReader&         reader,
			const char*               key,
			std::vector<uint64_t>& outGuids
		) {
			outGuids.clear();
			const JsonReader values = reader[key];
			if (!values.Valid() || !values.IsArray()) {
				return;
			}

			for (size_t i = 0; i < values.Size(); ++i) {
				const JsonReader value = values[i];
				if (!value.Valid()) {
					continue;
				}

				const auto guid = value.TryGetUint64();
				if (!guid.has_value()) {
					continue;
				}

				if (std::ranges::find(outGuids, guid.value()) != outGuids.end()) {
					continue;
				}
				outGuids.emplace_back(guid.value());
			}
		}

		struct CylinderShape {
			Vec3  center = Vec3::zero;
			float radius = 0.0f;
			float height = 0.0f;
		};

		[[nodiscard]] bool IsInsideCylinder(
			const CylinderShape& shape, const Vec3& point
		) {
			if (shape.radius <= 0.0f || shape.height <= 0.0f) {
				return false;
			}

			const float halfHeight = shape.height * 0.5f;
			const float yMin       = shape.center.y - halfHeight;
			const float yMax       = shape.center.y + halfHeight;
			if (point.y < yMin || point.y > yMax) {
				return false;
			}

			const float dx         = point.x - shape.center.x;
			const float dz         = point.z - shape.center.z;
			const float radiusSq   = shape.radius * shape.radius;
			const float distanceSq = dx * dx + dz * dz;
			return distanceSq <= radiusSq;
		}

		[[nodiscard]] bool TryGetWorldPosition(
			const Entity& entity, Vec3& outPosition
		) {
			const TransformComponent* transform = entity.GetComponent<
				TransformComponent>();
			if (!transform) {
				return false;
			}

			Mat4 world = transform->GetWorldMat();
			outPosition = world.GetTranslate();
			return true;
		}
	}

	void CylinderTriggerComponent::OnTick(const float deltaTime) {
		BaseComponent::OnTick(deltaTime);
		RunDetection();
	}

	BaseComponent::TICK_GROUP CylinderTriggerComponent::GetTickGroup() const {
		return TICK_GROUP::GAMEPLAY;
	}

	std::string_view CylinderTriggerComponent::GetStableName() const {
		return "engine.CylinderTrigger";
	}

	std::string_view CylinderTriggerComponent::GetComponentName() const {
		return "CylinderTrigger";
	}

#ifdef _DEBUG
	void CylinderTriggerComponent::DrawInspectorImGui() {
		ImGui::DragFloat("Radius", &mRadius, 0.05f, 0.0f, 1000.0f, "%.2f");
		ImGui::DragFloat("Height", &mHeight, 0.05f, 0.0f, 1000.0f, "%.2f");

		std::array<char, 128> targetTagBuffer = {};
		const size_t copyLength = std::min(
			mTargetTag.size(), targetTagBuffer.size() - 1
		);
		std::copy_n(mTargetTag.data(), copyLength, targetTagBuffer.data());
		if (
			ImGui::InputText(
				"Target Tag",
				targetTagBuffer.data(),
				targetTagBuffer.size()
			)
		) {
			mTargetTag = NormalizeTag(targetTagBuffer.data());
		}

		ImGui::Checkbox("Debug Log Enabled", &mDebugLogEnabled);
		ImGui::Text("Current overlaps: %zu", mPreviousDetectedGuids.size());
		ImGui::Text(
			"Enter Activate Targets: %zu", mEnterActivateEntityGuids.size()
		);
		ImGui::Text(
			"Exit Deactivate Targets: %zu",
			mExitDeactivateEntityGuids.size()
		);
	}
#endif

	void CylinderTriggerComponent::Deserialize(const JsonReader& reader) {
		const JsonReader radius = reader["radius"];
		if (radius.Valid()) {
			mRadius = radius.GetFloat(mRadius);
		}

		const JsonReader height = reader["height"];
		if (height.Valid()) {
			mHeight = height.GetFloat(mHeight);
		}

		const JsonReader targetTag = reader["targetTag"];
		if (targetTag.Valid()) {
			mTargetTag = NormalizeTag(targetTag.GetString(mTargetTag));
		}

		const JsonReader debugLogEnabled = reader["debugLogEnabled"];
		if (debugLogEnabled.Valid()) {
			mDebugLogEnabled = debugLogEnabled.GetBool(mDebugLogEnabled);
		}

		ReadGuidArrayUnique(
			reader,
			"enterActivateEntityGuids",
			mEnterActivateEntityGuids
		);
		ReadGuidArrayUnique(
			reader,
			"exitDeactivateEntityGuids",
			mExitDeactivateEntityGuids
		);
	}

	void CylinderTriggerComponent::Serialize(JsonWriter& writer) const {
		writer.Key("radius");
		writer.Write(mRadius);
		writer.Key("height");
		writer.Write(mHeight);
		writer.Key("targetTag");
		writer.Write(mTargetTag);
		writer.Key("debugLogEnabled");
		writer.Write(mDebugLogEnabled);
		writer.Key("enterActivateEntityGuids");
		writer.BeginArray();
		for (const uint64_t guid : mEnterActivateEntityGuids) {
			writer.Write(guid);
		}
		writer.EndArray();
		writer.Key("exitDeactivateEntityGuids");
		writer.BeginArray();
		for (const uint64_t guid : mExitDeactivateEntityGuids) {
			writer.Write(guid);
		}
		writer.EndArray();
	}

	void CylinderTriggerComponent::RunDetection() {
		Entity* owner = GetOwner();
		Scene*  scene = GetScene();
		if (!owner || !scene) {
			mPreviousDetectedGuids.clear();
			return;
		}

		Vec3 ownerPosition = Vec3::zero;
		if (!TryGetWorldPosition(*owner, ownerPosition)) {
			mPreviousDetectedGuids.clear();
			return;
		}

		const std::string normalizedTargetTag = NormalizeTag(mTargetTag);
		if (normalizedTargetTag.empty()) {
			mPreviousDetectedGuids.clear();
			return;
		}

		CylinderShape shape;
		shape.center = ownerPosition;
		shape.radius = mRadius;
		shape.height = mHeight;

		std::unordered_set<uint64_t> currentDetected;
		for (Entity* candidate : scene->FindEntitiesByTag(normalizedTargetTag)) {
			if (!candidate || candidate == owner) {
				continue;
			}

			Vec3 candidatePosition = Vec3::zero;
			if (!TryGetWorldPosition(*candidate, candidatePosition)) {
				continue;
			}

			if (!IsInsideCylinder(shape, candidatePosition)) {
				continue;
			}

			currentDetected.emplace(candidate->GetGuid());
		}

		const bool wasOccupied = !mPreviousDetectedGuids.empty();
		const bool isOccupied  = !currentDetected.empty();
		if (!wasOccupied && isOccupied) {
			ExecuteEntityActivationActions(
				mEnterActivateEntityGuids,
				true,
				"enterActivateEntityGuids"
			);
		}
		if (wasOccupied && !isOccupied) {
			ExecuteEntityActivationActions(
				mExitDeactivateEntityGuids,
				false,
				"exitDeactivateEntityGuids"
			);
		}

		EmitTransitionLogs(currentDetected);
		mPreviousDetectedGuids = std::move(currentDetected);
	}

	void CylinderTriggerComponent::ExecuteEntityActivationActions(
		const std::vector<uint64_t>& targetGuids,
		const bool                   activeState,
		const std::string_view       actionName
	) const {
		Scene* scene = GetScene();
		if (!scene) {
			return;
		}

		for (const uint64_t guid : targetGuids) {
			Entity* target = scene->FindEntity(guid);
			if (!target) {
				Warning(
					"CylinderTrigger",
					"{} target entity not found. guid={}",
					actionName,
					guid
				);
				continue;
			}
			target->SetActive(activeState);
		}
	}

	void CylinderTriggerComponent::EmitTransitionLogs(
		const std::unordered_set<uint64_t>& currentDetected
	) const {
		if (!mDebugLogEnabled) {
			return;
		}

		const Entity* owner = GetOwner();
		Scene*        scene = GetScene();
		if (!owner || !scene) {
			return;
		}

		for (const uint64_t guid : currentDetected) {
			const Entity* detected = scene->FindEntity(guid);
			if (!detected) {
				continue;
			}

			if (!mPreviousDetectedGuids.contains(guid)) {
				Msg(
					"CylinderTrigger",
					"[{}] Enter: {} ({})",
					owner->GetName(),
					detected->GetName(),
					guid
				);
			} else {
				DevMsg(
					"CylinderTrigger",
					"[{}] Stay: {} ({})",
					owner->GetName(),
					detected->GetName(),
					guid
				);
			}
		}

		for (const uint64_t guid : mPreviousDetectedGuids) {
			if (currentDetected.contains(guid)) {
				continue;
			}

			const Entity* detected = scene->FindEntity(guid);
			const std::string_view name = detected ?
				                              detected->GetName() :
				                              std::string_view("Unknown");
			Msg(
				"CylinderTrigger",
				"[{}] Exit: {} ({})",
				owner->GetName(),
				name,
				guid
			);
		}
	}

	REGISTER_COMPONENT(CylinderTriggerComponent);
}
