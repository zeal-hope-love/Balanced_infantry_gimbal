---
name: yaw-slip-ring-support
overview: 支持电滑环 yaw：新增 YAW_SLIP_RING 宏，校准时 yaw 限位数据无效化为 ±π、陀螺仪模式 yaw 跳过软限位钳位、INIT 回中时 yaw 不转（offset 取上电/进 INIT 时的当前位置）。pitch 校准逻辑不变。
todos:
  - id: yaw-slipring-macro
    content: gimbal_task.h 新增 YAW_SLIP_RING 宏（=1，电滑环 yaw 无限位）并注释回退说明
    status: completed
  - id: yaw-slipring-cali
    content: gimbal_task.c set_cali_gimbal_hook 条件编译：滑环时 yaw offset 写当前ecd、限位写±π
    status: completed
  - id: yaw-slipring-transit
    content: gimbal_task.c transit 的 yaw ENCONDE 沿滑环时重设 offset_ecd=当前ecd（INIT 回中失效化为原地不动）
    status: completed
  - id: yaw-slipring-limit
    content: gimbal_absolute_angle_limit 函数顶部滑环时 yaw 跳过钳位直接累加 add
    status: completed
  - id: yaw-slipring-build
    content: cmake --build --preset Debug 编译验证 + read_lints 无告警
    status: completed
  - id: yaw-slipring-memory
    content: 更新 MEMORY.md 记录电滑环 yaw 适配方案与验证步骤
    status: completed
---

## 需求分析
用户云台 yaw 采用**电滑环**（可无限旋转、无线缆堵转），导致标准 DJI 云台校准对 yaw 完全失效，需要适配：

1. **校准数据失效**：校准 yaw 步骤靠堵转记录 max/min 编码器值，滑环下永不堵转 → 每步空转 2 秒后记录随机多圈位置 → `offset_ecd=(max+min)/2` 与 `max/min_relative_angle` 全是随机数 → 软限位把 yaw 钳在随机扇区、INIT 时 yaw 乱转到随机朝向。
2. **滑环 yaw 本就不该有限位**：相对角限位公式在 yaw 跨 ±π 环绕时天然误判（relative 接近 π 时合法目标会被误钳），滑环 yaw 应无相对角限位、无回中动作。
3. pitch 校准数据有效，必须保持原逻辑不受影响。

## 方案要点（基于代码核实）
- 限位函数实为 `gimbal_absolute_angle_limit`（gimbal_task.c:882，gyro 模式），yaw 调用点 :847；
- 校准数据最终写入点为 `set_cali_gimbal_hook`（:401，校准完成后最后执行、最终生效）；
- INIT 回中走 ENCONDE 模式，`gimbal_mode_change_control_transit`（:780）进 ENCONDE 沿（:795-798）设 `relative_angle_set = relative_angle`——在此沿重设 yaw `offset_ecd=当前ecd` 可使 relative 恒 0，回中目标=当前位置，yaw 不动。


## 技术方案
### 修改点（3 个文件位置，1 个新宏）
1. **gimbal_task.h**：新增 `#define YAW_SLIP_RING 1`（电滑环 yaw 无限位开关，0 可回退标准线缆限位行为）。
2. **gimbal_task.c `set_cali_gimbal_hook`(:401)**：`#if YAW_SLIP_RING` 时 yaw 三项改为无害值——`offset_ecd` 写当前实测 ecd（消除随机中值）、`max_relative_angle=+3.141593f`、`min=-3.141593f`（覆盖钳位区间为全域，等效不钳）；`#else` 原样。cmd_cali_gimbal_hook(:457) 的临时写入随后必被 set_cali 覆盖，无需改动。
3. **gimbal_task.c `gimbal_mode_change_control_transit`(:795-798)**：yaw 进 ENCONDE 沿，`#if YAW_SLIP_RING` 时先 `offset_ecd = gimbal_motor_measure->ecd` 再设 relative_angle_set（=0）→ INIT 回中目标=当前位置，yaw 静止；pitch 分支照常。
4. **gimbal_task.c `gimbal_absolute_angle_limit`(:882)**：函数顶部 `#if YAW_SLIP_RING` 判断 `gimbal_motor == &gimbal_control.gimbal_yaw_motor`（文件级 static 全局可直接访问）则直接 `absolute_angle_set = rad_format(absolute_angle_set + add)` 并 return，跳过钳位；pitch 照常钳位。
5. `gimbal_relative_angle_limit`(:925) 无需改：max/min=±π 下钳位自然失效。

### 设计权衡
- **宏开关而非硬编码**：`YAW_SLIP_RING 0` 一键回退标准行为，pitch 逻辑零风险；
- **重设 offset 放切换沿**：transit 本身只在模式切换沿调用，天然防重复执行破坏运行期 relative 计算；
- **GYRO 模式不受影响**：yaw 绝对角控制不依赖 offset_ecd，motor_gyro 补偿公式只依赖 pitch relative_angle，均与 yaw offset 无关。

