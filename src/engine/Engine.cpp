#include <pch.h>

#ifdef _DEBUG
#include <imgui_internal.h>
// ImGuizmoのインクルードはImGuiより後
#include <ImGuizmo.h>
#endif

#include <engine/Engine.h>
#include <engine/Camera/CameraManager.h>
#include <engine/Debug/DebugDraw.h>
#include <engine/Debug/DebugHud.h>
#include <engine/Input/InputSystem.h>
#include <engine/OldConsole/ConCommand.h>
#include <engine/OldConsole/ConVarManager.h>
#include <engine/postprocess/PPBloom.h>
#include <engine/postprocess/PPChromaticAberration.h>
#include <engine/postprocess/PPRadialBlur.h>
#include <engine/postprocess/PPVignette.h>
#include <engine/renderer/SrvManager.h>
#include <engine/unnamed/subsystem/console/ConsoleScriptParser.h>
#include <engine/unnamed/subsystem/console/ConsoleSystem.h>
#include <engine/unnamed/subsystem/console/concommand/UnnamedConVar.h>
#include <engine/unnamed/subsystem/interface/ServiceLocator.h>
#include <engine/unnamed/subsystem/time/TimeSystem.h>
#include <engine/TextureManager/TexManager.h>
#include <engine/Window/MainWindow.h>
#include <engine/Window/WindowsUtils.h>
#include <engine/ImGui/ImGuiWidgets.h>

#include <game/scene/EmptyScene.h>
#include <game/scene/GameScene.h>

#include "platform/Win32App.h"

#include "ResourceSystem/Audio/AudioManager.h"

#include "unnamed/subsystem/input/device/keyboard/KeyboardDevice.h"
#include "unnamed/subsystem/input/device/mouse/MouseDevice.h"

namespace Unnamed {
	static constexpr std::string_view kChannel = "Engine";

	/// @brief コンストラクタ
	Engine::Engine() = default;

	/// @brief デストラクタ
	Engine::~Engine() = default;

	int Engine::Run() {
		if (!Init()) {
			UASSERT(false && "Failed to initialize Engine");
			return EXIT_FAILURE;
		}
		while (!Win32App::PollEvents()) {
			if (OldWindowManager::ProcessMessage()) {
				break;
			}
			Tick();
		}
		Shutdown();
		return EXIT_SUCCESS;
	}

	/// @brief 初期化
	/// @return 成功したらtrueを返す
	bool Engine::Init() {
		if (mConfig.mode == ENGINE_MODE::EDITOR) {
		}

#ifdef _DEBUG
		ConVarManager::RegisterConVar<bool>(
			"verbose", true, "Enable verbose logging"
		);
#else
		ConVarManager::RegisterConVar<bool>(
			"verbose", false, "Enable verbose logging"
		);
#endif

		// ConsoleSystemの初期化
		mConsoleSystem = std::make_unique<ConsoleSystem>();
		if (!mConsoleSystem->Init()) { return false; }

		// TimeSystemの初期化
		mTimeSystem = std::make_unique<TimeSystem>();
		if (!mTimeSystem->Init()) { return false; }

		// メインウィンドウの作成
		auto gameWindow = std::make_unique<MainWindow>();

		// ウィンドウ情報の設定
		WindowInfo gameWindowInfo = {
			.title     = "GameWindow",
			.width     = kClientWidth,
			.height    = kClientHeight,
			.style     = WS_OVERLAPPEDWINDOW,
			.exStyle   = 0,
			.hInstance = GetModuleHandle(nullptr),
			.className = "gameWindowClassName"
		};

		// ウィンドウの作成
		if (gameWindow->Create(gameWindowInfo)) {
			mWindowManager->AddWindow(std::move(gameWindow));
		} else {
			Fatal("Engine", "Failed to create main window.");
			throw std::runtime_error("Failed to create main window.");
		}

		// InputSystemの初期化
		mInputSystem = std::make_unique<UInputSystem>();
		if (!mInputSystem->Init()) { return false; }
		// デバイス登録
		const auto keyboardDevice = std::make_shared<KeyboardDevice>(
			mWindowManager->GetMainWindow()->GetWindowHandle()
		);
		const auto mouseDevice = std::make_shared<MouseDevice>(
			mWindowManager->GetMainWindow()->GetWindowHandle()
		);
		mInputSystem->RegisterDevice(keyboardDevice);
		mInputSystem->RegisterDevice(mouseDevice);

		mRenderer = std::make_unique<D3D12>(mWindowManager->GetMainWindow());

		mWindowManager->GetMainWindow()->SetResizeCallback(
			[this](
			[[maybe_unused]] const uint32_t width,
			[[maybe_unused]] const uint32_t height
		) {
				OnResize(width, height);
			}
		);

		InputSystem::Init();

		RegisterConsoleCommandsAndVariables();

		// 各マネージャーの初期化
		mResourceManager = std::make_unique<ResourceManager>(mRenderer.get());
		mSrvManager      = std::make_unique<SrvManager>();
		mSrvManager->Init(mRenderer.get());
		mResourceManager->Init();
		mRenderer->SetShaderResourceViewManager(mSrvManager.get());
		mRenderer->Init();

		mAudioManager = std::make_unique<AudioManager>();
		mAudioManager->Init();

#ifdef _DEBUG
		// ImGuiFontテクスチャ用にSRVを確保
		mSrvManager->AllocateForTexture2D();
		// ImGuiManagerの初期化
		mImGuiManager = std::make_unique<ImGuiManager>(
			mRenderer.get(), mSrvManager.get()
		);
#endif

		// コンソールを作成
		mConsole = std::make_unique<Console>();

#pragma region PostProcessInit
		auto* wndMgr = mWindowManager->GetMainWindow();

		const uint32_t kClWidth  = wndMgr->GetClientWidth();
		const uint32_t kClHeight = wndMgr->GetClientHeight();

		// オフスクリーンレンダリングターゲットの作成
		mOffscreenRtv = mRenderer->CreateRenderTargetTexture(
			kClWidth, kClHeight, kOffscreenClearColor, kBufferFormat
		);

		// オフスクリーン用の深度テクスチャの作成
		mOffscreenDsv = mRenderer->CreateDepthStencilTexture(
			kClWidth, kClHeight, DXGI_FORMAT_D32_FLOAT
		);

		// ピンポン用レンダーターゲットの作成
		for (auto& i : mPingRtv) {
			i = mRenderer->CreateRenderTargetTexture(
				kClWidth, kClHeight, kOffscreenClearColor, kBufferFormat
			);
		}

		// ポストプロセス後のレンダーターゲットの作成
		mPostProcessedRtv = mRenderer->CreateRenderTargetTexture(
			kClWidth, kClHeight, kOffscreenClearColor, kBufferFormat
		);

		// ポストプロセス後の深度テクスチャの作成
		mPostProcessedDsv = mRenderer->CreateDepthStencilTexture(
			kClWidth, kClHeight, DXGI_FORMAT_D32_FLOAT
		);

		mOffscreenRenderPassTargets = {
			.pRTVs        = &mOffscreenRtv.rtvHandle,
			.numRTVs      = 1,
			.pDSV         = &mOffscreenDsv.dsvHandle,
			.clearColor   = kOffscreenClearColor,
			.clearDepth   = 1.0f,
			.clearStencil = 0,
			.bClearColor  = true,
			.bClearDepth  = true,
		};

		mPostProcessedRenderPassTargets = {
			.pRTVs        = &mPostProcessedRtv.rtvHandle,
			.numRTVs      = 1,
			.pDSV         = &mPostProcessedDsv.dsvHandle,
			.clearColor   = kOffscreenClearColor,
			.clearDepth   = 1.0f,
			.clearStencil = 0,
			.bClearColor  = true,
			.bClearDepth  = true,
		};

		mPostChain.emplace_back(
			std::make_unique<CopyImagePass>(
				mRenderer->GetDevice(), mSrvManager.get()
			)
		);

		mPostChain.emplace_back(
			std::make_unique<PPBloom>(
				mRenderer->GetDevice(), mSrvManager.get()
			)
		);

		mPostChain.emplace_back(
			std::make_unique<PPBloom>(
				mRenderer->GetDevice(), mSrvManager.get()
			)
		);

		// 奇数個のポストプロセスは適応されない不具合があるのでダミー TODO: はい?
		reinterpret_cast<PPBloom*>(mPostChain.back().get())->SetStrength(0.0f);

		mPostChain.emplace_back(
			std::make_unique<PPVignette>(
				mRenderer->GetDevice(), mSrvManager.get()
			)
		);

		mPostChain.emplace_back(
			std::make_unique<PPChromaticAberration>(
				mRenderer->GetDevice(), mSrvManager.get()
			)
		);

		mPostChain.emplace_back(
			std::make_unique<PPRadialBlur>(
				mRenderer->GetDevice(), mSrvManager.get()
			)
		);
#pragma endregion

		TexManager::GetInstance()->Init(mRenderer.get(), mSrvManager.get());

		// スプライト
		mSpriteCommon = std::make_unique<SpriteCommon>();
		mSpriteCommon->Init(mRenderer.get());

		// パーティクル
		mParticleManager = std::make_unique<ParticleManager>();
		mParticleManager->Init(mRenderer.get(), mSrvManager.get());

		// ライン
		mLineCommon = std::make_unique<LineCommon>();
		mLineCommon->Init(mRenderer.get());

		DebugDraw::Init(mLineCommon.get());

		//---------------------------------------------------------------------
		// すべての初期化が完了
		//---------------------------------------------------------------------

		Console::SubmitCommand("neofetch");

		// エンティティローダーの作成
		mEntityLoader = std::make_unique<EntityLoader>();

		// シーンマネージャ/ファクトリーの作成
		mSceneFactory = std::make_unique<SceneFactory>();
		mSceneManager = std::make_shared<SceneManager>(*mSceneFactory);
		// ゲームシーンを登録
		mSceneFactory->RegisterScene<GameScene>("GameScene");
		mSceneFactory->RegisterScene<EmptyScene>("EmptyScene");
		// シーンの初期化
		mSceneManager->ChangeScene("GameScene");

		//---------------------------------------------------------------------
		// エディターの初期化
		//---------------------------------------------------------------------
		CheckEditorMode();

		assert(SUCCEEDED(mRenderer->GetCommandList()->Close()));

		ConsoleScriptParser scriptParser(
			"./content/core/cfg/config_default.cfg"
		);

		return true;
	}

	/// @brief 更新
	void Engine::Tick() {
		mTimeSystem->BeginFrame();
		float deltaTime = mTimeSystem->GetGameTime()->DeltaTime<float>();

		mInputSystem->Update(deltaTime);

#ifdef _DEBUG
		ImGuiManager::NewFrame();
		ImGuizmo::BeginFrame();
#endif

		mConsoleSystem->Update(deltaTime);

		// 前のフレームとeditorModeが違う場合はエディターモードを切り替える
		static bool bPrevEditorMode = mIsEditorMode;
		if (bPrevEditorMode != mIsEditorMode) {
			CheckEditorMode();
			bPrevEditorMode = mIsEditorMode;
		}

		/* ----------- 更新処理 ---------- */

		if (mIsEditorMode) {
#ifdef _DEBUG
			mEditor->DrawMenuBars();

			// DockSpace
			{
				const ImGuiViewport* viewport = ImGui::GetMainViewport();
				ImGui::SetNextWindowPos(viewport->WorkPos);
				ImGui::SetNextWindowSize(viewport->WorkSize);
				ImGui::SetNextWindowViewport(viewport->ID);
				ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
				ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
				ImGui::PushStyleVar(
					ImGuiStyleVar_WindowPadding,
					ImVec2(0.0f, 0.0f)
				);

				constexpr ImGuiDockNodeFlags dockSpaceFlags =
					ImGuiDockNodeFlags_PassthruCentralNode;
				constexpr ImGuiWindowFlags windowFlags =
					ImGuiWindowFlags_NoTitleBar |
					ImGuiWindowFlags_NoCollapse |
					ImGuiWindowFlags_NoResize |
					ImGuiWindowFlags_NoMove |
					ImGuiWindowFlags_NoBringToFrontOnFocus |
					ImGuiWindowFlags_NoNavFocus |
					ImGuiWindowFlags_NoBackground;

				ImGui::Begin("DockSpace", nullptr, windowFlags);

				ImGui::PopStyleVar(3);

				const ImGuiIO& io = ImGui::GetIO();
				if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
					const ImGuiID dockSpaceId = ImGui::GetID("MyDockSpace");
					ImGui::DockSpace(
						dockSpaceId, ImVec2(0.0f, 0.0f),
						dockSpaceFlags
					);
				}

				ImGui::End();
			}

			static auto tint = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
			static auto bg   = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

			ImGuiWindowFlags windowFlags =
				ImGuiWindowFlags_NoScrollbar |
				ImGuiWindowFlags_NoScrollWithMouse;

			if (ImGuizmo::IsUsing()) { windowFlags |= ImGuiWindowFlags_NoMove; }

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
			ImGui::Begin(
				"ViewPort",
				nullptr,
				windowFlags
			);
			ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());

			ImVec2     avail = ImGui::GetContentRegionAvail();
			const auto ptr   = mPingRtv[mPingIndex].srvHandleGPU.ptr;

			static int prevW = 0, prevH = 0;
			int        w     = static_cast<int>(avail.x);
			int        h     = static_cast<int>(avail.y);
			if ((w != prevW || h != prevH) && w > 0 && h > 0) {
				prevW = w;
				prevH = h;
			}

			if (ptr) {
				// リソースからテクスチャの幅と高さを取得
				auto        desc      = mPingRtv[mPingIndex].rtv->GetDesc();
				const float texWidth  = static_cast<float>(desc.Width);
				const float texHeight = static_cast<float>(desc.Height);

				const float availAspect = avail.x / avail.y;
				const float texAspect   = texWidth / texHeight;

				ImVec2 drawSize = avail;
				if (availAspect > texAspect) {
					drawSize.x = avail.y * texAspect;
				} else { drawSize.y = avail.x / texAspect; }

				float titleBarHeight = ImGui::GetCurrentWindow()->
					TitleBarHeight;

				ImVec2 offset = {
					(avail.x - drawSize.x) * 0.5f,
					(avail.y - drawSize.y) * 0.5f + titleBarHeight
				};
				ImGui::SetCursorPos(offset);

				ImVec2 viewportScreenPos = ImGui::GetCursorScreenPos();

				ImGui::ImageWithBg(
					ptr, // postProcessedRTV_のSRVを直接使用
					drawSize,
					ImVec2(0, 0), ImVec2(1, 1),
					bg, tint
				);

				mViewportLT   = {viewportScreenPos.x, viewportScreenPos.y};
				mViewportSize = {drawSize.x, drawSize.y};
			}
			ImGui::End();
			ImGui::PopStyleVar();
#endif

			if (mEditor) {
				mEditor->Update(mTimeSystem->GetGameTime()->DeltaTime<float>());
			}

#ifdef _DEBUG
			ImGui::Begin("Post Process");
			for (int i = 0; i < mPostChain.size(); ++i) {
				if (i == 2) { continue; }
				if (auto& postProcess = mPostChain[i]) {
					postProcess->Update(
						mTimeSystem->GetGameTime()->DeltaTime<float>()
					);
				}
			}
			ImGui::End();
#endif
		} else {
			mSceneManager->Update(
				mTimeSystem->GetGameTime()->ScaledDeltaTime<float>()
			);
			mViewportLT   = Vec2::zero;
			mViewportSize = {
				static_cast<float>(mWindowManager->GetMainWindow()->
				                                   GetClientWidth()),
				static_cast<float>(mWindowManager->GetMainWindow()->
				                                   GetClientHeight())
			};
		}

		InputSystem::Update();

#ifdef _DEBUG
		Console::Update();
		DebugHud::Update(mTimeSystem->GetGameTime()->ScaledDeltaTime<float>());
#endif

#ifdef _DEBUG
		DebugDraw::Update();
#endif
		CameraManager::Update(
			mTimeSystem->GetGameTime()->ScaledDeltaTime<float>()
		);

		mOffscreenRenderPassTargets.bClearColor =
			ConVarManager::GetConVar("r_clear")->GetValueAsBool();
		//---------------------------------------------------------------------
		// --- PreRender↓ ---
		mRenderer->PreRender();
		//---------------------------------------------------------------------

		mRenderer->SetViewportAndScissor(
			static_cast<uint32_t>(mOffscreenRtv.rtv->GetDesc().Width),
			mOffscreenRtv.rtv->GetDesc().Height
		);
		mRenderer->BeginRenderPass(mOffscreenRenderPassTargets);
		if (mIsEditorMode) { if (mEditor) { mEditor->Render(); } } else {
			mSceneManager->Render();
		}

#ifdef _DEBUG
		mLineCommon->Render();
		DebugDraw::Draw();
#endif

		// 先にバリアを設定
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource   = mOffscreenRtv.rtv.Get();
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.StateAfter  =
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

		mRenderer->GetCommandList()->ResourceBarrier(1, &barrier);

		auto* radialBlur = dynamic_cast<PPRadialBlur*>(mPostChain[5].get());
		if (radialBlur) { radialBlur->SetBlurStrength(blurStrength); }

		// ポストプロセスを適用するRTV
		ID3D12Resource* postProcessTarget = mOffscreenRtv.rtv.Get();
		for (auto& pass : mPostChain) {
			const uint32_t w = static_cast<uint32_t>(
				postProcessTarget->GetDesc().
				                   Width
			);
			const uint32_t h = postProcessTarget->GetDesc().Height;

			const uint32_t next = mPingIndex ^ 1; // 次のインデックス
			auto&          dest = mPingRtv[next];

			if (mIsEditorMode) {
				mRenderer->SetViewportAndScissor(
					static_cast<uint32_t>(mOffscreenRtv.rtv->GetDesc().Width),
					mOffscreenRtv.rtv->GetDesc().Height
				);
			} else {
				//mRenderer->BeginSwapChainRenderPass();
				mRenderer->SetViewportAndScissor(
					OldWindowManager::GetMainWindow()->GetClientWidth(),
					OldWindowManager::GetMainWindow()->GetClientHeight()
				);
			}

			const bool isLastPass = (&pass == &mPostChain.back());

			D3D12_CPU_DESCRIPTOR_HANDLE outRtvHandle{};

			if (isLastPass && !mIsEditorMode) {
				// ゲーム
				if (!bSwapchainPassBegun) {
					mRenderer->BeginSwapChainRenderPass();
					mRenderer->SetViewportAndScissor(
						OldWindowManager::GetMainWindow()->GetClientWidth(),
						OldWindowManager::GetMainWindow()->GetClientHeight()
					);
					bSwapchainPassBegun = true;
				}

				outRtvHandle = mRenderer->GetSwapChainRenderTargetView();
			} else {
				mRenderer->BeginRenderPass(
					{
						&dest.rtvHandle,
						1,
						&mPostProcessedDsv.dsvHandle,
						kOffscreenClearColor,
						1.0f,
						0,
						true,
						true
					}
				);
				mRenderer->SetViewportAndScissor(
					static_cast<uint32_t>(mOffscreenRtv.rtv->GetDesc().Width),
					mOffscreenRtv.rtv->GetDesc().Height
				);
				outRtvHandle = dest.rtvHandle;
			}

			if (mIsEditorMode) {
				// CopyImagePass実行後にSRVを再作成して最新の内容を反映
				if (mSrvManager) {
					// SRVを再作成
					mSrvManager->CreateSRVForTexture2D(
						mPostProcessedRtv.srvIndex,
						mPostProcessedRtv.rtv.Get(),
						mPostProcessedRtv.rtv->GetDesc().Format,
						1
					);

					// GPUハンドルを再取得
					mPostProcessedRtv.srvHandleGPU = mSrvManager->
						GetGPUDescriptorHandle(
							mPostProcessedRtv.srvIndex
						);
				}

				PostProcessContext context = {};
				context.commandList        = mRenderer->GetCommandList();
				context.inputTexture       = postProcessTarget;
				context.outRtv             = dest.rtvHandle;
				context.width              = w;
				context.height             = h;

				pass->Execute(context);
			} else {
				PostProcessContext context = {};
				context.commandList        = mRenderer->GetCommandList();
				context.inputTexture       = postProcessTarget;
				context.outRtv             = outRtvHandle;
				context.width              = OldWindowManager::GetMainWindow()->
					GetClientWidth();
				context.height = OldWindowManager::GetMainWindow()->
					GetClientHeight();

				pass->Execute(context);
			}

			postProcessTarget = dest.rtv.Get();
			mPingIndex        = next;
		}

		//---------------------------------------------------------------------
		// --- PostRender↓ ---
		if (mIsEditorMode) { mRenderer->BeginSwapChainRenderPass(); }

#ifdef _DEBUG
		mImGuiManager->EndFrame();
#endif

		// バリアを元に戻す
		barrier.Transition.StateBefore =
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		mRenderer->GetCommandList()->ResourceBarrier(1, &barrier);

		if (mIsEditorMode) {
			// mPostProcessedRtv のバリアを戻す
			D3D12_RESOURCE_BARRIER postBarrier = {};
			postBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			postBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			postBarrier.Transition.pResource = mPostProcessedRtv.rtv.Get();
			postBarrier.Transition.StateBefore =
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			postBarrier.Transition.StateAfter =
				D3D12_RESOURCE_STATE_RENDER_TARGET;
			mRenderer->GetCommandList()->ResourceBarrier(1, &postBarrier);
		}

		mRenderer->PostRender();

		mTimeSystem->EndFrame();
	}

	/// @brief シャットダウン
	void Engine::Shutdown() const {
		mRenderer->WaitPreviousFrame();

		DebugDraw::Shutdown();

		mCopyImagePass->Shutdown();

		TexManager::GetInstance()->Shutdown();

		mParticleManager->Shutdown();
		mParticleManager.reset();

		mRenderer->Shutdown();

#ifdef _DEBUG
		if (mImGuiManager) { mImGuiManager->Shutdown(); }
#endif

		mResourceManager->Shutdown();
		mResourceManager.reset();


		mInputSystem->Shutdown();
		mConsoleSystem->Shutdown();
		mTimeSystem->Shutdown();

		SpecialMsg(
			LogLevel::Success,
			"Engine",
			"アリーヴェ帰ルチ! (さよナランチャ"
		);
	}

	/// @brief ウィンドウリサイズ時の処理
	/// @param width 幅
	/// @param height 高さ
	void Engine::OnResize(const uint32_t width, const uint32_t height) {
		if (width == 0 || height == 0) { return; }

		// GPUの処理が終わるまで待つ
		mRenderer->Flush();

		mRenderer->Resize(width, height);

		mPostProcessedRtv.rtv.Reset();
		mPostProcessedDsv.dsv.Reset();

		mOffscreenRtv = mRenderer->CreateRenderTargetTexture(
			width, height,
			kOffscreenClearColor,
			mOffscreenRtv.srvIndex,
			kBufferFormat
		);
		mOffscreenDsv = mRenderer->CreateDepthStencilTexture(
			width, height,
			mOffscreenDsv.srvIndex,
			DXGI_FORMAT_D32_FLOAT
		);

		mPingRtv[0] = mRenderer->CreateRenderTargetTexture(
			width, height,
			kOffscreenClearColor,
			mPingRtv[0].srvIndex,
			kBufferFormat
		);

		mPingRtv[1] = mRenderer->CreateRenderTargetTexture(
			width, height,
			kOffscreenClearColor,
			mPingRtv[1].srvIndex,
			kBufferFormat
		);

		mPostProcessedRtv = mRenderer->CreateRenderTargetTexture(
			width, height,
			kOffscreenClearColor,
			mPostProcessedRtv.srvIndex,
			kBufferFormat
		);

		mPostProcessedDsv = mRenderer->CreateDepthStencilTexture(
			width, height,
			mPostProcessedDsv.srvIndex,
			DXGI_FORMAT_D32_FLOAT
		);

		mOffscreenRenderPassTargets.pRTVs   = &mOffscreenRtv.rtvHandle;
		mOffscreenRenderPassTargets.numRTVs = 1;
		mOffscreenRenderPassTargets.pDSV    = &mOffscreenDsv.dsvHandle;

		mPostProcessedRenderPassTargets.pRTVs   = &mPostProcessedRtv.rtvHandle;
		mPostProcessedRenderPassTargets.numRTVs = 1;
		mPostProcessedRenderPassTargets.pDSV    = &mPostProcessedDsv.dsvHandle;
	}

	/// @brief オフスクリーンレンダーテクスチャのリサイズ
	/// @param width 幅
	/// @param height 高さ
	void Engine::ResizeOffscreenRenderTextures(
		const uint32_t width,
		const uint32_t height
	) {
		if (width == 0 || height == 0) { return; }

		// GPUの処理が終わるまで待つ
		mRenderer->Flush();

		mRenderer->ResetOffscreenRenderTextures();

		mOffscreenRtv.rtv.Reset();
		mOffscreenDsv.dsv.Reset();

		mPostProcessedRtv.rtv.Reset();
		mPostProcessedDsv.dsv.Reset();

		mOffscreenRtv = {};
		mOffscreenDsv = {};

		mPostProcessedRtv = {};
		mPostProcessedDsv = {};

		mOffscreenRtv = mRenderer->CreateRenderTargetTexture(
			width, height,
			kOffscreenClearColor,
			kBufferFormat
		);
		mOffscreenDsv = mRenderer->CreateDepthStencilTexture(
			width, height,
			DXGI_FORMAT_D32_FLOAT
		);

		mPingRtv[0] = mRenderer->CreateRenderTargetTexture(
			width, height,
			kOffscreenClearColor,
			kBufferFormat
		);

		mPingRtv[1] = mRenderer->CreateRenderTargetTexture(
			width, height,
			kOffscreenClearColor,
			kBufferFormat
		);

		mPostProcessedRtv = mRenderer->CreateRenderTargetTexture(
			width, height,
			kOffscreenClearColor,
			kBufferFormat
		);

		mPostProcessedDsv = mRenderer->CreateDepthStencilTexture(
			width, height,
			DXGI_FORMAT_D32_FLOAT
		);

		mOffscreenRenderPassTargets.pRTVs   = &mOffscreenRtv.rtvHandle;
		mOffscreenRenderPassTargets.numRTVs = 1;
		mOffscreenRenderPassTargets.pDSV    = &mOffscreenDsv.dsvHandle;

		mPostProcessedRenderPassTargets.pRTVs   = &mPostProcessedRtv.rtvHandle;
		mPostProcessedRenderPassTargets.numRTVs = 1;
		mPostProcessedRenderPassTargets.pDSV    = &mPostProcessedDsv.dsvHandle;
	}

	/// @brief コンソールコマンドと変数の登録
	void Engine::RegisterConsoleCommandsAndVariables() {
		// コンソールコマンドを登録
		ConCommand::RegisterCommand("exit", Quit, "Exit the engine.");
		ConCommand::RegisterCommand("quit", Quit, "Exit the engine.");

#ifdef _DEBUG
		ConCommand::RegisterCommand(
			"toggleeditor",
			[]([[maybe_unused]] const std::vector<std::string>& args) {
				mIsEditorMode = !mIsEditorMode;
				Warning(
					"Engine",
					"Editor mode is now {}",
					std::to_string(mIsEditorMode)
				);
			},
			"Toggle editor mode."
		);
#endif

		// コンソール変数を登録
		ConVarManager::RegisterConVar(
			"cl_showpos", 2,
			"Draw current position at top of screen (1 = meter, 2 = hammer)"
		);
		ConVarManager::RegisterConVar(
			"cl_showfps", 2,
			"Draw fps meter (1 = fps, 2 = smooth)"
		);
		ConVarManager::RegisterConVar(
			"fps_max", kDefaultFpsMax,
			"Frame rate limiter"
		);
		ConVarManager::RegisterConVar<std::string>(
			"name", "unnamed",
			"Current user name",
			ConVarFlags::ConVarFlags_Notify
		);
		Console::SubmitCommand(
			"name " + WindowsUtils::GetWindowsUserName(), true
		);
		ConVarManager::RegisterConVar(
			"sensitivity", 2.0f,
			"Mouse sensitivity."
		);
		// World
		ConVarManager::RegisterConVar("sv_gravity", 1000.0f, "World gravity.");
		ConVarManager::RegisterConVar(
			"sv_maxvelocity", 3500.0f,
			"Maximum speed any ballistically moving object is allowed to attain per axis."
		);

		// Player
		ConVarManager::RegisterConVar(
			"sv_accelerate", 20.0f,
			"Linear acceleration amount (old value is 5.6)"
		);
		ConVarManager::RegisterConVar("sv_airaccelerate", 12.0f);
		ConVarManager::RegisterConVar(
			"sv_maxspeed", 320.0f,
			"Maximum speed a player can move."
		);
		ConVarManager::RegisterConVar(
			"sv_stopspeed", 100.0f,
			"Minimum stopping speed when on ground."
		);
		ConVarManager::RegisterConVar("sv_friction", 8.0f, "World friction.");
		ConVarManager::RegisterConVar(
			"sv_stepsize", 18.0f,
			"Maximum step height."
		);

		// デバッグ用にエンティティのaxisを表示するかのコンソール変数
		ConVarManager::RegisterConVar("ent_axis", 0, "Show entity axis");
	}

	/// @brief エンジン終了コマンド
	/// @param args 引数
	void Engine::Quit([[maybe_unused]] const std::vector<std::string>& args) {
		mWishShutdown = true;
	}

	/// @brief エディターモードの確認と切り替え
	void Engine::CheckEditorMode() {
		if (mIsEditorMode) {
			mEditor = std::make_unique<Editor>(
				mSceneManager.get(),
				mTimeSystem->GetGameTime()
			);
			mEditor->SetEntityLoader(mEntityLoader.get());
		} else { mEditor.reset(); }
	}

	bool                             Engine::mWishShutdown    = false;
	std::unique_ptr<AudioManager>    Engine::mAudioManager    = nullptr;
	std::unique_ptr<D3D12>           Engine::mRenderer        = nullptr;
	std::unique_ptr<ResourceManager> Engine::mResourceManager = nullptr;
	std::unique_ptr<ParticleManager> Engine::mParticleManager = nullptr;
	std::unique_ptr<SrvManager>      Engine::mSrvManager      = nullptr;
	std::unique_ptr<SpriteCommon>    Engine::mSpriteCommon    = nullptr;
	std::shared_ptr<SceneManager>    Engine::mSceneManager    = nullptr;
	float                            Engine::blurStrength     = 0.0f;

	Vec2 Engine::mViewportLT   = Vec2::zero;
	Vec2 Engine::mViewportSize = Vec2::zero;

	std::optional<std::string> Engine::mPendingSceneChange = std::nullopt;

#ifdef _DEBUG
	bool Engine::mIsEditorMode = true;
#else
	bool Engine::mIsEditorMode = false;
#endif
}
