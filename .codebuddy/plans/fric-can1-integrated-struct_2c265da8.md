---
name: fric-can1-integrated-struct
overview: 摩擦轮切换到 CAN1（与云台同总线），并将 shoot_control_t 中的摩擦轮散字段整合为 fric_motor_t 结构体数组。
todos:
  - id: fric-can1-macro
    content: CAN_receive.h 将 FRIC_CAN 改为 hcan1，摩擦轮与云台同总线
    status: completed
  - id: fric-motor-struct
    content: shoot.h 新增 fric_motor_t 并整合 shoot_control_t 摩擦轮字段为 fric_motor[2] 数组
    status: completed
  - id: fric-code-refactor
    content: shoot.c 改造 init/feedback/loop 为 fric_motor[i] 索引访问，观测变量同步更新
    status: completed
  - id: build-verify-report
    content: 编译验证并输出完整改动清单与 Live Watch 使用说明
    status: completed
---

## 产品概述
将摩擦轮总线从 CAN2 迁移到 CAN1（与云台三电机同总线），并把 shoot_control_t 中摩擦轮的散字段整合为一个 fric_motor_t 结构体数组，提升代码组织性。完成后编译验证并汇报改动清单。

## 核心功能
- 摩擦轮（M3508 电调 ID 1/2，0x201/0x202）发送帧 0x200 改走 CAN1，与云台 0x1FF 帧同总线共存（电调按帧 ID 区分，无冲突）
- 新增 fric_motor_t 整合结构体：速度环 PID、电机反馈指针、转速反馈、转向符号、电流输出，shoot_control_t 内以 fric_motor[2] 数组组织
- PID/指针/符号初始化改为循环写入，feedback/control_loop 改为索引访问
- 保留 volatile 观测变量 fric_watch_speed1/2、fric_watch_current1/2 供调试器实时监视
- 总线最终分配：CAN1 = 云台 0x1FF 帧（yaw/pitch/拨弹）+ 摩擦轮 0x200 帧（ID1/2）；CAN2 空闲

## Tech Stack
- STM32F407 + HAL + FreeRTOS（CMSIS-OS v1）+ CMake（arm-none-eabi-gcc，preset Debug），工程名 standard_robot
- 复用现有组件：pid.c（PID_POSITION）、user_lib.c（ramp_function_source_t）、CAN_receive.c（motor_measure_t）、detect_task（TOE）

## Implementation Approach
### 关键技术决策
1. **总线切换只动宏**：CAN_receive.h 中 FRIC_CAN 由 hcan2 改 hcan1。CAN1 上 0x200（摩擦轮）与 0x1FF（云台）两种帧并存，C620 电调按帧 ID 匹配自身电调 ID，同总线无冲突；RX 回调按 StdId 解析不受总线影响，反馈零改动
2. **结构体整合**（上轮已获用户认可的方向）：
   - shoot.h 新增 fric_motor_t：speed_pid（pid_type_def）、measure（const motor_measure_t*）、speed（fp32）、target_sign（fp32，宏 FRIC1/FRIC2_TARGET_SIGN 写入）、current（int16_t）
   - shoot_control_t 中删除散字段 fric_speed_pid[2]/fric_motor_measure[2]/fric_speed[2]/fric_current[2]/fric_speed_set，替换为 fric_motor[2] 数组 + 共享 fric_speed_ramp（斜坡目标为两轮共用，保留在控制结构顶层）
3. **代码简化**：shoot_init 循环初始化（PID_init/ramp 指针/target_sign 宏赋值）；shoot_feedback_update、shoot_control_loop 改为 for 循环索引访问 fric_motor[i]；CAN_cmd_fric 调用改为取 fric_motor[0].current / fric_motor[1].current
4. **爆炸半径控制**：CAN_receive.c 仅改一处宏引用；shoot.c 仅重排摩擦轮相关代码，状态机/拨弹/堵转反转/热量逻辑不动；观测变量定义与 Live Watch 用法不变

### 性能与可靠性
- 控制周期仍为 1ms，每周期一次 0x200 帧发送 + 两次 PID，负载可忽略
- 循环展开后与原逐字段访问性能等价（-O 优化下无差异）
- 转向符号从散落宏调用点收敛到结构体字段，镜像安装适配只改初始化一处

### Directory Structure Summary
```
application/
├── CAN_receive.h   # [MODIFY] FRIC_CAN 由 hcan2 改为 hcan1
├── shoot.h         # [MODIFY] 新增 fric_motor_t 定义；shoot_control_t 摩擦轮散字段替换为 fric_motor[2]+fric_speed_ramp
└── shoot.c         # [MODIFY] init 循环初始化（含 target_sign）；feedback/loop 改索引访问；观测变量更新处同步改字段路径
```
