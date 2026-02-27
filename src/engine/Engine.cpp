#include "Engine.h"

#include <pch.h>

// ReSharper disable CppUnusedIncludeDirective
#include <engine/ImGui/ImGuiManager.h>
#include <engine/Line/LineCommon.h>
#include <engine/Model/ModelCommon.h>
#include <engine/Object3D/Object3DCommon.h>
#include <engine/particle/ParticleManager.h>
#include <engine/postprocess/IPostProcess.h>
#include <engine/ResourceSystem/Audio/AudioManager.h>
#include <engine/Sprite/SpriteCommon.h>
// ReSharper restore CppUnusedIncludeDirective

#include "core/assets/AssetManager.h"
#include "core/assets/loader/MaterialAssetLoader.h"
#include "core/assets/loader/MaterialInstanceAssetLoader.h"
#include "core/assets/loader/MeshAssetLoader.h"
#include "core/assets/loader/PostFxChainLoader.h"
#include "core/assets/loader/ShaderProgramLoader.h"
#include "core/assets/loader/ShaderSourceLoader.h"
#include "core/assets/loader/TextureLoaderDirectXTex.h"

#include "editor/UEditorRuntime.h"

#include "engine/unnamed/framework/entity/UEntity.h"

#include "Platform/PlatformEventsImpl.h"
#include "Platform/WindowManager.h"

#include "postprocess/PostProcessPipeline.h"

#include "render/RenderModule.h"
#include "render/frame/RenderFrameContext.h"
#include "render/frame/RenderFrameInputs.h"
#include "render/rendergraph/RenderPassContext.h"

#include "renderer/RenderTargets.h"

#include "ResourceSystem/Manager/ResourceManager.h"

#include "rhi/RhiTypes.h"
#include "rhi/d3d12/D3D12Device.h"
#include "rhi/d3d12/D3D12Util.h"
#include "rhi/interface/IRhiDevice.h"

#include "ui/UImGuiLayer.h"

#include "unnamed/subsystem/console/concommand/UnnamedConCommand.h"
#include "unnamed/subsystem/input/device/keyboard/KeyboardDevice.h"
#include "unnamed/subsystem/input/device/mouse/MouseDevice.h"
#include "unnamed/subsystem/terminal/TerminalSystem.h"
#include "unnamed/subsystem/time/SystemClock.h"
#include "unnamed/subsystem/time/TimeSystem.h"

#include "world/UEditorWorld.h"
#include "world/UGameWorld.h"
#include "world/UWorld.h"

#ifdef _DEBUG
#include <imgui_internal.h>
// ImGuizmoのインクルードはImGuiより後! いいね!?
#endif

namespace Unnamed {
	namespace Rhi {
		class D3D12CommandContext;
		class D3D12Device;
	}

	/// @brief コンストラクタ
	Engine::Engine() = default;

	/// @brief デストラクタ
	Engine::~Engine() {}

	int Engine::Run() {
		_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF); // リークチェック
		[[maybe_unused]] HRESULT hr = CoInitializeEx(
			nullptr, COINIT_MULTITHREADED
		);
		timeBeginPeriod(1); // システムタイマーの分解能を上げる

		// 初期化
		if (!Init()) { UASSERT(false && "Failed to initialize Engine"); }

		// メインループ
		while (true) {
			mWindowManager->ProcessMessage();

			// ウィンドウのリサイズ処理
			for (const WindowId id : mWindowManager->GetAllWindowIds()) {
				Window* wnd = mWindowManager->FindWindowById(id);
				if (!wnd) { continue; }
				if (const auto resize = wnd->ConsumeResizeEvent()) {
					if (
						resize->width > 0 && resize->height > 0 &&
						(static_cast<uint32_t>(resize->width) !=
						 mLastResizeWidth ||
						 static_cast<uint32_t>(resize->height) !=
						 mLastResizeHeight)
					) {
						mLastResizeWidth = static_cast<uint32_t>(resize->width);
						mLastResizeHeight = static_cast<uint32_t>(resize->
							height);
						if (mRenderModule) {
							mRenderModule->OnResize(
								static_cast<uint32_t>(resize->width),
								static_cast<uint32_t>(resize->height)
							);
						}
					}
				}
			}

			if (mWindowManager->ShouldQuit() || mWishShutdown) { break; }

			Tick();
		}

		// シャットダウン
		Shutdown();
		timeEndPeriod(1);
		CoUninitialize();
		return EXIT_SUCCESS;
	}

	AudioManager* Engine::GetAudioManagerInstance() const {
		return mAudioManager.get();
	}

	D3D12* Engine::GetRendererInstance() const { return mRenderer.get(); }

	ResourceManager* Engine::GetResourceManagerInstance() const {
		return mResourceManager.get();
	}

	SpriteCommon* Engine::GetSpriteCommonInstance() const {
		return mSpriteCommon.get();
	}

	ParticleManager* Engine::GetParticleManagerInstance() const {
		return mParticleManager.get();
	}

	SrvManager* Engine::GetSrvManagerInstance() const {
		return mResourceManager->GetSrvManager();
	}

	TexManager* Engine::GetTexManagerInstance() const {
		return mResourceManager ?
			       mResourceManager->GetTexManager() :
			       nullptr;
	}

	Vec2 Engine::GetViewportLTInstance() const { return mViewportLT; }
	Vec2 Engine::GetViewportSizeInstance() const { return mViewportSize; }

	float& Engine::GetBlurStrengthInstance() { return mBlurStrength; }

	/// @brief ウィンドウリサイズ時の処理
	/// @param width 幅
	/// @param height 高さ
	void Engine::OnResize(const uint32_t width, const uint32_t height) {
		if (width == 0 || height == 0) { return; }

		if (mRenderTargets) { mRenderTargets->OnResize(width, height); }
		if (mPostProcessPipeline) {
			mPostProcessPipeline->OnResize(width, height);
		}
	}

	/// @brief オフスクリーンレンダーテクスチャのリサイズ
	/// @param width 幅
	/// @param height 高さ
	void Engine::ResizeOffscreenRenderTextures(
		const uint32_t width,
		const uint32_t height
	) {
		if (width == 0 || height == 0) { return; }

		if (mRenderTargets) { mRenderTargets->OnResize(width, height); }
		if (mPostProcessPipeline) {
			mPostProcessPipeline->OnResize(width, height);
		}
	}

	/// @brief コンソールコマンドと変数の登録
	void Engine::RegisterConsoleCommandsAndVariables() {
		// コンソールコマンドを登録
		static UnnamedConCommand quit(
			"quit",
			[this](const std::vector<std::string>&) {
				mWishShutdown = true;
				return true;
			},
			"Quit the engine."
		);

#ifdef _DEBUG
		static UnnamedConCommand toggleeditor(
			"toggleeditor",
			[this](const std::vector<std::string>&) {
				mIsEditorMode = !mIsEditorMode;
				Warning(
					"Engine",
					"Editor mode is now {}",
					std::to_string(mIsEditorMode)
				);
				return true;
			},
			"Toggle editor mode."
		);
#endif

		// コンソール変数を登録
		mConsoleSystem->ExecuteCommand(
			"name " + WindowsUtils::GetWindowsUserName(), EXEC_FLAG::SILENT
		);
	}

	std::size_t Engine::GetPostChainSize() const {
		return mPostProcessPipeline->GetPassCount();
	}

	IPostProcess* Engine::GetPostProcessAt(const int index) const {
		if (index < 0) { return nullptr; }
		return mPostProcessPipeline->GetPassAt(static_cast<size_t>(index));
	}

	uint64_t Engine::GetActivePingSrvGpuPtr() const {
		return mPostProcessPipeline->GetActivePingSrvGpuPtr();
	}

	D3D12_RESOURCE_DESC Engine::GetActivePingRtvDesc() const {
		return mPostProcessPipeline->GetActivePingRtvDesc();
	}

	/// @brief 初期化
	/// @return 成功したらtrueを返す
	bool Engine::Init() {
		SystemClock::Init();

		ServiceLocator::Register<Engine>(this);

		mConfig = {
#ifdef _DEBUG
			.mode = RUN_MODE::EDITOR,
#else
			.mode = RUN_MODE::STANDALONE,
#endif
			.window = {
				.title     = "Unnamed Engine",
				.width     = 1280,
				.height    = 720,
				.mode      = WINDOW_MODE::WINDOWED,
				.resizable = true
			},
		};

		// WindowManagerの初期化メインウィンドウ作成
		mWindowManager = std::make_unique<WindowManager>();
		if (!mWindowManager->Init(mConfig.window)) { return false; }

		// メインウィンドウのID取得
		const auto id = mWindowManager->GetMainWindowId();
		// メインウィンドウのポインタ取得
		const auto window = mWindowManager->FindWindowById(id);
		// HWND取得
		auto hwnd = window->GetHwnd();

		// ConsoleSystemの初期化
		mConsoleSystem = std::make_unique<ConsoleSystem>();
		if (!mConsoleSystem->Init()) { return false; }

		// TerminalSystem の初期化（ConsoleSystem 経由で操作するため Console を渡す）
		mTerminalSystem = std::make_unique<
			TerminalSystem>(mConsoleSystem.get());
		if (!mTerminalSystem->Init()) { return false; }

		mAssetManager = std::make_unique<AssetManager>();

		// 各ローダーの登録
		mAssetManager->RegisterLoader(
			std::move(std::make_unique<TextureLoaderDirectXTex>())
		);
		mAssetManager->RegisterLoader(
			std::move(std::make_unique<MeshAssetLoader>())
		);
		mAssetManager->RegisterLoader(
			std::move(std::make_unique<ShaderProgramLoader>(*mAssetManager))
		);
		mAssetManager->RegisterLoader(
			std::move(std::make_unique<MaterialAssetLoader>(*mAssetManager))
		);
		mAssetManager->RegisterLoader(
			std::move(
				std::make_unique<MaterialInstanceAssetLoader>(*mAssetManager)
			)
		);
		mAssetManager->RegisterLoader(
			std::move(std::make_unique<PostFxChainLoader>(*mAssetManager))
		);
		mAssetManager->RegisterLoader(
			std::move(std::make_unique<ShaderSourceLoader>(*mAssetManager))
		);

		// TimeSystemの初期化
		mTimeSystem = std::make_unique<TimeSystem>();
		if (!mTimeSystem->Init()) { return false; }

		// InputSystemの初期化
		mInputSystem = std::make_unique<UInputSystem>();
		if (!mInputSystem->Init()) { return false; }
		// デバイス登録
		const auto keyboardDevice = std::make_shared<KeyboardDevice>(hwnd);
		const auto mouseDevice    = std::make_shared<MouseDevice>(hwnd);
		mInputSystem->RegisterDevice(keyboardDevice);
		mInputSystem->RegisterDevice(mouseDevice);

		// プラットフォームイベントの作成
		mPlatformEvents = std::make_unique<PlatformEventsImpl>();
		// ウィンドウマネージャに登録
		mWindowManager->RegisterPlatformEvents(mPlatformEvents.get());
		// 入力システムをリスナーに登録
		mPlatformEvents->AddListener(mInputSystem.get());

		constexpr Rhi::DeviceDesc deviceDesc = {
			.enableDebugLayer         = true,
			.enableGpuBasedValidation = true
		};

		const Rhi::SwapChainDesc swapChainDesc = {
			.width       = static_cast<uint32_t>(window->GetDesc().width),
			.height      = static_cast<uint32_t>(window->GetDesc().height),
			.bufferCount = 2, // ダブルバッファリング
			.format      = Rhi::TEXTURE_FORMAT::R8G8B8A8_UNORM,
			.vSync       = false
		};

		mRhiDevice = Rhi::CreateD3D12Device(
			hwnd,
			deviceDesc,
			swapChainDesc
		);

		mRenderModule = std::make_unique<Render::RenderModule>(
			*mAssetManager, *mRhiDevice
		);
		mRenderModule->Init();
		mRenderFrameContext = std::make_unique<Render::RenderFrameContext>();

#ifdef _DEBUG
		auto& dx     = dynamic_cast<Rhi::D3D12Device&>(*mRhiDevice);
		mUImGuiLayer = std::make_unique<UImGuiLayer>(
			hwnd,
			dx,
			dx.GetSwapChain().GetBufferCount(),
			Rhi::ToDxgiFormat(dx.GetSwapChain().GetFormat())
		);

		mRenderModule->SetUiCallbacks(
			[this](Render::RenderPassContext& passContext) {
				if (mUImGuiLayer) {
					mUImGuiLayer->RenderMainDrawData(passContext);
				}
			},
			[this] {
				if (mUImGuiLayer) { mUImGuiLayer->RenderPlatformWindows(); }
			}
		);
#endif

		RegisterConsoleCommandsAndVariables();

		if (mConfig.mode == RUN_MODE::EDITOR) {
			auto& editorWorld = SwitchWorld<UEditorWorld>();
			editorWorld.LoadSceneFromFile("./content/core/scenes/sandbox.json");
#ifdef _DEBUG
			mUEditorRuntime = std::make_unique<UEditorRuntime>(
				editorWorld,
				*mWindowManager,
				*mRenderModule,
				*mUImGuiLayer
			);
#endif
		} else {
			auto& gameWorld = SwitchWorld<UGameWorld>();
			gameWorld.LoadSceneFromFile("./content/core/scenes/sandbox.json");
		}

		// 		mRenderer = std::make_unique<D3D12>(hwnd, window->GetDesc());
		//
		// 		InputSystem::Init();
		//
		// 		RegisterConsoleCommandsAndVariables();
		//
		// 		// 各マネージャーの初期化
		// 		mResourceManager = std::make_unique<ResourceManager>(mRenderer.get());
		//
		// 		mResourceManager->Init();
		// 		mRenderer->SetShaderResourceViewManager(
		// 			mResourceManager->GetSrvManager()
		// 		);
		// 		mRenderer->Init();
		//
		// 		mAudioManager = std::make_unique<AudioManager>();
		// 		mAudioManager->Init();
		//
		// #ifdef _DEBUG
		// 		// ImGuiFontテクスチャ用にSRVを確保
		// 		mResourceManager->GetSrvManager()->AllocateForTexture2D();
		// 		// ImGuiManagerの初期化
		// 		mImGuiManager = std::make_unique<ImGuiManager>(
		// 			hwnd, mRenderer.get(), mResourceManager->GetSrvManager()
		// 		);
		// #endif
		//
		// 		// コンソールを作成
		// 		mConsole = std::make_unique<Console>();
		//
		// #pragma region PostProcessInit
		// 		mRenderTargets.Init(
		// 			mRenderer.get(),
		// 			window->GetDesc().width,
		// 			window->GetDesc().height,
		// 			kOffscreenClearColor,
		// 			kBufferFormat,
		// 			DXGI_FORMAT_D32_FLOAT
		// 		);
		//
		// 		mPostProcessPipeline.Init(
		// 			mRenderer.get(),
		// 			mResourceManager->GetSrvManager(),
		// 			window->GetDesc().width,
		// 			window->GetDesc().height,
		// 			reinterpret_cast<const float*>(&kOffscreenClearColor),
		// 			kBufferFormat,
		// 			DXGI_FORMAT_D32_FLOAT
		// 		);
		//
		// 		mPostProcessPipeline.AddPass(
		// 			std::make_unique<PPBloom>(
		// 				mRenderer->GetDevice(), mResourceManager->GetSrvManager()
		// 			)
		// 		);
		//
		// 		mPostProcessPipeline.AddPass(
		// 			std::make_unique<PPVignette>(
		// 				mRenderer->GetDevice(), mResourceManager->GetSrvManager()
		// 			)
		// 		);
		//
		// 		mPostProcessPipeline.AddPass(
		// 			std::make_unique<PPChromaticAberration>(
		// 				mRenderer->GetDevice(), mResourceManager->GetSrvManager()
		// 			)
		// 		);
		//
		// 		mPostProcessPipeline.AddPass(
		// 			std::make_unique<PPRadialBlur>(
		// 				mRenderer->GetDevice(), mResourceManager->GetSrvManager()
		// 			)
		// 		);
		// #pragma endregion
		//
		// 		// スプライト
		// 		mSpriteCommon = std::make_unique<SpriteCommon>();
		// 		mSpriteCommon->Init(mRenderer.get());
		//
		// 		// パーティクル
		// 		mParticleManager = std::make_unique<ParticleManager>();
		// 		mParticleManager->Init(
		// 			mRenderer.get(), mResourceManager->GetSrvManager()
		// 		);
		//
		// 		// ライン
		// 		mLineCommon = std::make_unique<LineCommon>();
		// 		mLineCommon->Init(mRenderer.get());
		//
		// 		DebugDraw::Init(mLineCommon.get());
		//
		// 		//---------------------------------------------------------------------
		// 		// すべての初期化が完了
		// 		//---------------------------------------------------------------------
		//
		// 		Console::SubmitCommand("neofetch");
		//
		// 		//---------------------------------------------------------------------
		// 		// エディターの初期化
		// 		//---------------------------------------------------------------------
		//
		// 		assert(SUCCEEDED(mRenderer->GetCommandList()->Close()));
		//
		// 		mConsoleSystem->ExecuteCommand(
		// 			"exec ./content/core/cfg/config_default.cfg"
		// 		);
		//
		// 		// ワールドの作成とシーンの読み込み
		// 		SwitchWorld<UWorld>();
		// 		mWorld->LoadSceneFromFile("./content/core/scenes/sandbox.json");

		return true;
	}

	/// @brief 更新
	void Engine::Tick() {
		mTimeSystem->BeginFrame();
		const float deltaTime = mTimeSystem->GetGameTime()->DeltaTime<float>();

		// 入力システムの更新
		mInputSystem->Update(deltaTime);
		// マウスカーソルのロックと表示状態の確認
		mInputSystem->CheckMouseCursorLockAndVisibility(
			mWindowManager->FindWindowById(
				mWindowManager->GetMainWindowId()
			)->GetHwnd()
		);

#ifdef _DEBUG
		// Update内でImGuiを使えるように更新前にフレーム開始
		if (mUImGuiLayer) { mUImGuiLayer->BeginFrame(); }
#endif

		/* ----------- 更新処理 ---------- */

		mConsoleSystem->Update(deltaTime);
		mTerminalSystem->Update(deltaTime);

		// ワールドの更新
		if (mWorld) { mWorld->Tick(deltaTime); }

		Render::RenderFrameInputs inputs = {};
		// フレームインデックスとゲーム時間を設定
		inputs.frameIndex = mFrameIndex++;
		inputs.time       = static_cast<float>(mTimeSystem->GetGameTime()->
			TotalTime());
		if (mWorld && mRenderFrameContext) {
			// ワールドからレンダーフレーム入力を取得
			mWorld->FillRenderFrameInputs(
				inputs, *mRenderFrameContext, *mAssetManager
			);
		}

#ifdef _DEBUG
		if (mUImGuiLayer) {
			if (mUEditorRuntime && mIsEditorMode) {
				mUEditorRuntime->SetSceneOutput(
					mRenderModule->GetSceneOutputTextureId(),
					mRenderModule->GetSceneOutputSrvCpu(),
					mRenderModule->GetSceneOutputSize()
				);
				mUEditorRuntime->BuildUi();
			}
			mUImGuiLayer->EndFrame();
		}
#endif

		mRenderModule->Tick(inputs);

		mTimeSystem->EndFrame();
	}

	/// @brief シャットダウン
	void Engine::Shutdown() {
		//mRenderer->WaitPreviousFrame();

		if (mWorld) {
			mWorld->Shutdown();
			mWorld.reset();
		}

#ifdef _DEBUG
		if (mRenderModule) { mRenderModule->SetUiCallbacks({}, {}); }
		mUEditorRuntime.reset();
		mUImGuiLayer.reset();
#endif

		mRenderFrameContext.reset();
		mRenderModule.reset();
		mRhiDevice.reset();

		// 入力システムのリスナー解除
		if (mPlatformEvents && mInputSystem) {
			mPlatformEvents->RemoveListener(mInputSystem.get());
		}

		// 		DebugDraw::Shutdown();
		//
		// 		mParticleManager->Shutdown();
		// 		mParticleManager.reset();
		//
		// 		mRenderer->Shutdown();
		//
		// #ifdef _DEBUG
		// 		if (mImGuiManager) { mImGuiManager->Shutdown(); }
		// #endif
		//
		// 		mResourceManager->Shutdown();
		// 		mResourceManager.reset();

		if (mInputSystem) { mInputSystem->Shutdown(); }
		if (mTerminalSystem) { mTerminalSystem->Shutdown(); }

		SpecialMsg(
			LogLevel::Success,
			"Engine",
			"アリーヴェ帰ルチ! (さよナランチャ"
		);

		if (mConsoleSystem) { mConsoleSystem->Shutdown(); }
		if (mTimeSystem) { mTimeSystem->Shutdown(); }
		if (mWindowManager) { mWindowManager->Shutdown(); }
	}

	template <class TWorld, class... Args>
	TWorld& Engine::SwitchWorld(Args&&... args) {
		static_assert(std::is_base_of_v<UWorld, TWorld>);

		if (mWorld) {
			mWorld->Shutdown();
			mWorld.reset();
		}

		auto newWorld = std::make_unique<TWorld>(
			std::forward<Args>(args)...
		);
		TWorld* raw = newWorld.get();

		mWorld = std::move(newWorld);

		mWorld->Initialize();
		return *raw;
	}

	UWorld* Engine::GetWorld() const { return mWorld.get(); }
}
