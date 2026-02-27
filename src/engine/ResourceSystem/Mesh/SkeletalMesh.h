#pragma once
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <engine/ResourceSystem/Mesh/SubMesh.h>

#include "MeshTypes.h"

#include "engine/Animation/Animation.h"

/// @brief スケルタルメッシュクラス
class SkeletalMesh {
public:
	explicit SkeletalMesh(std::string name);

	~SkeletalMesh();

	void AddSubMesh(std::unique_ptr<SubMesh> subMesh);
	[[nodiscard]] const std::vector<std::unique_ptr<SubMesh>>&
	GetSubMeshes() const;

	[[nodiscard]] std::string GetName() const;

	// スケルトン関連
	void                          SetSkeleton(const Skeleton& skeleton);
	[[nodiscard]] const Skeleton& GetSkeleton() const;

	// アニメーション関連
	void AddAnimation(const std::string& name, const Animation& animation);
	[[nodiscard]] const Animation* GetAnimation(const std::string& name) const;
	[[nodiscard]] const std::map<std::string, Animation>& GetAnimations() const;

	void Render(ID3D12GraphicsCommandList* commandList) const;

	void ReleaseResource();

private:
	std::string                           mName;
	std::vector<std::unique_ptr<SubMesh>> mSubMeshes;
	Skeleton                              mSkeleton;
	std::map<std::string, Animation>      mAnimations;
};
