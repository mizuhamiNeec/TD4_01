#include <pch.h>

//-----------------------------------------------------------------------------

#include <engine/unnamed/subsystem/console/ConsoleSystem.h>
#include <engine/unnamed/subsystem/console/concommand/UnnamedConVar.h>
#include <engine/unnamed/subsystem/input/KeyNameTable.h>
#include <engine/unnamed/subsystem/input/UInputSystem.h>
#include <engine/unnamed/subsystem/input/device/keyboard/KeyboardDevice.h>
#include <engine/unnamed/subsystem/input/device/mouse/MouseDevice.h>
#include <engine/unnamed/subsystem/interface/ServiceLocator.h>
#include <engine/unnamed/subsystem/time/TimeSystem.h>
#include <engine/unnamed/subsystem/window/Win32/Win32WindowSystem.h>
#include <engine/unnamed/uengine/UEngine.h>
#include <game/gameframework/component/Camera/UCameraComponent.h>
#include <game/gameframework/component/MeshRenderer/MeshRendererComponent.h>
#include <game/gameframework/component/Rotator/RotatorComponent.h>
#include <game/gameframework/component/Transform/TransformComponent.h>

#include <runtime/assets/core/UAssetManager.h>
#include <runtime/assets/loaders/DirectXTexTextureLoader.h>
#include <runtime/assets/loaders/MaterialLoader.h>
#include <runtime/assets/loaders/MeshLoader.h>
#include <runtime/assets/loaders/RawLoader.h>
#include <runtime/assets/loaders/ShaderLoader.h>
#include <runtime/render/pipeline/UPipelineCache.h>
#include <runtime/render/resources/RenderResourceManager.h>
#include <runtime/render/resources/ShaderLibrary.h>

namespace Unnamed {
	constexpr std::string_view kChannel = "Engine";


	UEngine::UEngine() = default;


	UEngine::~UEngine() = default;


	void UEngine::Run() {
		Init();
		Tick();
		Shutdown();
	}


	void UEngine::Init() {
		// 挨拶大事!
		Msg(kChannel, "Hello!");

		// サブシステムの作成
		mSubsystems.emplace_back(std::make_unique<ConsoleSystem>());
		mSubsystems.emplace_back(std::make_unique<TimeSystem>());
		mSubsystems.emplace_back(std::make_unique<Win32WindowSystem>());
		mSubsystems.emplace_back(std::make_unique<UInputSystem>());

		// サブシステムの登録
		for (const auto& subsystem : mSubsystems) {
			if (subsystem->Init()) {
				auto name = std::string(subsystem->GetName());
				SpecialMsg(
					LogLevel::Success,
					"Engine",
					"Subsystem initialized: {}",
					subsystem->GetName()
				);
			} else {
				UASSERT(false && "Failed to initialize subsystem");
			}
		}

		// サブシステムをメンバに持っておく
		mConsole      = ServiceLocator::Get<ConsoleSystem>();
		mTime         = ServiceLocator::Get<TimeSystem>();
		mWindowSystem = ServiceLocator::Get<Win32WindowSystem>();
		mInputSystem  = ServiceLocator::Get<UInputSystem>();

		// ウィンドウの作成
		const IWindow::WindowCreateInfo info = {
			.title =
			"Main Window  -  " + kEngineBuildDate + " " + kEngineBuildTime,
			.clWidth         = 1280,
			.clHeight        = 720,
			.bIsResizable    = true,
			.bCreateAtCenter = false,
		};
		mWindowSystem->CreateNewWindow(info);

		const auto* mainWindow = mWindowSystem->GetWindows().front().get();

		// 作成したウィンドウに合わせてグラフィックスデバイスを初期化
		mGraphicsDevice                 = std::make_unique<GraphicsDevice>();
		const GraphicsDeviceInfo gdInfo = {
			.hWnd        = mainWindow->GetNativeHandle(),
			.width       = mainWindow->GetInfo().clWidth,
			.height      = mainWindow->GetInfo().clHeight,
			.bClearColor = false,
#ifdef _DEBUG
			.bEnableDebug = true,
#else
			.bEnableDebug = false
#endif
		};
		mGraphicsDevice->Init(gdInfo);

		// プラットフォームイベントの作成
		mPlatformEvents = std::make_unique<PlatformEventsImpl>();

		// 入力システムをリスナーに登録
		Win32WindowSystem::RegisterPlatformEvent(mPlatformEvents.get());
		mPlatformEvents->AddListener(mInputSystem);

#pragma region 入力システムにデバイスを登録
		{
			const auto hWnd = static_cast<HWND>(mainWindow->GetNativeHandle());
			const auto keyboardDevice = std::make_shared<KeyboardDevice>(hWnd);
			const auto mouseDevice = std::make_shared<MouseDevice>(hWnd);
			mInputSystem->RegisterDevice(keyboardDevice);
			mInputSystem->RegisterDevice(mouseDevice);
		}
#pragma endregion

#pragma region 適当にキーボードとマウスを割り当て
		{
			auto w = KeyNameTable::FromString("w");
			if (w.has_value()) {
				mInputSystem->BindAxis2D(
					"move",
					{
						.device = w->device,
						.code   = w->code,
					},
					INPUT_AXIS::Y,
					1.0f
				);
			}
			auto s = KeyNameTable::FromString("s");
			if (s.has_value()) {
				mInputSystem->BindAxis2D(
					"move",
					{
						.device = s->device,
						.code   = s->code,
					},
					INPUT_AXIS::Y,
					-1.0f
				);
			}
			auto d = KeyNameTable::FromString("d");
			if (d.has_value()) {
				if (d.has_value()) {
					mInputSystem->BindAxis2D(
						"move",
						{
							.device = d->device,
							.code   = d->code,
						},
						INPUT_AXIS::X,
						1.0f
					);
				}
			}
			auto a = KeyNameTable::FromString("a");
			if (a.has_value()) {
				mInputSystem->BindAxis2D(
					"move",
					{
						.device = a->device,
						.code   = a->code,
					},
					INPUT_AXIS::X,
					-1.0f
				);
			}

			auto q = KeyNameTable::FromString("q");
			if (q.has_value()) {
				mInputSystem->BindAxis1D(
					"vertical",
					{
						.device = q->device,
						.code   = q->code
					},
					-1.0f
				);
			}
			auto e = KeyNameTable::FromString("e");
			if (e.has_value()) {
				mInputSystem->BindAxis1D(
					"vertical",
					{
						.device = e->device,
						.code   = e->code
					},
					1.0f
				);
			}

			auto space = KeyNameTable::FromString("space");
			if (space.has_value()) {
				mInputSystem->BindAction(
					"hotreload",
					{
						.device = space->device,
						.code   = space->code
					}
				);
			}

			mInputSystem->BindAxis2D(
				"mouse",
				{
					.device = InputDeviceType::MOUSE,
					.code   = VM_X
				},
				INPUT_AXIS::X,
				1.0f
			);
			mInputSystem->BindAxis2D(
				"mouse",
				{
					.device = InputDeviceType::MOUSE,
					.code   = VM_Y
				},
				INPUT_AXIS::Y,
				1.0f
			);

			mInputSystem->BindAxis1D(
				"wheel",
				{
					.device = InputDeviceType::MOUSE,
					.code   = VM_WHEEL
				},
				1.0f
			);
		}
#pragma endregion

		// AMの作成と初期化
		mAssetManager = std::make_unique<UAssetManager>();
		auto matLoader = std::make_unique<MaterialLoader>(mAssetManager.get());
		auto texLoader = std::make_unique<DirectXTexTextureLoader>();
		auto shaderLoader = std::make_unique<ShaderLoader>(mAssetManager.get());
		auto rawFileLoader = std::make_unique<RawLoader>();
		auto meshLoader = std::make_unique<MeshLoader>();
		mAssetManager->RegisterLoader(std::move(matLoader));
		mAssetManager->RegisterLoader(std::move(texLoader));
		mAssetManager->RegisterLoader(std::move(shaderLoader));
		mAssetManager->RegisterLoader(std::move(rawFileLoader));
		mAssetManager->RegisterLoader(std::move(meshLoader));

		mUploadArena = std::make_unique<UploadArena>();

		// 128MB per Frame
		mUploadArena->Init(mGraphicsDevice->Device(), 128ull * 1024 * 1024);

		// RRMの作成と初期化
		mRenderResourceManager = std::make_unique<RenderResourceManager>(
			mGraphicsDevice.get(), mAssetManager.get(), mUploadArena.get()
		);

		// ShaderLibraryの作成と初期化
		mShaderLibrary = std::make_unique<ShaderLibrary>(
			mGraphicsDevice.get(), mAssetManager.get()
		);

		// RootSignatureCacheの作成と初期化
		mRootSignatureCache = std::make_unique<RootSignatureCache>(
			mGraphicsDevice.get()
		);

		// PipelineCacheの作成と初期化
		mPipelineCache = std::make_unique<UPipelineCache>(
			mGraphicsDevice.get(), mRootSignatureCache.get()
		);

		// RenderSubsystemの作成と初期化、サービスロケーターに登録
		mSubsystems.emplace_back(
			std::make_unique<URenderSubsystem>(
				mGraphicsDevice.get(),
				mRenderResourceManager.get(),
				mShaderLibrary.get(),
				mRootSignatureCache.get(),
				mPipelineCache.get(),
				mAssetManager.get()
			)
		);

		mRenderer = ServiceLocator::Get<URenderSubsystem>();
		mRenderer->Init();

		mWorld = std::make_unique<UWorld>("Sandbox");

		{
			mMaterialAsset = mAssetManager->LoadFromFile(
				"./content/core/materials/dev.mat", UASSET_TYPE::MATERIAL
			);
			mAssetManager->AddRef(mMaterialAsset);

			const auto meshAsset = mAssetManager->LoadFromFile(
				"./content/core/models/ShaderBall.obj", UASSET_TYPE::MESH
			);
			mAssetManager->AddRef(meshAsset);

			const auto meshAsset2 = mAssetManager->LoadFromFile(
				"./content/core/models/fan.obj", UASSET_TYPE::MESH
			);
			mAssetManager->AddRef(meshAsset2);

			// グリッドでエンティティをスポーン
			constexpr int   rows    = 16;
			constexpr int   cols    = 16;
			constexpr int   height  = 16;
			constexpr float spacing = 4.0f;
			constexpr float offsetX = (cols - 1) * spacing * 0.5f;
			constexpr float offsetZ = (rows - 1) * spacing * 0.5f;

			for (int i = 0; i < rows; ++i) {
				for (int j = 0; j < cols; ++j) {
					for (int k = 0; k < height; ++k) {
						std::string name = "Entity_" + std::to_string(i) + "_" +
						                   std::to_string(j);
						auto* entity = mWorld->SpawnEmpty(name);
						auto* tr     = entity->GetOrAddComponent<
							TransformComponent>();
						auto* meshRenderer = entity->GetOrAddComponent<
							MeshRendererComponent>();
						auto* rotator = entity->GetOrAddComponent<
							RotatorComponent>();
						rotator->SetRotationRate(Vec3::up * 10.0f);

						meshRenderer->meshAsset     = meshAsset;
						meshRenderer->materialAsset = mMaterialAsset;

						const float x = static_cast<float>(j) * spacing -
						                offsetX;
						const float z = static_cast<float>(i) * spacing -
						                offsetZ;
						const float y = static_cast<float>(k) * spacing -
						                offsetX;
						tr->SetPosition(Vec3(x, y, z));

						// tr->SetScale(
						// 	Random::Vec3Range(Vec3::one * 0.1f, Vec3::one * 8.0f)
						// );						
					}
				}
			}

			auto* ent          = mWorld->SpawnEmpty("fan");
			auto* tr           = ent->GetOrAddComponent<TransformComponent>();
			auto* meshRenderer = ent->GetOrAddComponent<
				MeshRendererComponent>();
			meshRenderer->meshAsset     = meshAsset2;
			meshRenderer->materialAsset = mMaterialAsset;
			tr->SetPosition(Vec3::up * 32.0f);
		}

		{
			auto* eCam       = mWorld->SpawnEmpty("Camera");
			mCameraTransform = eCam->GetOrAddComponent<TransformComponent>();
			mCamera          = eCam->GetOrAddComponent<UCameraComponent>();
			mCameraTransform->SetPosition(Vec3::backward * 4.0f);
		}

		for (const auto id : mAssetManager->AllAssets()) {
			const auto& meta = mAssetManager->Meta(id);
			Msg(
				kChannelNone, "Asset: {}, {}",
				meta.name.c_str(), ToString(meta.type)
			);

			for (const auto dep : mAssetManager->Dependencies(id)) {
				Msg(
					kChannelNone, "Dependency: {} -> {}",
					meta.name.c_str(), mAssetManager->Meta(dep).name.c_str()
				);
			}
		}

		auto camera = std::make_unique<UCameraComponent>();

		// 簡易的にマウスをウィンドウ中央に固定
		{
			RECT client;
			GetClientRect(
				static_cast<HWND>(mainWindow->GetNativeHandle()), &client
			);

			// クライアント座標の中心をスクリーン座標に変換
			POINT center;
			center.x = (client.right - client.left) / 2;
			center.y = (client.bottom - client.top) / 2;
			ClientToScreen(
				static_cast<HWND>(mainWindow->GetNativeHandle()), &center
			);

			RECT clip;
			clip.left   = center.x;
			clip.top    = center.y;
			clip.right  = center.x + 1;
			clip.bottom = center.y + 1;

			ClipCursor(&clip);
		}
	}


	void UEngine::Tick() {
		while (!mWindowSystem->AllClosed()) {
			mTime->BeginFrame();
			const float deltaTime = mTime->GetGameTime()->DeltaTime<float>();

			// サブシステムの更新
			for (const auto& subsystem : mSubsystems) {
				subsystem->Update(deltaTime);
			}

			//-----------------------------------------------------------------
			// 更新処理↓
			//-----------------------------------------------------------------

			Vec2 delta = mInputSystem->Axis2D("mouse");

			// 感度と回転値を計算
			constexpr float sensitivity  = 0.75f;
			constexpr float m_pitch      = 0.022f;
			constexpr float m_yaw        = 0.022f;
			constexpr float cl_pitchdown = 89.0f;
			constexpr float cl_pitchup   = 89.0f;

			static float pitch = 0.0f;
			static float yaw   = 0.0f;
			static float speed = 1.0f;

			speed += mInputSystem->Axis1D("wheel");

			pitch += delta.y * sensitivity * m_pitch;
			yaw   += delta.x * sensitivity * m_yaw;

			// ピッチをクランプ（上下回転の制限）
			pitch = std::clamp(pitch, -cl_pitchup, cl_pitchdown);

			// クォータニオンを生成（回転順序: ヨー → ピッチ）
			Quaternion yawRotation = Quaternion::AxisAngle(
				Vec3::up, yaw * Math::deg2Rad);
			Quaternion pitchRotation = Quaternion::AxisAngle(
				Vec3::right, pitch * Math::deg2Rad);

			// 回転を合成して設定
			Quaternion finalRotation = yawRotation * pitchRotation;
			mCameraTransform->SetRotation(finalRotation);

			auto prevPos = mCameraTransform->Position();
			auto mat     = mCameraTransform->WorldMat();
			auto forward = mat.GetForward();
			auto right   = mat.GetRight();
			auto up      = mat.GetUp();

			prevPos += forward * mInputSystem->Axis2D("move").y * speed *
				deltaTime;
			prevPos += right * mInputSystem->Axis2D("move").x * speed *
				deltaTime;
			prevPos += up * mInputSystem->Axis1D("vertical") * speed *
				deltaTime;

			mCameraTransform->SetPosition(prevPos);

			mWorld->PrePhysicsTick(deltaTime);
			mWorld->Tick(deltaTime);
			mWorld->PostPhysicsTick(deltaTime);

			if (mInputSystem->IsPressed("hotreload")) {
				Msg(kChannel, "HotReload triggered!");
				mAssetManager->Reload(mMaterialAsset);
			}

#ifdef _DEBUG
			ImGui::ShowDemoWindow();
#endif

			//-----------------------------------------------------------------
			const auto* cam        = mWorld->MainCamera();
			auto        mainWindow = mWindowSystem->GetWindows().front().get();
			auto        info       = mainWindow->GetInfo();
			float       width      = static_cast<float>(info.clWidth);
			float       height     = static_cast<float>(info.clHeight);
			float       aspect     = width / height;
			RenderView  view       = {};
			view                   = {
				.view      = UCameraComponent::View(mCameraTransform),
				.proj      = cam->Proj(aspect),
				.viewProj  = Mat4::identity,
				.cameraPos = mCameraTransform->Position()
			};
			view.viewProj = view.view * view.proj;
			mRenderer->SetView(view);
			mRenderer->BeginFrame();
			// 描画処理↓
			//-----------------------------------------------------------------

			// DO SOMETHING HERE!!

			//-----------------------------------------------------------------
			mRenderer->RenderWorld(*mWorld);
			mRenderer->EndFrame();
			mTime->EndFrame();
		}
	}


	void UEngine::Shutdown() const {
		mPlatformEvents->RemoveListener(mInputSystem);

		for (const auto& subsystem : mSubsystems) {
			subsystem->Shutdown();
		}

		mGraphicsDevice->Shutdown();

		Msg(kChannel, "アリーヴェ帰ルチ! (さよナランチャ");
	}
}
