---
name: vofa-fric-only-channels
overview: 删除 VOFA 遥控器 7 个监看通道（用户用调试器看即可），只保留摩擦轮 4 条曲线：ch0 fric0_target / ch1 fric0_set / ch2 fric0_get / ch3 fric0_out；帧长 20 字节，蓝牙零压力。
todos:
  - id: vofa-remove-rc-channels
    content: 注释 vofa_bind 中遥控器 7 行通道注册与任务循环镜像刷新调用，更新通道表注释（ch0~ch3 = fric0 四通道）
    status: completed
  - id: vofa-build-verify
    content: 用 cmake --build --preset Debug 编译验证并 read_lints 确认无告警，更新记忆文件
    status: completed
---

## 需求概述
用户发现 VOFA 波形里不需要遥控器通道值——调试时直接用 IDE 监视器（Live Watch）看遥控数据即可。要求把 VOFA 输出里的遥控器 7 个通道（rc_ch0~ch4、rc_s0、rc_s1）也去掉，只保留 0 号摩擦轮的 4 条曲线。

## 改动后波形（共 4 通道）
| 通道 | 内容 |
|---|---|
| ch0 | fric0_target（目标转速，右摇杆映射值） |
| ch1 | fric0_set（乘过转向符号的 PID 输入） |
| ch2 | fric0_get（实际转速 rpm） |
| ch3 | fric0_out（PID 输出电流） |

可调参数不变：fric0_kp / fric0_ki / fric0_kd / fric0_maxout / fric0_maxiout。

## 技术方案
- 仅改 `application/vofa_task.c`，其余文件不动：
  1. `vofa_bind()`：注释遥控器 7 行 `vofa_reg_channel` 注册（注释保留，日后需要时恢复），摩擦轮 4 通道注册不变，通道号自动变为 ch0~ch3；
  2. `vofa_task()` 任务循环：注释 `vofa_rc_mirror_update()` 调用（镜像数组与函数本体注释保留，共约 40B 开销，一并停掉更干净）；
  3. 更新文件头与 vofa_bind 注释中的通道顺序表；
  4. 删除 `remote_control.h` 依赖注释？保留（镜像函数仍引用 RC 结构体，注释状态不参与编译，无需动 include）。
- 编译验证：`cmake --build --preset Debug`（STM32CubeCLT 路径），read_lints 确认无告警。
- 收益：帧长 48B→20B，50Hz 下仅 1KB/s，BLE 蓝牙带宽余量充足，波形更稳。
