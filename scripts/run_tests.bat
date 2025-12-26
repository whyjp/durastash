@echo off
REM DuraStash 테스트 실행 스크립트
REM Usage: run_tests.bat [Release|Debug]

REM UTF-8 코드 페이지 설정 (한글 출력 깨짐 방지)
chcp 65001 >nul 2>&1

setlocal enabledelayedexpansion

set BUILD_TYPE=Release

REM 파라미터 파싱
if not "%~1"=="" set BUILD_TYPE=%~1

REM 프로젝트 루트 디렉토리
set "SCRIPT_DIR=%~dp0"
set "PROJECT_ROOT=%SCRIPT_DIR%.."
set "BUILD_DIR=%PROJECT_ROOT%\build"

echo === DuraStash 테스트 실행 ===
echo 빌드 타입: %BUILD_TYPE%
echo.

REM 빌드 디렉토리 확인
if not exist "%BUILD_DIR%" (
    echo 오류: 빌드 디렉토리가 없습니다. 먼저 빌드하세요: scripts\build.bat
    exit /b 1
)

cd /d "%BUILD_DIR%"

REM CTest 실행
echo === CTest 실행 ===
set CONFIG_NAME=%BUILD_TYPE%
if "%BUILD_TYPE%"=="RelWithDebInfo" set CONFIG_NAME=RelWithDebInfo

ctest -C %CONFIG_NAME% --output-on-failure
if errorlevel 1 (
    echo.
    echo 오류: 일부 테스트가 실패했습니다.
    cd /d "%PROJECT_ROOT%"
    exit /b 1
)

cd /d "%PROJECT_ROOT%"

echo.
echo === 모든 테스트 통과 ===

endlocal

