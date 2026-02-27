#pragma once
#include <string>

class ShaderManager;
class Shader;

/// @brief デフォルトシェーダクラス
class DefaultShader {
public:
	static const std::string kDefaultVSPath;
	static const std::string kDefaultPSPath;
	static const std::string kDefaultShaderName;

	static const std::string kDefaultSkinnedVSPath;
	static const std::string kDefaultSkinnedPSPath;
	static const std::string kDefaultSkinnedShaderName;

	static Shader* CreateDefaultShader(ShaderManager* shaderManager);
	static Shader* CreateDefaultSkinnedShader(ShaderManager* shaderManager);
};
