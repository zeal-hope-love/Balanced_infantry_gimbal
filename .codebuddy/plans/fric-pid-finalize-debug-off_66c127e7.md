---
name: fric-pid-finalize-debug-off
overview: 定稿摩擦轮 PID（Ki 0.1→0.5 写回 shoot.h 宏），关闭 FRIC_POWERON_AUTO 调试模式（全部测试代码编译排除），恢复正常遥控器射击链路（上拨开轮/下拨单发/长按连发/热量与无力保护），编译验证。
todos:
  - id: shoot-macro-finalize
    content: 修改 shoot.h 两处宏：FRIC_SPEED_PID_KI 0.1f→0.5f 固化调参结果，FRIC_POWERON_AUTO 1→0 关闭调试模式
    status: completed
  - id: shoot-finalize-build
    content: 用 STM32CubeCLT 路径 cmake.exe --build --preset Debug 编译验证，read_lints 确认无告警，更新记忆文件并向用户说明遥控发弹操作流程
    status: completed
---

## 需求概述
1. **固化调参结果**：摩擦轮速度环 PID 定稿 Kp=12、Ki=0.5（当前宏 Ki=0.1f 需改为 0.5f），写入 `shoot.h` 宏，上电即用该值。
2. **关闭调试模式**：`FRIC_POWERON_AUTO` 1→0，之前所有测试代码（恒置 READY_FRIC、右摇杆满映射转速、单轮驱动、FRIC_DEBUG_SPEED_MAX）均已在 `#if FRIC_POWERON_AUTO` 内，宏置 0 即全部编译排除，等效删除、可随时改回 1 恢复。
3. **接通发弹链路**：核实并恢复正常遥控射击逻辑——已确认 `shoot_control_loop()` 挂在 `gimbal_task.c:342`（1ms 周期随云台任务执行），正常分支逻辑完整：右拨杆上拨开摩擦轮（斜坡升 4500rpm）→ 升满自动进 READY_BULLET → 拨杆下拨单发/长按连发（含热量限制与云台无力停射保护）。宏关闭后该链路自动生效，无需额外代码。
4. 向用户说明遥控发弹操作流程。

## 遥控发弹操作流程（交付说明）
- 右拨杆（SHOOT_RC_MODE_CHANNEL）**上拨一次**：摩擦轮斜坡升速至 4500rpm，到位自动待命；
- **下拨一次**：单发一颗；按住不放：连发（长按触发 CONTINUE_BULLET）；
- 再次上拨：关摩擦轮；
- 键盘模式下中档可用 SHOOT_ON/OFF_KEYBOARD 开关摩擦轮；
- 保护：弹量热量超限自动停发；云台无力/遥控器离线自动停射。

## 技术方案
- **改动仅 2 处宏（application/shoot.h）**：
  1. `FRIC_SPEED_PID_KI` 0.1f → 0.5f（注释注明 2026-10-05 VOFA 实测定稿）；
  2. `FRIC_POWERON_AUTO` 1 → 0（注释注明调试完成关闭，改回 1 可恢复调试模式）。
- **无需改动 shoot.c / vofa_task.c**：调试代码块随宏失效；VOFA 的 fric0_target/set/get/out 四通道仍绑 fric_motor[0]（fric_speed_set 恢复由斜坡输出驱动），正常模式下照常监看波形。
- **发弹链路核实（已确认无需新代码）**：`shoot_control_loop()` 在 gimbal_task.c:342 随云台任务 1ms 调用；正常模式状态机（#else 分支）完整：STOP→READY_FRIC(上拨)→READY_BULLET(斜坡满)→READY→BULLET/CONTINUE(下拨/长按)；热量限制（referee 在线时）与 gimbal_cmd_to_shoot_stop 保护随 FRIC_POWERON_AUTO=0 恢复生效。
- **编译验证**：`cmake --build --preset Debug`（STM32CubeCLT 路径），确认无错误、无新增告警（宏置 0 后 `uint8_t i` 恢复使用，无 unused 告警），read_lints 检查改动文件。
- **收尾**：更新 .codebuddy/memory/MEMORY.md（摩擦轮 PID 定稿 Kp12/Ki0.5、调试模式关闭、遥控发弹操作口径）。
