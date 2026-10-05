---
name: disable-fric-auto-poweron
overview: 把 shoot.h 的 FRIC_POWERON_AUTO 从 1 改为 0，关闭"上电自动转摩擦轮"调试功能，恢复遥控器控制摩擦轮与云台无力保护，一行改动。
todos:
  - id: fric-auto-off
    content: 将 shoot.h 的 FRIC_POWERON_AUTO 改为 0，恢复遥控器控制摩擦轮与云台无力保护
    status: completed
  - id: fric-build-verify
    content: 用 cmake --build --preset Debug 编译验证并确认无告警
    status: completed
---

## 用户需求
关闭射击模块的调试功能：摩擦轮目前上电即自动缓升（无需遥控器），用户要求把这部分调试逻辑关掉（删除或注释均可）。

## 现状（已探明）
`application/shoot.h:36` 的 `#define FRIC_POWERON_AUTO 1` 是调试总开关，在 `application/shoot.c` 中控制三处编译分支：
1. `shoot_init`（86-90 行）：调试模式下上电即进入 `SHOOT_READY_FRIC` 自动升轮；关闭后回到 `SHOOT_STOP`，等待遥控器指令。
2. `shoot_set_mode`（228-240 行）：调试分支自动跳过遥控器开关轮逻辑；关闭后走正常分支（242-253 行）——遥控器 SHOOT_RC_MODE_CHANNEL 上拨一次开轮、再拨关闭。
3. `shoot_set_mode` 尾部（328-333 行）：调试模式跳过 `gimbal_cmd_to_shoot_stop()` 云台无力停轮保护；关闭后保护恢复生效（遥控器离线/云台无力时摩擦轮停止）。

## 方案
将 `FRIC_POWERON_AUTO` 改为 `0`。三处调试代码块被预处理器整体排除，等效于删除/注释；正常遥控器控制逻辑与云台无力保护自动恢复。保留 `#if` 结构不做物理删除，日后需要调试时改回 `1` 即可，改动最小且完全可逆。

## 技术方案
- 单行宏修改：`application/shoot.h:36` `FRIC_POWERON_AUTO` 1 → 0。
- 预处理排除等效删除三处调试代码块（shoot_init 初始状态、shoot_set_mode 自动升轮分支、云台无力保护跳过逻辑），无需触碰 shoot.c。
- 修改后用 `cmake.exe --build --preset Debug`（STM32CubeCLT 路径）编译验证，确认无错误无新增告警。
