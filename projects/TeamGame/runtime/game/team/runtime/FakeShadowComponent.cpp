#include "FakeShadowComponent.h"
#include "./core/ComponentRegistry.h"

#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"

#include <algorithm>
#include <cmath>

#ifdef _DEBUG
#include "imgui.h"
#endif

namespace MyGame {

	void FakeShadowComponent::OnAttached() {
		if (GetOwner()) {
			_transform = GetOwner()->GetComponent<Unnamed::TransformComponent>();
		}
		if (_transform) {
			_baseScale = _transform->GetScale();
		}
		ResolveSourceTransform();
	}

	void FakeShadowComponent::OnTick(float) {
		if (!_transform) {
			return;
		}
		if (!_sourceTransform) {
			ResolveSourceTransform();
		}
		if (!_sourceTransform) {
			return;
		}

		const Vec3 sourcePos = GetWorldPosition(*_sourceTransform);
		const float height = std::max(0.0f, sourcePos.y - _groundY);
		const float heightRate = std::clamp(height / std::max(0.001f, _maxHeight), 0.0f, 1.0f);
		const float scaleRate = _maxScale + (_minScale - _maxScale) * heightRate;

		// NOTE: 親子 Transform の回転を使わず、追従元の XZ だけを影のワールド座標へ反映する。
		// 理由：影は地面に置く板なので、ボールの回転を受けると裏面化や描画消失の原因になる。
		const Vec3 shadowPos(sourcePos.x, _groundY + _shadowYBias, sourcePos.z);
		const Vec3 shadowScale = _baseScale * std::max(0.001f, scaleRate);

		_transform->SetPosition(shadowPos);
		_transform->SetRotation(Quaternion::identity);
		_transform->SetScale(shadowScale);
		_transform->RequestInterpolationResync();
	}

	void FakeShadowComponent::OnDetached() {
		_transform = nullptr;
		_sourceTransform = nullptr;
	}

	std::string_view FakeShadowComponent::GetStableName() const {
		return "mygame.FakeShadow";
	}

	std::string_view FakeShadowComponent::GetComponentName() const {
		return "Fake Shadow";
	}

#ifdef _DEBUG
	void FakeShadowComponent::DrawInspectorImGui() {
		ImGui::Text("=== 仮影 ===");
		ImGui::Text("追従元: %s", _sourceTransform ? "親 Transform" : "なし");
		ImGui::SliderFloat("地面の高さ Y", &_groundY, -10.0f, 10.0f, "%.2f");
		ImGui::SliderFloat("影の浮かせ量", &_shadowYBias, 0.0f, 0.2f, "%.3f");
		ImGui::SliderFloat("影が最小になる高さ", &_maxHeight, 0.1f, 30.0f, "%.2f");
		ImGui::SliderFloat("最小スケール", &_minScale, 0.0f, 1.0f, "%.2f");
		ImGui::SliderFloat("最大スケール", &_maxScale, 0.0f, 3.0f, "%.2f");
		ImGui::DragFloat3("基準スケール", &_baseScale.x, 0.05f, 0.0f, 10.0f, "%.2f");
		ImGui::TextDisabled("影の回転は常に 0, 0, 0 に固定します");
	}
#endif

	void FakeShadowComponent::Deserialize(const Unnamed::JsonReader& reader) {
		if (auto val = reader.Read<float>("groundY")) {
			_groundY = val.value();
		}
		if (auto val = reader.Read<float>("shadowYBias")) {
			_shadowYBias = std::max(0.0f, val.value());
		}
		if (auto val = reader.Read<float>("maxHeight")) {
			_maxHeight = std::max(0.1f, val.value());
		}
		if (auto val = reader.Read<float>("minScale")) {
			_minScale = std::max(0.0f, val.value());
		}
		if (auto val = reader.Read<float>("maxScale")) {
			_maxScale = std::max(0.0f, val.value());
		}
		if (auto val = reader.Read<float>("baseScaleX")) {
			_baseScale.x = val.value();
		}
		if (auto val = reader.Read<float>("baseScaleY")) {
			_baseScale.y = val.value();
		}
		if (auto val = reader.Read<float>("baseScaleZ")) {
			_baseScale.z = val.value();
		}
	}

	void FakeShadowComponent::Serialize(Unnamed::JsonWriter& writer) const {
		writer.Key("groundY");
		writer.Write(_groundY);
		writer.Key("shadowYBias");
		writer.Write(_shadowYBias);
		writer.Key("maxHeight");
		writer.Write(_maxHeight);
		writer.Key("minScale");
		writer.Write(_minScale);
		writer.Key("maxScale");
		writer.Write(_maxScale);
		writer.Key("baseScaleX");
		writer.Write(_baseScale.x);
		writer.Key("baseScaleY");
		writer.Write(_baseScale.y);
		writer.Key("baseScaleZ");
		writer.Write(_baseScale.z);
	}

	void FakeShadowComponent::ResolveSourceTransform() {
		if (!_transform) {
			_sourceTransform = nullptr;
			return;
		}

		_sourceTransform = _transform->GetParent();
		if (_sourceTransform) {
			// NOTE: 親は追従元としてだけ使い、Transform 階層からは外す。
			// 理由：親の回転・スケールを受けると、影の Rotate 0,0,0 を保てなくなる。
			_transform->SetParent(nullptr, true);
			_transform->SetRotation(Quaternion::identity);
		}
	}

	Vec3 FakeShadowComponent::GetWorldPosition(const Unnamed::TransformComponent& transform) const {
		const Mat4& world = transform.GetWorldMat();
		return Vec3(world.m[3][0], world.m[3][1], world.m[3][2]);
	}

	REGISTER_COMPONENT(FakeShadowComponent);

}
