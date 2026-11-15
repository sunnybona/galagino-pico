@echo off
setlocal
rem Launch PicoBoy arcade studio. PATH python here has no Tcl/Tk, so search real installs first.
set "PY="
for %%P in (
  "E:\Program Files\python313\python.exe"
  "D:\Program Files\python3.13\python.exe"
  "C:\Program Files\python313\python.exe"
  "C:\Python313\python.exe"
) do (
  if not defined PY if exist "%%~P" set "PY=%%~P"
)
if not defined PY (
  for %%X in (python.exe) do set "PY=%%~$PATH:X"
)
if not defined PY (
  echo No python found. Install python.org build, or edit run_studio.bat.
  pause
  exit /b 1
)
echo Python: %PY%
"%PY%" "%~dp0tools\pico_arcade_studio.py"
if errorlevel 1 pause