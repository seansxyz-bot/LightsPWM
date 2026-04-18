@echo off

REM === CONFIG ===
set REPO_PATH=C:\Users\Space\OneDrive\Documents\Teensy\LightsPWM
set BRANCH=main

REM === CHECK MESSAGE ===
if "%~1"=="" (
    echo You must provide a commit message.
    echo Example: git_push.bat "updated html layout"
    pause
    exit /b
)

REM === GO TO REPO ===
cd /d "%REPO_PATH%" || (
    echo Failed to find repo path!
    pause
    exit /b
)

echo.
echo ===== GIT STATUS =====
git status

echo.
echo ===== ADDING FILES =====
git add .

echo.
echo ===== COMMITTING =====
git commit -m "%~1"
if %errorlevel% neq 0 (
    echo Nothing to commit or commit failed.
)

echo.
echo ===== PUSHING =====
git push origin %BRANCH%

echo.
echo ===== DONE =====
git status

pause