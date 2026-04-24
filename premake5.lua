ENGINE_NAME = "Unnamed"

ARCHITECTURE = "x64"

ROOT_DIR = path.getabsolute(".")
BIN_DIR = path.join(ROOT_DIR, "bin")
INT_DIR = path.join(BIN_DIR, "intermediate")

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

function UnnamedSettings()
	defines {
		"ENGINE_NAME=\"" .. ENGINE_NAME .. "\"",
		"ENGINE_VERSION=\"3.4.0\"",
		"_CRTDBG_MAP_ALLOC",
	}
end

function CommonSettings()
	language "C++"
	cppdialect "C++23"
	architecture(ARCHITECTURE)
	multiprocessorcompile "On"

	debugdir(ROOT_DIR)
	characterset "Unicode"
	filter "system:windows"
		buildoptions { "/utf-8" }
	filter {}
end

function WarningSettings()
	filter "system:windows"
		warnings "Extra"
		buildoptions { "/W4" }
		fatalwarnings { "All" }
		linkoptions { "/IGNORE:4099" }
	filter {}
end

function ConfigurationSettings()
	filter "configurations:Debug"
		defines { "_DEBUG" }
		symbols "On"
		runtime "Debug"

	filter "configurations:Develop"
		defines { "DEVELOP" }
		symbols "On"
		runtime "Release"

	filter "configurations:Release"
		runtime "Release"
		staticruntime "On"
		optimize "Speed"
		defines { "NDEBUG" }
	filter {}
end

function WindowsPlatformSettings()
	filter "system:windows"
		systemversion "latest"
		defines { "NOMINMAX" }
	filter {}
end

function CommonProjectSettings(projectName)
	CommonSettings()
	WarningSettings()
	ConfigurationSettings()
	UnnamedSettings()
	WindowsPlatformSettings()

	targetdir(path.join(BIN_DIR, outputdir, projectName))
	objdir(path.join(INT_DIR, outputdir, projectName))
end

function EngineIncludeDirs()
	includedirs {
		"src/",
		"src/thirdparty/assimp/include",
		"src/thirdparty/DirectXTex",
		"src/thirdparty/ImGui",
		"src/thirdparty/ImGuizmo",
		"src/thirdparty/nlohmann",
	}
end

function LinkAssimpByConfig()
	filter "configurations:Debug"
		links { "src/thirdparty/assimp/lib/Debug/assimp-vc143-mdd.lib" }

	filter "configurations:Develop"
		links { "src/thirdparty/assimp/lib/Release/assimp-vc143-md.lib" }

	filter "configurations:Release"
		links { "src/thirdparty/assimp/lib/Release/assimp-vc143-mt.lib" }
	filter {}
end

function CopyDxCompilerDlls()
	postbuildcommands {
		'copy /Y "$(WindowsSdkDir)bin\\$(TargetPlatformVersion)\\x64\\dxcompiler.dll" "%{cfg.targetdir}\\dxcompiler.dll"',
		'copy /Y "$(WindowsSdkDir)bin\\$(TargetPlatformVersion)\\x64\\dxil.dll" "%{cfg.targetdir}\\dxil.dll"'
	}
end

workspace(ENGINE_NAME)
	configurations { "Debug", "Develop", "Release" }

group "Thirdparty"
project "DirectXTex"
	kind "StaticLib"
	CommonProjectSettings("%{prj.name}")

	files {
		"src/thirdparty/DirectXTex/*.h",
		"src/thirdparty/DirectXTex/*.cpp",
		"src/thirdparty/DirectXTex/**.h",
		"src/thirdparty/DirectXTex/**.cpp",
	}

	includedirs {
		"src/thirdparty/DirectXTex",
		"src/thirdparty/DirectXTex/Shaders/Compiled",
	}

group "Runtime"
project "UnnamedEngineRuntime"
	kind "StaticLib"
	CommonProjectSettings("%{prj.name}")

	files {
		"src/pch.h",
		"src/pch.cpp",
		"src/core/**.h",
		"src/core/**.cpp",
		"src/engine/**.h",
		"src/engine/**.cpp",
		"content/**.hlsl",
		"content/**.hlsli",
	}

	filter { "files:content/**.hlsl" }
		excludefrombuild "On"
	filter { "files:content/**.hlsli" }
		excludefrombuild "On"
	filter {}

	excludes {
		"src/thirdparty/**",
		"src/transplantation/**",
		"src/engine/editor/**",
		"src/engine/gui/editor/**",
		"src/engine/ImGui/**",
		"src/engine/ui/ImGuiLayer.h",
		"src/engine/ui/ImGuiLayer.cpp",
		"src/engine/world/EditorWorld.h",
		"src/engine/world/EditorWorld.cpp",
		"src/engine/world/GameWorld.h",
		"src/engine/world/GameWorld.cpp",
		"src/app/**",
		"src/game/**",
	}

	EngineIncludeDirs()
	libdirs { "src/thirdparty/DirectXTex/" }
	links { "DirectXTex" }
	LinkAssimpByConfig()

	filter "configurations:Debug"
		defines { "UNNAMED_WITH_EDITOR" }
	filter {}

project "UnnamedEditorRuntime"
	kind "StaticLib"
	CommonProjectSettings("%{prj.name}")

	files {
		"src/pch.h",
		"src/pch.cpp",
		"src/engine/editor/**.h",
		"src/engine/editor/**.cpp",
		"src/engine/world/EditorWorld.h",
		"src/engine/world/EditorWorld.cpp",
		"src/engine/ImGui/**.h",
		"src/engine/ImGui/**.cpp",
		"src/engine/ui/ImGuiLayer.h",
		"src/engine/ui/ImGuiLayer.cpp",
		"src/engine/gui/editor/**.h",
		"src/engine/gui/editor/**.cpp",
		"src/thirdparty/ImGui/imgui.cpp",
		"src/thirdparty/ImGui/imgui_draw.cpp",
		"src/thirdparty/ImGui/imgui_widgets.cpp",
		"src/thirdparty/ImGui/imgui_tables.cpp",
		"src/thirdparty/ImGui/imgui_demo.cpp",
		"src/thirdparty/ImGui/imgui_impl_dx12.cpp",
		"src/thirdparty/ImGui/imgui_impl_win32.cpp",
		"src/thirdparty/ImGuizmo/ImGuizmo.cpp",
		"src/thirdparty/ImGuizmo/ImGuizmo.h",
	}

	EngineIncludeDirs()

	filter { "files:src/thirdparty/ImGui/**.cpp or files:src/thirdparty/ImGuizmo/**.cpp" }
		warnings "Off"
		buildoptions { "/WX-" }
	filter {}

	filter "configurations:Debug"
		defines { "UNNAMED_WITH_EDITOR" }
	filter {}

project "UnnamedGameParkour"
	kind "StaticLib"
	CommonProjectSettings("%{prj.name}")

	files {
		"src/pch.h",
		"src/pch.cpp",
		"src/game/**.h",
		"src/game/**.cpp",
	}

	excludes {
		"src/transplantation/**",
	}

	EngineIncludeDirs()

group "Applications"
project "UnnamedEditorApp"
	kind "WindowedApp"
	CommonProjectSettings("%{prj.name}")

	files {
		"src/pch.h",
		"src/pch.cpp",
		"src/app/EditorMain.cpp",
	}

	EngineIncludeDirs()
	links {
		"UnnamedEngineRuntime",
		"UnnamedEditorRuntime",
		"UnnamedGameParkour",
		"DirectXTex",
	}
	LinkAssimpByConfig()
	CopyDxCompilerDlls()
	linkoptions { "/WHOLEARCHIVE:UnnamedGameParkour.lib" }

project "UnnamedGameApp"
	kind "WindowedApp"
	CommonProjectSettings("%{prj.name}")

	files {
		"src/pch.h",
		"src/pch.cpp",
		"src/app/GameMain.cpp",
	}

	EngineIncludeDirs()
	links {
		"UnnamedEngineRuntime",
		"UnnamedGameParkour",
		"DirectXTex",
	}
	filter "configurations:Debug"
		links { "UnnamedEditorRuntime" }
	filter {}
	LinkAssimpByConfig()
	CopyDxCompilerDlls()
	linkoptions { "/WHOLEARCHIVE:UnnamedGameParkour.lib" }
