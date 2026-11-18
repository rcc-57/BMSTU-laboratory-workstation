# Run on Windows. Build only: does not open COM ports or launch the GUI.
$ErrorActionPreference = 'Stop'
if (!(Get-Command cmake -ErrorAction SilentlyContinue) -or !(Get-Command ctest -ErrorAction SilentlyContinue)) {
    throw 'Install CMake 3.21 or newer (including CTest), add its bin folder to PATH and reopen the terminal.'
}
$repoRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $repoRoot 'build/measurement-windows'
if (-not (Test-Path (Join-Path $repoRoot 'imgui/imgui.cpp'))) {
    if (!(Test-Path (Join-Path $repoRoot '.git')) -or !(Get-Command git -ErrorAction SilentlyContinue)) {
        throw 'Dear ImGui is missing. Copy the complete imgui source folder or use a Git checkout with submodules.'
    }
    & git -C $repoRoot submodule update --init --recursive -- imgui
    if ($LASTEXITCODE -ne 0) { throw 'Could not fetch the imgui submodule. See README_how_to_start.md.' }
}
$glfwSource = Join-Path $repoRoot 'deps/glfw'
$glfwOption = '-DFETCHCONTENT_SOURCE_DIR_GLFW='
if (Test-Path (Join-Path $glfwSource 'CMakeLists.txt')) {
    $glfwOption += $glfwSource
}
& cmake -S $PSScriptRoot -B $buildDir -G 'Visual Studio 17 2022' -A x64 -DBUILD_TESTING=ON -DCMAKE_DISABLE_FIND_PACKAGE_glfw3=ON $glfwOption
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
& cmake --build $buildDir --config Release --parallel
if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
& ctest --test-dir $buildDir -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Protocol tests failed.' }
Write-Host ('Built: ' + (Join-Path $buildDir 'Release/measurement_control.exe'))
