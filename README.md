# RoboMaster 云台开发板工程（STM32F407 + CMake）

基于 RoboMaster 官方 `standard_robot` 框架的 C 型开发板（STM32F407IGT6）云台工程。
已移除 Keil MDK 依赖，纯 CMake + GCC 构建；在此基础上完成了大量本地化改造：
摩擦轮迁移至 CAN 控制、yaw 电滑环适配、IMU 安装矩阵实测修正、内置 **VOFA+ 蓝牙在线调参系统**。

## 硬件配置速查

### 电机分配（全部 CAN1）

| 电机 | 型号/电调 | 电调 ID | 反馈 ID | 控制帧 | 备注 |
|------|----------|--------|---------|--------|------|
| 云台 yaw | GM6020 | 1 | 0x205 | 0x1FF 槽1（电压） | **电滑环**，可无限旋转 |
| 云台 pitch | GM6020 | 2 | 0x206 | 0x1FF 槽2（电压） | |
| 摩擦轮 0 | M3508+C620 | 1 | 0x201 | 0x200 槽1（电流） | |
| 摩擦轮 1 | M3508+C620 | 2 | 0x202 | 0x200 槽2（电流） | |
| 拨弹轮 | M2006+C610 | — | —（0x207 保留） | 0x1FF 槽3 | 暂未接 |

> ⚠️ GM6020 反馈 = `0x204 + ID`，C620/C610 反馈 = `0x200 + ID`，两套编号空间独立。
> 改电调拨码前先对照此表确认反馈 ID 不冲突（如 C620 ID5 的 0x205 会撞 GM6020 ID1）。

### 串口分配

| 串口 | 用途 | 备注 |
|------|------|------|
| USART1（PA9/PB7，外壳丝印 **UART2** 的 4-pin 座） | **VOFA+ 蓝牙调参**（115200） | 大夏龙雀 DX-BT36/37 透传 |
| USART3（100000 波特率 8E1） | DBUS 遥控接收机 | 专用，勿动 |
| USART6 | 裁判系统 | RX 在用，TX 空闲 |
| USB | 已禁用 | usb_task 已注释 |

### 遥控器操作

| 操作 | 效果 |
|------|------|
| **左拨杆 s[0]** | 上=陀螺仪模式 / 中=编码器模式 / 下=零力（无力+禁止射击） |
| **右拨杆 s[1]** | 上=开摩擦轮（斜坡升 4500rpm）/ 下=发弹（第一下单发，保持连发）/ 中=键盘鼠标模式 |
| **左摇杆水平 ch2** | yaw 旋转 |
| **左摇杆垂直 ch3** | pitch 俯仰（**推杆向上 = 抬头**） |
| 键盘 Q / E | 开 / 关摩擦轮（射击拨杆中档时） |
| 鼠标点击 / 长按 | 单发 / 连发 |
| 键盘 F | 云台 180° 掉头 |

## 工程结构

```
├── application/          # 应用层任务
│   ├── gimbal_task.c     # 云台（串级：角度环+速度环，含电滑环 yaw 适配）
│   ├── shoot.c           # 发射（摩擦轮速度环+拨弹角度环，CAN1 0x200/0x1FF）
│   ├── vofa_task.c       # VOFA+ 在线调参模块（蓝牙波形+PID 在线整定）
│   ├── remote_control.c  # 遥控（含 SBUS_FORWARD_TO_USART1 宏开关）
│   └── ...               # 底盘(未启用)/裁判系/IMU/检测等
├── bsp/boards/           # 板级支持包（CAN/USART/SPI/I2C/PWM 驱动封装）
├── components/
│   ├── controller/pid.c  # 通用位置式 PID
│   ├── algorithm/        # AHRS.lib 预编译姿态解算库 + user_lib
│   └── devices/          # BMI088/IST8310/OLED 驱动
├── Drivers/  Middlewares/  Inc/  Src/    # HAL / FreeRTOS / CubeMX 生成代码
├── cmake/  CMakeLists.txt  CMakePresets.json   # 构建系统
└── standard_robot.ioc    # CubeMX 工程文件
```

## 构建与烧录

环境：`arm-none-eabi-gcc` + `ninja` + `cmake ≥3.22`（推荐装
[STM32CubeCLT](https://www.st.com/en/development-tools/stm32clt-stm32cubeclt.html) 一站式齐活）。

```bash
# 首次（或工程移动后）生成缓存
cmake --preset Debug

# 编译（产物 build/Debug/standard_robot.elf）
cmake --build --preset Debug

# 烧录
STM32_Programmer_CLI -c port=SWD -w build/Debug/standard_robot.elf -v -rst
```

> ⚠️ 工程目录移动/改名后旧 `build/` 缓存会报路径错误——删掉 `build/` 重新 preset 即可。

## VOFA+ 在线调参指南

通过 USART1 蓝牙串口实现「波形监看 + PID 在线整定」，改参数不用重新烧录。

### 硬件与连接

1. 两块大夏龙雀蓝牙模块（DX-BT36/37）：一块接板上 4-pin 座（**外壳丝印 UART2**，即
   USART1：1=RXD 2=TXD 3=GND 4=5V，与模块交叉接），另一块接 USB-TTL 插电脑；
2. 两模块各执行 AT 配置（串口助手，`\r\n` 结尾）：`AT` → `AT+BAUD7`（115200）→
   `AT+BLUFISEND=1`（透传）→ `AT+INQ`/`AT+CONN1`/`AT+BIND<对端MAC>` 完成绑定；
3. VOFA+ 选 USB-TTL 的 COM 口，115200，协议 **JustFloat**。

### 波形通道（当前绑定 yaw 串级）

| 通道 | 内容 | 说明 |
|------|------|------|
| ch0 | yaw_set 角度目标 | rad，±π 环绕处跳变属正常 |
| ch1 | yaw_get 实际角度 | rad |
| ch2 | yaw_gyro_set 角度环输出（=速度环目标） | rad/s |
| ch3 | yaw_gyro 实际角速度 | rad/s |
| ch4 | yaw_out 速度环输出 | 电压 ±30000 |

### 在线改参数（命令，`\n` 结尾，Hex 不勾）

```
set yaw_ang_kp %.1f\n     角度环 Kp（当前 26）
set yaw_ang_kd %.2f\n     角度环 Kd（当前 0.3）
set yaw_spd_kp %.0f\n     速度环 Kp（当前 3600）
set yaw_spd_ki %.1f\n     速度环 Ki（当前 20）
get <参数名>              读当前值
list                      列出全部可调参数
```

滑条/命令按钮的 `%f` 占位符小数位数必须 ≥ 滑条步进的小数位数，否则数值被截断。

### 切换调参对象

`vofa_task.c` 的 `vofa_bind()` 内按注释切换：yaw（当前）/ pitch（定稿
15/0/0 + 2900/60/0）/ 摩擦轮（定稿 12/0.5/0）/ 遥控器通道，均为注释块，换注释重编译即可。

## 当前 PID 定稿值

| 电机 | 角度环 Kp/Ki/Kd | 速度环 Kp/Ki/Kd | 限幅 |
|------|----------------|----------------|------|
| pitch | 15 / 0 / 0 | 2900 / 60 / 0 | 角度环 out≤10；速度环 30000/10000 |
| yaw | 26 / 0 / 0.3 | 3600 / 20 / 0 | 角度环 out≤10；速度环 30000/5000 |
| 摩擦轮 | —（单级速度环） | 12 / 0.5 / 0 | 10000 / 5000 |

## 功能开关（`#define`，改后需重编译）

| 宏 | 位置 | 默认 | 说明 |
|----|------|------|------|
| `YAW_SLIP_RING` | gimbal_task.h | 1 | yaw 电滑环：无软限位、INIT 不回转、校准 yaw 数据无效化；改 0 恢复线缆限位逻辑 |
| `YAW_OFFSET_ECD` | gimbal_task.h | 5055 | 滑环 yaw 的"正前方"编码器中值，上电 INIT 转向该朝向 |
| `FRIC_POWERON_AUTO` | shoot.h | 0 | 1=调试模式（上电自动升轮、右摇杆直控转速、只跑摩擦轮环）；0=正常遥控射击 |
| `SBUS_FORWARD_TO_USART1` | remote_control.c | 0 | 1=把遥控帧从 USART1 转发（接图传用）；0=USART1 让给 VOFA |
| `GIMBAL_TEST_MODE` | gimbal_task.h | 0 | 云台测试模式 |

## 重要注意

- **IMU 安装矩阵已实测修正**（`INS_task.c` 的 `BMI088_BOARD_INSTALL_SPIN_MATRIX`）：
  本板芯片系 X=前/Y=左/Z=上，与官方 C 板贴装差 90°。**换板或重贴 IMU 后需重新做三轴实测并修正矩阵**，
  否则姿态漂移、轴错位；
- **陀螺仪校准**：双拨杆下档 + 双摇杆摆 `./\.` 保持 2 秒（上电 20 秒窗口内）；
- **云台校准**（编码器中值+限位）：双拨杆下档 + 双摇杆摆 `\../` 2 秒 + `\/` 2 秒。
  滑环 yaw 的校准数据会被 `YAW_SLIP_RING` 无效化（pitch 数据正常写入）；
- **改电调 ID / 重新生成 CubeMX**：CubeMX 重生成只覆盖 `Src/`、`Inc/`，用户代码须写在
  `USER CODE BEGIN/END` 内；VOFA 模块（vofa_task.c）只用标准 HAL API，重生成不影响。

## 已知警告（可忽略）

```
warning: AHRS.lib(ahrs.o) uses 2-byte wchar_t yet the output is to use 4-byte wchar_t
```

预编译库 `components/algorithm/AHRS.lib` 的固有属性，无实际影响，请勿尝试"修复"。

## 维护说明

- **新增/删除源文件**：编辑根 `CMakeLists.txt` 的 `target_sources` 段
- **新增头文件目录**：`target_include_directories` 段
- **修改外设配置**：用 STM32CubeMX 打开 `standard_robot.ioc` 重新生成（USART1 RX DMA
  Circular + NVIC 已配置供 VOFA 使用，重生成不会影响 vofa_task.c）
