---
name: fric-debug-id1-only
overview: 摩擦轮调试模式改为只驱动 0 号轮（电调 ID1，fric_motor[0]），1 号轮（ID2）电流置 0，使摇杆控制对象与 VOFA 调参对象统一；并附摩擦轮速度环推荐 PID 参考。
todos:
  - id: fric-debug-single-wheel
    content: "shoot.c 调试分支改为只驱动 fric_motor[0]，fric_motor[1].current 置 0，原循环保留于 #else 分支"
    status: completed
  - id: fric-single-build-verify
    content: 用 CLion 路径 cmake.exe --build --preset Debug 编译验证，read_lints 确认无告警，更新记忆文件
    status: completed
---


## 需求概述
1. **调试对象唯一化**：用户发现推右摇杆时转的是 1 号摩擦轮（电调 ID2，0x202），而 VOFA 调参绑定的和用户想调的是 0 号摩擦轮（电调 ID1，0x201）。要求调试模式下**只驱动 0 号轮（ID1）**，1 号轮（ID2）电流置 0，保证调参对象与 VOFA 波形/滑条一一对应。
2. **提供摩擦轮推荐 PID 参数**作为整定参考起点。

## 背景（已核实代码事实）
- `shoot.c` `shoot_control_loop`（约 207-214 行）：`for (i = 0; i < 2; i++) fric_motor[i].current = PID_calc(...)`，两轮共用同一目标 `fric_speed_set`（右摇杆 ch1 满映射 ±9000rpm），经 `CAN_cmd_fric(motor[0].current, motor[1].current)` 以 0x200 帧发给电调 ID1/ID2。
- `fric_motor[0]` = 电调 ID1（0x201 反馈）= 用户要调的 0 号轮；`fric_motor[1]` = 电调 ID2（0x202）= 现在转的 1 号轮。
- VOFA 的 `fric0_*` 参数与 `fric0_get` 通道均绑定 `fric_motor[0].speed_pid`，调参对象本来就是 ID1——只是 1 号轮也在跟着转，造成干扰与混淆。
- 现象（ID1 完全不转、ID2 转）同时提示 ID1 电调链路可能存在硬件问题（此前已知“一号轮 <100rpm”待查），单轮驱动后可通过 VOFA `fric0_get/fric0_out` 直接验证 ID1 是否响应电流。

## 交付内容
- 调试模式（FRIC_POWERON_AUTO=1）下：ID1 独占速度环输出，ID2 电流恒 0；正常模式（=0）两轮逻辑完全不变。
- 摩擦轮速度环推荐 PID 参考值与整定顺序（M3508+C620，转子转速环 1kHz）：起步 Kp=10、Ki=0.5、Kd=0（max_out=10000、max_iout=5000）；偏硬响应 Kp=15、Ki=0.8；当前工程值 Kp=12/Ki=0.1（Ki 偏小可能有稳态差）。整定顺序：Ki 先置 0 → Kp 加到临界轻微震荡 → 回撤 30~40% → Ki 从 0.2 起逐步加到稳态误差消除。
	


## 技术方案
### 改动点（仅 application/shoot.c 一处）
`shoot_control_loop` 摩擦轮电流计算段（`fric_speed_set` 摇杆映射覆盖之后、`CAN_cmd_fric` 之前）：

```c
#if FRIC_POWERON_AUTO
    //调试: 只驱动 0 号轮(电调ID1, 与VOFA fric0_*绑定一致), 1 号轮(ID2)电流置0
    shoot_control.fric_motor[0].current = (int16_t)PID_calc(&shoot_control.fric_motor[0].speed_pid,
            (fp32)shoot_control.fric_motor[0].measure->speed_rpm,
            shoot_control.fric_motor[0].target_sign * shoot_control.fric_speed_set);
    shoot_control.fric_motor[1].current = 0;
#else
    for (i = 0; i < 2; i++)
    {
        shoot_control.fric_motor[i].current = (int16_t)PID_calc(...原逻辑...);
    }
#endif
```

### 关键决策
- 用 `#if FRIC_POWERON_AUTO` 隔离，正常逻辑零影响、可一键回退（与既有调试宏结构一致）。
- 调试分支不使用循环变量 `i`，为避免 `-Wunused-but-set-variable` 告警，原 for 循环保留在 `#else` 分支内（`i` 声明保留，两分支共享声明，调试分支无引用——gcc 对循环内使用的变量在 #else 分支使用即不告警）。
- ID2 电流置 0 而非断发：`CAN_cmd_fric(0, current[1])` 帧结构不变，1 号轮失去力矩自然停转。
- 调试时验证手段：VOFA `fric0_get`（ch2）应跟随 `fric0_set`（ch1）；若 `fric0_out` 已输出而 `fric0_get` 恒 0/极小，即坐实 ID1 电调硬件链路问题（与此前“一号轮 <100rpm”待查现象吻合）。
	
