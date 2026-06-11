param(
    [ValidateSet("Debug", "Develop", "Release")]
    [string]$Configuration = "Release",
    [string]$Platform = "x64",
    [string]$OutputRoot = "",
    [switch]$SkipBuild,
    [switch]$ValidateStartup,
    [switch]$Force
)

$ErrorActionPreference = "Stop"

function Resolve-RepoRoot {
    $toolsRoot = Split-Path -Parent $PSCommandPath
    return (Resolve-Path (Join-Path $toolsRoot "..")).Path
}

function Copy-DirectoryContents {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Source,
        [Parameter(Mandatory = $true)]
        [string]$Destination
    )

    if (-not (Test-Path -LiteralPath $Source -PathType Container)) {
        throw "Required directory not found: $Source"
    }

	New-Item -ItemType Directory -Force -Path $Destination | Out-Null
	Get-ChildItem -LiteralPath $Source -Force | ForEach-Object {
		Copy-Item -LiteralPath $_.FullName -Destination $Destination -Recurse -Force
	}
}

function Copy-RequiredFile {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Source,
        [Parameter(Mandatory = $true)]
        [string]$Destination
    )

    if (-not (Test-Path -LiteralPath $Source -PathType Leaf)) {
        throw "Required file not found: $Source"
    }

    $destinationDir = Split-Path -Parent $Destination
    if (-not [string]::IsNullOrWhiteSpace($destinationDir)) {
        New-Item -ItemType Directory -Force -Path $destinationDir | Out-Null
    }
    Copy-Item -LiteralPath $Source -Destination $Destination -Force
}

$repoRoot = Resolve-RepoRoot
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = Join-Path $repoRoot "dist/TeamGame"
}

$packageRoot = [System.IO.Path]::GetFullPath($OutputRoot)

if ((Test-Path -LiteralPath $packageRoot) -and -not $Force) {
    throw "Output already exists: $packageRoot. Pass -Force to replace it."
}

if (-not $SkipBuild) {
    $projectPath = Join-Path $repoRoot "build/projects/TeamGameApp/TeamGameApp.vcxproj"
    if (-not (Test-Path -LiteralPath $projectPath -PathType Leaf)) {
        & (Join-Path $repoRoot "premake5.exe") vs2026
    }

    & msbuild $projectPath /m:1 "/p:Configuration=$Configuration" "/p:Platform=$Platform" "/p:MultiProcessorCompilation=false"
    if ($LASTEXITCODE -ne 0) {
        throw "TeamGameApp build failed with exit code $LASTEXITCODE."
    }
}

if (Test-Path -LiteralPath $packageRoot) {
    Remove-Item -LiteralPath $packageRoot -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $packageRoot | Out-Null

$outputDir = "$Configuration-windows-x86_64"
$appBuildRoot = Join-Path $repoRoot "bin/$outputDir/TeamGameApp"
$exeName = "TeamGameApp.exe"

Copy-RequiredFile -Source (Join-Path $appBuildRoot $exeName) -Destination (Join-Path $packageRoot $exeName)
Copy-RequiredFile -Source (Join-Path $appBuildRoot "dxcompiler.dll") -Destination (Join-Path $packageRoot "dxcompiler.dll")
Copy-RequiredFile -Source (Join-Path $appBuildRoot "dxil.dll") -Destination (Join-Path $packageRoot "dxil.dll")

Copy-DirectoryContents -Source (Join-Path $repoRoot "content") -Destination (Join-Path $packageRoot "content")

$packageTeamGameRoot = Join-Path $packageRoot "projects/TeamGame"
Copy-DirectoryContents -Source (Join-Path $repoRoot "projects/TeamGame/content") -Destination (Join-Path $packageTeamGameRoot "content")
Copy-RequiredFile -Source (Join-Path $repoRoot "projects/TeamGame/config/game_profile.json") -Destination (Join-Path $packageTeamGameRoot "config/game_profile.json")

if ($ValidateStartup) {
    $packageExe = Join-Path $packageRoot $exeName
    Push-Location $env:TEMP
    try {
        & $packageExe --validate-startup-only
        if ($LASTEXITCODE -ne 0) {
            throw "Packaged TeamGameApp startup validation failed with exit code $LASTEXITCODE."
        }
    } finally {
        Pop-Location
    }
}

Write-Host "Packaged TeamGameApp:"
Write-Host "  $packageRoot"
