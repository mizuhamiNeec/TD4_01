#include <engine/renderer/SrvManager.h>

#include <runtime/core/Properties.h>

#include <cassert>

#include "engine/OldConsole/Console.h"

/// @brief SRVマネージャーの初期化
/// @param d3d12 D3D12クラスへのポインタ
void SrvManager::Init(D3D12* d3d12) {
	// 引数で受け取ってメンバ変数に記録する
	mD3d12 = d3d12;
	assert(mD3d12 != nullptr && "D3D12 is null in SrvManager::Init");

	// ディスクリプタヒープの生成
	// SRV用のヒープでディスクリプタの数はkMaxSRVCount。SRVはShader内で触るものなので、ShaderVisibleはtrue
	mDescriptorHeap = mD3d12->CreateDescriptorHeap(
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, kMaxSrvCount, true
	);

	// ディスクリプタヒープが正常に作成されたかチェック
	assert(
		mDescriptorHeap != nullptr &&
		"Failed to create descriptor heap in SrvManager::Init"
	);

	// ディスクリプタ1個分のサイズを取得して記録
	mDescriptorSize = mD3d12->GetDevice()->GetDescriptorHandleIncrementSize(
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
	);

	// 使用状況管理配列の初期化
	mUsedTexture2DIndices.resize(
		kTexture2DEndIndex - kTexture2DStartIndex,
		false
	);
	mUsedTextureCubeIndices.resize(
		kTextureCubeEndIndex - kTextureCubeStartIndex, false
	);
	mUsedTextureArrayIndices.resize(
		kTextureArrayEndIndex - kTextureArrayStartIndex, false
	);
	mUsedStructuredBufferIndices.resize(
		kStructuredBufferEndIndex - kStructuredBufferStartIndex, false
	);

	Console::Print(
		"[SrvManager] フリーリスト方式でSRV管理を初期化しました\n",
		kConTextColorCompleted,
		Channel::RenderSystem
	);
}

/// @brief 描画前の準備
void SrvManager::PreDraw() const {
	// 描画用のDescriptorHeapの設定
	ID3D12DescriptorHeap* descriptorHeaps[] = {mDescriptorHeap.Get()};
	mD3d12->GetCommandList()->SetDescriptorHeaps(
		_countof(descriptorHeaps), descriptorHeaps
	);
}

/// @brief SRV生成(Texture2D用)
/// @param srvIndex SRVインデックス
/// @param pResource リソースへのポインタ
/// @param format フォーマット
/// @param mipLevels ミップレベル数
void SrvManager::CreateSRVForTexture2D(
	uint32_t        srvIndex,
	ID3D12Resource* pResource,
	DXGI_FORMAT     format,
	UINT            mipLevels
) const {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = mipLevels;
	srvDesc.Texture2D.PlaneSlice = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	// SRVを作成
	mD3d12->GetDevice()->CreateShaderResourceView(
		pResource, &srvDesc, GetCPUDescriptorHandle(srvIndex)
	);
}

/// @brief SRV生成(StructuredBuffer用)
/// @param srvIndex SRVインデックス
/// @param pResource リソースへのポインタ
/// @param numElements 要素数
/// @param structureByteStride 構造体のバイトサイズ
void SrvManager::CreateSRVForStructuredBuffer(
	uint32_t        srvIndex,
	ID3D12Resource* pResource,
	UINT            numElements,
	UINT            structureByteStride
) const {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = 0;

	// バッファのサイズを取得
	D3D12_RESOURCE_DESC resourceDesc = pResource->GetDesc();
	UINT64              bufferSize   = resourceDesc.Width;

	// numElements をバッファサイズに基づいて制限
	UINT maxElements = static_cast<UINT>(bufferSize / structureByteStride);
	numElements      = (std::min)(numElements, maxElements);

	srvDesc.Buffer.NumElements         = numElements;
	srvDesc.Buffer.StructureByteStride = structureByteStride;
	srvDesc.Buffer.Flags               = D3D12_BUFFER_SRV_FLAG_NONE;
	srvDesc.Format                     = DXGI_FORMAT_UNKNOWN;

	// SRVを作成
	mD3d12->GetDevice()->CreateShaderResourceView(
		pResource, &srvDesc, GetCPUDescriptorHandle(srvIndex)
	);
}

/// @brief SRV生成(TextureCube用)
/// @param index SRVインデックス
/// @param resource リソースへのスマートポインタ
/// @param format フォーマット
/// @param mipLevels ミップレベル数
void SrvManager::CreateSRVForTextureCube(
	uint32_t                               index,
	Microsoft::WRL::ComPtr<ID3D12Resource> resource,
	DXGI_FORMAT                            format,
	UINT                                   mipLevels
) {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.TextureCube.MipLevels = mipLevels;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;

	mD3d12->GetDevice()->CreateShaderResourceView(
		resource.Get(),
		&srvDesc,
		GetCPUDescriptorHandle(index)
	);
}

/// @brief グラフィックスパイプライン用にSRVをバインドします
/// @param rootParameterIndex ルートパラメータのインデックス
/// @param srvIndex SRVインデックス
void SrvManager::SetGraphicsRootDescriptorTable(
	const UINT     rootParameterIndex,
	const uint32_t srvIndex
) const {
	// インデックスが有効範囲内かチェック
	if (srvIndex >= kMaxSrvCount) {
		Console::Print(
			std::format(
				"[SrvManager] エラー: インデックス {}が上限({})を超えています\n",
				srvIndex, kMaxSrvCount
			),
			kConTextColorError,
			Channel::RenderSystem
		);
		return;
	}

	D3D12_GPU_DESCRIPTOR_HANDLE handle = GetGPUDescriptorHandle(srvIndex);
	mD3d12->GetCommandList()->SetGraphicsRootDescriptorTable(
		rootParameterIndex, handle
	);

	// 問題のテクスチャインデックスを監視 - これは常に出力
	if (srvIndex == 0) {
		Console::Print(
			std::format(
				"[SrvManager] 警告: インデックス0（デフォルトテクスチャ）がバインドされました - rootParameter={}\n",
				rootParameterIndex
			),
			kConTextColorWarning,
			Channel::RenderSystem
		);
	}
}

/// @brief SRVインデックスを1つ割り当てます
/// @return 割り当てられたSRVインデックス
uint32_t SrvManager::Allocate() {
	// 上限に達していないかチェックしてassert
	assert(mUseIndex < kMaxSrvCount);

	// return する番号を一旦記録しておく
	const int index = mUseIndex;
	// 次回のために番号を1進める
	mUseIndex++;

	// 上で記録した番号をreturn
	return index;
}

/// @brief 2Dテクスチャ用SRVインデックスを1つ割り当てます
/// @return 割り当てられた2Dテクスチャ用SRVインデックス
uint32_t SrvManager::AllocateForTexture2D() {
	uint32_t index;

	// フリーリストに返却されたインデックスがあるかチェック
	if (!mFreeTexture2DIndices.empty()) {
		// 再利用可能なインデックスを取得
		index = mFreeTexture2DIndices.front();
		mFreeTexture2DIndices.pop();
	} else {
		// 2Dテクスチャ用インデックス範囲の上限チェック
		assert(mTexture2DIndex < kTexture2DEndIndex);

		// 新しいインデックスを割り当て
		index = mTexture2DIndex;
		mTexture2DIndex++;
	}

	// 使用状況を記録
	size_t arrayIndex = index - kTexture2DStartIndex;
	if (arrayIndex < mUsedTexture2DIndices.size()) {
		mUsedTexture2DIndices[arrayIndex] = true;
	}

	return index;
}

/// @brief キューブマップテクスチャ用SRVインデックスを1つ割り当てます
/// @return 割り当てられたキューブマップテクスチャ用SRVインデックス
uint32_t SrvManager::AllocateForTextureCube() {
	uint32_t index;

	// フリーリストに返却されたインデックスがあるかチェック
	if (!mFreeTextureCubeIndices.empty()) {
		// 再利用可能なインデックスを取得
		index = mFreeTextureCubeIndices.front();
		mFreeTextureCubeIndices.pop();
	} else {
		// キューブマップテクスチャ用インデックス範囲の上限チェック
		assert(mTextureCubeIndex < kTextureCubeEndIndex);

		// 新しいインデックスを割り当て
		index = mTextureCubeIndex;
		mTextureCubeIndex++;
	}

	// 使用状況を記録
	size_t arrayIndex = index - kTextureCubeStartIndex;
	if (arrayIndex < mUsedTextureCubeIndices.size()) {
		mUsedTextureCubeIndices[arrayIndex] = true;
	}

	return index;
}

/// @brief テクスチャ配列用SRVインデックスを1つ割り当てます
/// @return 割り当てられたテクスチャ配列用SRVインデックス
uint32_t SrvManager::AllocateForTextureArray() {
	// テクスチャ配列用インデックス範囲の上限チェック
	assert(mTextureArrayIndex < kTextureArrayEndIndex);

	// return する番号を一旦記録しておく
	const uint32_t index = mTextureArrayIndex;
	// 次回のために番号を1進める
	mTextureArrayIndex++;

	// デバッグログ：テクスチャ配列用SRVインデックス割り当て状況を出力
	Console::Print(
		std::format(
			"[SrvManager] AllocateForTextureArray - Allocated texture array SRV index: {}, Next textureArrayIndex_: {}\n",
			index, mTextureArrayIndex
		),
		kConTextColorCompleted,
		Channel::RenderSystem
	);

	// 上で記録した番号をreturn
	return index;
}

/// @brief ストラクチャードバッファ用SRVインデックスを1つ割り当てます
/// @return 割り当てられたストラクチャードバッファ用SRVインデックス
uint32_t SrvManager::AllocateForStructuredBuffer() {
	uint32_t index;

	// フリーリストに返却されたインデックスがあるかチェック
	if (!mFreeStructuredBufferIndices.empty()) {
		// 再利用可能なインデックスを取得
		index = mFreeStructuredBufferIndices.front();
		mFreeStructuredBufferIndices.pop();

		Console::Print(
			std::format(
				"[SrvManager] AllocateForStructuredBuffer - Reused structured buffer SRV index: {} (from free list)\n",
				index
			),
			kConTextColorCompleted,
			Channel::RenderSystem
		);
	} else {
		// ストラクチャードバッファ用インデックス範囲の上限チェック
		assert(mStructuredBufferIndex < kStructuredBufferEndIndex);

		// 新しいインデックスを割り当て
		index = mStructuredBufferIndex;
		mStructuredBufferIndex++;

		Console::Print(
			std::format(
				"[SrvManager] AllocateForStructuredBuffer - Allocated new structured buffer SRV index: {}, Next structuredBufferIndex_: {}\n",
				index, mStructuredBufferIndex
			),
			kConTextColorCompleted,
			Channel::RenderSystem
		);
	}

	// 使用状況を記録
	size_t arrayIndex = index - kStructuredBufferStartIndex;
	if (arrayIndex < mUsedStructuredBufferIndices.size()) {
		mUsedStructuredBufferIndices[arrayIndex] = true;
	}

	return index;
}

/// @brief 連続した2Dテクスチャ用SRVインデックスを複数割り当てます
/// @param count 割り当てるインデックスの数
/// @return 割り当てられた最初の2Dテクスチャ用SRVインデックス
uint32_t SrvManager::AllocateConsecutiveTexture2DSlots(uint32_t count) {
	// 連続した2Dテクスチャスロットが確保できるかチェック
	if (mTexture2DIndex + count > kTexture2DEndIndex) {
		Console::Print(
			std::format(
				"[SrvManager] 警告: 連続した{}個の2Dテクスチャスロットを確保できません（現在のインデックス: {}）\n"
				"           代替として最初から再割り当てします\n",
				count, mTexture2DIndex
			),
			kConTextColorWarning,
			Channel::RenderSystem
		);

		// 代替案：最初から再割り当て（デバッグ用の一時的な対処）
		// 本来はフリーリストから連続スロットを探すか、より適切な管理が必要
		uint32_t startIndex = kTexture2DStartIndex;

		// 使用可能な連続スロットがあるかチェック
		if (startIndex + count <= kTexture2DEndIndex) { return startIndex; }
		Console::Print(
			std::format(
				"[SrvManager] エラー: 連続した{}個のスロットは利用できません\n",
				count
			),
			kConTextColorError,
			Channel::RenderSystem
		);
		return 0; // エラー値
	}

	// 開始インデックスを記録
	uint32_t startIndex = mTexture2DIndex;
	// インデックスを進める
	mTexture2DIndex += count;

	return startIndex;
}

/// @brief 連続したキューブマップテクスチャ用SRVインデックスを複数割り当てます
/// @param count 割り当てるインデックスの数
/// @return 割り当てられた最初のキューブマップテクスチャ用SRVインデックス
uint32_t SrvManager::AllocateConsecutiveTextureCubeSlots(uint32_t count) {
	// 連続したキューブマップテクスチャスロットが確保できるかチェック
	if (mTextureCubeIndex + count > kTextureCubeEndIndex) {
		Console::Print(
			std::format(
				"[SrvManager] エラー: 連続した{}個のキューブマップテクスチャスロットを確保できません（現在のインデックス: {}）\n",
				count, mTextureCubeIndex
			),
			kConTextColorError,
			Channel::RenderSystem
		);
		return 0; // エラー値
	}

	// 開始インデックスを記録
	uint32_t startIndex = mTextureCubeIndex;
	// インデックスを進める
	mTextureCubeIndex += count;

	Console::Print(
		std::format(
			"[SrvManager] 連続した{}個のキューブマップテクスチャスロットを確保: {}-{}\n",
			count, startIndex, startIndex + count - 1
		),
		kConTextColorCompleted,
		Channel::RenderSystem
	);

	return startIndex;
}

/// @brief Allocate2DTextureにリダイレクト
/// @return 割り当てられた2Dテクスチャ用SRVインデックス
uint32_t SrvManager::AllocateForTexture() { return AllocateForTexture2D(); }

/// @brief AllocateConsecutiveTexture2DSlotsにリダイレクト
/// @param count 割り当てるインデックスの数
/// @return 割り当てられた最初の2Dテクスチャ用SRVインデックス
uint32_t SrvManager::AllocateConsecutiveTextureSlots(uint32_t count) {
	return AllocateConsecutiveTexture2DSlots(count);
}

/// @brief 2Dテクスチャ用SRVインデックスを返却します
/// @param index 返却する2Dテクスチャ用SRVインデックス
void SrvManager::DeallocateTexture2D(uint32_t index) {
	// 範囲チェック
	if (index < kTexture2DStartIndex || index >= kTexture2DEndIndex) {
		Console::Print(
			std::format(
				"[SrvManager] エラー: 無効な2Dテクスチャインデックス {}を返却しようとしました\n",
				index
			),
			kConTextColorError,
			Channel::RenderSystem
		);
		return;
	}

	// 使用状況をクリア
	size_t arrayIndex = index - kTexture2DStartIndex;
	if (arrayIndex < mUsedTexture2DIndices.size()) {
		if (!mUsedTexture2DIndices[arrayIndex]) {
			Console::Print(
				std::format(
					"[SrvManager] 警告: 未使用の2Dテクスチャインデックス {}を返却しようとしました\n",
					index
				),
				kConTextColorWarning,
				Channel::RenderSystem
			);
			return;
		}
		mUsedTexture2DIndices[arrayIndex] = false;
	}

	// フリーリストに追加
	mFreeTexture2DIndices.push(index);

	Console::Print(
		std::format(
			"[SrvManager] 2Dテクスチャインデックス {}を返却しました（フリーリストサイズ: {}）\n",
			index, mFreeTexture2DIndices.size()
		),
		kConTextColorCompleted,
		Channel::RenderSystem
	);
}

/// @brief キューブマップテクスチャ用SRVインデックスを返却します
/// @param index 返却するキューブマップテクスチャ用SRVインデックス
void SrvManager::DeallocateTextureCube(uint32_t index) {
	// 範囲チェック
	if (index < kTextureCubeStartIndex || index >= kTextureCubeEndIndex) {
		Console::Print(
			std::format(
				"[SrvManager] エラー: 無効なキューブマップインデックス {}を返却しようとしました\n",
				index
			),
			kConTextColorError,
			Channel::RenderSystem
		);
		return;
	}

	// 使用状況をクリア
	size_t arrayIndex = index - kTextureCubeStartIndex;
	if (arrayIndex < mUsedTextureCubeIndices.size()) {
		if (!mUsedTextureCubeIndices[arrayIndex]) {
			Console::Print(
				std::format(
					"[SrvManager] 警告: 未使用のキューブマップインデックス {}を返却しようとしました\n",
					index
				),
				kConTextColorWarning,
				Channel::RenderSystem
			);
			return;
		}
		mUsedTextureCubeIndices[arrayIndex] = false;
	}

	// フリーリストに追加
	mFreeTextureCubeIndices.push(index);

	Console::Print(
		std::format(
			"[SrvManager] キューブマップインデックス {}を返却しました（フリーリストサイズ: {}）\n",
			index, mFreeTextureCubeIndices.size()
		),
		kConTextColorCompleted,
		Channel::RenderSystem
	);
}

/// @brief ストラクチャードバッファ用SRVインデックスを返却します
/// @param index 返却するストラクチャードバッファ用SRVインデックス
void SrvManager::DeallocateStructuredBuffer(uint32_t index) {
	// 範囲チェック
	if (index < kStructuredBufferStartIndex || index >=
	    kStructuredBufferEndIndex) {
		Console::Print(
			std::format(
				"[SrvManager] エラー: 無効なストラクチャードバッファインデックス {}を返却しようとしました\n",
				index
			),
			kConTextColorError,
			Channel::RenderSystem
		);
		return;
	}

	// 使用状況をクリア
	size_t arrayIndex = index - kStructuredBufferStartIndex;
	if (arrayIndex < mUsedStructuredBufferIndices.size()) {
		if (!mUsedStructuredBufferIndices[arrayIndex]) {
			Console::Print(
				std::format(
					"[SrvManager] 警告: 未使用のストラクチャードバッファインデックス {}を返却しようとしました\n",
					index
				),
				kConTextColorWarning,
				Channel::RenderSystem
			);
			return;
		}
		mUsedStructuredBufferIndices[arrayIndex] = false;
	}

	// フリーリストに追加
	mFreeStructuredBufferIndices.push(index);

	Console::Print(
		std::format(
			"[SrvManager] ストラクチャードバッファインデックス {}を返却しました（フリーリストサイズ: {}）\n",
			index, mFreeStructuredBufferIndices.size()
		),
		kConTextColorCompleted,
		Channel::RenderSystem
	);
}

/// @brief DeallocateTexture2Dにリダイレクト
/// @param index 返却する2Dテクスチャ用SRVインデックス
void SrvManager::DeallocateTexture(uint32_t index) {
	DeallocateTexture2D(index);
}

/// @brief テクスチャ配列用SRVインデックスを返却します
/// @param index 返却するテクスチャ配列用SRVインデックス
void SrvManager::DeallocateTextureArray(uint32_t index) {
	// 範囲チェック
	if (index < kTextureArrayStartIndex || index >= kTextureArrayEndIndex) {
		Console::Print(
			std::format(
				"[SrvManager] エラー: 無効なテクスチャ配列インデックス {}を返却しようとしました\n",
				index
			),
			kConTextColorError,
			Channel::RenderSystem
		);
		return;
	}

	// 使用状況をクリア
	size_t arrayIndex = index - kTextureArrayStartIndex;
	if (arrayIndex < mUsedTextureArrayIndices.size()) {
		if (!mUsedTextureArrayIndices[arrayIndex]) {
			Console::Print(
				std::format(
					"[SrvManager] 警告: 未使用のテクスチャ配列インデックス {}を返却しようとしました\n",
					index
				),
				kConTextColorWarning,
				Channel::RenderSystem
			);
			return;
		}
		mUsedTextureArrayIndices[arrayIndex] = false;
	}

	// フリーリストに追加
	mFreeTextureArrayIndices.push(index);

	Console::Print(
		std::format(
			"[SrvManager] テクスチャ配列インデックス {}を返却しました（フリーリストサイズ: {}）\n",
			index, mFreeTextureArrayIndices.size()
		),
		kConTextColorCompleted,
		Channel::RenderSystem
	);
}

/// @brief ディスクリプタヒープを取得します
/// @return ディスクリプタヒープへのポインタ
ID3D12DescriptorHeap* SrvManager::GetDescriptorHeap() const {
	assert(
		mDescriptorHeap != nullptr &&
		"Descriptor heap is null in SrvManager::GetDescriptorHeap"
	);
	return mDescriptorHeap.Get();
}

/// @brief CPUディスクリプタハンドルを取得します
/// @param index 取得するディスクリプタのインデックス
/// @return 取得したCPUディスクリプタハンドル
D3D12_CPU_DESCRIPTOR_HANDLE SrvManager::GetCPUDescriptorHandle(
	const uint32_t index
) const {
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = mDescriptorHeap->
		GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += mDescriptorSize * index;
	return handleCPU;
}

/// @brief GPUディスクリプタハンドルを取得します
/// @param index 取得するディスクリプタのインデックス
/// @return 取得したGPUディスクリプタハンドル
D3D12_GPU_DESCRIPTOR_HANDLE SrvManager::GetGPUDescriptorHandle(
	const uint32_t index
) const {
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = mDescriptorHeap->
		GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += mDescriptorSize * index;
	return handleGPU;
}

/// @brief CPUディスクリプタハンドルからインデックスを取得します
/// @param cpuHandle CPUディスクリプタハンドル
/// @return 取得したインデックス
uint32_t SrvManager::GetIndexFromCPUHandle(
	const D3D12_CPU_DESCRIPTOR_HANDLE& cpuHandle
) const {
	const uint32_t index = static_cast<uint32_t>(
		(
			cpuHandle.ptr - mDescriptorHeap->
			                GetCPUDescriptorHandleForHeapStart().
			                ptr
		) / mDescriptorSize
	);
	return index;
}

/// @brief GPUディスクリプタハンドルからインデックスを取得します
/// @param gpuHandle GPUディスクリプタハンドル
/// @return 取得したインデックス
uint32_t SrvManager::GetIndexFromGPUHandle(
	const D3D12_GPU_DESCRIPTOR_HANDLE& gpuHandle
) const {
	const uint32_t index = static_cast<uint32_t>(
		(
			gpuHandle.ptr - mDescriptorHeap->
			                GetGPUDescriptorHandleForHeapStart().
			                ptr
		) / mDescriptorSize
	);
	return index;
}

/// @brief まだSRVインデックスを割り当て可能かどうかを取得します
/// @return 割り当て可能ならtrue、上限に達しているならfalse
bool SrvManager::CanAllocate() const { return mUseIndex < kMaxSrvCount; }
