;-------------------------------------------------------------------------------
;  codestart.asm —— "boot to Flash" 启动入口（FLASH 版本专用）
;
;  复位后 boot ROM 采样启动模式引脚；若为 "Jump to Flash"，
;  则把 PC 跳到 FLASH 入口 0x33FFF6（即链接脚本 F28335.cmd 中的 BEGIN 段）。
;  本节就放在那里，内容是一条长跳转，跳到 C 启动例程 _c_int00。
;  参考 TI 的 DSP2833x_CodeStartBranch.asm
;-------------------------------------------------------------------------------
        .ref    _c_int00
        .sect   "codestart"
        .global code_start
code_start:
        LB      _c_int00
