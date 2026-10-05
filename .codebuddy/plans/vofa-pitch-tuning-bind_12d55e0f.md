---
name: vofa-pitch-tuning-bind
overview: VOFA 绑定切换为 pitch 调参：注释摩擦轮 4 通道与 6 参数（保留可恢复），新增 pitch 角度环(gimbal_PID_t absolute_angle_pid)与速度环(pid_type_def gyro_pid)参数共 10 个、观测通道 5 条（set/get/gyro_set/gyro/out）。
todos:
  - id: vofa-pitch-bind
    content: 注释 vofa_task.c fric0 注册块，新增 pitch 角度环(VOFA_PID_GIMBAL)/速度环(VOFA_PID_STD)参数与 5 条观测通道，更新通道表注释
    status: completed
  - id: vofa-pitch-build-verify
    content: 用 cmake --build --preset Debug 编译验证，read_lints 确认无告警，更新记忆文件
    status: completed
---

## 产品概述
将 VOFA 在线调参对象从摩擦轮切换到云台 pitch 电机：接入 pitch 的**串级双环**（角度环 + 速度环）参数与曲线，配合左摇杆做阶跃激励整定 PID。

## 核心功能
- **VOFA 通道切换**：注释 fric0 注册块（可恢复），注册 pitch 角度环/速度环共 10 个可调参数 + 5 条曲线。
- **激励源**：pitch 由**左摇杆垂直通道**（`PITCH_CHANNEL=3`，即 rc.ch[3]，范围 ±660）控制，推摇杆即给角度环目标阶跃。
- **模式前提**：左拨杆推**上档**（陀螺仪模式）才用 `gimbal_motor_absolute_angle_pid`；中档用的是另一个结构体（relative_angle_pid），测试时必须在上档。
- 拨弹/摩擦轮逻辑不受影响；拨弹轮保持未接状态。


## 技术方案
### 两个环用的结构体（用户提问的答案）
- **角度环**：`gimbal_PID_t`（云台专用结构体，gimbal_task.h 定义，成员 kp/ki/kd/max_out/max_iout/set/get/err/out/Pout/Iout/Dout），实例 = `gimbal_motor_t.gimbal_motor_absolute_angle_pid`，VOFA 注册类型 `VOFA_PID_GIMBAL`，参数名前缀 `pitch_ang_`。
- **速度环**：`pid_type_def`（components/controller/pid.h 通用结构体），实例 = `gimbal_motor_t.gimbal_motor_gyro_pid`，VOFA 注册类型 `VOFA_PID_STD`，参数名前缀 `pitch_spd_`。
- 串级关系：角度环输出 → `motor_gyro_set`（速度环目标）→ 速度环输出 → `current_set` → CAN_cmd_gimbal 0x1FF 槽2（GM6020 电压 ±30000）。pitch 反馈已随电调 ID2 自动适配（0x206 → chassis[5]），云台代码零改动。

### vofa_task.c 修改（vofa_bind 函数）
1. fric0 整块（5 参数 + 4 通道）注释保留，标记可恢复。
2. 新增 pitch 绑定（通过 `get_pitch_motor_point()` 判空后注册）：
   - `vofa_reg_pid("pitch_ang", &pitch->gimbal_motor_absolute_angle_pid, VOFA_PID_GIMBAL)` → pitch_ang_kp/ki/kd/maxout/maxiout
   - `vofa_reg_pid("pitch_spd", &pitch->gimbal_motor_gyro_pid, VOFA_PID_STD)` → pitch_spd_kp/ki/kd/maxout/maxiout
   - 通道（注册顺序 = ch0~ch4）：
     * ch0 `pitch_set` ← &absolute_angle_set（角度目标，左摇杆驱动）
     * ch1 `pitch_get` ← &absolute_angle（实际角度，rad）
     * ch2 `pitch_gyro_set` ← &motor_gyro_set（角度环输出=速度环目标，rad/s）
     * ch3 `pitch_gyro` ← &motor_gyro（实际角速度，rad/s）
     * ch4 `pitch_out` ← &current_set（速度环输出电压）
   - 帧 24B，50Hz 下约 1.2KB/s，蓝牙无压力。
3. 当前宏参数（gimbal_task.h）：角度环 15/0/0（max_out=10）、速度环 2900/60/0（max_out=30000/max_iout=10000）。

### VOFA 滑条配置（交付说明）
```
set pitch_ang_kp %.2f\n   0~30   步进0.5（当前15）
set pitch_ang_kd %.2f\n   0~1    步进0.05（当前0）
set pitch_spd_kp %.0f\n   0~6000 步进100（当前2900）
set pitch_spd_ki %.2f\n   0~200  步进5（当前60）
```
Hex 不勾，\n 结尾必须有，连续发送关闭。

### PID 推荐值（GM6020 pitch，串级 1kHz）
- 角度环：Kp=10~15（起步15），Ki=0，Kd=0.3~0.5（云台角度环加微分抑制超调，现值0）
- 速度环：Kp=1500~3500（起步2900），Ki=30~100（起步60），Kd=0
- 整定顺序：先内环后外环——先固定角度环只推摇杆看 ch3 追 ch2，调好速度环；再调角度环 Kp/Kd 看 ch1 追 ch0。

### 验证
`cmake --build --preset Debug`（STM32CubeCLT 路径）+ read_lints + 更新 .codebuddy/memory/MEMORY.md。
	
