/* ============================================================================
 *  tools/flash.js —— 把 FLASH 版本烧进 F28335（擦除 A/D 扇区 + 编程 + 校验）
 *  用法（由 flash.bat 调用）:
 *      dss.bat tools\flash.js
 *  说明：
 *    * .text/.econst 在 A 扇区(0x338000)，.TI.ramfunc 在 D 扇区(0x320000)
 *    * 时钟选项用默认值（属性文件默认 OSCCLK=30MHz / CLKINDIV=2 / PLLCR=10）
 * ========================================================================== */
importPackage(Packages.com.ti.debug.engine.scripting);
importPackage(Packages.com.ti.ccstudio.scripting.environment);
importPackage(Packages.java.lang);

var env = ScriptingEnvironment.instance();
env.setScriptTimeout(900000);

var server = env.getServer("DebugServer.1");
server.setConfig("C:/Users/ETO/workspace_ccstheia/DSP32Project/targetConfigs/TMS320F28335.ccxml");
var session = server.openSession("*", "*");
session.target.connect();
print("CONNECTED");

function opt(desc, fn) {
    try { fn(); print("  OK   : " + desc); }
    catch (e) { print("  SKIP : " + desc + "   (" + e + ")"); }
}

opt("A=true",  function () { session.flash.options.setBoolean("FlashSectorA", true);  });
opt("B=false", function () { session.flash.options.setBoolean("FlashSectorB", false); });
opt("C=false", function () { session.flash.options.setBoolean("FlashSectorC", false); });
opt("D=true",  function () { session.flash.options.setBoolean("FlashSectorD", true);  });
opt("E=false", function () { session.flash.options.setBoolean("FlashSectorE", false); });
opt("F=false", function () { session.flash.options.setBoolean("FlashSectorF", false); });
opt("G=false", function () { session.flash.options.setBoolean("FlashSectorG", false); });
opt("H=false", function () { session.flash.options.setBoolean("FlashSectorH", false); });
opt("FlashOperations = Erase, Program, Verify",
    function () { session.flash.options.setString("FlashOperations", "Erase, Program, Verify"); });

print("FLASHING ...");
session.memory.loadProgram("C:/Users/ETO/workspace_ccstheia/DSP32Project/Flash/DSP32Project_flash.out");
print("PROGRAMMED + VERIFIED");

session.flash.calculateChecksum();

/* 复位并按启动模式运行（拨码开关默认 FLASH 启动） */
session.target.reset();
session.target.runAsynch();
Thread.sleep(800);

session.terminate();
server.stop();
print("DONE - 已烧写并运行");
