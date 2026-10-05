---
name: fric-direction-flip
overview: 两摩擦轮物理转向均反：将 FRIC1_TARGET_SIGN 由 +1.0f 改为 -1.0f，FRIC2_TARGET_SIGN 由 -1.0f 改为 1.0f（shoot.h），两行宏修改，编译验证。
todos:
  - id: flip-target-sign
    content: shoot.h 将 FRIC1_TARGET_SIGN 改 -1.0f、FRIC2_TARGET_SIGN 改 1.0f 并同步注释
    status: completed
  - id: build-verify
    content: read_lints 确认无错误并用绝对路径 cmake 构建 Debug 验证
    status: completed
---

## 产品概述
摩擦轮方向修正：Bug② 修复后两轮闭环已稳定收敛（轮1 ≈ +4500 rpm、轮2 ≈ -4500 rpm，仍对转），但实测物理转向均与期望相反，需将两轮目标方向各翻转一次。

## 核心功能
- 翻转 `FRIC1_TARGET_SIGN`：+1.0f → -1.0f（轮1 反转）
- 翻转 `FRIC2_TARGET_SIGN`：-1.0f → 1.0f（轮2 反转）
- 修改后轮1 收敛 ≈ -4500 rpm、轮2 收敛 ≈ +4500 rpm，两轮仍保持对转


## 技术方案
- 仅修改 `application/shoot.h` 两行宏值，注释同步微调。
- 安全性依据：上一轮修复已使 `target_sign` 仅作用于目标转速（反馈直接用 `speed_rpm`），翻转符号只改变目标方向，闭环极性不变，不会复发正反馈失控。
- 无其他文件、逻辑改动；PID 参数、斜坡、状态机均不动。

### 构建验证
使用绝对路径工具链：`C:\ST\STM32CubeCLT_1.18.0\CMake\bin\cmake.exe --build <工程根> --preset Debug`，read_lints 确认无错误。

