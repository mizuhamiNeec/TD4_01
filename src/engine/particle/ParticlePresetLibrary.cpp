#include "ParticlePresetLibrary.h"

#include <fstream>
#include <filesystem>
#include <json.hpp>

#include "core/math/Vec4.h"

#include "engine/unnamed/subsystem/console/Log.h"

#include "Particle/ParticlePreset.h"

static constexpr std::string_view kChannel = "PPL";

using json = nlohmann::json;

//======================================================================
//  JSON 書き出し
//======================================================================
void to_json(json& j, const ParticlePreset& p)
{
	j = json{
		{ "name", p.name },

		// ========= emitterSettings =========
		{ "emitterSettings", {
			{ "vertexType",      static_cast<int>(p.emitterSettings.vertexType) },
			{ "textureFilePath", p.emitterSettings.textureFilePath },
			{ "blendMode",       static_cast<int>(p.emitterSettings.blendMode) }
		}},

		// ========= emitterSpawn =========
		{ "emitterSpawn", {
			{ "count",             p.emitterSpawn.count },
			{ "frequency",         p.emitterSpawn.frequency },
			{ "repeat",            p.emitterSpawn.repeat },
			{ "useRandomPosition", p.emitterSpawn.useRandomPosition },
			{ "useLocalSpace",     p.emitterSpawn.useLocalSpace }
		}},

		// ========= particleSpawn =========
{        "particleSpawn", {
	{ "initialScale",  { p.particleSpawn.initialScale.x,
						 p.particleSpawn.initialScale.y,
						 p.particleSpawn.initialScale.z } },
	{ "initialRotate", { p.particleSpawn.initialRotate.x,
						 p.particleSpawn.initialRotate.y,
						 p.particleSpawn.initialRotate.z } },
	{ "initialOffset", { p.particleSpawn.initialOffset.x,
						 p.particleSpawn.initialOffset.y,
						 p.particleSpawn.initialOffset.z } },

						 // ランダム範囲
						 { "initialScaleRandom", {
							 { "useRandom", p.particleSpawn.initialScaleRandom.useRandom },
							 { "min", { p.particleSpawn.initialScaleRandom.minValue.x,
										p.particleSpawn.initialScaleRandom.minValue.y,
										p.particleSpawn.initialScaleRandom.minValue.z } },
							 { "max", { p.particleSpawn.initialScaleRandom.maxValue.x,
										p.particleSpawn.initialScaleRandom.maxValue.y,
										p.particleSpawn.initialScaleRandom.maxValue.z } }
						 }},
						 { "initialRotateRandom", {
							 { "useRandom", p.particleSpawn.initialRotateRandom.useRandom },
							 { "min", { p.particleSpawn.initialRotateRandom.minValue.x,
										p.particleSpawn.initialRotateRandom.minValue.y,
										p.particleSpawn.initialRotateRandom.minValue.z } },
							 { "max", { p.particleSpawn.initialRotateRandom.maxValue.x,
										p.particleSpawn.initialRotateRandom.maxValue.y,
										p.particleSpawn.initialRotateRandom.maxValue.z } }
						 }},
						 { "initialOffsetRandom", {
							 { "useRandom", p.particleSpawn.initialOffsetRandom.useRandom },
							 { "min", { p.particleSpawn.initialOffsetRandom.minValue.x,
										p.particleSpawn.initialOffsetRandom.minValue.y,
										p.particleSpawn.initialOffsetRandom.minValue.z } },
							 { "max", { p.particleSpawn.initialOffsetRandom.maxValue.x,
										p.particleSpawn.initialOffsetRandom.maxValue.y,
										p.particleSpawn.initialOffsetRandom.maxValue.z } }
						 }}
				}},


		// ========= particleUpdate =========
		{ "particleUpdate", {
		   { "lifeTime",      p.particleUpdate.lifeTime },

		   { "velocity",      { p.particleUpdate.velocity.x,
						 p.particleUpdate.velocity.y,
						 p.particleUpdate.velocity.z } },
		   { "rotationSpeed", { p.particleUpdate.rotationSpeed.x,
						 p.particleUpdate.rotationSpeed.y,
						 p.particleUpdate.rotationSpeed.z } },
		   { "scaleSpeed",    { p.particleUpdate.scaleSpeed.x,
						 p.particleUpdate.scaleSpeed.y,
						 p.particleUpdate.scaleSpeed.z } },

						 // ランダム範囲
						 { "velocityRandom", {
							 { "useRandom", p.particleUpdate.velocityRandom.useRandom },
							 { "min", { p.particleUpdate.velocityRandom.minValue.x,
										p.particleUpdate.velocityRandom.minValue.y,
										p.particleUpdate.velocityRandom.minValue.z } },
							 { "max", { p.particleUpdate.velocityRandom.maxValue.x,
										p.particleUpdate.velocityRandom.maxValue.y,
										p.particleUpdate.velocityRandom.maxValue.z } }
						 }},
						 { "rotationRandom", {
							 { "useRandom", p.particleUpdate.rotationRandom.useRandom },
							 { "min", { p.particleUpdate.rotationRandom.minValue.x,
										p.particleUpdate.rotationRandom.minValue.y,
										p.particleUpdate.rotationRandom.minValue.z } },
							 { "max", { p.particleUpdate.rotationRandom.maxValue.x,
										p.particleUpdate.rotationRandom.maxValue.y,
										p.particleUpdate.rotationRandom.maxValue.z } }
						 }},
						 { "scaleRandom", {
							 { "useRandom", p.particleUpdate.scaleRandom.useRandom },
							 { "min", { p.particleUpdate.scaleRandom.minValue.x,
										p.particleUpdate.scaleRandom.minValue.y,
										p.particleUpdate.scaleRandom.minValue.z } },
							 { "max", { p.particleUpdate.scaleRandom.maxValue.x,
										p.particleUpdate.scaleRandom.maxValue.y,
										p.particleUpdate.scaleRandom.maxValue.z } }
						 }},

						 { "useGravity",    p.particleUpdate.useGravity }
					 }},


		// ========= render =========
		{ "render", {
			{ "color",        { p.render.color.x,
								p.render.color.y,
								p.render.color.z,
								p.render.color.w } },
			{ "useBillboard", p.render.useBillboard },
			{ "flipY",        p.render.flipY }
		}},
	};

	// -----------------------------
	// colorGradient のシリアライズ
	// -----------------------------
	{
		json gradJson;
		gradJson["enabled"] = p.render.colorGradient.enabled;

		json keysJson = json::array();
		for (const auto& key : p.render.colorGradient.keys) {
			keysJson.push_back({
				key.t,
				key.color.x,
				key.color.y,
				key.color.z,
				key.color.w
				});
		}
		gradJson["keys"] = keysJson;

		j["render"]["colorGradient"] = gradJson;
	}

	// -----------------------------
	// gradientTimeCurve のシリアライズ
	// -----------------------------
	{
		json timeCurveJson;
		timeCurveJson["enabled"] = p.render.gradientTimeCurve.enabled;

		json keysJson = json::array();
		for (const auto& key : p.render.gradientTimeCurve.keys) {
			keysJson.push_back({ key.t, key.v });
		}
		timeCurveJson["keys"] = keysJson;

		j["render"]["gradientTimeCurve"] = timeCurveJson;
	}

}

//======================================================================
//  JSON 読み込み
//======================================================================
void from_json(const json& j, ParticlePreset& p)
{
	p.name = j.value("name", "");

	// 古い形式との互換用: 直下に Vec3 / Vec4 がある場合も読む
	auto readVec3 = [&j](const char* key, Vec3 defaultValue) {
		Vec3 v = defaultValue;
		if (j.contains(key) && j[key].is_array() && j[key].size() == 3) {
			v.x = j[key][0].get<float>();
			v.y = j[key][1].get<float>();
			v.z = j[key][2].get<float>();
		}
		return v;
		};

	auto readVec4 = [&j](const char* key, Vec4 defaultValue) {
		Vec4 v = defaultValue;
		if (j.contains(key) && j[key].is_array() && j[key].size() == 4) {
			v.x = j[key][0].get<float>();
			v.y = j[key][1].get<float>();
			v.z = j[key][2].get<float>();
			v.w = j[key][3].get<float>();
		}
		return v;
		};

	// ========= emitterSettings =========
	if (j.contains("emitterSettings") && j["emitterSettings"].is_object()) {
		const auto& es = j["emitterSettings"];
		p.emitterSettings.vertexType = static_cast<VertexDataType>(
			es.value("vertexType", static_cast<int>(VertexDataType::Plane)));
		p.emitterSettings.textureFilePath = es.value("textureFilePath", "");
		p.emitterSettings.blendMode = static_cast<BlendMode>(
			es.value("blendMode", static_cast<int>(kBlendModeNormal)));
	}
	else {
		p.emitterSettings.vertexType = static_cast<VertexDataType>(
			j.value("vertexType", static_cast<int>(VertexDataType::Plane)));
		p.emitterSettings.textureFilePath = j.value("textureFilePath", "");
		p.emitterSettings.blendMode = static_cast<BlendMode>(
			j.value("blendMode", static_cast<int>(kBlendModeNormal)));
	}

	// ========= emitterSpawn =========
	if (j.contains("emitterSpawn") && j["emitterSpawn"].is_object()) {
		const auto& es = j["emitterSpawn"];
		p.emitterSpawn.count = es.value("count", 10u);
		p.emitterSpawn.frequency = es.value("frequency", 1.0f);
		p.emitterSpawn.repeat = es.value("repeat", false);
		p.emitterSpawn.useRandomPosition = es.value("useRandomPosition", false);
		p.emitterSpawn.useLocalSpace = es.value("useLocalSpace", false);
	}
	else {
		p.emitterSpawn.count = j.value("count", 10u);
		p.emitterSpawn.frequency = j.value("frequency", 1.0f);
		p.emitterSpawn.repeat = j.value("repeat", false);
		p.emitterSpawn.useRandomPosition = j.value("useRandomPosition", false);
		p.emitterSpawn.useLocalSpace = j.value("useLocalSpace", false);
	}

	// ========= particleSpawn =========
	if (j.contains("particleSpawn") && j["particleSpawn"].is_object()) {
		const auto& ps = j["particleSpawn"];
		auto read3 = [&ps](const char* key, Vec3 def) {
			Vec3 v = def;
			if (ps.contains(key) && ps[key].is_array() && ps[key].size() == 3) {
				v.x = ps[key][0].get<float>();
				v.y = ps[key][1].get<float>();
				v.z = ps[key][2].get<float>();
			}
			return v;
			};

		p.particleSpawn.initialScale = read3("initialScale", { 1,1,1 });
		p.particleSpawn.initialRotate = read3("initialRotate", { 0,0,0 });
		p.particleSpawn.initialOffset = read3("initialOffset", { 0,0,0 });

		// ----- ランダム値の読み込み -----
		auto readRandom3 = [&ps](const char* key, const RandomRange3& def) {
			RandomRange3 r = def;
			if (ps.contains(key) && ps[key].is_object()) {
				const auto& obj = ps[key];
				r.useRandom = obj.value("useRandom", def.useRandom);

				auto read3v = [&obj](const char* k, Vec3 dv) {
					Vec3 v = dv;
					if (obj.contains(k) && obj[k].is_array() && obj[k].size() == 3) {
						v.x = obj[k][0].get<float>();
						v.y = obj[k][1].get<float>();
						v.z = obj[k][2].get<float>();
					}
					return v;
					};

				r.minValue = read3v("min", def.minValue);
				r.maxValue = read3v("max", def.maxValue);
			}
			return r;
			};

		p.particleSpawn.initialScaleRandom = readRandom3("initialScaleRandom", RandomRange3{});
		p.particleSpawn.initialRotateRandom = readRandom3("initialRotateRandom", RandomRange3{});
		p.particleSpawn.initialOffsetRandom = readRandom3("initialOffsetRandom", RandomRange3{});
	}
	else {
		// 旧データ用（ランダム無し）
		p.particleSpawn.initialScale = readVec3("initialScale", { 1,1,1 });
		p.particleSpawn.initialRotate = readVec3("initialRotate", { 0,0,0 });
		p.particleSpawn.initialOffset = readVec3("initialOffset", { 0,0,0 });

		p.particleSpawn.initialScaleRandom = RandomRange3{};
		p.particleSpawn.initialRotateRandom = RandomRange3{};
		p.particleSpawn.initialOffsetRandom = RandomRange3{};
	}

	// ========= particleUpdate =========
	if (j.contains("particleUpdate") && j["particleUpdate"].is_object()) {
		const auto& pu = j["particleUpdate"];
		auto read3 = [&pu](const char* key, Vec3 def) {
			Vec3 v = def;
			if (pu.contains(key) && pu[key].is_array() && pu[key].size() == 3) {
				v.x = pu[key][0].get<float>();
				v.y = pu[key][1].get<float>();
				v.z = pu[key][2].get<float>();
			}
			return v;
			};

		p.particleUpdate.lifeTime = pu.value("lifeTime", 1.0f);
		p.particleUpdate.velocity = read3("velocity", { 0,0,0 });
		p.particleUpdate.rotationSpeed = read3("rotationSpeed", { 0,0,0 });
		p.particleUpdate.scaleSpeed = read3("scaleSpeed", { 0,0,0 });
		p.particleUpdate.useGravity = pu.value("useGravity", false);

		// ----- ランダム値の読み込み -----
		auto readRandom3 = [&pu](const char* key, const RandomRange3& def) {
			RandomRange3 r = def;
			if (pu.contains(key) && pu[key].is_object()) {
				const auto& obj = pu[key];
				r.useRandom = obj.value("useRandom", def.useRandom);

				auto read3v = [&obj](const char* k, Vec3 dv) {
					Vec3 v = dv;
					if (obj.contains(k) && obj[k].is_array() && obj[k].size() == 3) {
						v.x = obj[k][0].get<float>();
						v.y = obj[k][1].get<float>();
						v.z = obj[k][2].get<float>();
					}
					return v;
					};

				r.minValue = read3v("min", def.minValue);
				r.maxValue = read3v("max", def.maxValue);
			}
			return r;
			};

		p.particleUpdate.velocityRandom = readRandom3("velocityRandom", RandomRange3{});
		p.particleUpdate.rotationRandom = readRandom3("rotationRandom", RandomRange3{});
		p.particleUpdate.scaleRandom = readRandom3("scaleRandom", RandomRange3{});

		// scaleCurve の復元処理（既存処理）はこの下にそのまま残す
	}
	else {
		// 旧フォーマット互換（ランダム無し）
		p.particleUpdate.lifeTime = j.value("lifeTime", 1.0f);
		p.particleUpdate.velocity = readVec3("velocity", { 0,0,0 });
		p.particleUpdate.rotationSpeed = readVec3("rotationSpeed", { 0,0,0 });
		p.particleUpdate.scaleSpeed = readVec3("scaleSpeed", { 0,0,0 });
		p.particleUpdate.useGravity = j.value("useGravity", false);

		p.particleUpdate.velocityRandom = RandomRange3{};
		p.particleUpdate.rotationRandom = RandomRange3{};
		p.particleUpdate.scaleRandom = RandomRange3{};

		// 旧データではカーブ無効、など既存処理はそのまま
	}


	// ========= render =========
	if (j.contains("render") && j["render"].is_object()) {
		const auto& r = j["render"];
		auto read4 = [&r](const char* key, Vec4 def) {
			Vec4 v = def;
			if (r.contains(key) && r[key].is_array() && r[key].size() == 4) {
				v.x = r[key][0].get<float>();
				v.y = r[key][1].get<float>();
				v.z = r[key][2].get<float>();
				v.w = r[key][3].get<float>();
			}
			return v;
			};

		p.render.color = read4("color", { 1,1,1,1 });
		p.render.useBillboard = r.value("useBillboard", true);
		p.render.flipY = r.value("flipY", false);

		// ---- colorCurve ----
		p.render.colorCurve.enabled = false;
		p.render.colorCurve.keys.clear();
		if (r.contains("colorCurve") && r["colorCurve"].is_object()) {
			const auto& cc = r["colorCurve"];
			p.render.colorCurve.enabled = cc.value("enabled", false);

			if (cc.contains("keys") && cc["keys"].is_array()) {
				for (const auto& elem : cc["keys"]) {
					if (elem.is_array() && elem.size() == 2) {
						CurveKey key;
						key.t = elem[0].get<float>();
						key.v = elem[1].get<float>();
						p.render.colorCurve.keys.push_back(key);
					}
				}
			}
		}

		// ---- colorGradient ----
		p.render.colorGradient.enabled = false;
		p.render.colorGradient.keys.clear();
		if (r.contains("colorGradient") && r["colorGradient"].is_object()) {
			const auto& cg = r["colorGradient"];
			p.render.colorGradient.enabled = cg.value("enabled", false);

			if (cg.contains("keys") && cg["keys"].is_array()) {
				for (const auto& elem : cg["keys"]) {
					if (elem.is_array() && elem.size() == 5) {
						ColorKey4 key;
						key.t = elem[0].get<float>();
						key.color.x = elem[1].get<float>();
						key.color.y = elem[2].get<float>();
						key.color.z = elem[3].get<float>();
						key.color.w = elem[4].get<float>();
						p.render.colorGradient.keys.push_back(key);
					}
				}
			}
		}

		// ---- gradientTimeCurve ----
		p.render.gradientTimeCurve.enabled = false;
		p.render.gradientTimeCurve.keys.clear();
		if (r.contains("gradientTimeCurve") && r["gradientTimeCurve"].is_object()) {
			const auto& gc = r["gradientTimeCurve"];
			p.render.gradientTimeCurve.enabled = gc.value("enabled", false);

			if (gc.contains("keys") && gc["keys"].is_array()) {
				for (const auto& elem : gc["keys"]) {
					if (elem.is_array() && elem.size() == 2) {
						CurveKey key;
						key.t = elem[0].get<float>();
						key.v = elem[1].get<float>();
						p.render.gradientTimeCurve.keys.push_back(key);
					}
				}
			}
		}


	}
	else {
		p.render.color = readVec4("color", { 1,1,1,1 });
		p.render.useBillboard = j.value("useBillboard", true);
		p.render.flipY = j.value("flipY", false);

		p.render.colorCurve.enabled = false;
		p.render.colorCurve.keys.clear();

		p.render.colorGradient.enabled = false;
		p.render.colorGradient.keys.clear();

		p.render.gradientTimeCurve.enabled = false;
		p.render.gradientTimeCurve.keys.clear();
	}

}

bool ParticlePresetLibrary::SaveToJson(const ParticlePreset& preset,const std::string& directory)
{
	namespace fs = std::filesystem;

	try {
		// ディレクトリが無ければ作る
		if (!fs::exists(directory)) {
			fs::create_directories(directory);
		}

		std::string fileName = preset.name;
		if (fileName.empty()) {
			fileName = "UnnamedParticle";
		}

		fs::path path = fs::path(directory) / (fileName + ".json");

		// JSON にシリアライズ（to_json が呼ばれる）
		json j = preset;

		std::ofstream ofs(path);
		if (!ofs) {
			Error(kChannel, "Failed to open particle preset json: {}", path.string());
			return false;
		}
		ofs << j.dump(4); // インデント付きで保存
		ofs.close();

		// 内部マップ（メモリ）も更新
		presets_[preset.name] = preset;

		return true;
	}
	catch (const std::exception& e) {
		Fatal(kChannel, "Failed to save particle preset json: {}", e.what());
		return false;
	}
}

bool ParticlePresetLibrary::LoadFromJson(const std::string& presetName,ParticlePreset& outPreset,const std::string& directory)
{
	namespace fs = std::filesystem;

	fs::path path = fs::path(directory) / (presetName + ".json");
	if (!fs::exists(path)) {
		return false;
	}

	try {
		std::ifstream ifs(path);
		if (!ifs) {
			return false;
		}
		json j;
		ifs >> j;
		outPreset = j.get<ParticlePreset>();

		// 内部マップにも登録
		presets_[outPreset.name] = outPreset;
		return true;
	}
	catch (const std::exception& e) {
		Fatal(kChannel, "Failed to load particle preset json: {}", e.what());
		return false;
	}
}

void ParticlePresetLibrary::LoadAll(const std::string& directory)
{
	namespace fs = std::filesystem;

	presets_.clear();

	if (!fs::exists(directory)) {
		return;
	}

	for (auto& entry : fs::directory_iterator(directory)) {
		if (!entry.is_regular_file()) { continue; }
		if (entry.path().extension() != ".json") { continue; }

		try {
			std::ifstream ifs(entry.path());
			if (!ifs) { continue; }

			json j;
			ifs >> j;

			ParticlePreset preset = j.get<ParticlePreset>();
			if (!preset.name.empty()) {
				presets_[preset.name] = preset;
			}
		}
		catch (...) {
			// 壊れているファイルは無視
		}
	}
}

void ParticlePresetLibrary::Add(const ParticlePreset& preset)
{
	presets_[preset.name] = preset;
}

void ParticlePresetLibrary::Remove(const std::string& name)
{
	presets_.erase(name);
}

ParticlePreset* ParticlePresetLibrary::Find(const std::string& name)
{
	auto it = presets_.find(name);
	if (it == presets_.end()) return nullptr;
	return &it->second;
}

const ParticlePreset* ParticlePresetLibrary::Find(const std::string& name) const
{
	auto it = presets_.find(name);
	if (it == presets_.end()) return nullptr;
	return &it->second;
}

std::vector<std::string> ParticlePresetLibrary::GetAllNames() const
{
	std::vector<std::string> names;
	names.reserve(presets_.size());
	for (const auto& kv : presets_) {
		names.push_back(kv.first);
	}
	return names;
}