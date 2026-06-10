#include "TrashObjSpawnerComponent.h"

#include "TrashObjMoverComponent.h"
#include "engine/scene/Scene.h"
#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/components/mesh/StaticMeshRendererComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"

#ifdef _DEBUG
#include "imgui.h"
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>

namespace {
	constexpr float kPi = 3.14159265358979323846f;

	template <typename ComponentType>
	ComponentType* CloneComponent(
		Unnamed::Entity& dstEntity,
		const ComponentType& srcComponent
	) {
		// NOTE: Deserialize後にOnAttachedするため、通常のシーンロードと同じ初期化順に合わせる。
		Unnamed::JsonWriter writer("");
		writer.BeginObject();
		srcComponent.Serialize(writer);
		writer.EndObject();

		auto component = std::make_unique<ComponentType>();
		Unnamed::JsonReader reader(writer.GetRoot());
		component->Deserialize(reader);

		return static_cast<ComponentType*>(
			dstEntity.AddComponentInstance(std::move(component))
		);
	}

#ifdef _DEBUG
	bool InputString(const char* label, std::string& value)
	{
		// NOTE: このImGui環境ではstd::string overloadが無いため、固定長バッファで編集する。
		std::array<char, 128> buffer{};
		std::snprintf(buffer.data(), buffer.size(), "%s", value.c_str());
		if (!ImGui::InputText(label, buffer.data(), buffer.size())) {
			return false;
		}
		value = buffer.data();
		return true;
	}
#endif
}

void MyGame::TrashObjSpawnerComponent::OnAttached()
{
	// NOTE: ゲーム開始ごとに完全固定の並びにならないよう、実行時シードで乱数を初期化する。
	std::random_device seedSource;
	_randomEngine.seed(seedSource());
}

void MyGame::TrashObjSpawnerComponent::OnTick(float deltaTime)
{
	(void)deltaTime;
}

void MyGame::TrashObjSpawnerComponent::OnDetached()
{
	_templateEntityGuids.clear();
}

std::string_view MyGame::TrashObjSpawnerComponent::GetStableName() const
{
	return "mygame.TrashObjSpawnerComponent";
}

std::string_view MyGame::TrashObjSpawnerComponent::GetComponentName() const
{
	return "Trash Object Spawner Component";
}

#ifdef _DEBUG
void MyGame::TrashObjSpawnerComponent::DrawInspectorImGui()
{
	// NOTE: ゴミ波の見た目と密度はプレイ調整が多いため、実行中にも確認できるようにする。
	ImGui::Text("=== Trash Object Spawner Component ===");
	ImGui::Checkbox("Spawn Enabled", &_spawnEnabled);
	InputString("Template Tag", _templateTag);
	InputString("Spawned Trash Tag", _spawnedTrashTag);
	InputString("Spawned Name Prefix", _spawnedNamePrefix);
	InputString("Spawned Folder Path", _spawnedFolderPath);
	ImGui::DragFloat3("Spawn Center", &_spawnCenter.x, 0.1f);
	ImGui::DragFloat3("Spawn Half Extent", &_spawnHalfExtent.x, 0.1f, 0.0f, 999.0f);
	ImGui::DragFloat("Spawn Height Min", &_spawnHeightMin, 0.1f, -999.0f, 999.0f);
	ImGui::DragFloat("Spawn Height Max", &_spawnHeightMax, 0.1f, -999.0f, 999.0f);
	ImGui::DragFloat3("Initial Velocity Min", &_initialVelocityMin.x, 0.1f);
	ImGui::DragFloat3("Initial Velocity Max", &_initialVelocityMax.x, 0.1f);
	ImGui::DragFloat("Scale Min", &_scaleMin, 0.01f, 0.01f, 100.0f);
	ImGui::DragFloat("Scale Max", &_scaleMax, 0.01f, 0.01f, 100.0f);
	ImGui::Checkbox("Random Yaw", &_randomYaw);
	ImGui::Text("Template GUID Count: %d", static_cast<int>(_templateEntityGuids.size()));
	ImGui::Text("Spawned Total: %d", _spawnedTotal);
	if (ImGui::Button("Spawn 1")) {
		SpawnWave(1);
	}
	ImGui::SameLine();
	if (ImGui::Button("Spawn 10")) {
		SpawnWave(10);
	}
}
#endif

void MyGame::TrashObjSpawnerComponent::Deserialize(const Unnamed::JsonReader& reader)
{
	// NOTE: テンプレートEntity側の物理値を尊重し、Spawnerは配置と選択だけを担当する。
	_templateEntityGuids.clear();
	const Unnamed::JsonReader guidArray = reader["templateEntityGuids"].GetArray();
	for (size_t i = 0; i < guidArray.Size(); ++i) {
		const uint64_t guid = guidArray[i].GetUint64();
		if (guid != 0) {
			_templateEntityGuids.emplace_back(guid);
		}
	}

	if (auto val = reader.Read<std::string>("templateTag")) {
		_templateTag = val.value();
	}
	if (auto val = reader.Read<std::string>("spawnedTrashTag")) {
		_spawnedTrashTag = val.value();
	}
	if (auto val = reader.Read<std::string>("spawnedNamePrefix")) {
		_spawnedNamePrefix = val.value();
	}
	if (auto val = reader.Read<std::string>("spawnedFolderPath")) {
		_spawnedFolderPath = val.value();
	}
	if (reader["spawnCenter"].Valid()) {
		_spawnCenter = reader["spawnCenter"].GetVec3(_spawnCenter);
	}
	if (reader["spawnHalfExtent"].Valid()) {
		_spawnHalfExtent = reader["spawnHalfExtent"].GetVec3(_spawnHalfExtent);
	}
	if (auto val = reader.Read<float>("spawnHeightMin")) {
		_spawnHeightMin = val.value();
	}
	if (auto val = reader.Read<float>("spawnHeightMax")) {
		_spawnHeightMax = val.value();
	}
	if (reader["initialVelocityMin"].Valid()) {
		_initialVelocityMin =
			reader["initialVelocityMin"].GetVec3(_initialVelocityMin);
	}
	if (reader["initialVelocityMax"].Valid()) {
		_initialVelocityMax =
			reader["initialVelocityMax"].GetVec3(_initialVelocityMax);
	}
	if (auto val = reader.Read<float>("scaleMin")) {
		_scaleMin = std::max(0.01f, val.value());
	}
	if (auto val = reader.Read<float>("scaleMax")) {
		_scaleMax = std::max(0.01f, val.value());
	}
	if (auto val = reader.Read<bool>("randomYaw")) {
		_randomYaw = val.value();
	}
	if (auto val = reader.Read<bool>("spawnEnabled")) {
		_spawnEnabled = val.value();
	}

	if (_spawnHeightMax < _spawnHeightMin) {
		std::swap(_spawnHeightMin, _spawnHeightMax);
	}
	if (_scaleMax < _scaleMin) {
		std::swap(_scaleMin, _scaleMax);
	}
	_spawnHalfExtent.x = std::max(0.0f, _spawnHalfExtent.x);
	_spawnHalfExtent.y = 0.0f;
	_spawnHalfExtent.z = std::max(0.0f, _spawnHalfExtent.z);
}

void MyGame::TrashObjSpawnerComponent::Serialize(Unnamed::JsonWriter& writer) const
{
	// NOTE: シーン保存時に調整値を保持し、ゲームルール側の波数設定と独立して編集できるようにする。
	writer.Key("templateEntityGuids");
	writer.BeginArray();
	for (const uint64_t guid : _templateEntityGuids) {
		writer.Write(guid);
	}
	writer.EndArray();

	writer.Key("templateTag");
	writer.Write(_templateTag);
	writer.Key("spawnedTrashTag");
	writer.Write(_spawnedTrashTag);
	writer.Key("spawnedNamePrefix");
	writer.Write(_spawnedNamePrefix);
	writer.Key("spawnedFolderPath");
	writer.Write(_spawnedFolderPath);
	writer.WriteVec3("spawnCenter", _spawnCenter);
	writer.WriteVec3("spawnHalfExtent", _spawnHalfExtent);
	writer.Key("spawnHeightMin");
	writer.Write(_spawnHeightMin);
	writer.Key("spawnHeightMax");
	writer.Write(_spawnHeightMax);
	writer.WriteVec3("initialVelocityMin", _initialVelocityMin);
	writer.WriteVec3("initialVelocityMax", _initialVelocityMax);
	writer.Key("scaleMin");
	writer.Write(_scaleMin);
	writer.Key("scaleMax");
	writer.Write(_scaleMax);
	writer.Key("randomYaw");
	writer.Write(_randomYaw);
	writer.Key("spawnEnabled");
	writer.Write(_spawnEnabled);
}

void MyGame::TrashObjSpawnerComponent::SpawnWave(int count)
{
	if (!_spawnEnabled || count <= 0) {
		return;
	}

	std::vector<Unnamed::Entity*> templates = CollectTemplateEntities();
	if (templates.empty()) {
		return;
	}

	std::uniform_int_distribution<size_t> templateDist(0, templates.size() - 1);
	for (int i = 0; i < count; ++i) {
		Unnamed::Entity* templateEntity = templates[templateDist(_randomEngine)];
		if (!templateEntity) {
			continue;
		}
		(void)SpawnOne(*templateEntity);
	}
}

std::vector<Unnamed::Entity*> MyGame::TrashObjSpawnerComponent::CollectTemplateEntities() const
{
	std::vector<Unnamed::Entity*> result;

	auto* scene = GetScene();
	if (!scene && GetOwner()) {
		scene = GetOwner()->GetScene();
	}
	if (!scene) {
		return result;
	}

	result.reserve(_templateEntityGuids.empty() ? scene->GetEntityCount() : _templateEntityGuids.size());
	for (const uint64_t guid : _templateEntityGuids) {
		if (auto* entity = scene->FindEntity(guid)) {
			if (IsGeneratedTrashEntity(*entity)) {
				continue;
			}
			if (entity->GetComponent<TrashObjMoverComponent>() &&
				entity->GetComponent<Unnamed::StaticMeshRendererComponent>()) {
				result.emplace_back(entity);
			}
		}
	}
	if (!result.empty()) {
		return result;
	}

	if (!_templateTag.empty()) {
		for (auto* entity : scene->FindEntitiesByTag(_templateTag)) {
			if (!entity) {
				continue;
			}
			if (IsGeneratedTrashEntity(*entity)) {
				continue;
			}
			if (entity->GetComponent<TrashObjMoverComponent>() &&
				entity->GetComponent<Unnamed::StaticMeshRendererComponent>()) {
				result.emplace_back(entity);
			}
		}
	}
	if (!result.empty()) {
		return result;
	}

	for (const auto& entityPtr : scene->GetEntities()) {
		if (!entityPtr) {
			continue;
		}
		auto* entity = entityPtr.get();
		if (entity == GetOwner()) {
			continue;
		}
		if (IsGeneratedTrashEntity(*entity)) {
			continue;
		}
		if (entity->GetComponent<TrashObjMoverComponent>() &&
			entity->GetComponent<Unnamed::StaticMeshRendererComponent>()) {
			result.emplace_back(entity);
		}
	}
	return result;
}

Unnamed::Entity* MyGame::TrashObjSpawnerComponent::SpawnOne(Unnamed::Entity& templateEntity)
{
	auto* scene = GetScene();
	if (!scene && GetOwner()) {
		scene = GetOwner()->GetScene();
	}
	if (!scene) {
		return nullptr;
	}

	const uint64_t guid = scene->AllocateEntityId();
	Unnamed::Entity& entity =
		scene->CreateEntity(_spawnedNamePrefix + "_" + std::to_string(guid), guid, false);
	entity.SetFolderPath(_spawnedFolderPath);
	if (!_spawnedTrashTag.empty()) {
		(void)entity.AddTag(_spawnedTrashTag);
	}

	auto* transform = entity.AddComponent<Unnamed::TransformComponent>();
	if (const auto* srcTransform =
		templateEntity.GetComponent<Unnamed::TransformComponent>()) {
		transform->SetScale(srcTransform->GetScale());
	}

	const Vec3 spawnPosition = BuildRandomSpawnPosition();
	transform->SetPosition(spawnPosition);
	const float scaleMultiplier = RandomRange(_scaleMin, _scaleMax);
	transform->SetScale(transform->GetScale() * scaleMultiplier);
	if (_randomYaw) {
		const float yawRad = RandomRange(0.0f, kPi * 2.0f);
		transform->SetRotation(Quaternion::AxisAngle(Vec3::up, yawRad));
	}
	transform->RequestInterpolationResync();

	if (const auto* srcMesh =
		templateEntity.GetComponent<Unnamed::StaticMeshRendererComponent>()) {
		(void)CloneComponent(entity, *srcMesh);
	}
	if (const auto* srcTrash = templateEntity.GetComponent<TrashObjMoverComponent>()) {
		auto* trash = CloneComponent(entity, *srcTrash);
		if (trash) {
			trash->SetVelocity(BuildRandomInitialVelocity());
			trash->SetFalling(true);
		}
	}

	++_spawnedTotal;
	return &entity;
}

bool MyGame::TrashObjSpawnerComponent::IsGeneratedTrashEntity(
	const Unnamed::Entity& entity
) const
{
	// NOTE: 生成済みTrashも同じタグを持つため、次の波でテンプレート化しないよう除外する。
	const std::string_view folderPath = entity.GetFolderPath();
	if (!_spawnedFolderPath.empty() && folderPath == _spawnedFolderPath) {
		return true;
	}

	const std::string_view entityName = entity.GetName();
	return !_spawnedNamePrefix.empty() &&
		entityName.starts_with(_spawnedNamePrefix);
}

float MyGame::TrashObjSpawnerComponent::RandomRange(
	const float minValue,
	const float maxValue
)
{
	const float low = std::min(minValue, maxValue);
	const float high = std::max(minValue, maxValue);
	std::uniform_real_distribution<float> dist(low, high);
	return dist(_randomEngine);
}

Vec3 MyGame::TrashObjSpawnerComponent::BuildRandomSpawnPosition()
{
	return Vec3(
		_spawnCenter.x + RandomRange(-_spawnHalfExtent.x, _spawnHalfExtent.x),
		RandomRange(_spawnHeightMin, _spawnHeightMax),
		_spawnCenter.z + RandomRange(-_spawnHalfExtent.z, _spawnHalfExtent.z)
	);
}

Vec3 MyGame::TrashObjSpawnerComponent::BuildRandomInitialVelocity()
{
	return Vec3(
		RandomRange(_initialVelocityMin.x, _initialVelocityMax.x),
		RandomRange(_initialVelocityMin.y, _initialVelocityMax.y),
		RandomRange(_initialVelocityMin.z, _initialVelocityMax.z)
	);
}
