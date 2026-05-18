#include "ParticleManager.h"

#pragma comment(lib, "dxcompiler.lib")

#include <DirectXTex.h>
#include <json.hpp>
#include <numbers>

#include "../ParticleEmitterInstance.h"
#include "../ParticleSystem.h"

#include "engine/unnamed/subsystem/console/Log.h"

static constexpr std::string_view kChannel = "PtclMgr";

using json = nlohmann::json;

void ParticleManager::Initialize(VertexDataType type)
{
	dxCommon_ = DirectXCommon::GetInstance();
	srvManager_ = SrvManager::GetInstance();
	// ランダムエンジンの初期化
	randomEngine.seed(seedGenerator());

	// 各ブレンドモードのPSOを事前に生成してキャッシュ
	for (int mode = kBlendModeNone; mode < kCountOfBlendMode; ++mode) {
		GraphicsPipelineState(static_cast<BlendMode>(mode));
	}

	switch (type) {
	case VertexDataType::Plane:
		// 頂点リソースに頂点データを書き込む
		CreateVertexData();
		ringSamplerAdd = false;
		break;
	case VertexDataType::Ring:
		CreateRingVertexData();
		ringSamplerAdd = true;
		break;
	case VertexDataType::Cylinder:
		CreateCylinderVertexData();
		ringSamplerAdd = false;
	}

	// 頂点リソース、バッファービュー
	VertexBufferView();

	//書き込むためのアドレスを取得
	vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
	//頂点データをリソースにコピー
	std::memcpy(vertexData, modelData.vertices.data(), sizeof(VertexData) * modelData.vertices.size());

	BuildVertexBuffer(VertexDataType::Plane);
	BuildVertexBuffer(VertexDataType::Ring);
	BuildVertexBuffer(VertexDataType::Cylinder);
}

void ParticleManager::Update()
{
	if (!camera_) {
		return; // カメラが設定されていない場合は何もしない
	}

	// ========= 時間の取得（タイムスケール対応） =========
	float dt = TimeManager::GetInstance()->GetDeltaTime();

	// ========= カメラ行列の計算 =========
	// カメラが持っている ViewProjection 行列をそのまま使う
	// これにより fov / aspect / near / far の不一致が起きない
	Mat4 viewProjectionMatrix = camera_->GetViewProjectionMatrix();

	// ビルボード用にカメラ行列も取得
	Mat4 cameraMatrix = Mat4::Affine(
		{ 1.0f,1.0f,1.0f },
		camera_->GetRotate(),
		camera_->GetTranslate()
	);

	// ========= ビルボード行列の計算 =========
	Mat4 backToFrontMatrix = Mat4::RotateY(std::numbers::pi_v<float>);
	Mat4 billboardMatrix{};
	if (usebillboardMatrix) {
		billboardMatrix = backToFrontMatrix * cameraMatrix;
		// 平行移動は無視
		billboardMatrix.m[3][0] = 0.0f;
		billboardMatrix.m[3][1] = 0.0f;
		billboardMatrix.m[3][2] = 0.0f;
	}
	else {
		billboardMatrix = Mat4::identity;
	}

	// 1) System を更新（Emitter の Play/Stop タイミング制御だけ）
	systemManager_.UpdateAll(dt);

	// 2) すべての EmitterInstance を更新（シミュレーション）
	UpdateEmitters(dt);

	// 3) EmitterInstance → particleGroups のインスタンスバッファに詰める
	PopulateInstancesFromEmitters(viewProjectionMatrix, billboardMatrix);

}

void ParticleManager::Draw()
{
	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList().Get();

	// ルートシグネチャは一回だけ
	commandList->SetGraphicsRootSignature(rootSignature_.Get());

	// プリミティブトポロジ & VBV は共通
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	/*commandList->IASetVertexBuffers(0, 1, &vertexBufferView);*/

	SrvManager* srvManager = SrvManager::GetInstance();

	// 全てのパーティクルグループ
	for (auto& pair : particleGroups) {
		auto& group = pair.second;
		if (group.instanceCount == 0) {
			continue;
		}

		// グループの形状に対応する頂点バッファを取得 =====
		auto vbIt = vertexBuffers_.find(group.vertexType);
		if (vbIt == vertexBuffers_.end()) {
			// 未対応形状（Trailなど）はスキップ
			group.instanceCount = 0;
			continue;
		}
		const auto& vb = vbIt->second;

		// グループごとに頂点バッファを切り替え
		commandList->IASetVertexBuffers(0, 1, &vb.view);

		// このグループ専用のブレンドモードから PSO を取得
		auto psoIt = pipelineStateCache_.find(group.blendMode);
		if (psoIt == pipelineStateCache_.end() || !psoIt->second) {
			// 念のため、無ければノーマルにフォールバック			
			Warning(kChannel, "PSO for group blend mode not found, fallback to normal.");
			psoIt = pipelineStateCache_.find(kBlendModeNormal);
			if (psoIt == pipelineStateCache_.end() || !psoIt->second) {
				Warning(kChannel, "Default PSO not found. Skip this group.");
				continue;
			}
		}

		// グループごとに PSO をセット
		commandList->SetPipelineState(psoIt->second.Get());

		Vec2 textureLeftTop = group.textureLeftTop;
		Vec2 textureSize = group.textureSize;

		for (auto& particle : group.particleList) {
			// 必要ならUV計算の処理など
		}

		commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
		commandList->SetGraphicsRootDescriptorTable(2, srvManager->GetGPUDescriptorHandle(group.srvIndex));
		commandList->SetGraphicsRootDescriptorTable(1, srvManager->GetGPUDescriptorHandle(group.instancingSrvIndex));

		commandList->DrawInstanced(vb.vertexCount, group.instanceCount, 0, 0);

		group.instanceCount = 0;
	}
}

void ParticleManager::DrawImGuiParticlePresetEditor()
{
#ifdef USE_IMGUI

	using namespace ImGui;
	namespace fs = std::filesystem;

	static ParticlePreset preset;          // 現在編集中のプリセット
	static bool initialized = false;

	// テクスチャ候補（Resources 以下のみ）
	static std::vector<std::string> textureList;
	static int currentTextureIndex = -1;

	if (!initialized) {
		// 初期値
		preset.name = "NewParticle";
		preset.emitterSettings.vertexType = VertexDataType::Plane;
		preset.emitterSettings.textureFilePath = "";
		preset.emitterSettings.blendMode = kBlendModeNormal;
		preset.emitterSpawn.count = 10;
		preset.emitterSpawn.frequency = 1.0f;
		preset.emitterSpawn.repeat = false;
		preset.emitterSpawn.useRandomPosition = true;
		preset.particleSpawn.initialScale = { 1.0f, 1.0f, 1.0f };
		preset.particleSpawn.initialRotate = { 0.0f, 0.0f, 0.0f };
		preset.particleSpawn.initialOffset = { 0.0f, 0.0f, 0.0f };
		preset.particleUpdate.lifeTime = 1.0f;
		preset.particleUpdate.velocity = { 0.0f, 0.0f, 0.0f };
		preset.particleUpdate.rotationSpeed = { 0.0f, 0.0f, 0.0f };
		preset.particleUpdate.scaleSpeed = { 0.0f, 0.0f, 0.0f };
		preset.particleUpdate.useGravity = false;
		preset.render.color = { 1.0f, 1.0f, 1.0f, 1.0f };
		preset.render.useBillboard = true;
		preset.render.flipY = false;

		// Resources 以下からテクスチャ候補を列挙
		textureList.clear();
		const std::string textureDir = "Resources/ParticleTexture";

		if (fs::exists(textureDir)) {
			for (auto& entry : fs::directory_iterator(textureDir)) {
				if (!entry.is_regular_file()) { continue; }
				auto ext = entry.path().extension().string();
				std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
				if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".dds") {
					// ファイル名だけを保持する
					textureList.push_back(entry.path().filename().string());
				}
			}
		}
		if (!textureList.empty()) {
			currentTextureIndex = 0;
			// JSON に保存するパスは必ず Resources/ParticleTexture/ から始める
			preset.emitterSettings.textureFilePath = "Resources/ParticleTexture/" + textureList[0];
		}

		initialized = true;
	}

	// ウィンドウタイトル
	if (!Begin("パーティクルプリセットエディタ")) {
		End();
		return;
	}

	//==========================
	// プリセット作成/読み込みボタン
	//==========================
	if (ImGui::Button("新しいParticleの作成")) {
		// デフォルト値でリセット
		preset.name = "NewParticle";
		preset.emitterSettings.vertexType = VertexDataType::Plane;
		preset.emitterSettings.blendMode = kBlendModeNormal;
		preset.emitterSpawn.count = 10;
		preset.emitterSpawn.frequency = 1.0f;
		preset.emitterSpawn.repeat = false;
		preset.emitterSpawn.useRandomPosition = true;
		preset.particleSpawn.initialScale = { 1.0f, 1.0f, 1.0f };
		preset.particleSpawn.initialRotate = { 0.0f, 0.0f, 0.0f };
		preset.particleSpawn.initialOffset = { 0.0f, 0.0f, 0.0f };
		preset.particleUpdate.lifeTime = 1.0f;
		preset.particleUpdate.velocity = { 0.0f, 0.0f, 0.0f };
		preset.particleUpdate.rotationSpeed = { 0.0f, 0.0f, 0.0f };
		preset.particleUpdate.scaleSpeed = { 0.0f, 0.0f, 0.0f };
		preset.particleUpdate.useGravity = false;
		preset.render.color = { 1.0f, 1.0f, 1.0f, 1.0f };
		preset.render.useBillboard = true;
		preset.render.flipY = false;

		if (!textureList.empty()) {
			currentTextureIndex = 0;
			preset.emitterSettings.textureFilePath = "Resources/ParticleTexture/" + textureList[0];
		}
		else {
			currentTextureIndex = -1;
			preset.emitterSettings.textureFilePath.clear();
		}
	}
	SameLine();
	if (ImGui::Button("既存のParticleを編集")) {
		ImGui::OpenPopup("既存プリセット選択");
	}

	// ポップアップ：既存プリセット一覧
	if (ImGui::BeginPopup("既存プリセット選択")) {
		if (presetLibrary_.GetAll().empty()) {
			ImGui::TextColored(ImVec4(1, 0.5f, 0.5f, 1), "読み込まれているプリセットがありません");
		}
		else {
			ImGui::Text("編集するプリセットを選択してください");
			ImGui::Separator();

			for (auto& kv : presetLibrary_.GetAll()) {
				const std::string& presetName = kv.first;
				if (ImGui::Selectable(presetName.c_str())) {
					// 選択したプリセットの内容を現在の編集プリセットにコピー
					preset = kv.second;

					// テクスチャ名から currentTextureIndex を合わせる
					currentTextureIndex = -1;
					if (!preset.emitterSettings.textureFilePath.empty()) {
						std::string fileName = fs::path(preset.emitterSettings.textureFilePath).filename().string();
						for (int i = 0; i < (int)textureList.size(); ++i) {
							if (textureList[i] == fileName) {
								currentTextureIndex = i;
								break;
							}
						}
					}

					ImGui::CloseCurrentPopup();
				}
			}
		}
		ImGui::EndPopup();
	}

	ImGui::Separator();

	// --- プリセット名 ---
	{
		char buf[64];
		// 安全な strncpy_s を使用
		strncpy_s(buf, sizeof(buf), preset.name.c_str(), _TRUNCATE);
		if (InputText("プリセット名", buf, sizeof(buf))) {
			preset.name = buf;
		}
	}

	// --- 形状（メッシュ）選択 ---
	{
		const char* items[] = { "板(Plane)", "リング(Ring)", "円柱(Cylinder)" };
		int current = static_cast<int>(preset.emitterSettings.vertexType);
		if (Combo("形状タイプ", &current, items, IM_ARRAYSIZE(items))) {
			preset.emitterSettings.vertexType = static_cast<VertexDataType>(current);
		}
	}

	// --- ブレンドモード選択 ---
	{
		const char* blendItems[] = {
			"なし(None)",
			"通常(Normal)",
			"加算(Add)",
			"減算(Subtract)",
			"乗算(Multiply)",
			"スクリーン(Screen)"
		};
		int current = static_cast<int>(preset.emitterSettings.blendMode);
		if (Combo("ブレンドモード", &current, blendItems, IM_ARRAYSIZE(blendItems))) {
			preset.emitterSettings.blendMode = static_cast<BlendMode>(current);
		}
	}

	// --- テクスチャ選択（Resources/ParticleTexture 以下のファイル） ---
	if (!textureList.empty()) {
		if (currentTextureIndex < 0 || currentTextureIndex >= (int)textureList.size()) {
			currentTextureIndex = 0;
		}

		const char* previewText = textureList[currentTextureIndex].c_str();
		if (ImGui::BeginCombo("テクスチャファイル", previewText)) {

			// SRV マネージャ取得（プレビュー用）
			SrvManager* srvManager = SrvManager::GetInstance();

			for (int i = 0; i < (int)textureList.size(); ++i) {
				bool isSelected = (i == currentTextureIndex);
				const std::string& fileName = textureList[i];

				if (ImGui::Selectable(fileName.c_str(), isSelected)) {
					currentTextureIndex = i;

					// JSON 用のパスを更新
					preset.emitterSettings.textureFilePath = "Resources/ParticleTexture/" + fileName;
				}

				// 選択中項目のチェックマーク
				if (isSelected) {
					ImGui::SetItemDefaultFocus();
				}

				// マウスが乗ったらプレビュー画像を表示
				if (ImGui::IsItemHovered()) {
					ImGui::BeginTooltip();
					{
						// テクスチャをロード（既にロード済みなら内部でキャッシュされている）
						std::string fullPath = "Resources/ParticleTexture/" + fileName;
						TextureManager::GetInstance()->LoadTexture(fullPath);
						uint32_t texIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(fullPath);

						// SRV の GPU ハンドルを取得
						D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = srvManager->GetGPUDescriptorHandle(texIndex);

						// ImGui::Image に渡す ID を作成（DX12 実装に合わせてキャスト）
						ImTextureID imguiTexId = (ImTextureID)gpuHandle.ptr;

						ImVec2 previewSize(256.0f, 256.0f);
						ImGui::Image(imguiTexId, previewSize);
					}
					ImGui::EndTooltip();
				}
			}

			ImGui::EndCombo();
		}
	}
	else {
		ImGui::TextColored(ImVec4(1, 0, 0, 1),
			"Resources/ParticleTexture 以下にテクスチャが見つかりません");
	}

	// --- エミッター設定 ---
	Separator();
	Text("エミッタ設定");
	DragInt("発生数", (int*)&preset.emitterSpawn.count, 1, 1, 1000);
	DragFloat("発生間隔(秒)", &preset.emitterSpawn.frequency, 0.01f, 0.0f, 60.0f);
	Checkbox("繰り返し再生", &preset.emitterSpawn.repeat);
	Checkbox("ランダム位置を使用", &preset.emitterSpawn.useRandomPosition);
	Checkbox("ローカル空間モード", &preset.emitterSpawn.useLocalSpace);

	// --- パーティクル共通 ---
	Separator();
	Text("パーティクル共通設定");
	DragFloat3("初期スケール(scale)", &preset.particleSpawn.initialScale.x, 0.01f, 0.0f, 100.0f);
	DragFloat3("初期回転(rotate)", &preset.particleSpawn.initialRotate.x, 0.01f, -6.28f, 6.28f);
	DragFloat3("初期場所(translate)", &preset.particleSpawn.initialOffset.x, 0.01f, -1000.0f, 1000.0f);
	DragFloat3("初期速度 (Velocity)", &preset.particleUpdate.velocity.x, 0.01f, -1000.0f, 1000.0f);
	DragFloat3("回転速度(rad/秒)", &preset.particleUpdate.rotationSpeed.x, 0.01f, -10.0f, 10.0f);
	DragFloat3("スケール速度(/秒)", &preset.particleUpdate.scaleSpeed.x, 0.01f, -10.0f, 10.0f);

	// 色(RGBA)
	ColorEdit4("カラー (RGBA)", &preset.render.color.x);
	// ビルボードON/OFF
	Checkbox("常にカメラ目線(ビルボード)", &preset.render.useBillboard);
	Checkbox("上下反転(Flip Y)", &preset.render.flipY);
	DragFloat("寿命(秒)", &preset.particleUpdate.lifeTime, 0.01f, 0.0f, 100.0f);
	// 重力
	Checkbox("重力を使用する", &preset.particleUpdate.useGravity);

	// --- 保存ボタン ---
	Separator();
	if (Button("JSON を保存")) {
		if (SavePresetToJson(preset)) {
			TextColored(ImVec4(0, 1, 0, 1), "保存しました");
		}
		else {
			TextColored(ImVec4(1, 0, 0, 1), "保存に失敗しました");
		}
	}

	// 常に「今編集中の名前」を覚えておく
	presetLibrary_.SetCurrentEditingName(preset.name);

	End();

#endif // USE_IMGUI
}

bool ParticleManager::SavePresetToJson(const ParticlePreset& preset,const std::string& directory)
{
	// JSON 保存は ParticlePresetLibrary に委譲
	bool result = presetLibrary_.SaveToJson(preset, directory);
	if (!result) {
		return false;
	}

	// ===== 描画グループの再生成 =====
	auto itGroup = particleGroups.find(preset.name);
	if (itGroup != particleGroups.end()) {
		// いったん削除
		DeleteParticleGroup(preset.name);

		// 新しいテクスチャ&ブレンドモードで作成し直し
		CreateParticleGroup(preset.name,
			preset.emitterSettings.textureFilePath,
			preset.emitterSettings.blendMode);

		// プリセットの共通設定も反映
		SetFlipYToGroup(preset.name, preset.render.flipY);
		SetLifeTimeToGroup(preset.name, preset.particleUpdate.lifeTime);
		SetColorToGroup(preset.name, preset.render.color);

		// ビルボードは現在マネージャ全体設定
		SetUseBillboard(preset.render.useBillboard);

		SetVelocityToGroup(preset.name, preset.particleUpdate.velocity);
		SetRotationSpeedToGroup(preset.name, preset.particleUpdate.rotationSpeed);
		SetScaleSpeedToGroup(preset.name, preset.particleUpdate.scaleSpeed);
	}

	return true;
}

bool ParticleManager::LoadPresetFromJson(const std::string& presetName, ParticlePreset& outPreset, const std::string& directory)
{
	return presetLibrary_.LoadFromJson(presetName, outPreset, directory);
}

void ParticleManager::LoadAllPresets(const std::string& directory)
{
	// ParticlePresetLibrary に転送
	presetLibrary_.LoadAll(directory);
}

void ParticleManager::RegisterPreset(const ParticlePreset& preset)
{
	if (preset.name.empty()) {
		Error(kChannel, "ParticleManager::RegisterPreset : preset.name is empty. skipped.");
		return;
	}

	// (1) presetLibrary_ にメモリ登録
	presetLibrary_.Add(preset);

	// (2) 描画グループを準備（テクスチャ / ブレンドモードなどをセットアップ）
	//     EmitterInstance がパーティクルを出す前に、描画先グループが必要
	EnsureGroupForPreset(preset);
}

void ParticleManager::EmitByPresetName(const std::string& presetName, const Mat4& emitterTransform)
{
	// ---- プリセット検索 or 読み込み ----
	ParticlePreset preset;
	{
		// presetLibrary_ から検索
		if (const ParticlePreset* found = presetLibrary_.Find(presetName)) {
			preset = *found;
		}
		else {
			// まだメモリに無ければ JSON から読み込みを試す
			if (!LoadPresetFromJson(presetName, preset)) {
				Error(kChannel, "ParticleManager::EmitByPresetName : preset not found -> {}", presetName);
				return;
			}
		}
	}

	// JSON 側の name が空なら presetName を使う
	if (preset.name.empty()) {
		preset.name = presetName;
	}

	// プリセットに対応するグループを準備（新規作成 or 更新）して参照を取得
	ParticleGroup& group = EnsureGroupForPreset(preset);

	// ここでグループ名をもう一度ローカル変数にしておく
	const std::string& groupName = preset.name;

	const size_t beforeCount = group.particleList.size();

	// ---- 呼び出し元の Transform とプリセット値を合成 ----
	Transform t = emitterTransform;

	// スケールは乗算（呼び出し元の大きさに対して相対的に）
	t.scale.x *= preset.particleSpawn.initialScale.x;
	t.scale.y *= preset.particleSpawn.initialScale.y;
	t.scale.z *= preset.particleSpawn.initialScale.z;

	// 回転は加算
	t.rotate.x += preset.particleSpawn.initialRotate.x;
	t.rotate.y += preset.particleSpawn.initialRotate.y;
	t.rotate.z += preset.particleSpawn.initialRotate.z;

	// 位置はオフセット分だけ足す
	t.translate.x += preset.particleSpawn.initialOffset.x;
	t.translate.y += preset.particleSpawn.initialOffset.y;
	t.translate.z += preset.particleSpawn.initialOffset.z;

	// ---- 形状に応じて既存の Emit 系を呼ぶ ----
	switch (preset.emitterSettings.vertexType) {
	case VertexDataType::Plane:
		// Plane は通常の Emit。count と useRandomPosition をプリセットから。
		Emit(groupName, t, preset.emitterSpawn.count, preset.emitterSpawn.useRandomPosition);
		break;

	case VertexDataType::Ring:
		// Ring は 1 個だけの扱いなので count は無視
		RingEmit(groupName, t, preset.emitterSpawn.count);
		break;

	case VertexDataType::Cylinder:
		// Cylinder も 1 個だけ
		CylinderEmit(groupName, t, preset.emitterSpawn.count);
		break;

	default:
		Error(kChannel, "ParticleManager::EmitByPresetName : unsupported vertexType in preset -> {}", presetName);
		break;
	}

	// === ここからがポイント ===
	// Emit によって新しく追加されたパーティクルだけに preset を適用する
	const size_t afterCount = group.particleList.size();
	if (afterCount > beforeCount) {
		auto it = group.particleList.begin();
		std::advance(it, static_cast<std::ptrdiff_t>(beforeCount));
		for (; it != group.particleList.end(); ++it) {
			Particle& p = *it;
			p.velocity = preset.particleUpdate.velocity;
			p.rotationSpeed = preset.particleUpdate.rotationSpeed;
			p.scaleSpeed = preset.particleUpdate.scaleSpeed;
			p.color = preset.render.color;
			p.lifeTime = preset.particleUpdate.lifeTime;
			// カーブ用の基準値
			p.initialScale = p.transform.scale;
			p.initialColor = p.color;
		}
	}
}

ParticleEmitterInstance* ParticleManager::EmitPreset(const std::string& presetName, const Mat4& emitterTransform)
{
	// (1) プリセットが登録されているか確認
	const ParticlePreset* preset = presetLibrary_.Find(presetName);
	if (!preset) {
		Error(kChannel, "ParticleManager::EmitPreset : preset not registered -> {}", presetName);
		return nullptr;
	}

	// (2) 描画グループが準備されているか確認・準備
	//     （RegisterPreset 時に呼んでいるはずだが、保険として呼ぶ）
	EnsureGroupForPreset(*preset);

	// (3) EmitterInstance を生成（emitterInstances_ に登録される）
	ParticleEmitterInstance* instance =
		CreateEmitterInstanceFromPreset(presetName, emitterTransform);
	if (!instance) {
		// CreateEmitterInstanceFromPreset 側でログを出しているので、ここでは追加しない
		return nullptr;
	}

	// (4) 即再生開始
	//     Initialize 時に playing_=true になっているので、Play() は念のため
	instance->Play();

	return instance;
}

ParticlePreset* ParticleManager::FindPreset(const std::string& name)
{
	// ParticlePresetLibrary に転送
	return presetLibrary_.Find(name);
}

const ParticlePreset* ParticleManager::FindPreset(const std::string& name) const
{
	// ParticlePresetLibrary に転送
	return presetLibrary_.Find(name);
}

ParticleEmitterInstance* ParticleManager::CreateEmitterInstanceFromPreset(const std::string& presetName, const Mat4& emitterTransform)
{
	// プリセットを検索
	ParticlePreset* preset = FindPreset(presetName);
	if (!preset) {
		// 見つからない場合はログを出して nullptr を返す
		Error(
			kChannel, "ParticleManager::CreateEmitterInstanceFromPreset : preset not found -> {}",
			presetName
		);
		return nullptr;
	}

	// 新しいエミッターインスタンスを生成
	auto instance = std::make_unique<ParticleEmitterInstance>();

	// プリセットを渡して初期化
	// （現状の ParticleEmitterInstance は ParticlePreset 型を直接参照していない想定なので、
	//  必要に応じてクラス側の Initialize でプリセットの情報をコピーする形にしておく）
	instance->Initialize(preset);

	// Transform を設定
	instance->SetTransform(emitterTransform);

	// 管理コンテナに登録
	ParticleEmitterInstance* rawPtr = instance.get();
	emitterInstances_.push_back(std::move(instance));

	return rawPtr;
}

void ParticleManager::UpdateEmitters(float dt)
{
	// 今のところは単純に全エミッターを更新するだけ
	// 将来的には「再生中フラグ」「自動破棄」などをここに追加する
	for (auto& emitter : emitterInstances_) {
		if (emitter) {
			emitter->Update(dt);  // ← シミュレーションだけ
		}
	}
}

ParticleSystem* ParticleManager::CreateSystem(const std::string& systemName)
{
	return systemManager_.Create(systemName);
}

ParticleSystem* ParticleManager::FindSystem(const std::string& systemName)
{
	return systemManager_.Find(systemName);
}

ParticleEmitterInstance* ParticleManager::AddEmitterToSystem(const std::string& systemName, const std::string& presetName, const Mat4&
	emitterTransform)
{
	// EmitterInstance の生成 → ParticleManager の責務
	// System への登録 → ParticleSystemManager に委譲

	// (1) System を確保（pendingSettings 取得のために必要）
	ParticleSystem* system = systemManager_.Find(systemName);
	if (!system) {
		system = systemManager_.Create(systemName);
	}
	if (!system) {
		Error(kChannel, "ParticleManager::AddEmitterToSystem : failed to create/find system -> {}", systemName);
		return nullptr;
	}

	// (2) プリセットから EmitterInstance を作成（DirectX 依存なので Manager の責務）
	ParticleEmitterInstance* emitter =
		CreateEmitterInstanceFromPreset(presetName, emitterTransform);

	if (!emitter) {
		Error(
			kChannel, "ParticleManager::AddEmitterToSystem : failed to create emitter from preset -> {}",
		);
		return nullptr;
	}

	// (3) pendingSettings から startTime/duration を取得
	float startTime = 0.0f;
	float duration = -1.0f;
	bool  autoPlay = true;

	for (const auto& p : system->GetPendingSettings()) {
		if (p.presetName == presetName) {
			startTime = p.startTime;
			duration = p.duration;
			autoPlay = p.autoPlay;
			break;
		}
	}

	// (4) System への登録は ParticleSystemManager に委譲
	systemManager_.RegisterEmitter(systemName, emitter, startTime, duration, autoPlay);

	return emitter;
}

void ParticleManager::RegisterSystemPreset(const std::string& systemName, const std::string& presetName)
{
	systemManager_.RegisterPreset(systemName, presetName);
}

const std::vector<std::string>* ParticleManager::GetSystemPresets(const std::string& systemName) const
{
	return systemManager_.GetPresets(systemName);
}

std::vector<std::string> ParticleManager::GetAllSystemNames() const
{
	return systemManager_.GetAllNames();
}

void ParticleManager::ClearAllSystems()
{
	systemManager_.ClearAll();
}

void ParticleManager::EmitSystemByName(const std::string& systemName, const Mat4& emitterTransform)
{
	// EmitSystem() に丸投げする
	EmitSystem(systemName, emitterTransform);
}

bool ParticleManager::SaveSystemToJson(const std::string& systemName, const std::string& directory)
{
	return systemManager_.SaveToJson(systemName, directory);
}

bool ParticleManager::LoadSystemFromJson(const std::string& systemName, const std::string& directory)
{
	return systemManager_.LoadFromJson(systemName, directory);
}

void ParticleManager::LoadAllSystems(const std::string& directory)
{
	systemManager_.LoadAll(directory);
}

bool ParticleManager::RenameSystem(const std::string& oldName, const std::string& newName)
{
	return systemManager_.Rename(oldName, newName);
}

void ParticleManager::EmitSystem(const std::string& systemName, const Mat4& transform)
{
	systemManager_.EmitSystem(
		systemName,
		transform,
		[this, systemName](const std::string& presetName, const Mat4& tf)
		-> ParticleEmitterInstance*
		{
			// ParticleManager の AddEmitterToSystem を呼ぶ
			// （CreateEmitterInstanceFromPreset → emitterInstances_ への登録が行われる）
			return AddEmitterToSystem(systemName, presetName, tf);
		});
}

void ParticleManager::PopulateInstancesFromEmitters(const Mat4& viewProjectionMatrix,const Mat4& billboardMatrix)
{
	for (auto& emitterPtr : emitterInstances_) {
		if (!emitterPtr) {
			continue;
		}
		ParticleEmitterInstance& emitter = *emitterPtr;

		// どのプリセットか取得（※描画設定を見るため）
		const void* rawPreset = emitter.GetPresetRaw();
		if (!rawPreset) {
			continue;
		}
		const auto* preset = reinterpret_cast<const ParticlePreset*>(rawPreset);

		// このプリセットに対応する ParticleGroup を準備
		ParticleGroup& group = EnsureGroupForPreset(*preset);

		// Emitter が持っている粒子を全部読む（読み取り専用）
		const auto& particles = emitter.GetParticles();
		for (const auto& p : particles) {
			if (!p.active) {
				continue;
			}
			if (group.instanceCount >= kNumMaxInstance) {
				break;
			}

			// === Transform から World 行列を作る ===
			Vec3 scale = p.scale;
			Vec3 rotate = p.rotation;
			Vec3 position = p.position;

			// ★ Local Space モードの場合、エミッタのワールド行列を適用して
			//    パーティクルのローカル座標をワールド座標へ変換する。
			//    以前は平行移動だけ加算していたが、それだとエミッタの
			//    回転・スケールが粒子位置に反映されなかった。
			//    TransformPoint で行列全体（移動・回転・スケール）を適用する。
			if (emitter.IsLocalSpace()) {
				position = emitter.GetTransform().TransformPoint(position);
			}

			Mat4 scaleMatrix = Mat4::Scale(scale);

			Mat4 rotateMatrix =  Mat4::identity;
			if (usebillboardMatrix) {
				// ビルボード使用時は Z 回転だけ
				rotateMatrix = Mat4::RotateZ(rotate.z);
			}
			else {
				Mat4 rotX = Mat4::RotateX(rotate.x);
				Mat4 rotY = Mat4::RotateY(rotate.y);
				Mat4 rotZ = Mat4::RotateZ(rotate.z);
				rotateMatrix = rotZ * rotY * rotX;
			}

			Mat4 translateMatrix = Mat4::Translate(position); 

			Mat4 bbMatrix = usebillboardMatrix ? billboardMatrix : Mat4::identity;

			// world = S * Billboard * R * T
			Mat4 worldMatrix = scaleMatrix * bbMatrix * rotateMatrix * translateMatrix;

			Mat4 wvp = worldMatrix * viewProjectionMatrix;

			// === GPU インスタンスへ書き込み ===
			ParticleForGPU& gpu = group.instancingDataPtr[group.instanceCount];
			gpu.WVP = wvp;
			gpu.World = worldMatrix;

			// 色（Emitter 側で ColorCurve 適用済）
			Vec4 outColor = p.color;

			// 寿命によるフェードだけここで掛ける
			float age = (p.maxLife > 0.0f) ? (p.life / p.maxLife) : 0.0f;
			age = std::clamp(age, 0.0f, 1.0f);
			outColor.w *= (1.0f - age);
			if (outColor.w < 0.0f) { outColor.w = 0.0f; }

			gpu.color = outColor;
			gpu.flipY = emitter.IsFlipY() ? 1.0f : 0.0f;

			++group.instanceCount;
		}
	}
}


void ParticleManager::VertexBufferView()
{
	//==========頂点リソース生成==========//
	vertexResource = dxCommon_->CreateBufferResource(sizeof(VertexData) * modelData.vertices.size());

	////=========VertexBufferViewを作成する=========////

	//リソースの先頭のアドレスから使う
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
	//使用するリソースのサイズは頂点のサイズ
	vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * modelData.vertices.size());
	//1頂点あたりのサイズ
	vertexBufferView.StrideInBytes = sizeof(VertexData);
}

void ParticleManager::CreateVertexData()
{
	modelData.vertices.push_back({ .position = {1.0f, 1.0f, 0.0f, 1.0f}, .texcoord = {0.0f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f} });
	modelData.vertices.push_back({ .position = {-1.0f, 1.0f, 0.0f, 1.0f}, .texcoord = {1.0f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f} });
	modelData.vertices.push_back({ .position = {1.0f, -1.0f, 0.0f, 1.0f}, .texcoord = {0.0f, 1.0f}, .normal = {0.0f, 0.0f, 1.0f} });
	modelData.vertices.push_back({ .position = {1.0f, -1.0f, 0.0f, 1.0f}, .texcoord = {0.0f, 1.0f}, .normal = {0.0f, 0.0f, 1.0f} });
	modelData.vertices.push_back({ .position = {-1.0f, 1.0f, 0.0f, 1.0f}, .texcoord = {1.0f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f} });
	modelData.vertices.push_back({ .position = {-1.0f, -1.0f, 0.0f, 1.0f}, .texcoord = {1.0f, 1.0f}, .normal = {0.0f, 0.0f, 1.0f} });
}

void ParticleManager::CreateRingVertexData() {
	const uint32_t kRingDivide = 32;
	const float kOuterRadius = 1.0f;
	const float kInnerRadius = 0.2f;
	const float radianPerDivide = 2.0f * std::numbers::pi_v<float> / float(kRingDivide);

	modelData.vertices.clear();

	for (uint32_t index = 0; index < kRingDivide; ++index) {
		float sin = std::sin(index * radianPerDivide);
		float cos = std::cos(index * radianPerDivide);
		float sinNext = std::sin((index + 1) * radianPerDivide);
		float cosNext = std::cos((index + 1) * radianPerDivide);

		float u = float(index) / float(kRingDivide);
		float uNext = float(index + 1) / float(kRingDivide);

		// 外→内の三角形1
		modelData.vertices.push_back({ { -sin * kOuterRadius, cos * kOuterRadius, 0.0f, 1.0f }, { u, 0.0f }, { 0.0f, 0.0f, 1.0f } });
		modelData.vertices.push_back({ { -sinNext * kOuterRadius, cosNext * kOuterRadius, 0.0f, 1.0f }, { uNext, 0.0f }, { 0.0f, 0.0f, 1.0f } });
		modelData.vertices.push_back({ { -sin * kInnerRadius, cos * kInnerRadius, 0.0f, 1.0f }, { u, 1.0f }, { 0.0f, 0.0f, 1.0f } });

		// 三角形2（内→外）
		modelData.vertices.push_back({ { -sin * kInnerRadius, cos * kInnerRadius, 0.0f, 1.0f }, { u, 1.0f }, { 0.0f, 0.0f, 1.0f } });
		modelData.vertices.push_back({ { -sinNext * kOuterRadius, cosNext * kOuterRadius, 0.0f, 1.0f }, { uNext, 0.0f }, { 0.0f, 0.0f, 1.0f } });
		modelData.vertices.push_back({ { -sinNext * kInnerRadius, cosNext * kInnerRadius, 0.0f, 1.0f }, { uNext, 1.0f }, { 0.0f, 0.0f, 1.0f } });
	}
}

void ParticleManager::CreateCylinderVertexData() {
	const uint32_t kCylinderDivide = 32;
	const float kHeight = 1.0f;
	const float kRadius = 0.5f;
	const float radianPerDivide = 2.0f * std::numbers::pi_v<float> / float(kCylinderDivide);

	modelData.vertices.clear();

	for (uint32_t i = 0; i < kCylinderDivide; ++i) {
		float theta = i * radianPerDivide;
		float nextTheta = (i + 1) * radianPerDivide;

		float sinTheta = std::sin(theta), cosTheta = std::cos(theta);
		float sinNext = std::sin(nextTheta), cosNext = std::cos(nextTheta);

		Vec3 bottom1 = { kRadius * cosTheta, -kHeight / 2, kRadius * sinTheta };
		Vec3 bottom2 = { kRadius * cosNext, -kHeight / 2, kRadius * sinNext };
		Vec3 top1 = { kRadius * cosTheta, kHeight / 2, kRadius * sinTheta };
		Vec3 top2 = { kRadius * cosNext, kHeight / 2, kRadius * sinNext };

		// 側面（2三角形）
		modelData.vertices.push_back({ { bottom1.x, bottom1.y, bottom1.z, 1.0f }, { 0.0f, 1.0f }, { cosTheta, 0.0f, sinTheta } });
		modelData.vertices.push_back({ { bottom2.x, bottom2.y, bottom2.z, 1.0f }, { 1.0f, 1.0f }, { cosNext, 0.0f, sinNext } });
		modelData.vertices.push_back({ { top1.x, top1.y, top1.z, 1.0f }, { 0.0f, 0.0f }, { cosTheta, 0.0f, sinTheta } });

		modelData.vertices.push_back({ { top1.x, top1.y, top1.z, 1.0f }, { 0.0f, 0.0f }, { cosTheta, 0.0f, sinTheta } });
		modelData.vertices.push_back({ { bottom2.x, bottom2.y, bottom2.z, 1.0f }, { 1.0f, 1.0f }, { cosNext, 0.0f, sinNext } });
		modelData.vertices.push_back({ { top2.x, top2.y, top2.z, 1.0f }, { 1.0f, 0.0f }, { cosNext, 0.0f, sinNext } });

		// 上面
		modelData.vertices.push_back({ { 0.0f, kHeight / 2, 0.0f, 1.0f }, { 0.5f, 0.5f }, { 0.0f, 1.0f, 0.0f } });
		modelData.vertices.push_back({ { top1.x, top1.y, top1.z, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } });
		modelData.vertices.push_back({ { top2.x, top2.y, top2.z, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } });

		// 下面
		modelData.vertices.push_back({ { 0.0f, -kHeight / 2, 0.0f, 1.0f }, { 0.5f, 0.5f }, { 0.0f, -1.0f, 0.0f } });
		modelData.vertices.push_back({ { bottom2.x, bottom2.y, bottom2.z, 1.0f }, { 0.0f, 0.0f }, { 0.0f, -1.0f, 0.0f } });
		modelData.vertices.push_back({ { bottom1.x, bottom1.y, bottom1.z, 1.0f }, { 1.0f, 0.0f }, { 0.0f, -1.0f, 0.0f } });
	}
}


void ParticleManager::CreateParticleGroup(const std::string name,const std::string textureFilePath,BlendMode blendMode)
{
	// ----------------------------------------
	// 同名グループの二重登録チェック
	// ----------------------------------------
	if (particleGroups.find(name) != particleGroups.end()) {
		assert(false && "Particle group with this name already exists!");
	}

	// ----------------------------------------
	// テクスチャパスの決定（空ならデフォルトを使う）
	// ----------------------------------------
	std::string resolvedTexPath = textureFilePath;

	if (resolvedTexPath.empty()) {
		// ★ここは実際に存在するパーティクル用テクスチャに合わせてください
		//   さっき Inspector で使っていたパスにしています
		resolvedTexPath = "Resources/ParticleTexture/smoke.png";
		Warning(
			kChannel,"ParticleManager::CreateParticleGroup : textureFilePath is empty. Use default texture -> {}",
			resolvedTexPath
		);
		
	}

	// ----------------------------------------
	// 新たなパーティクルグループを作成
	// ----------------------------------------
	ParticleGroup newGroup;
	newGroup.materialFilePath = resolvedTexPath;

	// テクスチャ読み込み・SRV取得
	TextureManager::GetInstance()->LoadTexture(resolvedTexPath);
	newGroup.srvIndex =
		TextureManager::GetInstance()->GetTextureIndexByFilePath(resolvedTexPath);

	// テクスチャサイズを取得（必要なら後で使えるように）
	const DirectX::TexMetadata& metadata =
		TextureManager::GetInstance()->GetMetaData(resolvedTexPath);
	Vec2 textureSize = {
		static_cast<float>(metadata.width),
		static_cast<float>(metadata.height)
	};
	// 必要なら newGroup.textureSize = textureSize; など

	// 頂点数を取得
	newGroup.vertexCount = static_cast<UINT>(modelData.vertices.size());

	// インスタンシング用リソースの生成
	newGroup.instancingResource =
		dxCommon_->CreateBufferResource(sizeof(ParticleForGPU) * kNumMaxInstance);
	newGroup.instancingResource->Map(
		0, nullptr, reinterpret_cast<void**>(&newGroup.instancingDataPtr));

	for (uint32_t index = 0; index < kNumMaxInstance; ++index) {
		newGroup.instancingDataPtr[index].WVP = Mat4::identity;
		newGroup.instancingDataPtr[index].World = Mat4::identity;
		// newGroup.instancingDataPtr[index].Color = {1,1,1,1}; // 必要なら
	}

	// インスタンシング用 SRV を確保
	newGroup.instancingSrvIndex = srvManager_->Allocate();
	srvManager_->CreateSRVforStructuredBuffer(
		newGroup.instancingSrvIndex,
		newGroup.instancingResource.Get(),
		kNumMaxInstance,
		sizeof(ParticleForGPU));

	// グループごとのブレンドモードを設定
	newGroup.blendMode = blendMode;

	// マップに登録
	particleGroups.emplace(name, newGroup);

	// マテリアルデータの初期化
	CreateMaterialData();
}


void ParticleManager::SetFlipYToGroup(const std::string& groupName, bool flip)
{
	auto it = particleGroups.find(groupName);
	if (it == particleGroups.end()) return;
	it->second.flipY = flip;
}

void ParticleManager::CreateMaterialData()
{
	// マテリアル用のリソースを作成
	materialResource = dxCommon_->CreateBufferResource(sizeof(Material));

	//書き込むためのアドレスを取得
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	materialData->color = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
	//SpriteはLightingしないのfalseを設定する
	materialData->enableLighting = false;
	materialData->uvTransform = Mat4::identity;
}

void ParticleManager::InstancingMaxResource()
{
	Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource2 = dxCommon_->CreateBufferResource(sizeof(ParticleForGPU) * kNumMaxInstance);
	ParticleForGPU* instancingData2 = nullptr;
	instancingResource2->Map(0, nullptr, reinterpret_cast<void**>(&instancingData2));
	for (uint32_t index = 0; index < kNumMaxInstance; ++index)
	{
		instancingData2[index].WVP = Mat4::identity;
		instancingData2[index].World = Mat4::identity;
		//instancingData2[index].color = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
	}
}

void ParticleManager::BuildVertexBuffer(VertexDataType type)
{
	//指定形状の頂点バッファを作成し、vertexBuffers_ マップに登録する
	// 1. 頂点データを生成（形状ごとに関数を呼び分け）
	std::vector<VertexData> verts;
	switch (type) {
	case VertexDataType::Plane:
		verts = MakePlaneVertices();
		break;
	case VertexDataType::Ring:
		verts = MakeRingVertices();
		break;
	case VertexDataType::Cylinder:
		verts = MakeCylinderVertices();
		break;
	default:
		// 未対応形状（Trailなど）はここでは作らない
		return;
	}

	if (verts.empty()) return;

	// 2. GPUリソースを作成
	VertexBufferSet set;
	const UINT byteSize = static_cast<UINT>(sizeof(VertexData) * verts.size());

	// 既存コードの作法に合わせて dxCommon_ のヘルパーでバッファ確保
	set.resource = dxCommon_->CreateBufferResource(byteSize);

	// 3. CPU側データを GPU リソースにコピー
	VertexData* mapped = nullptr;
	set.resource->Map(0, nullptr, reinterpret_cast<void**>(&mapped));
	std::memcpy(mapped, verts.data(), byteSize);
	set.resource->Unmap(0, nullptr);

	// 4. Vertex Buffer View をセットアップ
	set.view.BufferLocation = set.resource->GetGPUVirtualAddress();
	set.view.SizeInBytes = byteSize;
	set.view.StrideInBytes = sizeof(VertexData);
	set.vertexCount = static_cast<uint32_t>(verts.size());

	// 5. マップに登録
	vertexBuffers_[type] = std::move(set);
}

std::vector<ParticleManager::VertexData> ParticleManager::MakePlaneVertices()
{
	std::vector<VertexData> v(6);

	v[0].position = { 1.0f,  1.0f, 0.0f, 1.0f };
	v[0].texcoord = { 0.0f, 0.0f };
	v[0].normal = { 0.0f, 0.0f, 1.0f };

	v[1].position = { -1.0f,  1.0f, 0.0f, 1.0f };
	v[1].texcoord = { 1.0f, 0.0f };
	v[1].normal = { 0.0f, 0.0f, 1.0f };

	v[2].position = { 1.0f, -1.0f, 0.0f, 1.0f };
	v[2].texcoord = { 0.0f, 1.0f };
	v[2].normal = { 0.0f, 0.0f, 1.0f };

	v[3].position = { 1.0f, -1.0f, 0.0f, 1.0f };
	v[3].texcoord = { 0.0f, 1.0f };
	v[3].normal = { 0.0f, 0.0f, 1.0f };

	v[4].position = { -1.0f,  1.0f, 0.0f, 1.0f };
	v[4].texcoord = { 1.0f, 0.0f };
	v[4].normal = { 0.0f, 0.0f, 1.0f };

	v[5].position = { -1.0f, -1.0f, 0.0f, 1.0f };
	v[5].texcoord = { 1.0f, 1.0f };
	v[5].normal = { 0.0f, 0.0f, 1.0f };

	return v;
}

std::vector<ParticleManager::VertexData> ParticleManager::MakeRingVertices()
{
	const uint32_t kRingDivide = 32;
	const float kOuterRadius = 1.0f;
	const float kInnerRadius = 0.2f;
	const float radianPerDivide = 2.0f * std::numbers::pi_v<float> / float(kRingDivide);

	std::vector<VertexData> v;
	v.reserve(kRingDivide * 6);  // 1分割あたり2三角形 = 6頂点

	for (uint32_t index = 0; index < kRingDivide; ++index) {
		float sin = std::sin(index * radianPerDivide);
		float cos = std::cos(index * radianPerDivide);
		float sinNext = std::sin((index + 1) * radianPerDivide);
		float cosNext = std::cos((index + 1) * radianPerDivide);

		float u = float(index) / float(kRingDivide);
		float uNext = float(index + 1) / float(kRingDivide);

		// 外→内の三角形1
		v.push_back({ { -sin * kOuterRadius,     cos * kOuterRadius,     0.0f, 1.0f }, { u,     0.0f }, { 0.0f, 0.0f, 1.0f } });
		v.push_back({ { -sinNext * kOuterRadius, cosNext * kOuterRadius, 0.0f, 1.0f }, { uNext, 0.0f }, { 0.0f, 0.0f, 1.0f } });
		v.push_back({ { -sin * kInnerRadius,     cos * kInnerRadius,     0.0f, 1.0f }, { u,     1.0f }, { 0.0f, 0.0f, 1.0f } });

		// 三角形2（内→外）
		v.push_back({ { -sin * kInnerRadius,     cos * kInnerRadius,     0.0f, 1.0f }, { u,     1.0f }, { 0.0f, 0.0f, 1.0f } });
		v.push_back({ { -sinNext * kOuterRadius, cosNext * kOuterRadius, 0.0f, 1.0f }, { uNext, 0.0f }, { 0.0f, 0.0f, 1.0f } });
		v.push_back({ { -sinNext * kInnerRadius, cosNext * kInnerRadius, 0.0f, 1.0f }, { uNext, 1.0f }, { 0.0f, 0.0f, 1.0f } });
	}

	return v;
}

std::vector<ParticleManager::VertexData> ParticleManager::MakeCylinderVertices()
{
	const uint32_t kCylinderDivide = 32;
	const float kHeight = 1.0f;
	const float kRadius = 0.5f;
	const float radianPerDivide = 2.0f * std::numbers::pi_v<float> / float(kCylinderDivide);

	std::vector<VertexData> v;
	v.reserve(kCylinderDivide * 12);  // 側面6 + 上面3 + 下面3 = 12頂点

	for (uint32_t i = 0; i < kCylinderDivide; ++i) {
		float theta = i * radianPerDivide;
		float nextTheta = (i + 1) * radianPerDivide;

		float sinTheta = std::sin(theta), cosTheta = std::cos(theta);
		float sinNext = std::sin(nextTheta), cosNext = std::cos(nextTheta);

		Vec3 bottom1 = { kRadius * cosTheta, -kHeight / 2, kRadius * sinTheta };
		Vec3 bottom2 = { kRadius * cosNext,  -kHeight / 2, kRadius * sinNext };
		Vec3 top1 = { kRadius * cosTheta,  kHeight / 2, kRadius * sinTheta };
		Vec3 top2 = { kRadius * cosNext,   kHeight / 2, kRadius * sinNext };

		// 側面（三角形1）
		v.push_back({ { bottom1.x, bottom1.y, bottom1.z, 1.0f }, { 0.0f, 1.0f }, { cosTheta, 0.0f, sinTheta } });
		v.push_back({ { bottom2.x, bottom2.y, bottom2.z, 1.0f }, { 1.0f, 1.0f }, { cosNext,  0.0f, sinNext  } });
		v.push_back({ { top1.x,    top1.y,    top1.z,    1.0f }, { 0.0f, 0.0f }, { cosTheta, 0.0f, sinTheta } });

		// 側面（三角形2）
		v.push_back({ { top1.x,    top1.y,    top1.z,    1.0f }, { 0.0f, 0.0f }, { cosTheta, 0.0f, sinTheta } });
		v.push_back({ { bottom2.x, bottom2.y, bottom2.z, 1.0f }, { 1.0f, 1.0f }, { cosNext,  0.0f, sinNext  } });
		v.push_back({ { top2.x,    top2.y,    top2.z,    1.0f }, { 1.0f, 0.0f }, { cosNext,  0.0f, sinNext  } });

		// 上面
		v.push_back({ { 0.0f,   kHeight / 2, 0.0f,   1.0f }, { 0.5f, 0.5f }, { 0.0f, 1.0f, 0.0f } });
		v.push_back({ { top1.x, top1.y,      top1.z, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } });
		v.push_back({ { top2.x, top2.y,      top2.z, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } });

		// 下面
		v.push_back({ { 0.0f,      -kHeight / 2, 0.0f,      1.0f }, { 0.5f, 0.5f }, { 0.0f, -1.0f, 0.0f } });
		v.push_back({ { bottom2.x, bottom2.y,    bottom2.z, 1.0f }, { 0.0f, 0.0f }, { 0.0f, -1.0f, 0.0f } });
		v.push_back({ { bottom1.x, bottom1.y,    bottom1.z, 1.0f }, { 1.0f, 0.0f }, { 0.0f, -1.0f, 0.0f } });
	}

	return v;
}

ParticleManager::Particle ParticleManager::MakeNewParticle(std::mt19937& randomEngine, const Vec3& translate)
{
	// 位置だけランダムにばら撒く
	std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);

	Particle particle{}; // ゼロ初期化

	particle.transform.scale = { 1.0f, 1.0f, 1.0f };
	particle.transform.rotate = { 0.0f, 0.0f, 0.0f };

	Vec3 randomTranslate{
		distribution(randomEngine),
		distribution(randomEngine),
		distribution(randomEngine)
	};
	particle.transform.translate = translate + randomTranslate;

	// 速度・色・寿命はプリセット側(Set系)で一括設定するので、
	// ここではデフォルト値だけ入れておく
	particle.velocity = { 0.0f, 0.0f, 0.0f };
	particle.rotationSpeed = { 0.0f, 0.0f, 0.0f };
	particle.scaleSpeed = { 0.0f, 0.0f, 0.0f };
	particle.color = { 1.0f, 1.0f, 1.0f, 1.0f };
	particle.lifeTime = 1.0f;
	particle.currentTime = 0.0f;

	particle.initialScale = particle.transform.scale;
	particle.initialColor = particle.color;


	return particle;
}

ParticleManager::Particle ParticleManager::PrimitiveMakeNewParticle(std::mt19937& randomEngine, const Vec3& translate)
{
	// ランダム生成器
	std::uniform_real_distribution<float> distRotate(0.0f, std::numbers::pi_v<float>);
	std::uniform_real_distribution<float> distScale(0.4f, 1.5f);

	Particle particle;

	// 横は固定、縦をランダム
	particle.transform.scale = { 0.05f, distScale(randomEngine), 1.0f };

	// Z軸方向にランダムに回転させる
	particle.transform.rotate = { 0.0f, 0.0f, distRotate(randomEngine) };

	// 位置や速度は固定（必要に応じて変更可）
	particle.transform.translate = translate;
	particle.velocity = { 0.0f, 0.0f, 0.0f };

	// 色や寿命も固定
	particle.color = { 1.0f, 1.0f, 1.0f, 1.0f };
	particle.lifeTime = 1.0f;
	particle.currentTime = 0;

	return particle;
}

ParticleManager::Particle ParticleManager::RingMakeNewParticle(const Vec3& translate)
{
	Particle p{};

	p.transform =  Mat4::Translate(translate);

	// パラメータは後でプリセットから Set～ で当てる
	p.velocity = { 0,0,0 };
	p.rotationSpeed = { 0,0,0 };
	p.scaleSpeed = { 0,0,0 };
	p.color = { 1,1,1,1 };
	p.lifeTime = 1.0f;
	p.currentTime = 0;

	return p;
}

ParticleManager::Particle ParticleManager::CylinderMakeNewParticle(const Vec3& translate)
{
	Particle p{};
	
	p.transform = Mat4::Translate(translate);

	// パラメータは後でプリセットから Set～ で当てる
	p.velocity = { 0,0,0 };
	p.rotationSpeed = { 0,0,0 };
	p.scaleSpeed = { 0,0,0 };
	p.color = { 1,1,1,1 };
	p.lifeTime = 1.0f;
	p.currentTime = 0;

	return p;
}

//======================================================================
// プリセットに対応する ParticleGroup を用意＆更新する共通ヘルパー
//======================================================================
ParticleManager::ParticleGroup& ParticleManager::EnsureGroupForPreset(const ParticlePreset& preset)
{
	// グループ名はプリセット名と同じにする
	std::string groupName = preset.name;

	// まだグループが無ければ作成
	auto itGroup = particleGroups.find(groupName);
	if (itGroup == particleGroups.end()) {
		// テクスチャとブレンドモードはプリセットから
		CreateParticleGroup(
			groupName,
			preset.emitterSettings.textureFilePath,
			preset.emitterSettings.blendMode
		);

		// 新規作成したグループに対して プリセットの設定を反映
		SetFlipYToGroup(groupName, preset.render.flipY);
		SetLifeTimeToGroup(groupName, preset.particleUpdate.lifeTime);
		SetColorToGroup(groupName, preset.render.color);
		// ビルボードは現在マネージャ全体設定
		SetUseBillboard(preset.render.useBillboard);
		// 重力フラグをグループに反映
		SetGravityToGroup(groupName, preset.particleUpdate.useGravity);
	}
	else {
		// 既にあるグループにも、最新プリセットの値を反映しておく
		SetFlipYToGroup(groupName, preset.render.flipY);
		SetLifeTimeToGroup(groupName, preset.particleUpdate.lifeTime);
		SetColorToGroup(groupName, preset.render.color);
		SetUseBillboard(preset.render.useBillboard);
		SetGravityToGroup(groupName, preset.particleUpdate.useGravity);
	}

	// ここで実際のグループ参照を取得
	ParticleGroup& group = particleGroups[groupName];

	// カーブも常に最新のものをコピー
	group.scaleCurve = preset.particleUpdate.scaleCurve;
	group.colorCurve = preset.render.colorCurve;

	// ===== プリセットの形状情報をグループに反映 =====
	group.vertexType = preset.emitterSettings.vertexType;

	// 形状に対応する頂点バッファから vertexCount を取得
	auto vbIt = vertexBuffers_.find(group.vertexType);
	if (vbIt != vertexBuffers_.end()) {
		group.vertexCount = vbIt->second.vertexCount;
	}

	return group;
}

void ParticleManager::Emit(const std::string& name, Mat4& transform, uint32_t count, bool useRandomPosition) {
	if (particleGroups.find(name) == particleGroups.end()) {
		assert("Specified particle group does not exist!");
		return;
	}

	ParticleGroup& group = particleGroups[name];

	// ★ 最大インスタンス数を超えないようにする（countではなくkNumMaxInstance）
	if (group.particleList.size() >= kNumMaxInstance) {
		return;
	}

	// 追加可能な残り数を計算
	uint32_t maxToAdd = static_cast<uint32_t>(kNumMaxInstance - group.particleList.size());
	uint32_t actualCount = (count < maxToAdd) ? count : maxToAdd;

	for (uint32_t i = 0; i < actualCount; ++i) {
		Particle particle{};

		if (useRandomPosition) {
			// 位置だけランダム
			particle = MakeNewParticle(randomEngine, transform.GetTranslate());
		}
		else {
			// 完全に固定位置で出す
			particle.transform= Mat4::Affine(Vec3::one, Vec3::zero, transform.GetTranslate());

			particle.velocity = { 0.0f, 0.0f, 0.0f };
			particle.rotationSpeed = { 0.0f, 0.0f, 0.0f };
			particle.scaleSpeed = { 0.0f, 0.0f, 0.0f };
			particle.color = { 1.0f, 1.0f, 1.0f, 1.0f };
			particle.lifeTime = 1.0f;
			particle.currentTime = 0.0f;
		}

		// Emit 引数側のスケール／回転を上書き
		particle.transform = Mat4::Affine(transform.GetScale(), transform.GetRotate(), particle.transform.GetTranslate());

		group.particleList.push_back(particle);
	}
}

void ParticleManager::PrimitiveEmit(const std::string name, Mat4& transform, uint32_t count)
{
	if (particleGroups.find(name) == particleGroups.end()) {
		// パーティクルグループが存在しない場合はエラーを出力して終了
		assert("Specified particle group does not exist!");

	}

	// 指定されたパーティクルグループが存在する場合、そのグループにパーティクルを追加
	ParticleGroup& group = particleGroups[name];

	// ★ 最大インスタンス数を超えないようにする
	if (group.particleList.size() >= kNumMaxInstance) {
		return;
	}

	uint32_t maxToAdd = static_cast<uint32_t>(kNumMaxInstance - group.particleList.size());
	uint32_t actualCount = (count < maxToAdd) ? count : maxToAdd;

	// 指定された数のパーティクルを生成して追加
	for (uint32_t i = 0; i < actualCount; ++i) {
		Particle particle = PrimitiveMakeNewParticle(randomEngine, transform.GetTranslate());
		
		particle.transform = Mat4::Affine(transform.GetScale(), transform.GetRotate(), particle.transform.GetTranslate());
		
		group.particleList.push_back(particle);
	}
}

void ParticleManager::RingEmit(const std::string& name, Mat4& transform, uint32_t count)
{
	if (particleGroups.find(name) == particleGroups.end()) {
		assert("Specified particle group does not exist!");
		return;
	}

	ParticleGroup& group = particleGroups[name];

	// ★ 最大インスタンス数を超えないようにする
	if (group.particleList.size() >= kNumMaxInstance) {
		return;
	}

	uint32_t maxToAdd = static_cast<uint32_t>(kNumMaxInstance - group.particleList.size());
	uint32_t actualCount = (count < maxToAdd) ? count : maxToAdd;

	for (uint32_t i = 0; i < actualCount; ++i) {
		Particle particle = RingMakeNewParticle(transform.GetTranslate());
		particle.transform = Mat4::Affine(transform.GetScale(), transform.GetRotate(), particle.transform.GetTranslate());
		
		group.particleList.push_back(particle);
	}
}

void ParticleManager::CylinderEmit(
	const std::string& name,
	Mat4&              transform, uint32_t count
)
{
	if (particleGroups.find(name) == particleGroups.end()) {
		assert("Specified particle group does not exist!");
		return;
	}

	ParticleGroup& group = particleGroups[name];

	// ★ 最大インスタンス数を超えないようにする
	if (group.particleList.size() >= kNumMaxInstance) {
		return;
	}

	uint32_t maxToAdd = static_cast<uint32_t>(kNumMaxInstance - group.particleList.size());
	uint32_t actualCount = (count < maxToAdd) ? count : maxToAdd;

	for (uint32_t i = 0; i < actualCount; ++i) {
		Particle particle = CylinderMakeNewParticle(transform.GetTranslate());
		particle.transform = Mat4::Affine(transform.GetScale(), transform.GetRotate(), particle.transform.GetTranslate());
		group.particleList.push_back(particle);
	}
}


void ParticleManager::SetScaleToGroup(const std::string& groupName, const Vec3& scale) {
	auto it = particleGroups.find(groupName);
	if (it == particleGroups.end()) return;

	for (auto& particle : it->second.particleList) {
		particle.transform = Mat4::Affine(scale, particle.transform.GetRotate(), particle.transform.GetTranslate());
	}
}

void ParticleManager::SetRotationToGroup(const std::string& groupName, const Vec3& rotation) {
	auto it = particleGroups.find(groupName);
	if (it == particleGroups.end()) return;

	for (auto& particle : it->second.particleList) {
		particle.transform = Mat4::Affine(particle.transform.GetScale(), rotation, particle.transform.GetTranslate());
	}
}

void ParticleManager::SetTranslationToGroup(const std::string& groupName, const Vec3& translation) {
	auto it = particleGroups.find(groupName);
	if (it == particleGroups.end()) return;

	for (auto& particle : it->second.particleList) {
		particle.transform = Mat4::Affine(particle.transform.GetScale(), particle.transform.GetRotate(), translation);
	}
}

void ParticleManager::SetVelocityToGroup(const std::string& groupName, const Vec3& velocity) {
	auto it = particleGroups.find(groupName);
	if (it == particleGroups.end()) return;

	for (auto& particle : it->second.particleList) {
		particle.velocity = velocity;
	}
}

void ParticleManager::SetRotationSpeedToGroup(const std::string& groupName, const Vec3& rotationSpeed)
{
	auto it = particleGroups.find(groupName);
	if (it == particleGroups.end()) return;

	for (auto& particle : it->second.particleList) {
		particle.rotationSpeed = rotationSpeed;
	}
}

void ParticleManager::SetScaleSpeedToGroup(const std::string& groupName, const Vec3& scaleSpeed)
{
	auto it = particleGroups.find(groupName);
	if (it == particleGroups.end()) return;

	for (auto& particle : it->second.particleList) {
		particle.scaleSpeed = scaleSpeed;
	}
}

void ParticleManager::SetColorToGroup(const std::string& groupName, const Vec4& color) {
	auto it = particleGroups.find(groupName);
	if (it == particleGroups.end()) return;

	for (auto& particle : it->second.particleList) {
		particle.color = color;
	}
}

void ParticleManager::SetLifeTimeToGroup(const std::string& groupName, float lifeTime)
{
	auto it = particleGroups.find(groupName);
	if (it == particleGroups.end()) return;

	for (auto& particle : it->second.particleList) {
		particle.lifeTime = lifeTime;
	}
}

void ParticleManager::SetGravityToGroup(const std::string& groupName, bool useGravity)
{
	auto it = particleGroups.find(groupName);
	if (it == particleGroups.end()) return;
	it->second.useGravity = useGravity;
}


void ParticleManager::SetCurrentTimeToGroup(const std::string& groupName, float currentTime)
{
	auto it = particleGroups.find(groupName);
	if (it == particleGroups.end()) return;

	for (auto& particle : it->second.particleList) {
		particle.currentTime = currentTime;
	}
}

void ParticleManager::DeleteParticleGroup(const std::string& groupName) {
	particleGroups.erase(groupName);
}


void ParticleManager::AdjustTextureSize(ParticleGroup& group, const std::string& textureFilePath)
{
	// テクスチャメタデータを取得
	const DirectX::TexMetadata& metadata = TextureManager::GetInstance()->GetMetaData(textureFilePath);

	// テクスチャサイズを設定
	group.textureSize.x = static_cast<float>(metadata.width);
	group.textureSize.y = static_cast<float>(metadata.height);
}


void ParticleManager::RootSignature()
{
	////=========DescriptorRange=========////

	//D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRangeForInstancing_[0].BaseShaderRegister = 0;  //0から始まる
	descriptorRangeForInstancing_[0].NumDescriptors = 1;  //数は1
	descriptorRangeForInstancing_[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;  //SRVを使う
	descriptorRangeForInstancing_[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;  //Offsetを自動計算


	////=========RootSignatureを生成する=========////

	//RootSignature作成
	/*D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};*/
	descriptionRootSignature_.Flags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;


	//RootSignature作成。複数設定できるので配列。今回は結果1つだけなので長さ1の配列
	/*D3D12_ROOT_PARAMETER rootParameters[4] = {};*/
	rootParameters_[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;    //CBVを使う
	rootParameters_[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;    //PixelShaderで使う
	rootParameters_[0].Descriptor.ShaderRegister = 0;    //レジスタ番号0とバインド
	rootParameters_[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters_[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters_[1].DescriptorTable.pDescriptorRanges = descriptorRangeForInstancing_;
	rootParameters_[1].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForInstancing_);
	rootParameters_[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;  //DescriptorTableを使う
	rootParameters_[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters_[2].DescriptorTable.pDescriptorRanges = descriptorRangeForInstancing_;
	rootParameters_[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForInstancing_);
	////=========平行光源をShaderで使う=========////
	rootParameters_[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;  //CBVを使う
	rootParameters_[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;  //PixelShaderで使う
	rootParameters_[3].Descriptor.ShaderRegister = 1;  //レジスタ番号1を使う
	////=========光源のカメラの位置をShaderで使う=========////
	rootParameters_[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;  //CBVを使う
	rootParameters_[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;  //PixelShaderで使う
	rootParameters_[4].Descriptor.ShaderRegister = 2;  //レジスタ番号1を使う
	////========ポイントライトをShaderで使う========////
	rootParameters_[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;  //CBVを使う
	rootParameters_[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;  //PixelShaderで使う
	rootParameters_[5].Descriptor.ShaderRegister = 3;  //レジスタ番号3を使う

	////========スポットライトをShaderで使う========////
	rootParameters_[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;  //CBVを使う
	rootParameters_[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;  //PixelShaderで使う
	rootParameters_[6].Descriptor.ShaderRegister = 4;  //レジスタ番号4を使う

	descriptionRootSignature_.pParameters = rootParameters_;    //ルートパラメータ配列へのポインタ
	descriptionRootSignature_.NumParameters = _countof(rootParameters_);    //配列の長さ


	////=========Samplerの設定=========////

	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;  //バイリニアフィルタ
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;  //0～1の範囲外をリピート
	if (ringSamplerAdd = true) {
		staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	}
	else {

		staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	}
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;  //比較しない
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;  //ありったけのMipmapを使う
	staticSamplers[0].ShaderRegister = 0;  //レジスタ番号0を使う
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;  //PixelShaderで使う
	descriptionRootSignature_.pStaticSamplers = staticSamplers;
	descriptionRootSignature_.NumStaticSamplers = _countof(staticSamplers);



	////シリアライズしてバイナリにする
	//Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
	//Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature_,
		D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr)) {
		Fatal( 
			kChannel, 
			"Failed to serialize root signature: %s",
			reinterpret_cast<char*>(errorBlob->GetBufferPointer())
		);
		assert(false);
	}
	//バイナリを元に生成
	/*Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature = nullptr;*/
	hr = dxCommon_->GetDevice().Get()->CreateRootSignature(0, signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));

	if (FAILED(hr)) {
		Fatal(kChannel, "rootsig");
		exit(1);
	}

	assert(SUCCEEDED(hr));
}

void ParticleManager::GraphicsPipelineState(BlendMode blendMode)
{
	//ルートシグネチャの生成
	RootSignature();

	////=========InputLayoutの設定を行う=========////

	InputLayout();


	////=========BlendStateの設定を行う=========////

	BlendState(blendMode);

	////=========RasterizerStateの設定を行う=========////

	RasterizerState();


	////=========ShaderをCompileする=========////

	vertexShaderBlob_ = dxCommon_->CompileShader(L"Resources/shaders/Particle.VS.hlsl",
		L"vs_6_0"/*, dxCommon->GetDxcUtils(), dxCommon->GetDxcCompiler(), dxCommon->GetIncludeHandler()*/);
	if (vertexShaderBlob_ == nullptr) {
		Fatal(kChannel, "vertexShaderBlob");
		exit(1);
	}
	assert(vertexShaderBlob_ != nullptr);
	pixelShaderBlob_ = dxCommon_->CompileShader(L"Resources/shaders/Particle.PS.hlsl",
		L"ps_6_0"/*, dxCommon->GetDxcUtils(), dxCommon->GetDxcCompiler(), dxCommon->GetIncludeHandler()*/);
	if (pixelShaderBlob_ == nullptr) {
		Fatal(kChannel, "pixelShaderBlob");
		exit(1);
	}
	assert(pixelShaderBlob_ != nullptr);


	////=========DepthStencilStateの設定を行う=========////

	DepthStencilState();


	////=========PSOを生成する=========////
	  // PSOを生成しキャッシュに保存
	auto pipelineState = PSO();
	if (pipelineState) {
		pipelineStateCache_[blendMode] = pipelineState;
	}
	else {
		Fatal(kChannel, "Failed to create PSO for blend mode: {}", std::to_string(blendMode));
		assert(false && "PSO creation failed!");
	}
}

void ParticleManager::InputLayout()
{
	////=========InputLayoutの設定を行う=========////

	/*D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};*/
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[2].SemanticName = "NORMAL";
	inputElementDescs[2].SemanticIndex = 0;
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	//D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc_.pInputElementDescs = inputElementDescs;
	inputLayoutDesc_.NumElements = _countof(inputElementDescs);
}

void ParticleManager::BlendState(const BlendMode blendMode)
{
	////=========BlendStateの設定を行う=========////

	 // すべての色要素を書き込む
	blendDesc_.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blendDesc_.RenderTarget[0].BlendEnable = TRUE;

	switch (blendMode) {
	case kBlendModeNormal:
		blendDesc_.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		blendDesc_.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		blendDesc_.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		break;
	case kBlendModeAdd:
		blendDesc_.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		blendDesc_.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
		blendDesc_.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		break;
	case kBlendModeSubtract:
		blendDesc_.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		blendDesc_.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
		blendDesc_.RenderTarget[0].BlendOp = D3D12_BLEND_OP_REV_SUBTRACT;
		break;
	case kBlendModeMultiply:
		blendDesc_.RenderTarget[0].SrcBlend = D3D12_BLEND_ZERO;
		blendDesc_.RenderTarget[0].DestBlend = D3D12_BLEND_SRC_COLOR;
		blendDesc_.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		break;
	case kBlendModeScreen:
		blendDesc_.RenderTarget[0].SrcBlend = D3D12_BLEND_INV_DEST_COLOR;
		blendDesc_.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
		blendDesc_.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		break;
	default:
		blendDesc_.RenderTarget[0].BlendEnable = FALSE;
		break;
	}

	// α値のブレンド設定
	blendDesc_.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ZERO;
	blendDesc_.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
	blendDesc_.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
}

void ParticleManager::RasterizerState()
{
	////=========RasterizerStateの設定を行う=========////

	//RasterizerStateの設定
	/*D3D12_RASTERIZER_DESC rasterizerDesc{};*/
	//裏面(時計回り)を表示しない
	rasterizerDesc_.CullMode = D3D12_CULL_MODE_BACK;
	//三角形の中を塗りつぶす
	rasterizerDesc_.FillMode = D3D12_FILL_MODE_SOLID;
	//カリングしない(裏面も表示させる)
	rasterizerDesc_.CullMode = D3D12_CULL_MODE_NONE;
}

void ParticleManager::DepthStencilState()
{
	////=========DepthStencilStateの設定を行う=========////

	////DepthStencilStateの設定
	//D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	//Depthの機能を有効化する
	depthStencilDesc_.DepthEnable = true;
	//書き込みします
	depthStencilDesc_.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	//比較関数はLessEqual。つまり、近ければ描画される
	depthStencilDesc_.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
}

Microsoft::WRL::ComPtr<ID3D12PipelineState> ParticleManager::PSO()
{
	// グラフィックスパイプラインのルートシグネチャを設定
	graphicsPipelineStateDesc.pRootSignature = rootSignature_.Get();

	// 入力レイアウトを設定
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc_;

	// 頂点シェーダーとピクセルシェーダーを設定
	graphicsPipelineStateDesc.VS = { vertexShaderBlob_->GetBufferPointer(), vertexShaderBlob_->GetBufferSize() };
	graphicsPipelineStateDesc.PS = { pixelShaderBlob_->GetBufferPointer(), pixelShaderBlob_->GetBufferSize() };

	// ブレンドステートを設定
	graphicsPipelineStateDesc.BlendState = blendDesc_;

	// ラスタライザーステートを設定
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc_;

	// レンダーターゲットの数を設定し、フォーマットを指定
	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

	// プリミティブトポロジの種類を設定します（ここでは三角形）
	graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	// サンプルの設定（標準の1xサンプリング）
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	// 深度ステンシルステートの設定
	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc_;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// グラフィックスパイプラインステートオブジェクト (PSO) を生成
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
	HRESULT hr = dxCommon_->GetDevice().Get()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&pipelineState));

	// PSOの生成に失敗した場合、ログを出力し、nullポインタを返す
	if (FAILED(hr)) {
		Error(kChannel, "Failed to create PSO: HRESULT = 0x%X", hr);
		return nullptr;
	}

	return pipelineState;
}
