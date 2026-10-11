@echo off
rem ============================================================================
rem  flash.bat  --  one click: build FLASH version + program it into the DSP
rem  (connect the XDS100 probe and power the board first, then double click)
rem ============================================================================
setlocal
set "PROJ=%~dp0"
set "PROJ=%PROJ:~0,-1%"
set "CCS_DIR=D:\Program Files\TI\CCS2101\ccs"

echo ============================================
echo   1/2  build FLASH version
echo ============================================
call "%PROJ%\build_flash.bat" || goto err

echo.
echo ============================================
echo   2/2  program into FLASH
echo ============================================
call "%CCS_DIR%\ccs_base\scripting\bin\dss.bat" "%PROJ%\tools\flash.js" || goto err

echo.
echo [OK] done. Power cycle the board to run standalone.
pause
exit /b 0

:err
echo.
echo [FAIL] see messages above
pause
exit /b 1
