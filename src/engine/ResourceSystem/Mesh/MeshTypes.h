#pragma once
#include <map>
#include <string>
#include <vector>

#include "core/math/Mat4.h"

#include "engine/Animation/Node.h"

// ボーン情報を格納する構造体
struct Bone {
	std::string name;
	int         id;
	Mat4        offsetMatrix; // ボーンのオフセット行列
};

// スケルトン情報を格納する構造体
struct Skeleton {
	std::vector<Bone>          bones;
	std::map<std::string, int> boneMap;      // ボーン名からIDへのマップ
	std::vector<Mat4>          boneMatrices; // ボーン変換行列
	Node                       rootNode;
};
