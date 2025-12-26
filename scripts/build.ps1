# DuraStash 빌드 스크립트
# Usage: .\scripts\build.ps1 [-BuildType Release|Debug] [-UsePrebuiltRocksDB] [-RocksDBRoot <path>]

param(
    [ValidateSet("Release", "Debug", "RelWithDebInfo")]
    [string]$BuildType = "Release",
    
    [switch]$UsePrebuiltRocksDB,
    
    [string]$RocksDBRoot = "third_party\rocksdb_build"
)

$ErrorActionPreference = "Stop"

# 프로젝트 루트 디렉토리
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $ProjectRoot "build"

Write-Host "=== DuraStash 빌드 ===" -ForegroundColor Cyan
Write-Host "빌드 타입: $BuildType" -ForegroundColor Yellow

# 사전 빌드된 RocksDB 확인
if ($UsePrebuiltRocksDB) {
    $RocksDBPath = if ([System.IO.Path]::IsPathRooted($RocksDBRoot)) { 
        $RocksDBRoot 
    } else { 
        Join-Path $ProjectRoot $RocksDBRoot 
    }
    
    if (-not (Test-Path (Join-Path $RocksDBPath "lib"))) {
        Write-Warning "사전 빌드된 RocksDB를 찾을 수 없습니다: $RocksDBPath"
        Write-Host "먼저 RocksDB를 빌드하세요: .\scripts\build_rocksdb.ps1" -ForegroundColor Yellow
        exit 1
    }
    Write-Host "사전 빌드된 RocksDB 사용: $RocksDBPath" -ForegroundColor Green
}

# 빌드 디렉토리 생성
if (-not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null
}

# CMake 구성
Write-Host "`n=== CMake 구성 ===" -ForegroundColor Cyan
Push-Location $BuildDir

$CMakeArgs = @(
    "..",
    "-G", "Visual Studio 18 2026",
    "-A", "x64",
    "-DCMAKE_BUILD_TYPE=$BuildType"
)

if ($UsePrebuiltRocksDB) {
    $CMakeArgs += "-DUSE_PREBUILT_ROCKSDB=ON"
    $CMakeArgs += "-DROCKSDB_ROOT=$RocksDBPath"
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

Pop-Location

Write-Host "`n=== 빌드 완료 ===" -ForegroundColor Green
Write-Host "빌드 결과:" -ForegroundColor Cyan
$BinDir = Join-Path $BuildDir "bin\$ConfigName"
if (Test-Path $BinDir) {
    Get-ChildItem $BinDir -Filter "*.exe" | ForEach-Object { 
        Write-Host "  $($_.Name)" -ForegroundColor Gray 
    }
}

