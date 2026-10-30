@echo off
cd /d "%~dp0"

if not exist backend.exe (
    echo Backend executable not found in this folder.
    pause
    exit /b 1
)

taskkill /F /IM backend.exe >nul 2>nul
start "Shop Backend" cmd /k backend.exe

rem Give the backend a moment to start, then open the app it serves.
timeout /t 1 /nobreak >nul
start "" "http://localhost:8080/"

echo Shop app started.
