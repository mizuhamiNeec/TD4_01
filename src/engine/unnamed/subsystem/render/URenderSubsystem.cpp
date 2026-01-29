#include <pch.h>
#include <unordered_set>

#include <engine/unnamed/subsystem/interface/ServiceLocator.h>
#include <engine/unnamed/subsystem/render/URenderSubsystem.h>
#include <game/gameframework/component/MeshRenderer/MeshRendererComponent.h>
#include <game/gameframework/component/Transform/TransformComponent.h>
#include <game/gameframework/world/UWorld.h>

#include "engine/unnamed/uuploadarena/UploadArena.h"

#include "runtime/assets/core/UAssetManager.h"
#include "runtime/materials/UMaterialRuntime.h"
#include "runtime/render/resources/RenderResourceManager.h"
#include "runtime/render/utils/Frustum.h"
#include "runtime/render/utils/RenderUtils.h"

namespace Unnamed {
	namespace {
		/// @brief ワールド行列とビュー行列からビュー空間での深度を計算します
		/// @param matWorld ワールド行列
		/// @param view ビュー行列
		/// @return ビュー空間での深度値
		float ComputeViewDepth(const Mat4& world, const Mat4& view) {
			const auto worldPos = Vec4(
				world.m[3][0], world.m[3][1],
				world.m[3][2], 1.0f
			);
			const Vec4 viewPos = worldPos * view;
			return viewPos.z;
		}
	}

	/// @brief コンストラクタ
	/// @param graphicsDevice グラフィックスデバイス
	/// @param renderResourceManager レンダーリソースマネージャ
	/// @param shaderLibrary シェーダーライブラリ
	/// @param rootSignatureCache ルートシグネチャキャッシュ
	/// @param pipelineCache パイプラインキャッシュ
	/// @param assetManager アセットマネージャ
	URenderSubsystem::URenderSubsystem(
		GraphicsDevice*        graphicsDevice,
		RenderResourceManager* renderResourceManager,
		ShaderLibrary*         shaderLibrary,
		RootSignatureCache*    rootSignatureCache,
		UPipelineCache*        pipelineCache,
		UAssetManager*         assetManager
	) : mGraphicsDevice(graphicsDevice),
	    mRenderResourceManager(renderResourceManager),
	    mShaderLibrary(shaderLibrary),
	    mRootSignatureCache(rootSignatureCache),
	    mPipelineCache(pipelineCache),
	    mAssetManager(assetManager) {
		ServiceLocator::Register<URenderSubsystem>(this);
	}

	FrameContext URenderSubsystem::GetContext() const { return mContext; }

	/// @brief 初期化
	/// @return 初期化成功ならtrue
	bool URenderSubsystem::Init() {
		if (mAssetManager) {
			mAssetManager->RegisterReload(
				[this](AssetID id) { OnAssetReloaded(id); }
			);
		}

		DevMsg("Render", "sizeof(instanceData) = {}", sizeof(InstanceData));

		return true;
	}

	const std::string_view URenderSubsystem::GetName() const {
		return "RenderSubsystem";
	}

	/// @brief フレーム開始処理
	void URenderSubsystem::BeginFrame() {
		mItems.clear();

		for (auto& [keep, fence, fenceValue] : mTransient) {
			if (fence && fence->GetCompletedValue() >= fenceValue) {
				keep.clear();
				fence.Reset();
				fenceValue = 0;
			}
		}

		mContext = mGraphicsDevice->BeginFrame();

		const bool ok = mRenderResourceManager->GetUploadArena()->BeginFrame(
			mContext.backIndex
		);
		if (!ok) {
			auto fb = mGraphicsDevice->GetFrameBuffer(mContext.backIndex);
			fb.fence->SetEventOnCompletion(fb.fenceValue, fb.event);
			WaitForSingleObject(fb.event, INFINITE);
			mRenderResourceManager->GetUploadArena()->BeginFrame(
				mContext.backIndex
			);
		}

		auto& [keep, fence, fenceValue] = mTransient[mContext.backIndex];
		keep.clear();
		fence.Reset();
		fenceValue = 0;

		FrameCBData f;
		f = {
			.view      = mView.view,
			.proj      = mView.proj,
			.viewProj  = mView.viewProj,
			.cameraPos = mView.cameraPos,
			.time      = 0.0f
		};
		mFrameCBVA = UploadCB(&f, sizeof(f));

		ObjectCBData dummyObj = {
			.world                 = Mat4::identity,
			.worldInverseTranspose = Mat4::identity,
		};
		mDummyObjectCBVA = UploadCB(&dummyObj, sizeof(dummyObj));

		// フレーム開始時にフラスタムの更新
		mFrustum = Frustum::FromViewProjRowVec(mView.viewProj);
	}

	/// @brief ワールドのレンダリング
	/// @param world レンダリングするワールド
	void URenderSubsystem::RenderWorld(const UWorld& world) {
		Collect(world, Mat4::identity);
		std::ranges::sort(
			mItems,
			[](const RenderItem& a, const RenderItem& b) {
				if (a.psoId != b.psoId) { return a.psoId < b.psoId; }

				if (a.materialKey != b.materialKey) {
					return a.materialKey < b.materialKey;
				}

				if (a.meshHandle.id != b.meshHandle.id) {
					return a.meshHandle.id < b.meshHandle.id;
				}

				if (a.meshHandle.gen != b.meshHandle.gen) {
					return a.meshHandle.gen < b.meshHandle.gen;
				}

				return a.depthVS < b.depthVS;
			}
		);

		// デバッグ: 描画順序を確認（最初の10個だけ）
		static bool logged = false;
		if (!logged && !mItems.empty()) {
			DevMsg("Render", "=== Draw Order (first 10) ===");
			for (size_t i = 0; i < std::min<size_t>(10, mItems.size()); ++i) {
				DevMsg(
					"Render",
					"[{}] depth={:.2f}, pso={}, mat={}",
					i,
					mItems[i].depthVS,
					mItems[i].psoId,
					mItems[i].materialKey
				);
			}
			logged = true;
		}

		DrawItems();
	}

	/// @brief フレーム終了処理
	void URenderSubsystem::EndFrame() {
		mGraphicsDevice->EndFrame(mContext);

		auto buffer = mGraphicsDevice->GetFrameBuffer(mContext.backIndex);
		mTransient[mContext.backIndex].fence = buffer.fence;
		mTransient[mContext.backIndex].fenceValue = buffer.fenceValue;
		mRenderResourceManager->FlushUploads(
			buffer.fence.Get(), buffer.fenceValue
		);
	}

	/// @brief ワールドからレンダリングアイテムを収集します
	/// @param world ワールド
	/// @param parent 親の変換行列
	void URenderSubsystem::Collect(const UWorld& world, const Mat4& parent) {
		for (auto& e : world.Entities()) {
			if (!e) { continue; }
			auto* tr = e->GetComponent<TransformComponent>();
			auto* mr = e->GetComponent<MeshRendererComponent>();
			if (tr && mr) {
				if (
					mr->EnsureGPU(
						mGraphicsDevice, mRenderResourceManager,
						mShaderLibrary, mRootSignatureCache,
						mPipelineCache, mContext.cmd
					)
				) {
					const MeshGPU* mesh = mRenderResourceManager->GetMesh(
						mr->meshHandle
					);
					if (!mesh) { continue; }

					const Mat4 worldMat = tr->WorldMat() * parent;

					mr->UpdateWorldBoundsCache(
						worldMat, tr->WorldRevision(), mesh->bounds
					);
					const auto& bs = mr->WorldBoundsSphere();
					if (!mFrustum.TestSphere(bs.center, bs.radius)) {
						continue;
					}

					RenderItem it{};
					it.matWorld   = worldMat;
					it.tr         = tr;
					it.meshHandle = mr->meshHandle;

					it.materialAsset = mr->materialAsset;
					it.material      = EnsureMaterialGPU(
						it.materialAsset, mContext.cmd
					);
					if (!it.material) { continue; }

					it.depthVS = ComputeViewDepth(it.matWorld, mView.view);
					it.psoId = it.material->pso.id;
					it.materialKey = it.materialAsset;
					it.rsPtr = mRootSignatureCache->Get(it.material->root);

					mItems.emplace_back(std::move(it));
				}
			}
		}

		for (const auto& child : world.Children()) {
			const auto& worldPtr        = child.world;
			const auto& parentTransform = child.parentTransform;
			Mat4        p               = parent;
			if (parentTransform) { p = parentTransform->WorldMat() * p; }
			if (worldPtr) { Collect(*worldPtr, p); }
		}
	}

	/// @brief レンダリングアイテムを描画します
	void URenderSubsystem::DrawItems() {
		auto* cmd = mContext.cmd;

		const std::array heaps = {
			mGraphicsDevice->GetSrvAllocator()->GetHeap(),
			mGraphicsDevice->GetSamplerAllocator()->GetHeap()
		};
		cmd->SetDescriptorHeaps(static_cast<UINT>(heaps.size()), heaps.data());

		const ID3D12RootSignature* lastRs  = nullptr;
		uint32_t                   lastPso = 0;
		uint32_t                   lastMat = UINT32_MAX;

		size_t i = 0;
		while (i < mItems.size()) {
			const RenderItem& head = mItems[i];

			const MeshGPU* mesh = mRenderResourceManager->GetMesh(
				head.meshHandle
			);

			if (!mesh) {
				++i;
				continue;
			}

			// 同じPSO、マテリアル、メッシュをまとめる
			size_t j = i;
			while (j < mItems.size()) {
				const auto& it = mItems[j];
				if (it.psoId != head.psoId) { break; }
				if (it.materialKey != head.materialKey) { break; }
				if (it.meshHandle != head.meshHandle) { break; }
				++j;
			}

			const uint32_t instanceCount = static_cast<uint32_t>(j - i);

			if (head.psoId != lastPso || head.rsPtr != lastRs) {
				cmd->SetGraphicsRootSignature(head.rsPtr);
				cmd->SetPipelineState(mPipelineCache->Get({head.psoId}));
				lastPso = head.psoId;
				lastRs  = head.rsPtr;

				head.material->Apply(
					cmd, mRenderResourceManager, mContext.backIndex
				);
				lastMat = head.materialKey;
			} else if (head.materialKey != lastMat) {
				head.material->Apply(
					cmd, mRenderResourceManager,
					mContext.backIndex
				);
				lastMat = head.materialKey;
			}

			// FrameCB
			cmd->SetGraphicsRootConstantBufferView(
				head.material->rootParams.frameCB, mFrameCBVA
			);

			// Dummy
			cmd->SetGraphicsRootConstantBufferView(
				head.material->rootParams.objectCB, mDummyObjectCBVA
			);

			// インスタンスデータをアップロードアリーナに書く
			const size_t bytes = sizeof(InstanceData) * instanceCount;
			const auto   slice = mRenderResourceManager->GetUploadArena()->
				Allocate(bytes, 16);
			if (!slice.cpu) {
				Warning(
					"Render",
					"UploadArena capacity exceeded (instances={}, bytes={})",
					instanceCount, bytes
				);
				break;
			}
			auto* dst = reinterpret_cast<InstanceData*>(slice.cpu);
			for (size_t k = 0; k < instanceCount; ++k) {
				const auto& srcItem = mItems[i + k];
				const auto& [
					worldCol0, worldCol1, worldCol2,
					normalCol0, normalCol1, normalCol2
				] = srcItem.tr->RenderCache();

				std::memcpy(dst[k].worldCol0, worldCol0, sizeof(float) * 4);
				std::memcpy(dst[k].worldCol1, worldCol1, sizeof(float) * 4);
				std::memcpy(dst[k].worldCol2, worldCol2, sizeof(float) * 4);

				std::memcpy(
					dst[k].normalCol0, normalCol0, sizeof(float) * 4
				);
				std::memcpy(
					dst[k].normalCol1, normalCol1, sizeof(float) * 4
				);
				std::memcpy(
					dst[k].normalCol2, normalCol2, sizeof(float) * 4
				);
			}

			cmd->SetGraphicsRootShaderResourceView(
				head.material->rootParams.instanceSRV,
				slice.gpuVirtualAddress
			);

			mRenderResourceManager->BindVertexBuffer(cmd, mesh->vb);
			mRenderResourceManager->BindIndexBuffer(cmd, mesh->ib);
			cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			if (mesh->indexCount > 0) {
				cmd->DrawIndexedInstanced(
					mesh->indexCount,
					instanceCount,
					mesh->firstIndex,
					mesh->baseVertex,
					0
				);
			} else { cmd->DrawInstanced(3, instanceCount, 0, 0); }

			i = j;
		}
	}

	UMaterialRuntime* URenderSubsystem::EnsureMaterialGPU(
		AssetID materialAsset, ID3D12GraphicsCommandList* cmd
	) {
		if (materialAsset == kInvalidAssetID || !mAssetManager) {
			return nullptr;
		}

		const auto& meta = mAssetManager->Meta(materialAsset);
		if (meta.type != UASSET_TYPE::MATERIAL) { return nullptr; }

		auto& entry = mMaterialCache[materialAsset];

		if (!entry.runtime || entry.lastBuiltVersion != meta.version) {
			entry.runtime                = std::make_unique<UMaterialRuntime>();
			entry.runtime->materialAsset = materialAsset;

			const bool ok = entry.runtime->BuildCPU(
				mAssetManager,
				mShaderLibrary,
				mRootSignatureCache,
				mPipelineCache,
				mGraphicsDevice
			);

			if (!ok) {
				entry.runtime.reset();
				return nullptr;
			}

			entry.runtime->RealizeGPU(mRenderResourceManager, cmd);
			entry.lastBuiltVersion = meta.version;
		}

		return entry.runtime.get();
	}

	void URenderSubsystem::OnAssetReloaded(AssetID id) {
		if (!mAssetManager) { return; }

		const auto& meta = mAssetManager->Meta(id);

		// マテリアルが更新された
		if (meta.type == UASSET_TYPE::MATERIAL) {
			mMaterialCache.erase(id);
			return;
		}

		// テクスチャ、シェーダーが更新されたら、参照しているマテリアルを無効にする
		if (meta.type == UASSET_TYPE::TEXTURE || meta.type ==
		    UASSET_TYPE::SHADER) {
			// 参照元
			const auto dependencies = mAssetManager->Dependencies(id);
			for (const AssetID parent : dependencies) {
				const auto& parentMeta = mAssetManager->Meta(parent);
				if (parentMeta.type == UASSET_TYPE::MATERIAL) {
					mMaterialCache.erase(parent);
				}
			}
		}
	}

	/// @brief 定数バッファをアップロードします
	/// @param src コピー元データ
	/// @param bytes コピーするバイト数
	/// @return GPU仮想アドレス
	D3D12_GPU_VIRTUAL_ADDRESS URenderSubsystem::UploadCB(
		const void* src, const size_t bytes
	) const {
		const size_t aligned = (bytes + 255) & ~static_cast<size_t>(255);
		const auto   slice = mRenderResourceManager->GetUploadArena()->Allocate(
			aligned, 256
		);
		if (!slice.cpu) {
			// 足りないときのフォールバック（警告だけ出して早期 return でもOK）
			Warning(
				"Render",
				"UploadArena capacity exceeded (size={}, frame={})",
				aligned, mContext.backIndex
			);
			return 0;
		}
		std::memcpy(slice.cpu, src, bytes);
		return slice.gpuVirtualAddress;
	}
}
