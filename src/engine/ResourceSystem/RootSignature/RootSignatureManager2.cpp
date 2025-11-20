#include <ranges>

#include <engine/ResourceSystem/RootSignature/RootSignature2.h>
#include <engine/ResourceSystem/RootSignature/RootSignatureManager2.h>

RootSignature2* RootSignatureManager2::GetOrCreateRootSignature(
	const std::string& key, const RootSignatureDesc& desc) {
	// 既に作成済みのルートシグネチャがあるか確認
	if (mRootSignatures.contains(key)) {
		return mRootSignatures[key].get();
	}

	// なかったので作成
	auto rootSignature = std::make_unique<RootSignature2>();
	rootSignature->Init(mDevice, desc);

	// 作成したルートシグネチャを登録
	mRootSignatures[key] = std::move(rootSignature);

	return mRootSignatures[key].get();
}

void RootSignatureManager2::Init(ID3D12Device* device) {
	mDevice = device;
}

void RootSignatureManager2::Shutdown() {
	// 各RootSignature2インスタンスが保持しているリソースを解放
	for (auto& rootSignature : mRootSignatures | std::views::values) {
		if (rootSignature) {
			// RootSignature内のComPtrを明示的にリセット
			rootSignature->Release();
			rootSignature.reset();
		}
	}

	// マップをクリア
	mRootSignatures.clear();

	// デバイスへの参照をクリア
	mDevice = nullptr;
}

std::unordered_map<std::string, std::unique_ptr<RootSignature2>>
RootSignatureManager2::mRootSignatures;
ID3D12Device* RootSignatureManager2::mDevice = nullptr;
