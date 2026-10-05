---
name: pitch-rc-sen-flip
overview: 翻转 PITCH_RC_SEN 符号（-0.000006f → +0.000006f），使左摇杆上推 = 枪口抬起；仅一行宏改动，IMU 轴修正后原负号已失去依据。
todos:
  - id: pitch-sen-flip
    content: 修改 gimbal_task.h:106 PITCH_RC_SEN 为 0.000006f 并注释翻正依据
    status: completed
  - id: pitch-sen-build
    content: cmake --build --preset Debug 编译验证 + read_lints 无告警
    status: completed
  - id: pitch-sen-memory
    content: 更新 MEMORY.md 记录"推杆向上=抬头"方向约定与 PID 对比结论
    status: completed
---

用户要求两件事：
1. 把 pitch 摇杆方向"改回去"：当前 `PITCH_RC_SEN = -0.000006f` 导致左摇杆上推（ch3=+660）→ 枪口向下，需翻正为 +0.000006f，使"推杆向上 = 抬头"（符合常见操作手习惯）。IMU 轴修正后 euler pitch 已是"抬头为正"，翻符号只影响摇杆目标方向，不影响已调好的闭环稳定性，PID 无需重调。yaw 的 `YAW_RC_SEN` 用户未反馈问题，不动。
2. 给出当前 PID 与推荐 PID 的对比（口头交付，无需改代码）：角度环 15/0/0 + 速度环 2900/60/0，对照 RM 串级经验区间，全部在推荐范围内，唯一可选项是角度环 Kd 0.3~0.5。

## Tech Stack
- 现有 STM32F407 + FreeRTOS 工程，单宏修改：`application/gimbal_task.h:106`。
- `#define PITCH_RC_SEN -0.000006f` → `0.000006f`，加注释记录修改依据（2026-10-05 IMU 轴修正后翻正，推杆向上=抬头）。
- 编译验证：`cmake --build --preset Debug`（STM32CubeCLT 路径），read_lints 确认无告警。
- 更新 `.codebuddy/memory/MEMORY.md` 记录方向约定变更。
## 影响范围
- 仅 1 个宏；闭环方向自洽性不受影响（angle loop 的 set-get 反馈链未动），PID 整定结果保持有效。
- 鼠标灵敏度 `PITCH_MOUSE_SEN`（+0.00015f，正值）方向与新约定一致，无需改。
