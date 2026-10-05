---
name: fix-imu-mount-matrix
overview: 按用户实测修正 INS_task.c 的 BMI088 安装矩阵：yaw=芯片Z(左转为正)、pitch=-芯片Y(抬头为正)、roll=-芯片X，行列式+1；修复 ch3 轴错位与 ch1 漂移。改后需重新做陀螺仪校准并按步骤验证符号。
todos:
  - id: imu-matrix-fix
    content: 修改 INS_task.c:43-46 安装矩阵为 {−1,0,0; 0,−1,0; 0,0,1}，加注释记录实测依据
    status: completed
  - id: imu-matrix-build
    content: cmake --build --preset Debug 编译验证 + read_lints 无告警
    status: completed
  - id: imu-matrix-memory
    content: 更新 .codebuddy/memory/MEMORY.md 记录 IMU 贴装实测结论与新矩阵
    status: completed
---

## 产品概述
根据用户三轴实测数据修正 IMU 安装方向矩阵，使陀螺仪轴映射与实际贴装一致，修复 pitch 速度环反馈轴错位与 ch1 姿态漂移。

## 实测数据（用户已确认）
- accel[2] = +9.8~9.9（水平静止），accel[0]/[1] ≈ 0 → 芯片 Z 轴竖直向上
- 俯视逆时针（yaw 正向）→ gyro[2] = **+** → 芯片 Z = yaw 轴
- 抬头（pitch 正向）→ gyro[1] = **−** → 芯片 Y = 左向水平轴（pitch 轴，符号为负）
- 推论：芯片系 X=前、Y=左、Z=上（右手系自洽）

## 核心功能
- 修改 `BMI088_BOARD_INSTALL_SPIN_MATRIX`，使 INS_gyro/INS_accel 轴映射匹配实际贴装
- 效果：INS_gyro[1]（pitch，ch3 绑定）抬头为正；INS_gyro[2]（yaw）左转为正；accel[2] 保持 +9.8（AHRS 重力约定不变）
- 矩阵行列式 = +1（右手系约束），gyro 与 accel 共用同一矩阵保持刚体一致性


## 技术方案
### 唯一修改点：`application/INS_task.c:43-46`

当前（官方 C 板贴装假设，与用户板不符）：
```c
#define BMI088_BOARD_INSTALL_SPIN_MATRIX    \
    {0.0f, 1.0f, 0.0f},                     \
    {-1.0f, 0.0f, 0.0f},                    \
    {0.0f, 0.0f, 1.0f}
```

改为（按实测：芯片 X=前、Y=左、Z=上）：
```c
#define BMI088_BOARD_INSTALL_SPIN_MATRIX    \
    {-1.0f, 0.0f, 0.0f},                    \
    {0.0f, -1.0f, 0.0f},                    \
    {0.0f, 0.0f, 1.0f}
```

### 推导依据
- body 系约定（由代码控制方向反推）：pitch 抬头时 INS_gyro[1] 必须为正（角度环 set>get 输出正速度命令，负反馈要求实测同号）→ 抬头绕芯片 Y 为负旋转 → body_y = −chipY
- body_x 取 −chipX（后向轴）是为了配平行列式：det = (−1)(−1)(1) = +1，右手系成立；若只翻 Y 行 det=−1（非刚体旋转），AHRS 四元数解算会崩
- accel[2] = +chipZ = +9.8 保持不变，AHRS 重力符号约定不受影响
- 旁效验证：gimbal_task.c:739-740 的 yaw 垂直轴投影补偿公式 cos(θ)·gyro_z − sin(θ)·gyro_x 与新矩阵方向自洽

### 影响范围
- 仅 `INS_task.c` 一个宏；gimbal_task/shoot/vofa 零改动
- 轴映射变了 → 已存的陀螺仪零漂偏置（flash 校准数据）失效，烧录后必须重新做陀螺仪校准

### 验证步骤（交付用户）
1. 编译烧录 → 执行陀螺仪校准（双拨杆下档 + 双摇杆摆 `./\.` 保持 2s）清零漂
2. 左拨杆下档（无力）：手转 pitch，ch3 应抬头为正/低头为负；转 yaw 不再影响 ch3
3. ch1 静止平稳（漂移消失，AHRS 输入已正确）
4. 左拨杆上档等 INIT 完成：推杆看 ch1 追 ch0 方向正确（推杆抬头 ch0/ch1 同向）；方向反了报告，翻 PITCH_RC_SEN 符号即可

