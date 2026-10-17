@echo off
rem Thin wrapper: build.bat --vc7 "C:\...\Vc7" [--identical] [more build.py options]
rem Picks "py -3" if available, otherwise "python".
setlocal
where py >nul 2>nul
if %errorlevel%==0 (
  py -3 "%~dp0build.py" %*
) else (
  python "%~dp0build.py" %*
)
exit /b %errorlevel%
