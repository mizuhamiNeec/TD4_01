#include "USceneSerializer.h"

#include "UScene.h"

#include "core/ComponentRegistry.h"
#include "core/guidgenerator/GuidGenerator.h"
#include "core/json/JsonReader.h"
#include "core/json/JsonWriter.h"

#include "engine/unnamed/framework/entity/UEntity.h"
#include "engine/unnamed/subsystem/console/Log.h"

namespace Unnamed {
	static constexpr std::string_view kChannel = "USceneSerializer";

	bool USceneSerializer::LoadFromFile(
		UScene& scene, const std::string& path, GuidGenerator& guidGen
	) {
		const JsonReader root(path);
		if (!root.Valid()) {
			Error(kChannel, "Failed to open scene: {}", path);
			return false;
		}
		return Deserialize(scene, root, guidGen);
	}

	bool USceneSerializer::SaveToFile(
		const UScene& scene, const std::string& path
	) {
		JsonWriter writer(path);
		Serialize(scene, writer);
		return writer.Save();
	}

	namespace {
		bool ReadBoolOr(
			const JsonReader& r, const char* key, const bool fallback
		) {
			const JsonReader v = r[key];
			return v.Valid() ? v.GetBool() : fallback;
		}

		uint64_t ReadU64Or(
			const JsonReader& r, const char* key, const uint64_t fallback
		) {
			const JsonReader v = r[key];
			return v.Valid() ? v.GetUint64() : fallback;
		}

		std::string ReadStringOr(
			const JsonReader& r, const char* key, const char* fallback
		) {
			const JsonReader v = r[key];
			return v.Valid() ? v.GetString() : std::string(fallback);
		}
	}

	bool USceneSerializer::Deserialize(
		UScene& scene, const JsonReader& root, GuidGenerator& guidGen
	) {
		const JsonReader entities = root["entities"];

		// エンティティ配列の検証 
		if (!entities.Valid()) { return false; }

		// 各エンティティの読み込み
		for (size_t i = 0; i < entities.Size(); ++i) {
			const JsonReader e = entities[i];
			if (!e.Valid()) { continue; }

			const std::string name = ReadStringOr(e, "name", "unnamed");
			const uint64_t guid = ReadU64Or(e, "guid", 0);
			const bool isEditorOnly = ReadBoolOr(e, "isEditorOnly", false);
			const bool entityActive = ReadBoolOr(e, "active", true);

			const uint64_t finalGuid = guid != 0 ? guid : guidGen.Alloc();

			UEntity& entity = scene.CreateEntity(name, finalGuid, isEditorOnly);
			entity.SetActive(entityActive);

			const JsonReader comps = e["components"];
			if (!comps.Valid()) { continue; }

			for (size_t ci = 0; ci < comps.Size(); ++ci) {
				const JsonReader c = comps[ci];
				if (!c.Valid()) { continue; }

				const std::string type = ReadStringOr(c, "type", "");
				if (type.empty()) { continue; }

				auto comp = ComponentRegistry::Get().Create(type);
				if (!comp) {
					Warning(kChannel, "Unknown component type: {}", type);
					Warning(kChannel, "コンポーネントの登録を忘れている可能性があります。");
					continue;
				}

				// GUIDを読む。0 の場合は新規割り当て
				const uint64_t compGuid = ReadU64Or(c, "guid", 0);
				comp->SetGuid(compGuid != 0 ? compGuid : guidGen.Alloc());

				if (compGuid != 0) {
					// GUIDがある場合はそれを使用
					comp->SetGuid(compGuid);
				} else {
					// GUIDがない場合は仮のGUIDを割り当て
					comp->SetGuid(0x200000 + ci);
				}

				comp->SetActive(ReadBoolOr(c, "active", true));

				const JsonReader data = c["data"];
				if (data.Valid()) {
					// コンポーネントに任せる
					comp->Deserialize(data);
				}

				(void)entity.AddComponentInstance(std::move(comp));
			}
		}

		// シーンのポストロード処理
		scene.OnPostLoad();

		return true;
	}

	void USceneSerializer::Serialize(
		const UScene& scene, JsonWriter& writer
	) {
		writer.BeginObject();

		writer.Key("version");
		writer.Write(1);

		writer.Key("entities");
		writer.BeginArray();

		for (const auto& ePtr : scene.GetEntities()) {
			if (!ePtr) { continue; }
			const UEntity& e = *ePtr;

			writer.BeginObject();

			writer.Key("name");
			writer.Write(std::string(e.GetName()));

			writer.Key("guid");
			writer.Write(e.GetGuid());

			writer.Key("isEditorOnly");
			writer.Write(e.IsEditorOnly());

			writer.Key("active");
			writer.Write(e.IsActive());

			writer.Key("components");
			writer.BeginArray();

			e.ForEachComponent(
				[&writer](const UBaseComponent& c) {
					writer.BeginObject();

					writer.Key("type");
					writer.Write(std::string(c.GetStableName()));

					writer.Key("guid");
					writer.Write(c.GetGuid());

					writer.Key("active");
					writer.Write(c.IsActive());

					writer.Key("data");
					writer.BeginObject();
					c.Serialize(writer);
					writer.EndObject();

					writer.EndObject();
				}
			);

			writer.EndArray();
			writer.EndObject();
		}

		writer.EndArray();
		writer.EndObject();
	}
}
