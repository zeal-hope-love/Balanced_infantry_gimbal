---
name: fric-debug-stick-tuning
overview: 摩擦轮回调试模式：FRIC_POWERON_AUTO=1 且状态机钉死 SHOOT_READY_FRIC（不进拨弹），右摇杆垂直通道直接映射目标转速（不走斜坡），VOFA 保留遥控器 7 通道 + fric0 四条曲线 + 速度环 5 参数在线调。
todos:
  - id: fric-debug-mode-restore
    content: shoot.h FRIC_POWERON_AUTO 改回 1；shoot.c 调试分支恒置 READY_FRIC、公共 READY_BULLET 转移加条件编译包裹
    status: completed
  - id: fric-stick-mapping
    content: shoot.c fric_speed_set 后加右摇杆垂直通道线性映射覆盖块（#if FRIC_POWERON_AUTO，越斜坡直控目标）
    status: completed
  - id: vofa-bind-rework
    content: vofa_task.c vofa_bind 重写：删云台与鼠标键盘通道（注释保留），extern shoot_control 注册 fric0 整组参数与 target/set/get/out 4 通道，更新通道表注释
    status: completed
  - id: vofa-fric-build-verify
    content: cmake --build --preset Debug 编译验证并 read_lints 确认无告警，更新记忆文件
    status: completed
---

## 产品概述
摩擦轮速度环调试模式：摩擦轮上电即进入调试态（只跑摩擦轮速度环，永不进拨弹/发射逻辑），**遥控器右摇杆垂直通道双向满映射为 0 号摩擦轮目标转速**（上推正转 / 下推反转 / 回中 0，无斜坡，映射区间 ±9000rpm 贴 M3508 转子物理极限），配合 VOFA 滑条在线修改速度环 Kp/Ki。

## 核心功能
- **双向满映射**：rc.ch[1]（-660~660）线性映射为 **-9000~9000 rpm**（宏 FRIC_DEBUG_SPEED_MAX=9000.0f，shoot.h 可改），不钳位；回中即 0 停转。
- **调试模式回归**：FRIC_POWERON_AUTO 改回 1，状态机恒置 SHOOT_READY_FRIC，屏蔽 ramp 满进 READY_BULLET 公共转移。
- **不干扰其他设备**：shoot_mode 每周期强制覆盖，拨杆/键盘对射击状态机失效；云台/底盘电机未接电无实际影响。
- **VOFA 在线调参**：遥控器 7 通道 + 摩擦轮 4 通道；滑条 set 直改 fric0_kp/ki/maxout 等立即生效。

## 关键实现决策
1. **调试模式收敛**：shoot.c:228-240 调试分支简化为每周期无条件 shoot_mode = SHOOT_READY_FRIC；shoot.c:265-268 公共转移（ramp 满→READY_BULLET）用 #if !FRIC_POWERON_AUTO 包住。
2. **摇杆满映射越过斜坡**：shoot.c:206 fric_speed_set = ramp.out 之后追加调试覆盖块（#if FRIC_POWERON_AUTO）：
   `fric_speed_set = (fp32)rc.ch[1] * (FRIC_DEBUG_SPEED_MAX / 660.0f);`（-9000~9000）
3. **双源冲突规避**：fric0_target 通道绑 &fric_speed_set 只读，不注册为 set 参数。
4. **vofa_task.c 绑定**：extern shoot_control_t shoot_control（非 static 全局）；vofa_reg_pid("fric0", &fric_motor[0].speed_pid, VOFA_PID_STD)；通道 fric0_target/set/get/out。
5. **通道裁剪**：删云台 8 通道 + mouse/key 3 通道（注释保留），遥控器 7 通道保留。

## 修改文件清单
```
application/shoot.h      [MOD] FRIC_POWERON_AUTO 0→1；新增 FRIC_DEBUG_SPEED_MAX 9000.0f 宏
application/shoot.c      [MOD] 调试分支恒置 READY_FRIC；公共 READY_BULLET 转移包裹；fric_speed_set 后加摇杆满映射覆盖块
application/vofa_task.c  [MOD] vofa_bind 重写：删云台/mouse/key 通道（注释保留）、新增 fric0 参数与 4 通道
```

## 调参工作流
- 滑条（ASCII 文本，Hex 不勾）：set fric0_kp %%\n（0~50，步进0.5）、set fric0_ki %%\n（0~5，步进0.05）、set fric0_maxout %%\n（0~16384，步进500）
- 右摇杆 ±阶跃，看 ch9 get 追 ch7 target；调好写回宏。
- 注意：满速 9000 接近 M3508 转子极限（9254rpm），大目标下 PID 若震荡会顶电流限幅，属正常调试现象，别长时间满速跑。