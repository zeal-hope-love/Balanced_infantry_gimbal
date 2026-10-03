# STM32F407 RoboMaster 工程模板（纯 CMake 构建）

基于 RoboMaster 官方 `standard_robot` 框架的 STM32F407IGT6 工程模板，已移除 Keil MDK 依赖，仅使用 CMake + GCC 构建。

## 工程结构

```
├── application/          # 应用层任务（云台/底盘/发射/裁判系/IMU 等）
├── bsp/boards/           # 板级支持包（CAN/USART/SPI/I2C/PWM 驱动封装）
├── components/           # 组件（PID/姿态解算/BMI088/IST8310/OLED/算法库）
│   └── algorithm/AHRS.lib  # 预编译姿态解算库（GNU ARM EABI hard-float）
├── Drivers/              # STM32F4 HAL 库
├── Middlewares/          # FreeRTOS + USB Device 协议栈
├── Inc/  Src/            # CubeMX 生成的外设初始化代码
├── cmake/                # 工具链文件 + CubeMX 生成的 CMake 子工程
├── CMakeLists.txt        # 工程主构建脚本（源文件/头文件/宏定义在此维护）
├── CMakePresets.json     # Debug / Release 预设（Ninja 生成器）
├── STM32F407XX_FLASH.ld  # GCC 链接脚本
├── startup_stm32f407xx.s # GCC 版启动文件
└── standard_robot.ioc    # CubeMX 工程文件（改外设配置后重新生成代码用）
```

## 环境依赖

构建只需要三个**通用工具**在 `PATH` 中，不绑定任何安装方式：

| 工具 | 版本要求 | 获取方式（任选） |
|------|---------|-----------------|
| `arm-none-eabi-gcc` | ≥ 10（推荐 13.x） | ① STM32CubeCLT 一站式装齐（见下）② ARM 官方 [gnu-toolchain](https://developer.arm.com/downloads/-/gnu-rm) ③ xPack ④ Linux: `sudo apt install gcc-arm-none-eabi` |
| `ninja` | 任意近期版本 | ① 随 STM32CubeCLT ② [ninja 官方发布页](https://github.com/ninja-build/ninja/releases) ③ VS Code CMake Tools 自带 |
| `cmake` | ≥ 3.22（preset v3 要求） | ① 随 STM32CubeCLT ② [cmake.org](https://cmake.org/download/) |

**路线 A（最省事）：安装 [STM32CubeCLT](https://www.st.com/en/development-tools/stm32clt-stm32cubeclt.html)**，它一次性提供 CMake + Ninja + arm-none-eabi-gcc + ST-LINK/OpenOCD 烧录调试工具，安装时勾选"加入 PATH"即可。

**路线 B（不要 ST 全家桶）**：分别安装 ARM GCC、Ninja、CMake 并加入 PATH。

验证环境是否就绪：

```bash
arm-none-eabi-gcc --version
ninja --version
cmake --version
```

## 构建步骤

```bash
# 1. 生成构建缓存（首次或工程移动/改名后必须全新执行）
cmake --preset Debug

# 2. 编译
cmake --build --preset Debug

# 产物
build/Debug/standard_robot.elf
```

Release 版将 `Debug` 替换为 `Release` 即可。

> ⚠️ 工程目录移动、重命名或复制到其他位置后，旧的 `build/` 缓存会因绝对路径失效而报
> `source does not match ... used to generate cache`——删掉 `build/` 重新 `cmake --preset Debug` 即可。

## 烧录

编译产物为标准 ELF，可用任意工具烧录：

```bash
# STM32CubeProgrammer CLI（随 STM32CubeCLT）
STM32_Programmer_CLI -c port=SWD -w build/Debug/standard_robot.elf -v -rst

# 或 OpenOCD（随 STM32CubeCLT）
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg -c "program build/Debug/standard_robot.elf verify reset exit"
```

## 已知警告（可忽略）

链接时会输出：

```
warning: AHRS.lib(ahrs.o) uses 2-byte wchar_t yet the output is to use 4-byte wchar_t
```

这是预编译库 `components/algorithm/AHRS.lib` 的固有属性，工程代码未跨边界传递 `wchar_t`，无实际影响，所有环境下均会出现，请勿尝试"修复"。

## 维护说明

- **新增/删除源文件**：编辑根 `CMakeLists.txt` 的 `target_sources` 段
- **新增头文件目录**：`target_include_directories` 段
- **修改外设配置**：用 STM32CubeMX 打开 `standard_robot.ioc` 重新生成（注意：重新生成会覆盖 `Src/`、`Inc/` 和 `cmake/stm32cubemx/CMakeLists.txt`，用户代码写在 `USER CODE BEGIN/END` 区域内才安全）
