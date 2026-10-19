#ifndef MOTOR_H
#define MOTOR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// 闭环速度范围，对应 $spd 指令（Closed-loop speed range）.
#define MOTOR_SPEED_MAX 1000

/// 开环 PWM 范围，对应 $pwm 指令（Open-loop PWM range）.
#define MOTOR_PWM_MAX 3600

/// 驱动板电机通道，对应板上的 M1~M4（Motor channels）.
typedef enum {
  MOTOR_M1 = 0, ///< 左轮（Left wheel）.
  MOTOR_M2,     ///< 未使用（Unused）.
  MOTOR_M3,     ///< 右轮（Right wheel）.
  MOTOR_M4,     ///< 未使用（Unused）.
  MOTOR_CH_COUNT
} MotorChannel;

/// 初始化驱动板并让两轮保持停止（Configure the board and stop both wheels）.
void Motor_Init(void);

/// 设置驱动板速度 PID，发送后驱动板会重启并停转（Sets the speed PID）.
void Motor_Set_Pid(float p, float i, float d);

/// 设置左右轮闭环速度，范围 -MOTOR_SPEED_MAX~MOTOR_SPEED_MAX（Speed）.
void Motor_Set_Speed(int16_t left, int16_t right);

/// 设置左右轮开环 PWM，范围 -MOTOR_PWM_MAX~MOTOR_PWM_MAX（PWM）.
void Motor_Set_Pwm(int16_t left, int16_t right);

/// 停止两轮（Stop both wheels）.
void Motor_Stop(void);

/// 最近一次上报的累计编码器值（Total encoder count）.
int32_t Motor_Get_Encoder(MotorChannel ch);

/// 最近一次上报的 10ms 编码器增量（10 ms encoder delta）.
int32_t Motor_Get_Encoder_Delta(MotorChannel ch);

/// 最近一次上报的速度，单位 mm/s（Speed in mm/s）.
float Motor_Get_Speed(MotorChannel ch);

/// 一次性原子复制所有通道数据，输出指针可为 NULL（Atomic snapshot）.
void Motor_Get_Snapshot(int32_t total[MOTOR_CH_COUNT],
                        int32_t delta[MOTOR_CH_COUNT],
                        float speed[MOTOR_CH_COUNT]);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_H */
