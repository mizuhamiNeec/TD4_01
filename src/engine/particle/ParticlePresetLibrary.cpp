#include "ParticlePresetLibrary.h"

#include <fstream>
#include <filesystem>
#include <json.hpp>

#include "core/math/Vec4.h"

#include "engine/unnamed/subsystem/console/Log.h"

#include "Particle/ParticlePreset.h"
#include "Particle/Modules/ParticleModuleRegistry.h"

static constexpr std::string_view kChannel = "PPL";

using json = nlohmann::json;

// ======================================================================
//  共通ヘルパー（Vec / Random / Curve / Gradient の JSON 変換）
// ======================================================================
namespace {

	json WriteVec3(const Vec3& v)
	{
		return json::array({ v.x, v.y, v.z });
	}

	json WriteVec4(const Vec4& v)
	{
		return json::array({ v.x, v.y, v.z, v.w });
	}

	Vec3 ReadVec3(const json& j, const Vec3& def)
	{
		Vec3 v = def;
		if (j.is_array() && j.size() == 3) {
			v.x = j[0].get<float>();
			v.y = j[1].get<float>();
			v.z = j[2].get<float>();
		}
		return v;
	}

	Vec4 ReadVec4(const json& j, const Vec4& def)
	{
		Vec4 v = def;
		if (j.is_array() && j.size() == 4) {
			v.x = j[0].get<float>();
			v.y = j[1].get<float>();
			v.z = j[2].get<float>();
			v.w = j[3].get<float>();
		}
		return v;
	}

	// RandomRange3 -> { useRandom, min[3], max[3] }
	json WriteRandom3(const RandomRange3& r)
	{
		return json{
			{ "useRandom", r.useRandom },
			{ "min", WriteVec3(r.minValue) },
			{ "max", WriteVec3(r.maxValue) },
		};
	}

	RandomRange3 ReadRandom3(const json& j, const RandomRange3& def)
	{
		RandomRange3 r = def;
		if (j.is_object()) {
			r.useRandom = j.value("useRandom", def.useRandom);
			if (j.contains("min")) { r.minValue = ReadVec3(j["min"], def.minValue); }
			if (j.contains("max")) { r.maxValue = ReadVec3(j["max"], def.maxValue); }
		}
		return r;
	}

	// Curve1D -> { enabled, keys:[[t,v],...] }
	json WriteCurve1D(const Curve1D& c)
	{
		json keys = json::array();
		for (const auto& k : c.keys) {
			keys.push_back(json::array({ k.t, k.v }));
		}
		return json{ { "enabled", c.enabled }, { "keys", keys } };
	}

	Curve1D ReadCurve1D(const json& j)
	{
		Curve1D c;
		if (j.is_object()) {
			c.enabled = j.value("enabled", false);
			if (j.contains("keys") && j["keys"].is_array()) {
				for (const auto& elem : j["keys"]) {
					if (elem.is_array() && elem.size() == 2) {
						CurveKey key;
						key.t = elem[0].get<float>();
						key.v = elem[1].get<float>();
						c.keys.push_back(key);
					}
				}
			}
		}
		return c;
	}

	// ColorGradient -> { enabled, keys:[[t,r,g,b,a],...] }
	json WriteGradient(const ColorGradient& g)
	{
		json keys = json::array();
		for (const auto& k : g.keys) {
			keys.push_back(json::array({
				k.t, k.color.x, k.color.y, k.color.z, k.color.w }));
		}
		return json{ { "enabled", g.enabled }, { "keys", keys } };
	}

	ColorGradient ReadGradient(const json& j)
	{
		ColorGradient g;
		if (j.is_object()) {
			g.enabled = j.value("enabled", false);
			if (j.contains("keys") && j["keys"].is_array()) {
				for (const auto& elem : j["keys"]) {
					if (elem.is_array() && elem.size() == 5) {
						ColorKey4 key;
						key.t = elem[0].get<float>();
						key.color.x = elem[1].get<float>();
						key.color.y = elem[2].get<float>();
						key.color.z = elem[3].get<float>();
						key.color.w = elem[4].get<float>();
						g.keys.push_back(key);
					}
				}
			}
		}
		return g;
	}

	// ------------------------------------------------------------------
	//  モジュール種別ごとの params 変換
	//
	//  ※ パラメータの実体は ParticlePreset の各サブ構造体が持つ。
	//    ここはモジュール種別ごとに「どのフィールドが自分の params か」を
	//    知っていて、JSON との橋渡しをするだけ。
	// ------------------------------------------------------------------
	json WriteModuleParams(const std::string& type, const ParticlePreset& p)
	{
		json params = json::object();

		if (type == "Lifetime") {
			params["lifeTime"] = p.particleUpdate.lifeTime;
		}
		else if (type == "Location") {
			params["useRandomPosition"]   = p.emitterSpawn.useRandomPosition;
			params["initialOffset"]       = WriteVec3(p.particleSpawn.initialOffset);
			params["initialOffsetRandom"] = WriteRandom3(p.particleSpawn.initialOffsetRandom);
		}
		else if (type == "Velocity") {
			params["velocity"]       = WriteVec3(p.particleUpdate.velocity);
			params["velocityRandom"] = WriteRandom3(p.particleUpdate.velocityRandom);
			params["useGravity"]     = p.particleUpdate.useGravity;
		}
		else if (type == "Rotation") {
			params["initialRotate"]       = WriteVec3(p.particleSpawn.initialRotate);
			params["initialRotateRandom"] = WriteRandom3(p.particleSpawn.initialRotateRandom);
			params["rotationSpeed"]       = WriteVec3(p.particleUpdate.rotationSpeed);
			params["rotationRandom"]      = WriteRandom3(p.particleUpdate.rotationRandom);
		}
		else if (type == "Size") {
			params["initialScale"]       = WriteVec3(p.particleSpawn.initialScale);
			params["initialScaleRandom"] = WriteRandom3(p.particleSpawn.initialScaleRandom);
			params["scaleSpeed"]         = WriteVec3(p.particleUpdate.scaleSpeed);
			params["scaleRandom"]        = WriteRandom3(p.particleUpdate.scaleRandom);
			params["scaleCurve"]         = WriteCurve1D(p.particleUpdate.scaleCurve);
		}
		else if (type == "Color") {
			params["color"]             = WriteVec4(p.render.color);
			params["colorCurve"]        = WriteCurve1D(p.render.colorCurve);
			params["colorGradient"]     = WriteGradient(p.render.colorGradient);
			params["gradientTimeCurve"] = WriteCurve1D(p.render.gradientTimeCurve);
		}

		return params;
	}

	void ReadModuleParams(const std::string& type, const json& params, ParticlePreset& p)
	{
		if (!params.is_object()) { return; }

		if (type == "Lifetime") {
			p.particleUpdate.lifeTime = params.value("lifeTime", p.particleUpdate.lifeTime);
		}
		else if (type == "Location") {
			p.emitterSpawn.useRandomPosition =
				params.value("useRandomPosition", p.emitterSpawn.useRandomPosition);
			if (params.contains("initialOffset")) {
				p.particleSpawn.initialOffset =
					ReadVec3(params["initialOffset"], p.particleSpawn.initialOffset);
			}
			if (params.contains("initialOffsetRandom")) {
				p.particleSpawn.initialOffsetRandom =
					ReadRandom3(params["initialOffsetRandom"], p.particleSpawn.initialOffsetRandom);
			}
		}
		else if (type == "Velocity") {
			if (params.contains("velocity")) {
				p.particleUpdate.velocity =
					ReadVec3(params["velocity"], p.particleUpdate.velocity);
			}
			if (params.contains("velocityRandom")) {
				p.particleUpdate.velocityRandom =
					ReadRandom3(params["velocityRandom"], p.particleUpdate.velocityRandom);
			}
			p.particleUpdate.useGravity = params.value("useGravity", p.particleUpdate.useGravity);
		}
		else if (type == "Rotation") {
			if (params.contains("initialRotate")) {
				p.particleSpawn.initialRotate =
					ReadVec3(params["initialRotate"], p.particleSpawn.initialRotate);
			}
			if (params.contains("initialRotateRandom")) {
				p.particleSpawn.initialRotateRandom =
					ReadRandom3(params["initialRotateRandom"], p.particleSpawn.initialRotateRandom);
			}
			if (params.contains("rotationSpeed")) {
				p.particleUpdate.rotationSpeed =
					ReadVec3(params["rotationSpeed"], p.particleUpdate.rotationSpeed);
			}
			if (params.contains("rotationRandom")) {
				p.particleUpdate.rotationRandom =
					ReadRandom3(params["rotationRandom"], p.particleUpdate.rotationRandom);
			}
		}
		else if (type == "Size") {
			if (params.contains("initialScale")) {
				p.particleSpawn.initialScale =
					ReadVec3(params["initialScale"], p.particleSpawn.initialScale);
			}
			if (params.contains("initialScaleRandom")) {
				p.particleSpawn.initialScaleRandom =
					ReadRandom3(params["initialScaleRandom"], p.particleSpawn.initialScaleRandom);
			}
			if (params.contains("scaleSpeed")) {
				p.particleUpdate.scaleSpeed =
					ReadVec3(params["scaleSpeed"], p.particleUpdate.scaleSpeed);
			}
			if (params.contains("scaleRandom")) {
				p.particleUpdate.scaleRandom =
					ReadRandom3(params["scaleRandom"], p.particleUpdate.scaleRandom);
			}
			if (params.contains("scaleCurve")) {
				p.particleUpdate.scaleCurve = ReadCurve1D(params["scaleCurve"]);
			}
		}
		else if (type == "Color") {
			if (params.contains("color")) {
				p.render.color = ReadVec4(params["color"], p.render.color);
			}
			if (params.contains("colorCurve")) {
				p.render.colorCurve = ReadCurve1D(params["colorCurve"]);
			}
			if (params.contains("colorGradient")) {
				p.render.colorGradient = ReadGradient(params["colorGradient"]);
			}
			if (params.contains("gradientTimeCurve")) {
				p.render.gradientTimeCurve = ReadCurve1D(params["gradientTimeCurve"]);
			}
		}
	}

	// 旧形式（version 無し・particleUpdate / render が直下にある形式）の読み込み。
	// 既存のセーブデータを読めるよう、従来の処理をそのまま残す。
	void ReadLegacyPreset(const json& j, ParticlePreset& p)
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
		}
		else {
			p.particleUpdate.lifeTime = j.value("lifeTime", 1.0f);
			p.particleUpdate.velocity = readVec3("velocity", { 0,0,0 });
			p.particleUpdate.rotationSpeed = readVec3("rotationSpeed", { 0,0,0 });
			p.particleUpdate.scaleSpeed = readVec3("scaleSpeed", { 0,0,0 });
			p.particleUpdate.useGravity = j.value("useGravity", false);

			p.particleUpdate.velocityRandom = RandomRange3{};
			p.particleUpdate.rotationRandom = RandomRange3{};
			p.particleUpdate.scaleRandom = RandomRange3{};
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

			if (r.contains("colorCurve")) {
				p.render.colorCurve = ReadCurve1D(r["colorCurve"]);
			}
			if (r.contains("colorGradient")) {
				p.render.colorGradient = ReadGradient(r["colorGradient"]);
			}
			if (r.contains("gradientTimeCurve")) {
				p.render.gradientTimeCurve = ReadCurve1D(r["gradientTimeCurve"]);
			}
		}
		else {
			p.render.color = readVec4("color", { 1,1,1,1 });
			p.render.useBillboard = j.value("useBillboard", true);
			p.render.flipY = j.value("flipY", false);
		}
	}

} // namespace

// ======================================================================
//  JSON 書き出し（常に version 2 = モジュールリスト形式で出力）
// ======================================================================
void to_json(json& j, const ParticlePreset& p)
{
	j = json::object();
	j["version"] = 2;
	j["name"] = p.name;

	// ---- emitter 設定（ビルボード等の描画プロパティもここに含める） ----
	j["emitterSettings"] = {
		{ "vertexType",      static_cast<int>(p.emitterSettings.vertexType) },
		{ "textureFilePath", p.emitterSettings.textureFilePath },
		{ "blendMode",       static_cast<int>(p.emitterSettings.blendMode) },
		{ "useBillboard",    p.render.useBillboard },
		{ "flipY",           p.render.flipY },
	};

	// ---- emitter スポーン設定 ----
	j["emitterSpawn"] = {
		{ "count",         p.emitterSpawn.count },
		{ "frequency",     p.emitterSpawn.frequency },
		{ "repeat",        p.emitterSpawn.repeat },
		{ "useLocalSpace", p.emitterSpawn.useLocalSpace },
	};

	// ---- モジュールスタック ----
	// preset.modules が空（コード生成プリセット等）なら既定スタックを出力する。
	std::vector<ModuleSlot> slots = p.modules;
	if (slots.empty()) {
		for (const auto& type : ParticleModuleRegistry::Get().GetDefaultOrder()) {
			slots.push_back(ModuleSlot{ type, true });
		}
	}

	json modulesJson = json::array();
	for (const auto& slot : slots) {
		modulesJson.push_back(json{
			{ "type",    slot.type },
			{ "enabled", slot.enabled },
			{ "params",  WriteModuleParams(slot.type, p) },
			});
	}
	j["modules"] = modulesJson;
}

// ======================================================================
//  JSON 読み込み（version 2 と旧形式の両方に対応）
// ======================================================================
void from_json(const json& j, ParticlePreset& p)
{
	p = ParticlePreset{};

	// modules 配列があれば version 2（モジュールリスト形式）とみなす
	const bool newFormat = j.contains("modules") && j["modules"].is_array();

	if (!newFormat) {
		// ---- 旧形式 ----
		ReadLegacyPreset(j, p);
		// 旧データにはモジュールスタックが無いので、既定スタックを与えて
		// 次回保存時に version 2 形式へ自動移行されるようにする。
		for (const auto& type : ParticleModuleRegistry::Get().GetDefaultOrder()) {
			p.modules.push_back(ModuleSlot{ type, true });
		}
		return;
	}

	// ---- version 2: モジュールリスト形式 ----
	p.name = j.value("name", "");

	if (j.contains("emitterSettings") && j["emitterSettings"].is_object()) {
		const auto& es = j["emitterSettings"];
		p.emitterSettings.vertexType = static_cast<VertexDataType>(
			es.value("vertexType", static_cast<int>(VertexDataType::Plane)));
		p.emitterSettings.textureFilePath = es.value("textureFilePath", "");
		p.emitterSettings.blendMode = static_cast<BlendMode>(
			es.value("blendMode", static_cast<int>(kBlendModeNormal)));
		p.render.useBillboard = es.value("useBillboard", true);
		p.render.flipY = es.value("flipY", false);
	}

	if (j.contains("emitterSpawn") && j["emitterSpawn"].is_object()) {
		const auto& sp = j["emitterSpawn"];
		p.emitterSpawn.count = sp.value("count", 10u);
		p.emitterSpawn.frequency = sp.value("frequency", 1.0f);
		p.emitterSpawn.repeat = sp.value("repeat", false);
		p.emitterSpawn.useLocalSpace = sp.value("useLocalSpace", false);
	}

	// モジュール配列：種類・順序・有効状態を読み、params をサブ構造体へ展開する
	for (const auto& entry : j["modules"]) {
		if (!entry.is_object()) { continue; }

		ModuleSlot slot;
		slot.type = entry.value("type", "");
		slot.enabled = entry.value("enabled", true);
		if (slot.type.empty()) { continue; }

		if (entry.contains("params")) {
			ReadModuleParams(slot.type, entry["params"], p);
		}
		p.modules.push_back(slot);
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
