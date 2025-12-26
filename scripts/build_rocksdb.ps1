# RocksDB 사전 빌드 스크립트
# Usage: .\scripts\build_rocksdb.ps1 [-BuildType Release|Debug] [-LinkType Static|Shared]

param(
    [ValidateSet("Release", "Debug", "RelWithDebInfo")]
    [string]$BuildType = "Release",
    
    [ValidateSet("Static", "Shared")]
    [string]$LinkType = "Static"
)

$ErrorActionPreference = "Stop"

# 프로젝트 루트 디렉토리
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$RocksDBDir = Join-Path $ProjectRoot "third_party\rocksdb"
$BuildDir = Join-Path $ProjectRoot "build_rocksdb"
$InstallDir = Join-Path $ProjectRoot "third_party\rocksdb_build"

Write-Host "=== RocksDB 사전 빌드 ===" -ForegroundColor Cyan
Write-Host "빌드 타입: $BuildType" -ForegroundColor Yellow
Write-Host "링크 타입: $LinkType" -ForegroundColor Yellow
Write-Host "소스 디렉토리: $RocksDBDir" -ForegroundColor Yellow
Write-Host "빌드 디렉토리: $BuildDir" -ForegroundColor Yellow
Write-Host "설치 디렉토리: $InstallDir" -ForegroundColor Yellow

# RocksDB 소스 확인
if (-not (Test-Path (Join-Path $RocksDBDir "CMakeLists.txt"))) {
    Write-Error "RocksDB 소스가 없습니다. 서브모듈을 초기화하세요: git submodule update --init --recursive"
    exit 1
}

# 빌드 디렉토리 생성
if (Test-Path $BuildDir) {
    Write-Host "기존 빌드 디렉토리 삭제 중..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $BuildDir
}
New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null

# CMake 구성
Write-Host "`n=== CMake 구성 ===" -ForegroundColor Cyan
Push-Location $BuildDir

$CMakeArgs = @(
    "..\third_party\rocksdb",
    "-G", "Visual Studio 18 2026",
    "-A", "x64",
    "-DCMAKE_BUILD_TYPE=$BuildType",
    "-DCMAKE_INSTALL_PREFIX=$InstallDir",
    "-DWITH_TESTS=OFF",
    "-DWITH_BENCHMARK_TOOLS=OFF",
    "-DWITH_TOOLS=OFF",
    "-DWITH_CORE_TOOLS=OFF"
)

if ($LinkType -eq "Shared") {
    $CMakeArgs += "-DROCKSDB_BUILD_SHARED=ON"
} else {
    $CMakeArgs += "-DROCKSDB_BUILD_SHARED=OFF"
}

& cmake @CMakeArgs
if ($LASTEXITCODE -ne 0) {
    Write-Error "CMake 구성 실패"
    Pop-Location
    exit 1
}

# 빌드
Write-Host "`n=== 빌드 중 ===" -ForegroundColor Cyan
$ConfigName = if ($BuildType -eq "RelWithDebInfo") { "RelWithDebInfo" } else { $BuildType }
& cmake --build . --config $ConfigName --parallel
if ($LASTEXITCODE -ne 0) {
    Write-Error "빌드 실패"
    Pop-Location
    exit 1
}

# 설치
Write-Host "`n=== 설치 중 ===" -ForegroundColor Cyan
& cmake --install . --config $ConfigName
if ($LASTEXITCODE -ne 0) {
    Write-Error "설치 실패"
    Pop-Location
    exit 1
}

Pop-Location

Write-Host "`n=== 빌드 완료 ===" -ForegroundColor Green
Write-Host "RocksDB가 다음 위치에 설치되었습니다:" -ForegroundColor Green
Write-Host "  $InstallDir" -ForegroundColor Yellow

# 빌드된 파일 확인
$LibDir = Join-Path $InstallDir "lib"
$IncludeDir = Join-Path $InstallDir "include"
Write-Host "`n빌드된 파일:" -ForegroundColor Cyan
if (Test-Path $LibDir) {
    Get-ChildItem $LibDir | ForEach-Object { Write-Host "  $($_.Name)" -ForegroundColor Gray }
}
Write-Host "헤더 파일:" -ForegroundColor Cyan
if (Test-Path $IncludeDir) {
    Write-Host "  $IncludeDir" -ForegroundColor Gray
}

