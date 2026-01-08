#pragma once
#include <map>
#include <memory>
#include <string>

class D3D12;
class ModelCommon;
class Model;

/// @brief モデル管理クラス
class ModelManager {
public:
	static ModelManager* GetInstance();

	void        Init(D3D12* d3d12);
	static void Shutdown();

	void                 LoadModel(const std::string& filePath);
	[[nodiscard]] Model* FindModel(const std::string& filePath) const;

	ModelManager(ModelManager&)            = delete;
	ModelManager& operator=(ModelManager&) = delete;

private:
	ModelManager()  = default;
	~ModelManager() = default;

	std::unique_ptr<ModelCommon> mModelCommon;

	// モデルデータ
	std::map<std::string, std::unique_ptr<Model>> mModels;
};
