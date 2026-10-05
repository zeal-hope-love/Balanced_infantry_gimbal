---
name: fric-wheel-fix
overview: 诊断并修复摩擦轮一号轮速<100rpm、二号轮失控冲到9000rpm的问题：加反馈离线保护、超速失控保护、单轮调试开关，并给出电调ID/TARGET_SIGN验证步骤。
todos:
  - id: fric-watch-enhance
    content: 在 shoot.c/h 新增 fric_watch_pid_out/set、fric_fault_flag 等观测变量并每周期刷新，确认 PID 实际在算
    status: pending
  - id: fric-runaway-protect
    content: 在 shoot_control_loop 增加反馈缺失(toe_is_error)与正反馈失控(超速+输出饱和计时)保护，异常切断电流
    status: pending
  - id: sign-verify-doc
    content: 在 shoot.h 补充 TARGET_SIGN 验证流程注释，说明 9000rpm 失控=符号反向、fric_speed_set 斜坡为设计行为
    status: pending
  - id: build-verify
    content: 用绝对路径 cmake 构建 Debug 验证编译通过，并整理调试器观测确认步骤（PID全零原因排查）
    status: pending
---


## 用户问题诊断与摩擦轮控制修复

### 需求说明
用户正在调试双摩擦轮（M3508+C620，CAN1，电调ID 1/2），通过调试器监视 `shoot_control_t`（发射控制）结构体，发现四个现象，需要解释原因并修复：

1. **一号摩擦轮转速极慢（<100 rpm）**——疑似 0x201 电调无反馈（`fric_motor[0].measure->speed_rpm` 恒为 0，速度环输出饱和但反馈恒零）或电调 ID/接线不符。
2. **二号摩擦轮转速高达 9000 rpm（远超目标 4500）**——9000rpm ≈ M3508 转子空载极限（~9254rpm），判断为速度环正反馈失控：`FRIC2_TARGET_SIGN=-1` 与电机实际转向不匹配，或 0x202 反馈缺失导致 PID 输出恒饱和，电机飞转到机械极限。
3. **监视的 PID 字段全为零**——摩擦轮 `speed_pid` 每 1ms 都在计算不可能全零；可能是看的是 `trigger_motor_pid`（当前模式下输出本来≈0），或结构体改动后未重新编译烧录导致调试器符号/地址错位，需要通过新增观测变量交叉验证。
4. **`fric_speed_set` 一直在增长**——这是设计行为：目标转速斜坡 0→4500rpm，步进 5rpm/ms，约 0.9s 后恒定在 4500（SHOOT_STOP 时反向缓降到 0），并非异常。

### 修复目标
- 为摩擦轮速度环增加**失控保护**：反馈缺失检测与正反馈失控检测，异常时切断电流并置错误标志，避免电机持续飞转。
- 增强**调试观测变量**（PID 输出、反馈值、错误标志），使「PID 是否在算」「反馈是否有效」可在调试器中一眼确认。
- 明确 `FRIC1/FRIC2_TARGET_SIGN` 的验证流程，依据观测结果修正二号轮转向符号。
- 解答用户全部四个疑问。



## 技术方案

### 技术栈
- 现有工程：STM32F407 + FreeRTOS + HAL，C 语言，CMake 构建（preset Debug）。
- 完全复用现有模式，不引入新框架。

### 现有控制链路（已核实）
```mermaid
flowchart LR
    A[gimbal_task 1ms] --> B[shoot_control_loop]
    B --> C[shoot_set_mode 状态机<br/>FRIC_POWERON_AUTO=1 上电自动缓升]
    B --> D[shoot_feedback_update<br/>右键按下临时抬高斜坡上限5500]
    C --> E[ramp_calc 斜坡 0→4500rpm<br/>5rpm/ms 约0.9s]
    E --> F[fric_speed_set = ramp.out]
    F --> G[每轮速度环PID<br/>fdb=sign*speed_rpm, set=sign*fric_speed_set]
    G --> H[CAN_cmd_fric<br/>CAN1 0x200帧 slot0→ID1 slot1→ID2]
    H --> I[电调反馈0x201/0x202<br/>→motor_chassis[0]/[1]]
```

### 实现要点
1. **失控保护**（新增于 shoot.c）：
   - **反馈缺失检测**：复用 `detect_task` 的 `toe_is_error(CHASSIS_MOTOR1_TOE/CHASSIS_MOTOR2_TOE)`（CAN_receive.c:87 已有 detect_hook），反馈离线时该轮电流置 0，避免盲发饱和电流。
   - **正反馈失控检测**：若 `|speed_rpm|` 超过目标转速 1.5 倍且 PID 输出持续饱和（同号满幅）超过阈值时间（如 500ms），判定失控，切断该轮电流并置 `fric_fault_flag` 位。
   - 保护只针对摩擦轮，不影响拨弹电机逻辑，兼容 `FRIC_POWERON_AUTO` 状态机。
2. **观测增强**：沿用现有 `fric_watch_*`（volatile int16_t）模式，新增 `fric_watch_pid_out1/2`、`fric_watch_set1/2`、`fric_fault_flag`，全部每周期刷新，供调试器实时监视确认 PID 是否在算。
3. **符号验证流程**：在 shoot.h 注释中写明验证步骤——若某轮转速冲向 ~9000rpm（机械极限），说明该轮 `TARGET_SIGN` 与实际方向相反，翻转对应宏即可；结合 `fric_watch_speed` 符号判断。
4. **性能**：保护逻辑为每周期 O(1) 判断（比较+计数器），对 1kHz 控制环无影响；不改动 CAN 收发中断路径。

### 构建验证
使用本机绝对路径工具链构建：`C:\Users\PoXia\AppData\Local\Programs\CLion\bin\cmake\win\x64\bin\cmake.exe --build --preset Debug`，read_lints 确认无错误。

