#pragma once
#include <random>
#include <string>
#include <unordered_map>
#include <vector>
#include <filesystem>
#include <d3d12.h>

#include <wrl/client.h>

#include <dxcapi.h>

#include "../ParticlePreset.h"

#include "core/math/Mat4.h"
#include "core/math/Vec3.h"
#include "core/math/Vec4.h"

#include "engine/particle/ParticlePresetLibrary.h"
#include "engine/particle/ParticleSystemManager.h"

class ParticleSystem;
class ParticleEmitterInstance;
struct ParticlePreset;
enum class VertexDataType;

class ParticleManager
{
public:

	//ParticleのEmitter構造体
	struct Emitter {
		Mat4 transform;
		uint32_t count;  //発生数
		float frequency;  //発生頻度
		float frequencyTime;  //頻度用時刻
	};

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(VertexDataType type);

	/// <summary>
	/// 更新
	/// </summary>
	void Update();

	/// <summary>
	/// 描画
	/// </summary>
	void Draw();

	/// <summary>
	/// パーティクルグループの生成
	/// </summary>
	void CreateParticleGroup(const std::string name, const std::string textureFilePath, BlendMode blendMode = kBlendModeNormal);

	// カメラの設定
	void SetCamera(Camera* camera) { this->camera_ = camera; }

	// cylinderの反転
	void SetFlipYToGroup(const std::string& groupName, bool flip);

	/// ImGui 上でプリセットを編集＆保存するエディタ
	void DrawImGuiParticlePresetEditor();

	/// プリセットを JSON に保存する（直接呼んでもOK）
	bool SavePresetToJson(const ParticlePreset& preset,
		const std::string& directory = "Resources/Particle");

	/// JSON からプリセットを読み込む（name.json を読む）
	bool LoadPresetFromJson(const std::string& presetName,
		ParticlePreset& outPreset,
		const std::string& directory = "Resources/Particle");

	/// ディレクトリ内のすべてのプリセットを読み込む（今後のため）
	void LoadAllPresets(const std::string& directory = "Resources/Particle");

	// ===== コードから直接プリセットを登録する =====
	/// コードで組み立てた ParticlePreset を登録する
	/// （JSON を経由せず、メモリ上に直接登録する）
	/// 登録後は EmitPreset() / EmitByPresetName() で名前指定して使える
	void RegisterPreset(const ParticlePreset& preset);

	/// <summary>
	/// JSON プリセット名からパーティクルを発生させる
	/// </summary>
	/// <param name="presetName">JSON ファイル名と同じプリセット名</param>
	/// <param name="emitterTransform">発生元の Transform（位置/回転/スケール）</param>
	void EmitByPresetName(const std::string& presetName, const Mat4& emitterTransform);

	/// 登録済みプリセットから、単発の EmitterInstance を作って即再生する
	/// System に属さない単独の Emitter として動作する
	/// @return 作成された EmitterInstance（操作したい場合のみ使う。捨てても OK）
	ParticleEmitterInstance* EmitPreset(const std::string& presetName, const Mat4& emitterTransform);

	const std::string& GetCurrentEditingPresetName() const { return presetLibrary_.GetCurrentEditingName(); }

	// setter
	void SetCurrentEditingPresetName(const std::string& name) { presetLibrary_.SetCurrentEditingName(name); }

	// プリセット取得（編集用）
	ParticlePreset* FindPreset(const std::string& name);
	const ParticlePreset* FindPreset(const std::string& name) const;

	/// </summary>
	/// <param name="presetName">ParticlePreset 名</param>
	/// <param name="emitterTransform">発生位置・回転・スケール</param>
	/// <returns>生成されたエミッターインスタンス（nullptr の場合は失敗）</returns>
	ParticleEmitterInstance* CreateEmitterInstanceFromPreset(
		const std::string& presetName,
		const Mat4& emitterTransform);

	/// <summary>
	/// 新システム用エミッターの更新処理
	/// （内部から Update() の最後で呼び出す）
	/// </summary>
	void UpdateEmitters(float dt);

	// ---- System 管理用API（新規） ----

	// System を 1 つ作成してコンテナに登録
	ParticleSystem* CreateSystem(const std::string& systemName);

	// 既に存在する System を名前で取得（なければ nullptr）
	ParticleSystem* FindSystem(const std::string& systemName);

	// 指定 System に、新しいエミッターを 1 つ追加
	// （どのプリセットを使うか、Transform はここで渡す）
	ParticleEmitterInstance* AddEmitterToSystem(
		const std::string& systemName,
		const std::string& presetName,
		const Mat4&        emitterTransform
	);

	// ----------------------------------------
	// Systemにプリセットを登録する簡易API
	// （JSONは変えず、コード／エディタ側から設定する用）
	// ----------------------------------------
	void RegisterSystemPreset(const std::string& systemName,
		const std::string& presetName);

	// Systemに登録されているプリセット一覧を取得（エディタ用）
	const std::vector<std::string>* GetSystemPresets(const std::string& systemName) const;

	// System名一覧（プルダウン表示用など）
	std::vector<std::string> GetAllSystemNames() const;

	// Systemを一括削除したいとき用
	void ClearAllSystems();

	// System名を指定して、登録されている全プリセットを一括Emitする
	void EmitSystemByName(const std::string& systemName, const Mat4& emitterTransform);

	// --- System JSON 保存/読み込み ---
	bool SaveSystemToJson(const std::string& systemName, const std::string& directory = "Resources/ParticleSystem");

	bool LoadSystemFromJson(const std::string& systemName, const std::string& directory = "Resources/ParticleSystem");

	void LoadAllSystems(const std::string& directory = "Resources/ParticleSystem");

	// System 名変更（Editor のリネーム用）
	bool RenameSystem(const std::string& oldName, const std::string& newName);


private:

	// プリセット管理は ParticlePresetLibrary に集約
	// （JSON 保存/読込・名前検索・プリセット保有を担当）
	ParticlePresetLibrary presetLibrary_;

	// System 管理を専用クラスに委譲
	ParticleSystemManager systemManager_;

	// ===== 新しいエミッター / システム管理用コンテナ =====

	// 新しいエミッターインスタンス（Niagara でいう Emitter Instance）
	std::vector<std::unique_ptr<ParticleEmitterInstance>> emitterInstances_;

	// System を使った Emit（ステップ2で実装）
	void EmitSystem(const std::string& systemName, const Mat4& transform);

	// EmitterInstanceのパーティクルをインスタンシングバッファへ書き込む
	void PopulateInstancesFromEmitters(const Mat4& viewProjectionMatrix, const Mat4& billboardMatrix);

	/// <summary>
	/// 頂点リソースの生成、バッファービューの作成
	/// </summary>
	void VertexBufferView();

	/// <summary>
	/// 頂点データに書き込む
	/// </summary>
	void CreateVertexData();

	/// <summary>
	/// 頂点データに書き込む
	/// </summary>
	void CreateRingVertexData();

	/// <summary>
	/// 頂点データに書き込む
	/// </summary>
	void CreateCylinderVertexData();

	/// <summary>
	/// マテリアルデータの初期化
	/// </summary>
	void  CreateMaterialData();

	/// <summary>
	/// InstancingMax用のResource
	/// </summary>
	void InstancingMaxResource();

private:

	// 頂点データの拡張
	struct VertexData {
		Vec4 position;
		Vec2 texcoord;
		Vec3 normal;
	};

	// マテリアルを拡張する
	struct Material {
		Vec4 color;
		int32_t enableLighting;
		float padding[3];
		Mat4 uvTransform;
		float shininess;
	};

	// MaterialData構造体
	struct MaterialData {
		std::string textureFilePath;
		uint32_t textureIndex = 0;
	};

	struct TransformationMatrix {
		Mat4 WVP;
		Mat4 World;
		Mat4 WorldInverseTranspose;
	};

	// Node構造体
	struct Node {
		Mat4 localMatrix;  // NodeのTransform
		std::string name;  // Nodeの名前
		std::vector<Node> children;  // 子供のNode
	};

	// ModelData構造体
	struct ModelData {
		std::vector<VertexData>vertices;
		MaterialData material;
		Node rootNode;
	};

	//Particle構造体
	struct Particle {
		Mat4 transform;
		Vec3 velocity;
		Vec3   rotationSpeed{ 0.0f, 0.0f, 0.0f };   // 回転速度(rad/秒)
		Vec3   scaleSpeed{ 0.0f, 0.0f, 0.0f };      // スケール変化速度(/秒)
		Vec4   color{ 1.0f, 1.0f, 1.0f, 1.0f };
		float lifeTime;
		float currentTime;

		// カーブ用の基準値
		Vec3 initialScale{ 1.0f, 1.0f, 1.0f };
		Vec4 initialColor{ 1.0f, 1.0f, 1.0f, 1.0f };
	};

	struct ParticleForGPU
	{
		Mat4 WVP;
		Mat4 World;
		Vec4 color;
		float flipY; // ← 追加（0.0f or 1.0f）
		float padding[3]; // アラインメントのため
	};

	// パーティクルグループ構造体の定義
	struct ParticleGroup {
		// マテリアルデータ (テクスチャファイルパスとテクスチャ用SRVインデックス)
		std::string materialFilePath;
		int srvIndex;

		// パーティクルのリスト (std::list<Particle>型)
		std::list<Particle> particleList;

		// インスタンシングデータ用SRVインデックス
		int instancingSrvIndex;

		// インスタンシングリソース
		Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource = nullptr;

		// インスタンス数
		UINT instanceCount = 0;

		UINT vertexCount = 32; // ← 追加（描画に使うインスタンスあたりの頂点数）

        // このグループが使う頂点形状（Plane / Ring / Cylinder など）
        // EnsureGroupForPreset() で preset.emitterSettings.vertexType から設定される
		VertexDataType vertexType = VertexDataType::Plane;

		// インスタンシングデータを書き込むためのポインタ
		ParticleForGPU* instancingDataPtr = nullptr;

		Vec2 textureLeftTop = { 0.0f, 0.0f }; // テクスチャ左上座標
		Vec2 textureSize = { 0.0f, 0.0f }; // テクスチャサイズを追加

		bool flipY = false; // デフォルトは反転しない

		bool useGravity = false;

		BlendMode blendMode = kBlendModeNormal;

		// このグループに適用するカーブ
		Curve1D scaleCurve;
		Curve1D colorCurve;

	};

	// ===== 形状ごとの頂点バッファ生成 =====

    // 指定形状の頂点バッファを vertexBuffers_ に登録する
	void BuildVertexBuffer(VertexDataType type);

	// 形状ごとの頂点データ配列を生成して返す（リソースは作らない）
	std::vector<VertexData> MakePlaneVertices();
	std::vector<VertexData> MakeRingVertices();
	std::vector<VertexData> MakeCylinderVertices();

	/// <summary>
	/// パーティクル生成器
	/// </summary>
	/// <param name="randomEngine"></param>
	/// <param name="translate"></param>
	/// <returns></returns>
	Particle MakeNewParticle(std::mt19937& randomEngine, const Vec3& translate);

	/// <summary>
	/// Primitiveパーティクル生成器
	/// </summary>
	/// <param name="randomEngine"></param>
	/// <param name="translate"></param>
	/// <returns></returns>
	Particle PrimitiveMakeNewParticle(std::mt19937& randomEngine, const Vec3& translate);

	/// <summary>
	/// ringパーティクル生成器
	/// </summary>
	/// <param name="translate"></param>
	/// <returns></returns>
	Particle RingMakeNewParticle(const Vec3& translate);

	/// <summary>
	/// cylinderパーティクル生成器
	/// </summary>
	/// <param name="translate"></param>
	/// <returns></returns>
	Particle CylinderMakeNewParticle(const Vec3& translate);

public:

	// プリセットの内容に合わせて ParticleGroup を準備し、
	// カーブやフラグ類を反映したうえで参照を返す共通ヘルパー
	ParticleGroup& EnsureGroupForPreset(const ParticlePreset& preset);

	/// <summary>
	/// エミッター
	/// </summary>
	/// <param name="name"></param>
	/// <param name="position"></param>
	/// <param name="count"></param>
	void Emit(const std::string& name, Mat4& transform, uint32_t count, bool useRandomPosition);

	void PrimitiveEmit(const std::string name, Mat4& transform, uint32_t count);

	void RingEmit(const std::string& name, Mat4& transform, uint32_t count);

	void CylinderEmit(const std::string& name, Mat4& transform, uint32_t count);

	// スケール
	void SetScaleToGroup(const std::string& groupName, const Vec3& scale);

	// 回転
	void SetRotationToGroup(const std::string& groupName, const Vec3& rotation);

	// 位置
	void SetTranslationToGroup(const std::string& groupName, const Vec3& translation);

	// 速度
	void SetVelocityToGroup(const std::string& groupName, const Vec3& velocity);

	// 回転速度
	void SetRotationSpeedToGroup(const std::string& groupName, const Vec3& rotationSpeed);

	// スケール速度
	void SetScaleSpeedToGroup(const std::string& groupName, const Vec3& scaleSpeed);

	// 色
	void SetColorToGroup(const std::string& groupName, const Vec4& color);

	// 寿命（LifeTime）を一括設定
	void SetLifeTimeToGroup(const std::string& groupName, float lifeTime);

	// 重力
	void SetGravityToGroup(const std::string& groupName, bool useGravity);

	// 初期時間（CurrentTime）を一括設定
	void SetCurrentTimeToGroup(const std::string& groupName, float currentTime);

	void DeleteParticleGroup(const std::string& groupName);

	void SetUseBillboard(bool use) { usebillboardMatrix = use; }


private:

	// 複数のパーティクルグループを保持
	std::unordered_map<std::string, ParticleGroup> particleGroups;

	// モデル読み込み
	ModelData modelData;

	// バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;  // 頂点バッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource;  // マテリアル用の定数バッファ
	// バッファリソース内のデータを指すポインタ
	VertexData* vertexData = nullptr;
	Material* materialData = nullptr;
	// バッファリソースの使い道を補完するビュー
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;

	// ===== Phase1 追加: 形状ごとの頂点バッファ =====
    // Plane / Ring / Cylinder それぞれに対応する頂点バッファをまとめて管理する

    // 1形状分の頂点バッファ情報
	struct VertexBufferSet {
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		D3D12_VERTEX_BUFFER_VIEW view{};
		uint32_t vertexCount = 0;
	};

	// 形状 → 頂点バッファのマップ
	// 起動時に Plane / Ring / Cylinder の3つを作っておき、
	// 描画時にグループの vertexType で引いて切り替える
	std::unordered_map<VertexDataType, VertexBufferSet> vertexBuffers_;


	Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource;

	// 乱数生成器の初期化
	std::random_device seedGenerator;
	std::mt19937 randomEngine;

	// パーティクルの最大出力数
	const uint32_t kNumMaxInstance = 10000;

private:
	static const int kWindowWidth = 1280;
	static const int kWindowHeight = 720;

	bool usebillboardMatrix = true;

	// Cameraの初期化
	Camera* camera_ = nullptr;
	//常にカメラ目線
	Mat4 cameraTransform{ {1.0f,1.0f,1.0f},{1.0f,1.0f,1.0f},{0.0f,23.0f,10.0f} };

	Mat4 worldviewProjectionMatrix;

	//テクスチャ切り出しサイズ
	Vec2 textureSize = { 0.0f,0.0f };
	std::string FilePath = {};
	//サイズ
	Vec2 size = { 640.0f,360.0f };

	bool ringSamplerAdd = false;

private:

	//テクスチャサイズイメージ合わせ
	void AdjustTextureSize(ParticleGroup& group, const std::string& textureFilePath);

	/// <summary>
	/// ルートシグネチャの作成
	/// </summary>
	void RootSignature();

	/// <summary>
	/// グラフィックスパイプラインの生成
	/// </summary>
	void GraphicsPipelineState(BlendMode blendMode);

	/// <summary>
	/// InputLayoutの設定
	/// </summary>
	void InputLayout();

	/// <summary>
	/// BlendStateの設定
	/// </summary>
	void BlendState(BlendMode blendMode);

	// メンバ変数にブレンドモードを追加
	BlendMode blendMode_; // 現在のブレンドモード
	// ブレンドモードごとにPSOをキャッシュするためのマップ
	std::unordered_map<BlendMode, Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateCache_;

	/// <summary>
	/// RasterizerStateの設定
	/// </summary>
	void RasterizerState();

	/// <summary>
	/// DepthStencilStateの設定
	/// </summary>
	void DepthStencilState();

	/// <summary>
	/// PSOの生成
	/// </summary>
	//void PSO();

	Microsoft::WRL::ComPtr<ID3D12PipelineState> PSO();

private:
	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
	// Modelの初期化
	Model* model_ = nullptr;

	//--------RootSignature部分--------//

	//DescriptorRange
	D3D12_DESCRIPTOR_RANGE descriptorRangeForInstancing_[1] = {};

	//RootSignature
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature_{};
	D3D12_ROOT_PARAMETER rootParameters_[7] = {};

	//バイナリを元に生成
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;

	//シリアライズしてバイナリにする
	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;

	//--------PSO部分--------//

	//PSO
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	//graphicsPipelineStateの生成
	Microsoft::WRL::ComPtr<ID3D12PipelineState>graphicsPipelineState = nullptr;

	//Shaderをコンパイルする
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob_{};
	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob_{};

	//デフォルトカメラ
	Camera* defaultCamera = nullptr;


	//--------InputLayout部分--------//

	//InputLayout
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc_{};
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};

	//--------BlendState部分--------//

	//BlendState
	D3D12_BLEND_DESC blendDesc_{};

	//--------RasterizerState部分--------//

	//RasterizerStateの設定
	D3D12_RASTERIZER_DESC rasterizerDesc_{};

	//--------DepthStencilState部分--------//

	//DepthStencilStateの設定
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc_{};
};
