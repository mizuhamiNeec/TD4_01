#pragma once
#include <memory>

#include <engine/CubeMap/CubeMap.h>

#include <game/scene/base/BaseScene.h>

#include <runtime/physics/core/UPhysics.h>

class D3D12;

class EmptyScene : public BaseScene {
public:
	~EmptyScene() override;
	void Init() override;
	void Update(float deltaTime) override;
	void Render() override;
	void Shutdown() override;

private:
	D3D12*                   mRenderer = nullptr;
};
