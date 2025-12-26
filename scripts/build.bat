@echo off
REM DuraStash 빌드 스크립트
REM Usage: build.bat [Release|Debug] [UsePrebuiltRocksDB] [RocksDBRoot]

REM UTF-8 코드 페이지 설정 (한글 출력 깨짐 방지)
chcp 65001 >nul 2>&1

setlocal enabledelayedexpansion

set BUILD_TYPE=Release
set USE_PREBUILT=0
set ROCKSDB_ROOT=third_party\rocksdb_build

REM 프로젝트 루트 디렉토리 (먼저 설정)
set "SCRIPT_DIR=%~dp0"
set "PROJECT_ROOT=%SCRIPT_DIR%.."
set "BUILD_DIR=%PROJECT_ROOT%\build"

REM 파라미터 파싱
if not "%~1"=="" set BUILD_TYPE=%~1
if /i "%~2"=="UsePrebuiltRocksDB" set USE_PREBUILT=1
if not "%~3"=="" set ROCKSDB_ROOT=%~3

echo === DuraStash 빌드 ===
echo 빌드 타입: %BUILD_TYPE%
echo.

REM 사전 빌드된 RocksDB 확인
set "USE_BUILD_DIR=0"
if %USE_PREBUILT%==1 (
    REM 빌드 디렉토리에서 직접 찾기 시도 (설치가 실패했을 수 있음)
    set "ROCKSDB_BUILD_DIR=!PROJECT_ROOT!\build_rocksdb"
    if exist "!ROCKSDB_BUILD_DIR!\Release\rocksdb.lib" (
        echo 빌드 디렉토리에서 RocksDB를 찾았습니다: !ROCKSDB_BUILD_DIR!
        REM CMake에서 빌드 디렉토리를 직접 사용하도록 설정
        set "ROCKSDB_LIB_DIR=!ROCKSDB_BUILD_DIR!\Release"
        set "ROCKSDB_INCLUDE_DIR=!PROJECT_ROOT!\third_party\rocksdb\include"
        set "USE_BUILD_DIR=1"
    ) else (
        REM 설치 디렉토리 확인
        set "ROCKSDB_PATH=!ROCKSDB_ROOT!"
        if not exist "!ROCKSDB_PATH!" set "ROCKSDB_PATH=!PROJECT_ROOT!\!ROCKSDB_ROOT!"
        
        if not exist "!ROCKSDB_PATH!\lib" (
            echo 오류: 사전 빌드된 RocksDB를 찾을 수 없습니다.
            echo   설치 디렉토리: !ROCKSDB_PATH!
            echo   빌드 디렉토리: !ROCKSDB_BUILD_DIR!
            echo 먼저 RocksDB를 빌드하세요: scripts\build_rocksdb.bat
            exit /b 1
        ) else (
            echo 사전 빌드된 RocksDB 사용: !ROCKSDB_PATH!
        )
    )
)

REM 빌드 디렉토리 생성
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

REM CMake 구성
echo.
echo === CMake 구성 ===
cd /d "%BUILD_DIR%"

set CMAKE_ARGS=-G "Visual Studio 18 2026" -A x64 -DCMAKE_BUILD_TYPE=%BUILD_TYPE%

if %USE_PREBUILT%==1 (
    if "!USE_BUILD_DIR!"=="1" (
        REM 빌드 디렉토리에서 직접 사용
        set CMAKE_ARGS=!CMAKE_ARGS! -DUSE_PREBUILT_ROCKSDB=ON -DROCKSDB_LIBRARY=!ROCKSDB_LIB_DIR!\rocksdb.lib -DROCKSDB_INCLUDE_DIR=!ROCKSDB_INCLUDE_DIR!
    ) else (
        REM 설치 디렉토리 사용
        set CMAKE_ARGS=!CMAKE_ARGS! -DUSE_PREBUILT_ROCKSDB=ON -DROCKSDB_ROOT=!ROCKSDB_PATH!
    )
)

cmake .. %CMAKE_ARGS%
if errorlevel 1 (
    echo 오류: CMake 구성 실패
    cd /d "%PROJECT_ROOT%"
    exit /b 1
)

REM 빌드
echo.
echo === 빌드 중 ===
set CONFIG_NAME=%BUILD_TYPE%
if "%BUILD_TYPE%"=="RelWithDebInfo" set CONFIG_NAME=RelWithDebInfo

REM CPU 코어 수 확인 및 병렬 빌드 설정
set /a CPU_CORES=%NUMBER_OF_PROCESSORS%
if %CPU_CORES% LSS 1 set CPU_CORES=4
REM MSBuild 병렬 빌드: 코어 수만큼 병렬 작업 수행 (최대 성능)
set /a MSBUILD_JOBS=%CPU_CORES%
echo 병렬 빌드: %CPU_CORES% 코어 사용 (MSBuild 작업: %MSBUILD_JOBS%)

REM Visual Studio 생성기 사용 시 MSBuild를 직접 호출하여 최대 병렬 빌드 성능 확보
REM /m 옵션: 병렬 프로젝트 빌드, /p:CL_MPCount: 컴파일러 병렬 컴파일
REM .sln 파일이 있으면 사용, 없으면 cmake --build 사용
if exist "*.sln" (
    msbuild *.sln /p:Configuration=%CONFIG_NAME% /p:Platform=x64 /m:%MSBUILD_JOBS% /p:CL_MPCount=%MSBUILD_JOBS% /t:Build /v:minimal
) else (
    cmake --build . --config %CONFIG_NAME% --parallel %CPU_CORES% -- /m:%MSBUILD_JOBS% /p:CL_MPCount=%MSBUILD_JOBS%
)
if errorlevel 1 (
    echo 오류: 빌드 실패
    cd /d "%PROJECT_ROOT%"
    exit /b 1
)

cd /d "%PROJECT_ROOT%"

echo.
echo === 빌드 완료 ===
echo 빌드 결과:
set "BIN_DIR=%BUILD_DIR%\bin\%CONFIG_NAME%"
if exist "%BIN_DIR%" (
    dir /b "%BIN_DIR%\*.exe" 2>nul
)

endlocal

