Write-Host "Generating projects..."

try {
    premake5.exe vs2026

    if ($LASTEXITCODE -eq 0) {
        Write-Host "Premake5 execution completed."
    } else {
        Write-Host "An error occurred during Premake5 execution."
        Read-Host "Press Enter to continue..."
        exit 1
    }
} catch {
    Write-Host "Failed to execute premake5.exe. Please download the latest version of Premake and add it to your system PATH."
    Read-Host "Press Enter to continue..."
    exit 1
}
