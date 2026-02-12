#pragma once
#include <string>

#include "core/guidgenerator/GuidGenerator.h"

namespace Unnamed {
	class UScene;
	class JsonReader;
	class JsonWriter;

	class USceneSerializer {
	public:
		static bool LoadFromFile(
			UScene& scene, const std::string& path, GuidGenerator& guidGen
		);
		static bool SaveToFile(const UScene& scene, const std::string& path);

		static bool Deserialize(
			UScene& scene, const JsonReader& root, GuidGenerator& guidGen
		);
		static void Serialize(const UScene& scene, JsonWriter& writer);
	};
}
