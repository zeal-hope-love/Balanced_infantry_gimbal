---
name: fric-wheel-can1-test
overview: 新增上电自动的开环电流缓坡测试任务，驱动 CAN1 上 ID1/ID2 两颗 M3508 摩擦轮，并说明用户如何自行禁用 chassis_task 及其原理。
todos:
  - id: create-fric-test-task
    content: 新建 application/fric_test_task.c/.h：实现 WAIT_INIT→RAMP_UP→HOLD→RAMP_DOWN→FINISH 自动状态机，复用 CAN_cmd_chassis 发送电流，读取 get_chassis_motor_measure_point(0/1) 转速反馈，反馈丢失立即置零，结束后持续发零电流帧
    status: completed
  - id: register-task
    content: 在 Src/freertos.c 中按现有 osThreadDef/osThreadCreate 模式注册 fric_test 任务（优先级 osPriorityNormal，栈 128）
    status: completed
  - id: oled-display
    content: 扩展 application/oled_task.c：增加显示两个摩擦轮实时转速与当前测试阶段
    status: completed
  - id: chassis-disable-guide
    content: 编写 chassis_task 禁用指引（改法+原理+预期现象说明，含 0x203/0x204 离线蜂鸣属正常的提示）
    status: completed
  - id: build-verify
    content: 使用绝对路径 cmake.exe --build --preset Debug 编译验证无错误，确认宏参数默认值安全（目标电流≤3000、斜率≤100/10ms）
    status: completed
---

## 产品概述
为现有 RoboMaster 标准云台工程新增一个“上电自动摩擦轮测试”功能：两个 M3508+C620 摩擦轮接在 CAN1，电调 ID 分别为 1（0x201）、2（0x202），上电后自动执行“电流缓升 → 稳速 → 缓降 → 停止”的开环测试流程，全程通过转速反馈验证硬件与链路。

## 核心功能
- 上电自动开始测试（延后数秒启动，避免初始化阶段误转），无需任何操作
- 开环电流缓坡：电流从 0 缓慢爬升至安全目标值（约 2000~3000，远低于 16384 上限），保持数秒后缓降回 0
- 实时读取 0x201/0x202 的 speed_rpm 反馈验证电机在线与转动方向；反馈丢失立即置零电流
- 测试结束后持续发送零电流帧（C620 无超时停转机制，必须持续发零保证安全）
- 无 OLED：观察方式改为调试器 Live Watch（用户自行操作，我提供监视变量指引）
- 附带 chassis_task 禁用操作指引：用户自行修改，需说明“怎么改、为什么这样就 OK”
- 预期现象说明：未接的 0x203/0x204 电机会触发 detect_task 离线告警蜂鸣，属正常现象

## 技术栈
- 现有工程：STM32F407 + HAL + FreeRTOS（CMSIS-OS v1）+ CMake（arm-none-eabi-gcc，preset 名 Debug）
- 完全复用现有 CAN 基础设施，不引入新依赖

## 实现思路
### 数据链路（全部复用，零改动）
- 发送：`CAN_cmd_chassis(fric1_current, fric2_current, 0, 0)` —— 已实现，发 0x200 帧到 hcan1（CHASSIS_CAN），前两字节即 ID1/ID2 电机电流
- 接收：`HAL_CAN_RxFifo0MsgPendingCallback` 已解析 0x201/0x202，自动填充 `motor_chassis[0/1]` 并调用 `detect_hook(CHASSIS_MOTOR1_TOE+i)`
- 反馈读取：`get_chassis_motor_measure_point(0)` / `get_chassis_motor_measure_point(1)`

### 新增测试任务（application/fric_test_task.c/.h）
独立任务文件，不侵入现有业务逻辑（云台发 hcan2 的 0x1FF，与 CAN1 无冲突）：
- 状态机：`WAIT_INIT(延时5s) → RAMP_UP → HOLD → RAMP_DOWN → FINISH(持续发零)`
- 缓升斜率：每 10ms 增加约 50~100（约 5s 到达目标），给用户充足观察/急停时间
- 安全保护：任一电机转速反馈持续缺失（复用 detect_task 在线状态或直接判 speed_rpm 长期不变）立即进入 FINISH 置零；电流上限宏约束
- 两轮施加同号电流，通过反馈转速符号判断安装方向是否对转
- 暴露全局观测变量 `fric_test_current`、`fric_test_state`（volatile，供 Live Watch）

### 观察方式（无 OLED，调试器 Live Watch，用户自行操作）
调试进入后 Add Live Watch（或 Expressions 窗口）添加：
- `fric_test_current` —— 当前输出电流指令
- `fric_test_state` —— 当前状态机阶段
- `get_chassis_motor_measure_point(0)->speed_rpm` / `(1)->speed_rpm` —— 两轮实时转速
- Periodic Refresh 打开（View → Periodic Window Update）即可实时刷新

### chassis_task 冲突（用户自行处理，只提供指引）
- 改法：`Src/freertos.c` 中注释掉 chassis_task 的创建（约 155-156 行），或在 `chassis_task.c:177-183` 条件编译跳过 `CAN_cmd_chassis` 调用
- 为什么 OK：摩擦轮与底盘电机共享 0x200 帧，若 chassis_task 照常运行，其底盘 PID 会把摩擦轮当作 1/2 号底盘电机驱动（速度环看到的是摩擦轮转速，输出失控电流，可能直接满电流堵转）；禁用后 CAN1 的 0x200 帧只由测试任务发出，链路干净无干扰
- 测试期间遥控器不操作即可（gimbal_task 云台输出走 CAN2，不影响）

## 关键接口
```c
// fric_test_task.h 对外接口（遵循现有 RM 工程任务文件模式）
extern void fric_test_task(void const *argument);
// 内部参数宏：FRIC_TEST_TARGET_CURRENT / FRIC_TEST_RAMP_STEP /
// FRIC_TEST_HOLD_TIME / FRIC_TEST_TASK_DELAY，集中定义便于调整
// 观测变量：volatile int16_t fric_test_current; volatile uint8_t fric_test_state;
```
