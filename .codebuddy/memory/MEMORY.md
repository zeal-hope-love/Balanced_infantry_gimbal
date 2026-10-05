# gimbal_20.standard_robot_vscode 长期记忆

## 构建环境（2026-10-04 更新，覆盖旧记录）
- cmake：`C:/ST/STM32CubeCLT_1.18.0/CMake/bin/cmake.exe`（旧 CLion 路径已失效）；构建命令 `cmake --build --preset Debug`（**target SBV 已不存在**，目标名 standard_robot）。
- arm-none-eabi-gcc：`C:/Users/PoXia/AppData/Local/stm32cube/bundles/gnu-tools-for-stm32/14.3.1+st.2/bin/`（stm32cube bundles 管理，cmake 自动找，无需手动指定）。
- PowerShell：调 exe 用 `& "路径"`；无 tail，用 `Select-Object -Last N`。

## VOFA+ 调参模块（2026-10-04 落地，2026-10-05 调整为摩擦轮调参模式）
- 模块 `application/vofa_task.c/h`，对外 API 仅 vofa_reg_param / vofa_reg_channel / vofa_reg_pid（VOFA_PID_STD=pid_type_def / VOFA_PID_GIMBAL=gimbal_PID_t）。
- 传输 USART1 蓝牙 115200（大夏龙雀 DX-BT36/37 透传双模块 AT+BIND 配对，接外壳丝印 UART2 的 4-pin 座 = STM32 USART1 PA9/PB7）；RX DMA Circular+空闲中断（CubeMX 由用户配，代码只用 HAL_UARTEx_ReceiveToIdle_DMA + HAL_UARTEx_RxEventCallback，重生成不覆盖）；TX 复用 usart1_tx_dma_enable 忙检查丢帧。
- 命令：`set <name> <val>` / `get <name>` / `list`（\n 结尾，VOFA 文本模式发，Hex 不勾，%%占位符配滑条），指针直写立即生效；浮点转串手写 vofa_ftoa（newlib-nano 无 %f）。
- 当前绑定（用户"调哪个输出哪个"原则，按需增删）：**yaw 串级调参**——ch0 yaw_set / ch1 yaw_get / ch2 yaw_gyro_set(角度环输出=速度环目标) / ch3 yaw_gyro / ch4 yaw_out；参数 yaw_ang_kp/ki/kd/maxout/maxiout（角度环 gimbal_PID_t absolute_angle_pid，当前 26/0/0.3）+ yaw_spd_kp/ki/kd/maxout/maxiout（速度环 pid_type_def gyro_pid，当前 3600/20/0）。激励源=左摇杆水平 ch2；yaw 角度 ±π 环绕，ch0/ch1 在 ±3.14 跳变属正常。pitch 串级注册块注释保留（pitch 定稿 15/0/0 + 2900/60/0），fric0/遥控器通道同样注释保留。级联整定口诀：内环看 ch3 追 ch2（调 spd），外环看 ch1 追 ch0（调 ang）。
- **摩擦轮 PID 已定稿**（2026-10-05 VOFA 整定）：Kp=12、Ki=0.5、Kd=0，max_out=10000/max_iout=5000，已写回 shoot.h 宏。调试模式 FRIC_POWERON_AUTO 已关（=0），全部调试代码（恒置 READY_FRIC/摇杆满映射±9000rpm/单轮驱动 ID1/FRIC_DEBUG_SPEED_MAX）保留在 #if 内随时可恢复。恢复正常遥控射击：右拨杆(SHOOT_RC_MODE_CHANNEL=1) 上=开摩擦轮(斜坡4500)/下=发弹(单发→保持连发)/中=键盘鼠标模式(Q/E开关轮,鼠标发弹长按连发)；发弹机构=拨弹轮(trigger M2006,角度环单发转0.314rad)+弹仓微动开关(BUTTEN_TRIG_PIN)做待发检测；热量/云台无力保护恢复生效。C620 speed_rpm 是转子转速，M3508 极限≈9254rpm。
- shoot.c:71 shoot_control 非 static 全局，vofa_task.c extern 直绑，shoot.c 零接口改动。
- usb_task 已禁用；remote_control.c 有 SBUS_FORWARD_TO_USART1 宏（默认0）释放 TX。
- 用户偏好：摇杆做激励源直控目标（不要 VOFA override 接管、暂不用斜坡）；"调哪个输出哪个"通道按需精简；参数断电不保存，调好写回宏。

## 用户协作偏好（gimbal_20 工程）
- 调试阶段用户用调试器 Live Watch 观察变量，交付时需给出明确的观测路径（看哪个结构体哪个字段、正常值、异常含义）。
- 用户反感冗余调试变量和未经要求的"保护逻辑"：观测一律走现有结构体本体（如 shoot_control），加保护/观测/新功能前必须先征得同意。
- 改动前先捋清并讲清现有流程（对照工程内已验证的同构实现，如云台 yaw/pitch 电机链路），经用户确认后再动代码。

## gimbal_20 yaw 电滑环适配（2026-10-05）
- yaw 装电滑环可无限旋转：gimbal_task.h 新增 YAW_SLIP_RING=1 + YAW_OFFSET_ECD=5055（用户实测指定的"正前方"编码器中值，初版 4180 已更正）。
- 三处配合：gimbal_init 启动即写入固定 offset/±π 限位；set_cali_gimbal_hook 滑环时不采用校准 yaw 数据（滑环无堵转，校准 yaw 记录的是随机多圈值）；gimbal_absolute_angle_limit 滑环时 yaw 跳过相对角钳位（±π 环绕处该公式会误判卡住），pitch 照常钳位。
- 上电 INIT：pitch 正常回中，yaw 转向 4180 朝向（车头对齐）。YAW_SLIP_RING 改 0 可回退标准线缆限位逻辑。
- 云台校准只需做一次（pitch 数据已固化 flash）；换全片擦除才需重做。

## gimbal_20 IMU 安装矩阵（2026-10-05 实测修正）
- 用户板 BMI088 贴装与官方 C 板假设差 90°：实测 accel[2]=+9.8（芯片Z=竖直向上）、左转 gyro[2]=+、抬头 gyro[1]=−、roll=gyro[0]。芯片系 X=前,Y=左,Z=上（右手系）。
- INS_task.c:43 BMI088_BOARD_INSTALL_SPIN_MATRIX 已改为 {-1,0,0; 0,-1,0; 0,0,1}（det=+1）：INS_gyro[1]=−chipY（pitch，抬头为正）、INS_gyro[2]=+chipZ（yaw，左转为正）；accel[2] 保持 +9.8。gyro 与 accel 共用此矩阵。
- 矩阵改动后 flash 里的陀螺仪零漂校准数据失效，必须重做陀螺仪校准（双拨杆下档+双摇杆摆 ./\. 保持2s）。
- 用户遥控器实机映射（2026-10-05 用户最终纠正）：云台 = **左摇杆**（ch2 水平=yaw，ch3 垂直=pitch，DJI 默认约定）；云台模式拨杆=s[0]；射击拨杆=s[1]（上开轮/下发弹/中键盘鼠标）；摩擦轮调试时激励曾用右摇杆垂直 ch1。PITCH_RC_SEN 已翻正为 +0.000006f（IMU 轴修正后约定：推杆向上=抬头）；鼠标 PITCH_MOUSE_SEN +0.00015f 方向一致。pitch PID 定稿：角度环 15/0/0（max_out 10）、速度环 2900/60/0（max_out 30000/max_iout 10000），均在推荐区间；可选项：角度环 Kd 0.3~0.5（底盘晃动大出现过冲再加）。

## gimbal_20 摩擦轮现状（2026-10-04）
- 摩擦轮 = 2×M3508+C620，CAN1，0x200 帧（CAN_cmd_fric），电调 ID 1/2（0x201/0x202），反馈进 motor_chassis[0]/[1]；云台三电机也在 CAN1（0x1FF，0x205/0x206/0x207）；chassis_task 未创建（freertos.c 注释掉）。
- 控制链路（shoot.c）：fric_motor[i].measure 指针 → 单级速度环 PID（KP12/KI0.1/KD0，MAX_OUT 10000）→ target_sign（FRIC1=+1/FRIC2=-1）适配镜像安装 → CAN_cmd_fric；目标 fric_speed_set 由斜坡 0→4500rpm（5rpm/ms）。
- FRIC_POWERON_AUTO：2026-10-05 用户要求关闭，已改为 0（shoot.h:36）——上电自动升轮调试功能编译排除，恢复遥控器 SHOOT_RC_MODE_CHANNEL 上拨开轮/再拨关，云台无力停轮保护(gimbal_cmd_to_shoot_stop)恢复生效；需要调试时改回 1。
- 摩擦轮转速异常已解决（2026-10-05 用户确认）：此前现象为一号轮 <100rpm、二号轮 ~9000rpm 失控。
- PID 参数都在 shoot.h 宏；PID 初始化在 shoot_init/gimbal_init，上电即执行。
