ENGINE_NAME = "Unnamed"

ARCHITECTURE = "x64"

ROOT_DIR = path.getabsolute(".")
BIN_DIR = path.join(ROOT_DIR, "bin")
INT_DIR = path.join(BIN_DIR, "intermediate")
BUILD_DIR = path.join(ROOT_DIR, "build")
PROJECT_FILES_DIR = path.join(BUILD_DIR, "projects")

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
		linkoptions { "/IGNORE:4099" } -- Assimpが発狂するので無視
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
	ConfigurationSettings()
	UnnamedSettings()
	WindowsPlatformSettings()

	location(path.join(PROJECT_FILES_DIR, projectName))
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

function GenerateProjects() 
	prebuildcommands {
    	'powershell -NoProfile -ExecutionPolicy Bypass -Command "Write-Host \\"[Premake PreBuild] %{prj.name} %{cfg.buildcfg} %{cfg.platform}\\"; Get-Date | Out-File -Encoding utf8 \\"%{wks.location}/prebuild_test.log\\""',
	}
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

project "Lua"
	kind "StaticLib"
	CommonProjectSettings("%{prj.name}")

	files {
		"src/thirdparty/lua/*.h",
		"src/thirdparty/lua/*.c",
	}

	includedirs {
		"src/thirdparty/lua",
	}

group "Engine/Runtime"
project "UnnamedEngineRuntime"
	kind "StaticLib"
	CommonProjectSettings("%{prj.name}")
	WarningSettings()

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
	}

	EngineIncludeDirs()
	libdirs { "src/thirdparty/DirectXTex/" }
	links { "DirectXTex", "Lua" }
	LinkAssimpByConfig()

	filter "configurations:Debug"
		defines { "UNNAMED_WITH_EDITOR" }
	filter {}

project "UnnamedEditorRuntime"
	kind "StaticLib"
	CommonProjectSettings("%{prj.name}")
	WarningSettings()

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
		"src/engine/unnamed/subsystem/editorluasystem/**.h",
		"src/engine/unnamed/subsystem/editorluasystem/**.cpp",
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
		warnings "Extra"
		disablewarnings { "4189" }
	filter {}

	filter "configurations:Debug"
		defines { "UNNAMED_WITH_EDITOR" }
filter {}
	
group "Games/TeamGame"
-- Phase 11: TeamGame runtime sources moved from src/game to projects/TeamGame/runtime.
project "TeamGameRuntime"
	kind "StaticLib"
	CommonProjectSettings("%{prj.name}")

	files {
		"src/pch.h",
		"src/pch.cpp",
		"projects/TeamGame/runtime/**.h",
		"projects/TeamGame/runtime/**.cpp",
	}

	excludes {
		"src/transplantation/**",
	}

	EngineIncludeDirs()
	includedirs { "projects/TeamGame/runtime" }

group "Engine/Applications"
project "UnnamedEditorApp"
	kind "WindowedApp"
	CommonProjectSettings("%{prj.name}")
	WarningSettings()

	files {
		"src/pch.h",
		"src/pch.cpp",
		"src/app/AppLaunchOptions.h",
		"src/app/GameModuleFactory.h",
		"src/app/GameModuleFactory.cpp",
		"src/app/EditorMain.cpp",
	}

	EngineIncludeDirs()
	includedirs {
		"projects/TeamGame/runtime",
	}
	links {
		"UnnamedEngineRuntime",
		"UnnamedEditorRuntime",
		"TeamGameRuntime",
		"DirectXTex",
	}
	filter "configurations:Debug"
		defines { "UNNAMED_WITH_EDITOR" }
	filter {}
	LinkAssimpByConfig()
	GenerateProjects()
	CopyDxCompilerDlls()
	linkoptions {
		"/WHOLEARCHIVE:TeamGameRuntime.lib",
	}

group "Games/TeamGame"
project "TeamGameApp"
	kind "WindowedApp"
	CommonProjectSettings("%{prj.name}")

	files {
		"src/pch.h",
		"src/pch.cpp",
		"src/app/AppLaunchOptions.h",
		"src/app/GameModuleFactory.h",
		"src/app/GameModuleFactory.cpp",
		"src/app/TeamGameMain.cpp",
	}

	EngineIncludeDirs()
	includedirs { "projects/TeamGame/runtime" }
	links {
		"UnnamedEngineRuntime",
		"TeamGameRuntime",
		"DirectXTex",
	}
	filter "configurations:Debug"
		links { "UnnamedEditorRuntime" }
		defines { "UNNAMED_WITH_EDITOR" }
	filter {}
	LinkAssimpByConfig()
	GenerateProjects()
	CopyDxCompilerDlls()
