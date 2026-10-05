---
name: vofa-yaw-tuning-bind
overview: VOFA 绑定从 pitch 切换到 yaw 调参：注释 pitch 注册块（保留可恢复），新增 yaw 角度环(gimbal_PID_t absolute_angle_pid，含 Kd 0.3)/速度环(pid_type_def gyro_pid)参数共 10 个、观测通道 5 条（set/get/gyro_set/gyro/out）。
todos:
  - id: vofa-yaw-bind
    content: 修改 vofa_task.c vofa_bind：注释 pitch 块，新增 yaw 角度环/速度环参数注册与 5 条观测通道（get_yaw_motor_point 判空），更新块顶注释
    status: completed
  - id: vofa-yaw-build
    content: cmake --build --preset Debug 编译验证 + read_lints 无告警
    status: completed
  - id: vofa-yaw-memory
    content: 更新 MEMORY.md 记录 VOFA 已切换至 yaw 调参（参数名/通道表/激励源）及校准结论
    status: completed
---

## 产品概述
将 VOFA+ 调参链路从 pitch 串级切换到 yaw 串级：监看 yaw 角度/角速度/输出波形，并支持 `set/get/list` 命令在线修改 yaw 两环 PID。同时确认校准问题：pitch 验收时云台校准已做过、IMU 矩阵修正后陀螺仪校准已重做，yaw 调参**不需要再做任何校准**。

## Core Features
- vofa_bind 切换绑定目标：注释 pitch 注册块（保留可恢复），新增 yaw 绑定（判空保护）
- 可调参数：`yaw_ang_kp/ki/kd/maxout/maxiout`（角度环 gimbal_PID_t，当前 26/0/0.3）+ `yaw_spd_kp/ki/kd/maxout/maxiout`（速度环 pid_type_def，当前 3600/20/0）
- 监看通道 ch0~ch4：`yaw_set / yaw_get / yaw_gyro_set / yaw_gyro / yaw_out`，与 pitch 时期通道语义一一对应
- 激励源说明：左摇杆水平（ch2，YAW_CHANNEL=2）推一下回中即阶跃
- 编译验证 + 记忆更新


## Tech Stack
- 现有 STM32F407 + FreeRTOS 工程，沿用刚建好的 vofa_task 模块（USART1 蓝牙 JustFloat + set/get/list 命令），零架构改动
- 唯一修改点：`application/vofa_task.c` 的 `vofa_bind()`（第 409~452 行）
- yaw 特殊点：`motor_gyro` 带重力向量补偿（`gimbal_task.c:739-740` 的 cos/sin 公式），IMU 矩阵修正后反馈轴已正确，pitch≈0 时补偿项退化为纯 `INS_gyro[2]`，可直接复用现有通道绑定方式

## Implementation Approach
完全复制 pitch 绑定的成熟模式（vofa_reg_pid + vofa_reg_channel + 判空），仅替换绑定目标：
1. pitch 注册块整体注释（标记"调 yaw 时可恢复"）
2. 新增 yaw 块：`get_yaw_motor_point()` 判空 → `vofa_reg_pid("yaw_ang", &yaw->gimbal_motor_absolute_angle_pid, VOFA_PID_GIMBAL)` → `vofa_reg_pid("yaw_spd", &yaw->gimbal_motor_gyro_pid, VOFA_PID_STD)` → 5 条通道绑定（absolute_angle_set / absolute_angle / motor_gyro_set / motor_gyro / current_set）
3. 更新块顶注释（通道顺序表 + 激励源 = 左摇杆水平 ch2 + 云台模式拨杆上档）
4. 容量校验：yaw 新增 10 参数 + 5 通道，注册表 64/24 上限充足

## 影响范围
- 仅 `vofa_task.c` 一个函数体；gimbal_task/pid/vofa 协议层零改动
- 校准无需重做：云台校准（编码器中值/限位）与 IMU 矩阵无关且已做过（pitch 验收时 max/min 已正常）；陀螺仪校准在矩阵修正后已重做（ch1 静止稳定为证）

## Yaw PID 现状 vs 推荐区间（调参前基线）
| 环 | Kp | Ki | Kd | max_out / max_iout | 评价 |
|---|---|---|---|---|---|
| 角度环 yaw_ang | 26 | 0 | 0.3 | 10 / 0 | 全部在推荐区间内，且已带 Kd（pitch 调完后补的经验也适用） |
| 速度环 yaw_spd | 3600 | 20 | 0 | 30000 / 5000 | 区间内；yaw 无重力，Ki 小于 pitch(60) 合理 |


