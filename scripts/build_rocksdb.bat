@echo off
REM RocksDB 사전 빌드 스크립트
REM Usage: build_rocksdb.bat [Release|Debug] [Static|Shared]

REM UTF-8 코드 페이지 설정 (한글 출력 깨짐 방지)
chcp 65001 >nul 2>&1

setlocal enabledelayedexpansion

set BUILD_TYPE=Release
set LINK_TYPE=Static

REM 파라미터 파싱
if not "%~1"=="" set BUILD_TYPE=%~1
if not "%~2"=="" set LINK_TYPE=%~2

REM 프로젝트 루트 디렉토리
set "SCRIPT_DIR=%~dp0"
set "PROJECT_ROOT=%SCRIPT_DIR%.."
set "ROCKSDB_DIR=%PROJECT_ROOT%\third_party\rocksdb"
set "BUILD_DIR=%PROJECT_ROOT%\build_rocksdb"
set "INSTALL_DIR=%PROJECT_ROOT%\third_party\rocksdb_build"

echo === RocksDB 사전 빌드 ===
echo 빌드 타입: %BUILD_TYPE%
echo 링크 타입: %LINK_TYPE%
echo 소스 디렉토리: %ROCKSDB_DIR%
echo 빌드 디렉토리: %BUILD_DIR%
echo 설치 디렉토리: %INSTALL_DIR%
echo.

REM RocksDB 소스 확인
if not exist "%ROCKSDB_DIR%\CMakeLists.txt" (
    echo 오류: RocksDB 소스가 없습니다. 서브모듈을 초기화하세요: git submodule update --init --recursive
    exit /b 1
)

REM 빌드 디렉토리 생성
if exist "%BUILD_DIR%" (
    echo 기존 빌드 디렉토리 삭제 중...
    rmdir /s /q "%BUILD_DIR%"
)
mkdir "%BUILD_DIR%"

REM CMake 구성
echo.
echo === CMake 구성 ===
cd /d "%BUILD_DIR%"

set CMAKE_ARGS=-G "Visual Studio 18 2026" -A x64 -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DCMAKE_INSTALL_PREFIX=%INSTALL_DIR% -DWITH_TESTS=OFF -DWITH_BENCHMARK_TOOLS=OFF -DWITH_TOOLS=OFF -DWITH_CORE_TOOLS=OFF

if "%LINK_TYPE%"=="Shared" (
    set CMAKE_ARGS=%CMAKE_ARGS% -DROCKSDB_BUILD_SHARED=ON
) else (
    set CMAKE_ARGS=%CMAKE_ARGS% -DROCKSDB_BUILD_SHARED=OFF
)

cmake ..\third_party\rocksdb %CMAKE_ARGS%
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

REM 설치
echo.
echo === 설치 중 ===
cmake --install . --config %CONFIG_NAME%
if errorlevel 1 (
    echo 오류: 설치 실패
    cd /d "%PROJECT_ROOT%"
    exit /b 1
)

cd /d "%PROJECT_ROOT%"

echo.
echo === 빌드 완료 ===
echo RocksDB가 다음 위치에 설치되었습니다:
echo   %INSTALL_DIR%
echo.

REM 빌드된 파일 확인
set "LIB_DIR=%INSTALL_DIR%\lib"
set "INCLUDE_DIR=%INSTALL_DIR%\include"
echo 빌드된 파일:
if exist "%LIB_DIR%" (
    dir /b "%LIB_DIR%"
)
echo 헤더 파일:
if exist "%INCLUDE_DIR%" (
    echo   %INCLUDE_DIR%
)

endlocal

