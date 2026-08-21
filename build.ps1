# build.ps1 - Build script for Windows Clock Widget
# Uses LLVM MinGW (installed via winget)

$ErrorActionPreference = "Stop"

# -- Locate compiler --
$compilerDir = "$env:LOCALAPPDATA\Microsoft\WinGet\Packages\MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\llvm-mingw-20260616-ucrt-x86_64\bin"

$GPP     = Join-Path $compilerDir "g++.exe"
$WINDRES = Join-Path $compilerDir "windres.exe"

if (-not (Test-Path $GPP)) {
    $fallbacks = @(
        "C:\Program Files\LLVM\bin\clang++.exe"
    )
    foreach ($fb in $fallbacks) {
        if (Test-Path $fb) {
            $GPP = $fb
            $WINDRES = Join-Path (Split-Path $fb) "llvm-windres.exe"
            break
        }
    }
    if (-not (Test-Path $GPP)) {
        Write-Error "Could not find g++ or clang++. Please install LLVM MinGW."
        exit 1
    }
}

Write-Host "========================================"  -ForegroundColor Cyan
Write-Host "  Building Windows Clock Widget..."       -ForegroundColor Cyan
Write-Host "========================================"  -ForegroundColor Cyan
Write-Host ""
Write-Host "Compiler: $GPP" -ForegroundColor DarkGray

$srcDir = $PSScriptRoot
$outExe = Join-Path $srcDir "WindowsClock.exe"
$resObj = Join-Path $srcDir "resource.o"

# -- Step 1: Compile resource file --
Write-Host "[1/2] Compiling resources..." -ForegroundColor Yellow
& $WINDRES (Join-Path $srcDir "resource.rc") -o $resObj
if ($LASTEXITCODE -ne 0) {
    Write-Error "Resource compilation failed!"
    exit 1
}
Write-Host "  [OK] Resources compiled" -ForegroundColor Green

# -- Step 2: Compile and link --
Write-Host "[2/2] Compiling and linking..." -ForegroundColor Yellow

$sourceFiles = @(
    (Join-Path $srcDir "main.cpp"),
    (Join-Path $srcDir "settings.cpp"),
    (Join-Path $srcDir "renderer.cpp"),
    (Join-Path $srcDir "tray_icon.cpp"),
    (Join-Path $srcDir "settings_dialog.cpp"),
    $resObj
)

$compileArgs = @(
    "-std=c++17",
    "-O2",
    "-DUNICODE",
    "-D_UNICODE",
    "-DWINVER=0x0A00",
    "-D_WIN32_WINNT=0x0A00",
    "-mwindows",
    "-o", $outExe
) + $sourceFiles + @(
    "-lgdi32",
    "-lgdiplus",
    "-lcomctl32",
    "-lcomdlg32",
    "-lshell32",
    "-lole32",
    "-ladvapi32",
    "-ldwmapi",
    "-luuid",
    "-luxtheme",
    "-loleaut32"
)

& $GPP @compileArgs
if ($LASTEXITCODE -ne 0) {
    Write-Error "Compilation failed!"
    exit 1
}

# Clean up intermediate files
Remove-Item $resObj -ErrorAction SilentlyContinue

# -- Done --
$size = (Get-Item $outExe).Length / 1KB
Write-Host ""
Write-Host "========================================"  -ForegroundColor Green
Write-Host "  Build successful!"                       -ForegroundColor Green
Write-Host "========================================"  -ForegroundColor Green
Write-Host "  Output: $outExe"                         -ForegroundColor White
Write-Host "  Size:   $([math]::Round($size, 1)) KB"   -ForegroundColor White
