---
name: remove-fric-speed-field
overview: 删除 fric_motor_t 中冗余的 speed 字段，PID 计算直接读 measure->speed_rpm（用户已确认）。
todos:
  - id: remove-speed-field
    content: shoot.h 删除 fric_motor_t 的 speed 字段，shoot.c 三处引用改为直接读 measure->speed_rpm
    status: completed
  - id: build-verify
    content: 编译验证（cmake --build . --preset Debug）并确认 lint 零错误，汇报改动点与 Live Watch 说明
    status: completed
---


## 产品概述
清理 fric_motor_t 结构体中的冗余字段：删除 `speed`（与 `measure->speed_rpm` 完全重复的转手拷贝），PID 计算和观测变量改为直接读取 `measure->speed_rpm`；保留 `current`（输出指令，与反馈 `given_current` 数据流向不同，必须独立存放）。功能行为完全不变，纯代码结构清理。

## 核心功能
- shoot.h：fric_motor_t 删除 `fp32 speed` 字段，保留 speed_pid / measure / target_sign / current 四字段
- shoot.c：shoot_feedback_update 中删除 speed 拷贝语句；shoot_control_loop 中 PID_calc 反馈参数改为直接读 `(fp32)fric_motor[i].measure->speed_rpm`
- 观测变量 fric_watch_speed1/2 改为直接读 `fric_motor[i].measure->speed_rpm`，Live Watch 用法不变
- 编译验证，确认行为等价（仅 FLASH/RAM 占用微变）



## Tech Stack
- 现有工程：STM32F407 + HAL + FreeRTOS（CMSIS-OS v1）+ CMake（arm-none-eabi-gcc，preset 名 Debug），工程名 standard_robot
- 构建命令：`cmake --build . --preset Debug`（PATH 中 C:\ST\STM32CubeCLT_1.18.0 的 cmake）

## Implementation Approach
### 实现思路
1. **shoot.h**：`fric_motor_t` 删除 `fp32 speed` 字段（约 shoot.h:120 区域，定义处注释"转速反馈 rpm"一并删除）
2. **shoot.c** 三处引用同步修改：
   - `shoot_feedback_update`：删除 `shoot_control.fric_motor[i].speed = ...->speed_rpm` 两行拷贝（摩擦轮转速反馈直接从 measure 指针读取，数据仍由 CAN RX 中断统一填充 motor_chassis[]，无第二份数据）
   - `shoot_control_loop` 的 PID 计算循环：反馈参数 `shoot_control.fric_motor[i].target_sign * shoot_control.fric_motor[i].speed` 改为 `shoot_control.fric_motor[i].target_sign * (fp32)shoot_control.fric_motor[i].measure->speed_rpm`（保持符号宏乘法语义不变）
   - 观测变量更新处：`fric_watch_speed1/2 = (int16_t)shoot_control.fric_motor[i].measure->speed_rpm`
3. **性能与可靠性**：指针间接寻址替代数组拷贝，-O 优化下无差异；数据源唯一（CAN RX 中断写 motor_chassis[]），消除同步隐患；Live Watch 观测路径不变
4. **爆炸半径控制**：不触碰 PID 输出逻辑、CAN_cmd_fric 发送、状态机、拨弹/堵转逻辑；current 字段保留（输出指令与 given_current 反馈数据流向相反，不可合并）

