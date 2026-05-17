#include "ParticleSystemManager.h"

#include <fstream>
#include <filesystem>
#include <json.hpp>

#include "engine/unnamed/subsystem/console/Log.h"

#include "Particle/ParticleSystem.h"

static constexpr std::string_view kChannel = "PtclSysMgr";

using json = nlohmann::json;

// ===============================================
// 注意：このファイルは b-1 の段階では空の実装のみ
// b-3 以降で ParticleManager から実装を移植する
// ===============================================

// ===== 生成・検索 =====

ParticleSystem* ParticleSystemManager::Create(const std::string& systemName)
{
	// すでに同名 System があればそれを返す
	if (auto* existing = Find(systemName)) {
		return existing;
	}

	// 新しく System を作成して登録
	auto system = std::make_unique<ParticleSystem>(systemName);
	ParticleSystem* rawPtr = system.get();
	systems_.push_back(std::move(system));
	return rawPtr;
}

ParticleSystem* ParticleSystemManager::Find(const std::string& systemName)
{
	for (auto& system : systems_) {
		if (system && system->GetName() == systemName) {
			return system.get();
		}
	}
	return nullptr;
}

const ParticleSystem* ParticleSystemManager::Find(const std::string& systemName) const
{
	for (const auto& system : systems_) {
		if (system && system->GetName() == systemName) {
			return system.get();
		}
	}
	return nullptr;
}

bool ParticleSystemManager::Rename(const std::string& oldName, const std::string& newName)
{
	if (oldName.empty() || newName.empty() || oldName == newName) {
		return false;
	}

	ParticleSystem* target = Find(oldName);
	if (!target) {
		return false;
	}

	// 同名チェック
	for (const auto& sys : systems_) {
		if (sys && sys.get() != target && sys->GetName() == newName) {
			Warning(kChannel, "ParticleSystemManager::Rename : name already exists : {}", newName);
			return false;
		}
	}

	target->SetName(newName);
	return true;
}

void ParticleSystemManager::ClearAll()
{
	systems_.clear();
}

// ===== 一覧 =====

std::vector<std::string> ParticleSystemManager::GetAllNames() const
{
	std::vector<std::string> result;
	result.reserve(systems_.size());

	for (const auto& system : systems_) {
		if (system) {
			result.push_back(system->GetName());
		}
	}
	return result;
}

// ===== プリセット登録 =====

void ParticleSystemManager::RegisterPreset(const std::string& systemName,const std::string& presetName)
{
	if (systemName.empty() || presetName.empty()) {
		return;
	}

	// System を確保（なければ作る）
	ParticleSystem* system = Find(systemName);
	if (!system) {
		system = Create(systemName);
	}
	if (!system) {		
		Error(
			kChannel, 
			"ParticleSystemManager::RegisterPreset : failed to create/find system : {}",
			systemName
			);
		return;
	}

	// System 側にプリセット名を登録
	system->AddPresetName(presetName);
}

const std::vector<std::string>* ParticleSystemManager::GetPresets(const std::string& systemName) const
{
	if (systemName.empty()) { return nullptr; }

	for (const auto& system : systems_) {
		if (system && system->GetName() == systemName) {
			return &system->GetPresetNames();
		}
	}
	return nullptr;
}

// ===== Emitter 登録 =====

void ParticleSystemManager::RegisterEmitter(const std::string& systemName,ParticleEmitterInstance* emitter,float startTime,float duration,bool autoPlay)
{
	if (!emitter) {
		Error(
			kChannel,
			"ParticleSystemManager::RegisterEmitter : emitter is null"
			);
		return;
	}

	// System を確保（なければ作る）
	ParticleSystem* system = Find(systemName);
	if (!system) {
		system = Create(systemName);
	}
	if (!system) {
		Error(kChannel, "ParticleSystemManager::RegisterEmitter : failed to create/find system : {}", systemName);
		return;
	}

	// System に登録（所有権は呼び出し側 = ParticleManager に残す）
	system->AddEmitter(emitter, startTime, duration, autoPlay);
}

// ===== JSON =====

void ParticleSystemManager::EmitSystem(
	const std::string& systemName,
	const Mat4&        transform,
	const std::function<ParticleEmitterInstance*(
		const std::string& presetName,
		const Mat4&        transform
	)>& emitterFactory
)
{
	if (systemName.empty()) {
		return;
	}

	// System が存在するかチェック
	ParticleSystem* system = Find(systemName);
	if (!system) {
		return;
	}

	// この System に紐付いているプリセット名一覧
	const auto& presetNames = system->GetPresetNames();

	// Emitter がまだ作られていなければ新規作成
	// 作成自体は初回のみ。その後は再作成しない
	if (system->GetEmitters().empty()) {
		for (const std::string& presetName : presetNames) {
			// EmitterInstance の生成は呼び出し側のコールバックに委譲
			// （DirectX 依存の処理を ParticleSystemManager に持ち込まないため）
			if (emitterFactory) {
				emitterFactory(presetName, transform);
			}
		}
	}

	// 各エミッターの Transform を最新に更新
	for (auto& entry : system->GetEmitters()) {
		if (entry.emitter) {
			entry.emitter->SetTransform(transform);
		}
	}

	// System 全体を Play する
	// ParticleSystem::Update() 側で startTime に応じて各 Emitter が Play される
	// 既に再生中なら何もしない
	if (!system->IsPlaying()) {
		system->Play();
	}
}

bool ParticleSystemManager::SaveToJson(const std::string& systemName,const std::string& directory)
{
	if (systemName.empty()) {
		return false;
	}

	ParticleSystem* system = Find(systemName);
	if (!system) {
		Error(kChannel, "ParticleSystemManager::SaveToJson : system not found : {}", systemName);
		return false;
	}

	namespace fs = std::filesystem;

	// ディレクトリがなければ作る
	if (!fs::exists(directory)) {
		fs::create_directories(directory);
	}

	fs::path path = fs::path(directory) / (systemName + ".json");

	json j;

	// ===== 基本情報 =====
	j["name"] = system->GetName();

	// ===== System 全体の設定 =====
	j["loop"] = system->IsLoop();
	j["duration"] = system->GetDuration();

	// ===== emitters 配列形式で保存 =====
	json emittersJson = json::array();

	const auto& presetNames = system->GetPresetNames();

	for (const std::string& presetName : presetNames) {
		// デフォルト値
		float startTime = 0.0f;
		float duration = -1.0f;
		bool  autoPlay = true;

		// 既存の Emitter から startTime/duration を取得
		auto* entry = system->FindEntryByPresetName(presetName);
		if (entry) {
			startTime = entry->startTime;
			duration = entry->duration;
			autoPlay = entry->autoPlay;
		}
		else {
			// Emitter がまだ作られていない場合、pendingSettings から取得
			for (const auto& p : system->GetPendingSettings()) {
				if (p.presetName == presetName) {
					startTime = p.startTime;
					duration = p.duration;
					autoPlay = p.autoPlay;
					break;
				}
			}
		}

		json e;
		e["preset"] = presetName;
		e["startTime"] = startTime;
		e["duration"] = duration;
		e["autoPlay"] = autoPlay;

		emittersJson.push_back(e);
	}

	j["emitters"] = emittersJson;
	j["presets"] = presetNames;

	std::ofstream ofs(path);
	if (!ofs) {
		Error(kChannel, "ParticleSystemManager::SaveToJson : failed to open file : {}", path.string());
		return false;
	}

	ofs << j.dump(4);
	return true;
}

bool ParticleSystemManager::LoadFromJson(const std::string& systemName,const std::string& directory)
{
	if (systemName.empty()) {
		return false;
	}

	namespace fs = std::filesystem;
	fs::path path = fs::path(directory) / (systemName + ".json");

	if (!fs::exists(path)) {
		Error(kChannel, "ParticleSystemManager::LoadFromJson : file not found : {}", path.string());
		return false;
	}

	std::ifstream ifs(path);
	if (!ifs) {
		Error(kChannel, "ParticleSystemManager::LoadFromJson : failed to open file : {}", path.string());
		return false;
	}

	json j;
	ifs >> j;

	std::string nameInJson = j.value("name", systemName);
	if (nameInJson.empty()) {
		nameInJson = systemName;
	}

	// System を作成 or 取得
	ParticleSystem* system = Create(nameInJson);
	if (!system) {
		Error(kChannel, "ParticleSystemManager::LoadFromJson : failed to create system : {}", nameInJson);
		return false;
	}

	// いったんプリセット名リストと pending 設定をクリア
	system->ClearPresetNames();
	system->ClearPendingSettings();

	// ===== System 全体の設定 =====
	if (j.contains("loop")) {
		system->SetLoop(j.value("loop", false));
	}
	if (j.contains("duration")) {
		system->SetDuration(j.value("duration", 0.0f));
	}

	// ===== emitters 配列を優先的に読む =====
	auto emittersIt = j.find("emitters");
	if (emittersIt != j.end() && emittersIt->is_array()) {

		for (const auto& elem : *emittersIt) {
			if (!elem.is_object()) continue;

			std::string presetName = elem.value("preset", std::string());
			if (presetName.empty()) continue;

			float startTime = elem.value("startTime", 0.0f);
			float duration = elem.value("duration", -1.0f);
			bool  autoPlay = elem.value("autoPlay", true);

			// System 側に登録
			system->AddPresetName(presetName);

			// startTime/duration は pendingSettings に保存（Emitter 作成時に反映）
			ParticleSystem::PendingEmitterSetting setting;
			setting.presetName = presetName;
			setting.startTime = startTime;
			setting.duration = duration;
			setting.autoPlay = autoPlay;
			system->GetPendingSettings().push_back(setting);
		}
	}
	else {
		// ===== presets 配列のみ =====
		auto presetsIt = j.find("presets");
		if (presetsIt != j.end() && presetsIt->is_array()) {
			for (const auto& elem : *presetsIt) {
				if (elem.is_string()) {
					std::string presetName = elem.get<std::string>();
					if (!presetName.empty()) {
						system->AddPresetName(presetName);

						// デフォルト値で pending 設定も追加
						ParticleSystem::PendingEmitterSetting setting;
						setting.presetName = presetName;
						setting.startTime = 0.0f;
						setting.duration = -1.0f;
						setting.autoPlay = true;
						system->GetPendingSettings().push_back(setting);
					}
				}
			}
		}
	}

	return true;
}

void ParticleSystemManager::LoadAll(const std::string& directory)
{
	namespace fs = std::filesystem;

	ClearAll();

	if (!fs::exists(directory)) {
		return;
	}

	for (auto& entry : fs::directory_iterator(directory)) {
		if (!entry.is_regular_file()) {
			continue;
		}

		fs::path path = entry.path();
		if (path.extension() != ".json") {
			continue;
		}

		// ファイル名(拡張子なし)を systemName として扱う
		std::string systemName = path.stem().string();
		LoadFromJson(systemName, directory);
	}
}

// ===== Update 駆動 =====

void ParticleSystemManager::UpdateAll(float dt)
{
	for (auto& system : systems_) {
		if (system) {
			system->Update(dt);
		}
	}
}
