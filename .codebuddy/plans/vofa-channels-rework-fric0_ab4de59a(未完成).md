---
name: vofa-channels-rework-fric0
overview: 精简 VOFA 监看通道：删 yaw/pitch 8 通道与鼠标键盘 3 通道，保留遥控器 7 通道；新增 0 号摩擦轮（fric_motor[0]，CAN 0x201）的 5 个可调参数与 4 条曲线。shoot.c 零改动（extern 全局 shoot_control）。
todos:
  - id: vofa-trim-channels
    content: 注释 vofa_bind 中云台 8 通道与 mouse/key 注册，保留遥控器 7 通道并更新通道表注释
    status: pending
  - id: vofa-bind-fric0
    content: vofa_task.c 引入 shoot.h 并注册 fric0 整组 PID 参数、fric0_target 与 4 个观测通道
    status: pending
    dependencies:
      - vofa-trim-channels
  - id: vofa-fric-build
    content: cmake --build --preset Debug 编译验证并 read_lints 确认无告警
    status: pending
    dependencies:
      - vofa-bind-fric0
---

## 需求概述
按“调哪个输出哪个”的原则精简 VOFA 波形通道，并把 0 号摩擦轮接入在线调参：

1. **通道裁剪**：删除 yaw/pitch 的 8 个观测通道和 mouse_x/mouse_y/key_v 3 个通道（云台参数注册一并暂停，注释保留便于日后恢复）；保留遥控器 7 个通道（rc_ch0~ch4、rc_s0、rc_s1）。
2. **接入 0 号摩擦轮**（CAN1 0x201，`fric_motor[0]`）：
   - 可调参数：fric0_kp/ki/kd/maxout/maxiout（速度环整组）+ fric0_target（目标转速，滑条实时控）；
   - 观测通道：fric0_target、fric0_set、fric0_get、fric0_out。
3. **滑条控件配置教学**：确认 VOFA 命令用 ASCII 文本（不勾 Hex），教用户配置滑条实时控制 fric0_target。

改后波形共 11 通道：ch0~ch6 遥控器，ch7 fric0_target，ch8 fric0_set，ch9 fric0_get，ch10 fric0_out。帧长从 76B 降到 48B，蓝牙带宽余量更足。


## 技术方案
### 已核实的代码事实
- `shoot.c:71` 的 `shoot_control_t shoot_control;` 为**非静态全局变量**，`shoot.h` 有完整类型定义 → `vofa_task.c` 直接 `#include "shoot.h"` + `extern shoot_control_t shoot_control;` 即可绑定，**shoot.c 零改动**。
- 摩擦轮控制链（shoot.c:204-214）：`fric_speed_set = fric_speed_ramp.out`（每周期被斜坡覆盖，不能直接 set）；`ramp_calc` 每周期把 out 增减 `FRIC_SPEED_RAMP_ADD_VALUE` 并 clamp 到 `[min_value, max_value]` → **改 `fric_speed_ramp.max_value` 即实时改目标转速**（set 0 时 out 被钳到 0，等效停轮）。
- PID 结构 `pid_type_def` 的 set/fdb/out 成员由 `PID_calc` 每周期刷新 → 直接注册这三个成员地址作为观测通道。

### vofa_task.c 修改点（vofa_bind 函数）
1. 注释掉 yaw/pitch 的 `vofa_reg_pid` x4 与 8 个 `vofa_reg_channel`（加标记注释，日后取消注释即恢复）；删除 mouse_x/mouse_y/key_v 三行注册（vofa_rc_mirror 数组与镜像刷新保留，仅不注册后 3 个通道）。
2. 新增摩擦轮绑定块：
```c
extern shoot_control_t shoot_control;   // 放文件头部
vofa_reg_pid("fric0", &shoot_control.fric_motor[0].speed_pid, VOFA_PID_STD);
vofa_reg_param("fric0_target", &shoot_control.fric_speed_ramp.max_value);
vofa_reg_channel("fric0_target", &shoot_control.fric_speed_ramp.max_value);
vofa_reg_channel("fric0_set", &shoot_control.fric_motor[0].speed_pid.set);
vofa_reg_channel("fric0_get", &shoot_control.fric_motor[0].speed_pid.fdb);
vofa_reg_channel("fric0_out", &shoot_control.fric_motor[0].speed_pid.out);
```
3. 同步更新文件头与 vofa_bind 注释中的通道顺序表（ch0~ch10）。
4. 参数总量 12 个（5+1 保留 + 暂注释的云台 20 个不在表内），容量宏无需调整；通道 11 个 < VOFA_CHANNEL_MAX=24。

### VOFA 滑条配置（用户操作，非代码）
- 命令控件：发送内容 `set fric0_target %%\n`（**ASCII 文本，Hex 不勾**——Hex 模式是把输入当原始字节序列发送，适合协议帧，不适合文本命令；VOFA 会把控件值代入 %% 占位符，`\n` 结尾触发板子解析）；
- 控件类型选滑条：最小值 `0`，最大值 `4500`（FRIC_SPEED_TARGET），步进 `50`；
- 连续发送建议关闭，拖动即发；开启则间隔 ≥500ms 防止抢占蓝牙带宽；
- 调 PID 参数另建滑条/按钮：`set fric0_kp %%\n`（0~50）、`set fric0_ki %%\n`（0~5）。

### 编译验证
`cmake --build --preset Debug`（STM32CubeCLT 路径），确认无错误无新增告警。
	
