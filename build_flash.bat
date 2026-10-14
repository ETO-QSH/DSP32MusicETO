@echo off
rem ============================================================================
rem  build_flash.bat -- build FLASH version (standalone)
rem  layout: HardWare\<MODULE>\ + User\ + DSP2833x_Libraries\ (TI official) + tools\
rem  NOTE: keep this file ASCII-only. Chinese text in .bat breaks cmd parsing.
rem ============================================================================
setlocal
set "CCS_DIR=D:\Program Files\TI\CCS2101\ccs"
set "CG_ROOT=%CCS_DIR%\tools\compiler\ti-cgt-c2000_25.11.1.LTS"
set "CC=%CG_ROOT%\bin\cl2000.exe"
set "PROJ=%~dp0"
set "PROJ=%PROJ:~0,-1%"
set "H=%PROJ%\HardWare"
set "LIB=%PROJ%\DSP2833x_Libraries"
set "LSRC=%LIB%\DSP2833x_common\source"
set "LCMD=%LIB%\DSP2833x_common\cmd"
set "LHDR=%LIB%\DSP2833x_headers"
set "OUT=%PROJ%\Flash"
set "CFLAGS=-v28 -ml -mt --float_support=fpu32 -g --diag_warning=225 --diag_wrap=off --display_error_number --abi=coffabi -DFLASH_BUILD"
set INC="--include_path=%PROJ%\User" "--include_path=%H%\GPIO" "--include_path=%H%\DELAY" "--include_path=%H%\LED" "--include_path=%H%\SEG" "--include_path=%H%\KEY" "--include_path=%H%\LCD1602" "--include_path=%H%\BEEP" "--include_path=%H%\AUDIO" "--include_path=%H%\OLED" "--include_path=%LHDR%\include" "--include_path=%LIB%\DSP2833x_common\include" "--include_path=%CG_ROOT%\include"

if not exist "%OUT%" mkdir "%OUT%"
pushd "%OUT%"
del /q *.obj 2>nul

echo === [1/3] TI official library ===
"%CC%" %CFLAGS% %INC% --compile_only "%LHDR%\source\DSP2833x_GlobalVariableDefs.c" --output_file="lib_gv.obj"   || goto err
"%CC%" %CFLAGS% %INC% --compile_only "%LSRC%\DSP2833x_SysCtrl.c"    --output_file="lib_sys.obj"  || goto err
"%CC%" %CFLAGS% %INC% --compile_only "%LSRC%\DSP2833x_Gpio.c"       --output_file="lib_gpio.obj" || goto err
"%CC%" %CFLAGS% %INC% --compile_only "%LSRC%\DSP2833x_PieCtrl.c"    --output_file="lib_pie.obj"  || goto err
"%CC%" %CFLAGS% %INC% --compile_only "%LSRC%\DSP2833x_PieVect.c"    --output_file="lib_piev.obj" || goto err
"%CC%" %CFLAGS% %INC% --compile_only "%LSRC%\DSP2833x_DefaultIsr.c" --output_file="lib_isr.obj"  || goto err
"%CC%" %CFLAGS% %INC% --compile_only "%LSRC%\DSP2833x_CpuTimers.c"  --output_file="lib_cpu.obj"  || goto err
"%CC%" %CFLAGS% %INC% --compile_only "%LSRC%\DSP2833x_EPwm.c"       --output_file="lib_epwm.obj" || goto err
"%CC%" %CFLAGS% %INC% --compile_only "%LSRC%\DSP2833x_MemCopy.c"    --output_file="lib_mem.obj"  || goto err
"%CC%" %CFLAGS% %INC% --compile_only "%LSRC%\DSP2833x_usDelay.asm"         --output_file="lib_usd.obj" || goto err
"%CC%" %CFLAGS% %INC% --compile_only "%LSRC%\DSP2833x_CodeStartBranch.asm" --output_file="lib_cs.obj"  || goto err
"%CC%" %CFLAGS% %INC% --compile_only "%LSRC%\DSP2833x_ADC_cal.asm"         --output_file="lib_cal.obj" || goto err
"%CC%" %CFLAGS% %INC% --compile_only "%LSRC%\DSP2833x_DisInt.asm"          --output_file="lib_di.obj"  || goto err
"%CC%" %CFLAGS% %INC% --compile_only "%LSRC%\DSP2833x_DBGIER.asm"          --output_file="lib_dbg.obj" || goto err

echo === [2/3] HardWare drivers + User app ===
"%CC%" %CFLAGS% %INC% --compile_only "%PROJ%\User\main.c"     --output_file="main.obj"     || goto err
"%CC%" %CFLAGS% %INC% --compile_only "%PROJ%\User\app.c"      --output_file="app.obj"      || goto err
"%CC%" %CFLAGS% %INC% --compile_only "%H%\GPIO\bsp.c"         --output_file="bsp.obj"      || goto err
"%CC%" %CFLAGS% %INC% --compile_only "%H%\DELAY\delay.c"      --output_file="delay.obj"    || goto err
"%CC%" %CFLAGS% %INC% --compile_only "%H%\LED\led.c"          --output_file="led.obj"      || goto err
"%CC%" %CFLAGS% %INC% --compile_only "%H%\SEG\smg.c"          --output_file="smg.obj"      || goto err
"%CC%" %CFLAGS% %INC% --compile_only "%H%\KEY\key.c"          --output_file="key.obj"      || goto err
"%CC%" %CFLAGS% %INC% --compile_only "%H%\LCD1602\lcd1602.c"  --output_file="lcd1602.obj"  || goto err
"%CC%" %CFLAGS% %INC% --compile_only "%H%\AUDIO\audio.c"      --output_file="audio.obj"    || goto err
"%CC%" %CFLAGS% %INC% --compile_only "%H%\OLED\oled.c"      --output_file="oled.obj"    || goto err

echo === [3/3] link (official F28335.cmd + DSP2833x_Headers_nonBIOS.cmd) ===
"%CC%" %CFLAGS% -z -m"DSP32Project_flash.map" --stack_size=0x300 --warn_sections "-i%CG_ROOT%\lib" --reread_libs --rom_model -o "DSP32Project_flash.out" main.obj app.obj bsp.obj delay.obj led.obj smg.obj key.obj lcd1602.obj audio.obj oled.obj lib_gv.obj lib_sys.obj lib_gpio.obj lib_pie.obj lib_piev.obj lib_isr.obj lib_cpu.obj lib_epwm.obj lib_mem.obj lib_usd.obj lib_cs.obj lib_cal.obj lib_di.obj lib_dbg.obj "%PROJ%\tools\F28335.cmd" "%LHDR%\cmd\DSP2833x_Headers_nonBIOS.cmd" -llibc.a || goto err

popd
echo.
echo [OK] FLASH build: %OUT%\DSP32Project_flash.out
exit /b 0

:err
popd
echo.
echo [FAIL] build error
exit /b 1
