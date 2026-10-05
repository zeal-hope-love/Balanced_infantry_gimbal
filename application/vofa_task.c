/**
  ****************************(C) COPYRIGHT 2026****************************
  * @file       vofa_task.c
  * @brief      VOFA+ 上位机调参模块 (USART1 蓝牙串口, 115200)
  *
  *  数据通路:
  *   上行(波形): 注册通道表 -> JustFloat 打包(帧尾 00 00 80 7F) -> usart1 TX DMA
  *   下行(命令): RX DMA Circular + 空闲中断 -> HAL_UARTEx_RxEventCallback 入 SPSC 环形缓冲
  *               -> 任务侧逐字节解析 "set <名字> <值>\n" / "get <名字>\n" / "list\n"
  *               -> 查注册表拿到 float* 指针直写, 立即生效, 文本回显
  *
  *  VOFA+ 配置: 协议 JustFloat, 串口波特率 115200
  *  蓝牙模块配置: AT+BAUD7(115200) -> AT+BLUFISEND=1(透传) -> 双模块 AT+BIND 绑定
  *
  * @note       串口外设(RX DMA Circular + USART1 全局中断)在 CubeMX 配置,
  *             本文件只用标准 HAL API, 重新生成代码不会覆盖本模块
  * @history
  *  V1.0.0     Oct-4-2026      1. done
  ==============================================================================
  */
#include "vofa_task.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "cmsis_os.h"

#include "main.h"               // HAL 句柄链
#include "usart.h"              // huart1
#include "bsp_usart.h"          // usart1_tx_dma_enable (TX 一次性 DMA 发送)

//DMA 句柄定义在 usart.c (usart.h 未导出, 这里 extern)
extern DMA_HandleTypeDef hdma_usart1_tx;
extern DMA_HandleTypeDef hdma_usart1_rx;

#include "gimbal_task.h"        // get_yaw_motor_point / get_pitch_motor_point / gimbal_PID_t
#include "pid.h"                // pid_type_def
#include "remote_control.h"     // get_remote_control_point / RC_ctrl_t
#include "shoot.h"              // shoot_control_t / fric_motor_t

//射击数据全局实例(shoot.c 定义, 非 static), 调试模式直接绑定摩擦轮速度环
extern shoot_control_t shoot_control;

//---------------------------- 可调宏 ----------------------------
//波形发送频率(Hz)。BLE 蓝牙透传空中吞吐有限(几 KB/s), 默认 50Hz:
//18 通道 x 4B + 4B 帧尾 = 76B x 50Hz = 3.8KB/s, 在 BLE 承受范围内
#define VOFA_SEND_HZ            50u

//注册表容量
#define VOFA_PARAM_MAX          64u
#define VOFA_CHANNEL_MAX        24u
#define VOFA_NAME_LEN           24u

//RX: DMA 循环缓冲 与 任务侧环形缓冲
#define VOFA_RX_DMA_BUF_SIZE    128u
#define VOFA_RX_RING_SIZE       256u
//单条命令最大长度
#define VOFA_CMD_LEN_MAX        64u

//回显文本最大长度(单次)
#define VOFA_ECHO_BUF_SIZE      64u

//---------------------------- 存储布局 ----------------------------
//注册表条目: 名字拷贝存储(防止调用方传栈上临时串) + 数据指针
typedef struct
{
    char  name[VOFA_NAME_LEN];
    fp32 *addr;
} vofa_entry_t;

static vofa_entry_t vofa_param_table[VOFA_PARAM_MAX];     //可被 set 命令修改的参数
static uint8_t      vofa_param_count = 0;
static vofa_entry_t vofa_channel_table[VOFA_CHANNEL_MAX]; //只读监看通道
static uint8_t      vofa_channel_count = 0;

//RX: DMA 硬件循环缓冲(中断写) + SPSC 环形缓冲(中断写头/任务写尾, 无锁)
static uint8_t           vofa_rx_dma_buf[VOFA_RX_DMA_BUF_SIZE];
static volatile uint16_t vofa_rx_dma_last = 0;  //回调上次处理到的 DMA 偏移
static uint8_t           vofa_rx_ring[VOFA_RX_RING_SIZE];
static volatile uint16_t vofa_rx_head = 0;      //中断写
static volatile uint16_t vofa_rx_tail = 0;      //任务写

//TX: 波形帧缓冲 与 文本回显缓冲(DMA normal 模式发送期间内存不能动, 各用独立缓冲)
static uint8_t vofa_tx_wave_buf[VOFA_CHANNEL_MAX * 4u + 4u];
static uint8_t vofa_tx_echo_buf[VOFA_ECHO_BUF_SIZE];

//命令行缓冲(任务上下文独占, 无需保护)
static char    vofa_cmd_buf[VOFA_CMD_LEN_MAX];
static uint8_t vofa_cmd_len = 0;

//遥控器通道 fp32 镜像 (RC 结构体是整型, 注册表只认 float*)
//顺序: ch0..ch4, s0, s1, mouse_x, mouse_y, key_v (共10个)
static fp32 vofa_rc_mirror[10];

//---------------------------- 工具: fp32 -> 字符串 ----------------------------
//newlib-nano 默认 printf 不支持 %f/%g(需 -u _printf_float), 手写转换避免链接依赖
//固定 3 位小数, 参数整定精度足够
static void vofa_ftoa(char *buf, uint16_t size, fp32 v)
{
    int32_t ipart = (int32_t)v;
    fp32 frac = v - (fp32)ipart;
    if (frac < 0.0f)
    {
        frac = -frac;
    }
    int32_t fpart = (int32_t)(frac * 1000.0f + 0.5f);
    if (fpart >= 1000)
    {
        fpart -= 1000;
        ipart += (v >= 0.0f) ? 1 : -1;
    }
    if (v < 0.0f && ipart == 0)
    {
        snprintf(buf, (size_t)size, "-0.%03d", (int)fpart);
    }
    else
    {
        snprintf(buf, (size_t)size, "%d.%03d", (int)ipart, (int)fpart);
    }
}

//---------------------------- TX 层 ----------------------------
//TX DMA 是否忙(normal 模式: EN 位为 1 表示正在发)
static inline bool_t vofa_tx_busy(void)
{
    return (hdma_usart1_tx.Instance->CR & DMA_SxCR_EN) != 0u;
}

//发送 JustFloat 波形帧: 打包全部监看通道 + 帧尾, 忙则整帧丢弃(不阻塞任务)
static void vofa_send_wave(void)
{
    uint16_t n = vofa_channel_count;
    if (n == 0u)
    {
        return;
    }
    if (vofa_tx_busy())
    {
        return; //上帧未发完(蓝牙慢), 丢帧不堆积
    }

    for (uint16_t i = 0; i < n; i++)
    {
        fp32 v = *vofa_channel_table[i].addr;
        memcpy(&vofa_tx_wave_buf[i * 4u], &v, 4u); //小端 float, 与 JustFloat 一致
    }
    vofa_tx_wave_buf[n * 4u + 0u] = 0x00u;         //帧尾 00 00 80 7F (float 1.0f 的字节序)
    vofa_tx_wave_buf[n * 4u + 1u] = 0x00u;
    vofa_tx_wave_buf[n * 4u + 2u] = 0x80u;
    vofa_tx_wave_buf[n * 4u + 3u] = 0x7Fu;

    usart1_tx_dma_enable(vofa_tx_wave_buf, (uint16_t)(n * 4u + 4u));
}

//发送文本回显: 任务上下文, 忙则等待(文本低频, 阻塞自身不影响其他任务)
static void vofa_send_text(const char *text)
{
    uint16_t len = (uint16_t)strlen(text);
    if (len > VOFA_ECHO_BUF_SIZE)
    {
        len = VOFA_ECHO_BUF_SIZE;
    }
    memcpy(vofa_tx_echo_buf, text, len);

    while (vofa_tx_busy())
    {
        osDelay(2);
    }
    usart1_tx_dma_enable(vofa_tx_echo_buf, len);
    osDelay(2); //给 DMA 一点启动时间, 避免连续回显时第二轮判断失误
}

//---------------------------- 注册表层 ----------------------------
static fp32 *vofa_param_find(const char *name)
{
    for (uint8_t i = 0; i < vofa_param_count; i++)
    {
        if (strcmp(vofa_param_table[i].name, name) == 0)
        {
            return vofa_param_table[i].addr;
        }
    }
    return NULL;
}

static bool_t vofa_entry_add(vofa_entry_t *table, uint8_t *count, uint8_t max,
                             const char *name, fp32 *addr)
{
    if (*count >= max || name == NULL || addr == NULL || strlen(name) >= VOFA_NAME_LEN)
    {
        return false;
    }
    vofa_entry_t *e = &table[*count];
    strncpy(e->name, name, VOFA_NAME_LEN - 1u);
    e->name[VOFA_NAME_LEN - 1u] = '\0';
    e->addr = addr;
    (*count)++;
    return true;
}

bool_t vofa_reg_param(const char *name, fp32 *addr)
{
    if (vofa_param_find(name) != NULL)
    {
        return false; //重名
    }
    return vofa_entry_add(vofa_param_table, &vofa_param_count, VOFA_PARAM_MAX, name, addr);
}

bool_t vofa_reg_channel(const char *name, const fp32 *addr)
{
    return vofa_entry_add(vofa_channel_table, &vofa_channel_count, VOFA_CHANNEL_MAX,
                          name, (fp32 *)addr);
}

bool_t vofa_reg_pid(const char *prefix, void *pid, uint8_t pid_kind)
{
    char name[VOFA_NAME_LEN];
    bool_t ok = true;

    if (pid_kind == VOFA_PID_GIMBAL)
    {
        gimbal_PID_t *p = (gimbal_PID_t *)pid;
        snprintf(name, VOFA_NAME_LEN, "%s_kp", prefix);
        ok &= vofa_reg_param(name, &p->kp);
        snprintf(name, VOFA_NAME_LEN, "%s_ki", prefix);
        ok &= vofa_reg_param(name, &p->ki);
        snprintf(name, VOFA_NAME_LEN, "%s_kd", prefix);
        ok &= vofa_reg_param(name, &p->kd);
        snprintf(name, VOFA_NAME_LEN, "%s_maxout", prefix);
        ok &= vofa_reg_param(name, &p->max_out);
        snprintf(name, VOFA_NAME_LEN, "%s_maxiout", prefix);
        ok &= vofa_reg_param(name, &p->max_iout);
    }
    else
    {
        pid_type_def *p = (pid_type_def *)pid;
        snprintf(name, VOFA_NAME_LEN, "%s_kp", prefix);
        ok &= vofa_reg_param(name, &p->Kp);
        snprintf(name, VOFA_NAME_LEN, "%s_ki", prefix);
        ok &= vofa_reg_param(name, &p->Ki);
        snprintf(name, VOFA_NAME_LEN, "%s_kd", prefix);
        ok &= vofa_reg_param(name, &p->Kd);
        snprintf(name, VOFA_NAME_LEN, "%s_maxout", prefix);
        ok &= vofa_reg_param(name, &p->max_out);
        snprintf(name, VOFA_NAME_LEN, "%s_maxiout", prefix);
        ok &= vofa_reg_param(name, &p->max_iout);
    }
    return ok;
}

//---------------------------- 命令解析层 ----------------------------
//简易分词: 空格/Tab 分隔, 返回当前 token 并推进 p
static char *vofa_next_token(char **p)
{
    char *s = *p;
    while (*s == ' ' || *s == '\t')
    {
        s++;
    }
    if (*s == '\0')
    {
        *p = s;
        return NULL;
    }
    char *tok = s;
    while (*s != '\0' && *s != ' ' && *s != '\t')
    {
        s++;
    }
    if (*s != '\0')
    {
        *s = '\0';
        s++;
    }
    *p = s;
    return tok;
}

//任务上下文执行, 收到完整一行(已去 \n)后调用
static void vofa_cmd_exec(char *line)
{
    char *p = line;
    char *cmd = vofa_next_token(&p);

    if (cmd == NULL)
    {
        return;
    }

    if (strcmp(cmd, "list") == 0)
    {
        char line_buf[VOFA_NAME_LEN + 24u];
        for (uint8_t i = 0; i < vofa_param_count; i++)
        {
            char vbuf[16];
            vofa_ftoa(vbuf, sizeof(vbuf), *vofa_param_table[i].addr);
            snprintf(line_buf, sizeof(line_buf), "%s=%s\r\n",
                     vofa_param_table[i].name, vbuf);
            vofa_send_text(line_buf);
        }
        vofa_send_text("list end\r\n");
        return;
    }

    if (strcmp(cmd, "get") == 0)
    {
        char *name = vofa_next_token(&p);
        if (name == NULL)
        {
            vofa_send_text("ERR usage: get <name>\r\n");
            return;
        }
        fp32 *addr = vofa_param_find(name);
        if (addr == NULL)
        {
            vofa_send_text("ERR no param\r\n");
            return;
        }
        char vbuf[16];
        vofa_ftoa(vbuf, sizeof(vbuf), *addr);
        char line_buf[VOFA_NAME_LEN + 24u];
        snprintf(line_buf, sizeof(line_buf), "%s=%s\r\n", name, vbuf);
        vofa_send_text(line_buf);
        return;
    }

    if (strcmp(cmd, "set") == 0)
    {
        char *name = vofa_next_token(&p);
        char *val = vofa_next_token(&p);
        if (name == NULL || val == NULL)
        {
            vofa_send_text("ERR usage: set <name> <val>\r\n");
            return;
        }
        fp32 *addr = vofa_param_find(name);
        if (addr == NULL)
        {
            vofa_send_text("ERR no param\r\n");
            return;
        }
        *addr = (fp32)strtof(val, NULL); //指针直写, 下一拍控制周期生效
        char vbuf[16];
        vofa_ftoa(vbuf, sizeof(vbuf), *addr);
        char line_buf[VOFA_NAME_LEN + 24u];
        snprintf(line_buf, sizeof(line_buf), "OK %s=%s\r\n", name, vbuf);
        vofa_send_text(line_buf);
        return;
    }

    vofa_send_text("ERR cmd, use: set/get/list\r\n");
}

//逐字节喂入命令行状态机(任务上下文)
static void vofa_rx_byte(uint8_t byte)
{
    if (byte == '\n' || byte == '\r')
    {
        if (vofa_cmd_len > 0u)
        {
            vofa_cmd_buf[vofa_cmd_len] = '\0';
            vofa_cmd_exec(vofa_cmd_buf);
            vofa_cmd_len = 0;
        }
        return;
    }
    if (vofa_cmd_len < VOFA_CMD_LEN_MAX - 1u)
    {
        vofa_cmd_buf[vofa_cmd_len++] = (char)byte;
    }
    else
    {
        vofa_cmd_len = 0; //超长行, 整行丢弃防溢出
    }
}

//---------------------------- RX 中断层 ----------------------------
/**
  * @brief  HAL UART 接收事件回调(重写弱函数)
  *         USART1 RX DMA Circular 模式下, 空闲中断/半满/全满都会进到这里,
  *         Size 为"从缓冲区起点到当前写入位置"的字节数, 用与上次的差值取出新数据
  * @note   只做字节搬运入 SPSC 环形缓冲, 解析在任务上下文
  */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance != USART1)
    {
        return;
    }

    uint16_t pos = vofa_rx_dma_last % VOFA_RX_DMA_BUF_SIZE;
    while (pos != Size)
    {
        uint16_t next = (uint16_t)((vofa_rx_head + 1u) % VOFA_RX_RING_SIZE);
        if (next == vofa_rx_tail)
        {
            break; //环形缓冲满, 丢新数据(命令频率极低, 不会发生)
        }
        vofa_rx_ring[vofa_rx_head] = vofa_rx_dma_buf[pos];
        vofa_rx_head = next;
        pos = (uint16_t)((pos + 1u) % VOFA_RX_DMA_BUF_SIZE);
    }
    vofa_rx_dma_last = Size;
}

//---------------------------- 业务绑定 ----------------------------
//波形通道顺序(VOFA 里显示为 ch0..N):
//  ch0 fric0_target(目标转速, 右摇杆映射值)  ch1 fric0_set  ch2 fric0_get(实际转速)  ch3 fric0_out
//可调参数: fric0_kp / fric0_ki / fric0_kd / fric0_maxout / fric0_maxiout (0号摩擦轮速度环)
//遥控器通道已撤下(调试时用 IDE 监视器看 rc_ctrl/vofa_rc_mirror 即可), 需要时取消下方注释
static void vofa_bind(void)
{
    /* ---- 遥控器 7 通道(已撤下, 需要时取消注释) ----
    (void)vofa_reg_channel("rc_ch0", &vofa_rc_mirror[0]);
    (void)vofa_reg_channel("rc_ch1", &vofa_rc_mirror[1]);
    (void)vofa_reg_channel("rc_ch2", &vofa_rc_mirror[2]);
    (void)vofa_reg_channel("rc_ch3", &vofa_rc_mirror[3]);
    (void)vofa_reg_channel("rc_ch4", &vofa_rc_mirror[4]);
    (void)vofa_reg_channel("rc_s0", &vofa_rc_mirror[5]);
    (void)vofa_reg_channel("rc_s1", &vofa_rc_mirror[6]);
    ---- 遥控器通道注册结束 ---- */

    //---- 0号摩擦轮速度环(调试模式: 右摇杆垂直通道直控目标转速, 无斜坡) ----
    (void)vofa_reg_pid("fric0", &shoot_control.fric_motor[0].speed_pid, VOFA_PID_STD);
    (void)vofa_reg_channel("fric0_target", &shoot_control.fric_speed_set);               //目标(摇杆映射值, 只读)
    (void)vofa_reg_channel("fric0_set", &shoot_control.fric_motor[0].speed_pid.set);     //PID 实际输入(乘过转向符号)
    (void)vofa_reg_channel("fric0_get", &shoot_control.fric_motor[0].speed_pid.fdb);     //实际转速 rpm
    (void)vofa_reg_channel("fric0_out", &shoot_control.fric_motor[0].speed_pid.out);     //PID 输出电流

    /* ---- 云台 yaw/pitch(暂不输出, 需要时取消注释; 注意遥控器镜像注释号同步) ----
    const gimbal_motor_t *yaw = get_yaw_motor_point();
    if (yaw != NULL)
    {
        (void)vofa_reg_pid("yaw_ang", (void *)&yaw->gimbal_motor_absolute_angle_pid, VOFA_PID_GIMBAL);
        (void)vofa_reg_pid("yaw_spd", (void *)&yaw->gimbal_motor_gyro_pid, VOFA_PID_STD);

        (void)vofa_reg_channel("yaw_set", &yaw->absolute_angle_set);
        (void)vofa_reg_channel("yaw_get", &yaw->absolute_angle);
        (void)vofa_reg_channel("yaw_gyro", &yaw->motor_gyro);
        (void)vofa_reg_channel("yaw_cur", &yaw->current_set);
    }

    const gimbal_motor_t *pitch = get_pitch_motor_point();
    if (pitch != NULL)
    {
        (void)vofa_reg_pid("pitch_ang", (void *)&pitch->gimbal_motor_absolute_angle_pid, VOFA_PID_GIMBAL);
        (void)vofa_reg_pid("pitch_spd", (void *)&pitch->gimbal_motor_gyro_pid, VOFA_PID_STD);

        (void)vofa_reg_channel("pitch_set", &pitch->absolute_angle_set);
        (void)vofa_reg_channel("pitch_get", &pitch->absolute_angle);
        (void)vofa_reg_channel("pitch_gyro", &pitch->motor_gyro);
        (void)vofa_reg_channel("pitch_cur", &pitch->current_set);
    }

    (void)vofa_reg_channel("mouse_x", &vofa_rc_mirror[7]);
    (void)vofa_reg_channel("mouse_y", &vofa_rc_mirror[8]);
    (void)vofa_reg_channel("key_v", &vofa_rc_mirror[9]);
    ---- 云台/鼠标键盘注册结束 ---- */
}

//遥控器整型 -> fp32 镜像刷新
static void vofa_rc_mirror_update(void)
{
    const RC_ctrl_t *rc = get_remote_control_point();
    if (rc == NULL)
    {
        return;
    }
    for (uint8_t i = 0; i < 5u; i++)
    {
        vofa_rc_mirror[i] = (fp32)rc->rc.ch[i];
    }
    vofa_rc_mirror[5] = (fp32)rc->rc.s[0];
    vofa_rc_mirror[6] = (fp32)rc->rc.s[1];
    vofa_rc_mirror[7] = (fp32)rc->mouse.x;
    vofa_rc_mirror[8] = (fp32)rc->mouse.y;
    vofa_rc_mirror[9] = (fp32)rc->key.v;
}

//---------------------------- 任务入口 ----------------------------
void vofa_task(void const *argument)
{
    (void)argument;

    //1. 启动 RX: DMA Circular + 空闲/半满/全满事件 -> HAL_UARTEx_RxEventCallback
    (void)HAL_UARTEx_ReceiveToIdle_DMA(&huart1, vofa_rx_dma_buf, VOFA_RX_DMA_BUF_SIZE);

    //2. 注册云台 PID 参数/观测通道与遥控器通道
    vofa_bind();

    vofa_send_text("vofa ready, cmd: set/get/list\r\n");

    for (;;)
    {
        //3. 消费环形缓冲里的命令字节
        while (vofa_rx_tail != vofa_rx_head)
        {
            uint8_t b = vofa_rx_ring[vofa_rx_tail];
            vofa_rx_tail = (uint16_t)((vofa_rx_tail + 1u) % VOFA_RX_RING_SIZE);
            vofa_rx_byte(b);
        }

        //4. 遥控器镜像刷新(通道未注册, 仅刷新 vofa_rc_mirror 数组供调试器观察)
        vofa_rc_mirror_update();

        //5. 波形发送
        vofa_send_wave();

        osDelay(1000u / VOFA_SEND_HZ);
    }
}
