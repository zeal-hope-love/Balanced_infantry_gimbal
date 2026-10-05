/**
  ****************************(C) COPYRIGHT 2026****************************
  * @file       vofa_task.h
  * @brief      VOFA+ 上位机调参模块对外接口
  *             传输通道: USART1 蓝牙串口(大夏龙雀 DX-BT36/37 透传, 115200)
  *             上行: JustFloat 波形帧; 下行: ASCII 命令 set/get/list
  * @note       其他模块只需调用下面 3 个注册函数即可接入:
  *             1. vofa_reg_channel(name, &变量)  -> 该变量 50Hz 发到 VOFA 画波形(只读)
  *             2. vofa_reg_param(name, &变量)    -> 可被 VOFA 命令 "set 名字 值" 在线修改
  *             3. vofa_reg_pid(前缀, &pid, 类型) -> 一键注册整组 Kp/Ki/Kd/max_out/max_iout
  *             注册的均为指针, set 命令直接写 PID 结构体成员, 立即生效
  * @history
  *  V1.0.0     Oct-4-2026      1. done
  ==============================================================================
  */
#ifndef VOFA_TASK_H
#define VOFA_TASK_H

#include "struct_typedef.h"

//vofa_reg_pid 的 pid_kind 参数: 区分两种 PID 结构体
#define VOFA_PID_STD     0u  // components/controller/pid.h   的 pid_type_def (Kp/Ki/Kd 大写)
#define VOFA_PID_GIMBAL  1u  // application/gimbal_task.h     的 gimbal_PID_t (kp/ki/kd 小写)

/**
  * @brief  vofa 任务入口, 在 freertos.c 用 osThreadCreate 创建
  *         内部完成: RX DMA 启动 / 云台 PID 与遥控器通道绑定 / 50Hz 波形发送 / 命令解析
  */
extern void vofa_task(void const *argument);

/**
  * @brief          注册一个可在线修改的浮点参数
  * @param[in]      name: 参数名(命令里用), 例如 "yaw_spd_kp"
  * @param[in]      addr: 变量地址, 建议 &pid.Kp 这类结构体成员(指针直写, 立即生效)
  * @retval         true 成功 / false 表满或重名
  * @note           仅允许在调度器启动前(vofa_task 绑定阶段)调用, 不要在其他任务运行后注册
  */
extern bool_t vofa_reg_param(const char *name, fp32 *addr);

/**
  * @brief          注册一个监看通道(只读), 每个发送周期采一次值发给 VOFA 画波形
  * @param[in]      name: 通道名(仅代码注释/回显用, JustFloat 不传名字, VOFA 里显示为 ch0..N)
  * @param[in]      addr: 变量地址
  * @retval         true 成功 / false 表满
  * @note           注册顺序 = VOFA 波形通道顺序: 先注册的是 ch0, 依次递增
  */
extern bool_t vofa_reg_channel(const char *name, const fp32 *addr);

/**
  * @brief          一键注册一整组 PID 可调参数 (Kp/Ki/Kd/max_out/max_iout)
  * @param[in]      prefix: 参数名前缀, 例如 "yaw_spd" -> 生成 yaw_spd_kp/ki/kd/maxout/maxiout
  * @param[in]      pid:    PID 结构体指针 (gimbal_motor_t 内成员取地址时直接传成员地址)
  * @param[in]      pid_kind: VOFA_PID_STD 或 VOFA_PID_GIMBAL
  * @retval         true 成功
  */
extern bool_t vofa_reg_pid(const char *prefix, void *pid, uint8_t pid_kind);

#endif
