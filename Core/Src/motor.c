#include "main.h"
#include "motor.h"
#include "usart.h"

#include <stdio.h>
#include <string.h>

/// 电机类型：1=520, 2=310, 3=TT 带编码器, 4=TT 无编码器（Motor type）.
#define MOTOR_TYPE 1
/// 520 电机减速比（Gear reduction ratio）.
#define MOTOR_PHASE 30
/// 520 电机编码器磁环线数（Magnetic ring lines）.
#define MOTOR_LINE 11
/// 轮子直径，单位 mm（Wheel diameter in mm）.
#define MOTOR_WHEEL_DIAMETER 67.0f
/// 实测 PWM 死区（PWM dead zone measured on the chassis）.
#define MOTOR_DEADZONE 1900

/// 控制板通道：M1 左轮，M3 右轮（M1 = left wheel, M3 = right wheel）.
#define MOTOR_CH_LEFT 0
#define MOTOR_CH_RIGHT 2

/// 轮子方向反了时置 1（Set to 1 when a wheel turns the wrong way）.
#define MOTOR_LEFT_INVERT 1
#define MOTOR_RIGHT_INVERT 1

/// 驱动板串口与发送超时（Driver board UART and TX timeout）.
#define MOTOR_UART (&huart3)
#define MOTOR_UART_TIMEOUT_MS 20U

/// 上报开关：累计编码器、10ms 编码器、速度（Upload reports）.
#define MOTOR_UPLOAD_TOTAL_ENCODER 1
#define MOTOR_UPLOAD_DELTA_ENCODER 1
#define MOTOR_UPLOAD_SPEED 1

/// 接收组帧缓冲区长度（RX frame buffer length）.
#define MOTOR_RX_BUF_LEN 64

static char s_rx_buf[MOTOR_RX_BUF_LEN];
static uint8_t s_rx_len;
static uint8_t s_rx_active;
static uint8_t s_rx_byte;

/// 一帧完整的驱动板反馈数据（One complete set of board feedback）.
typedef struct {
  int32_t total[MOTOR_CH_COUNT];
  int32_t delta[MOTOR_CH_COUNT];
  float speed[MOTOR_CH_COUNT];
} Motor_Feedback;

/// 双缓冲：中断写非活动区，写完后切换活动索引（Double buffer, index flip）.
static volatile Motor_Feedback s_feedback[2];
static volatile uint8_t s_feedback_active;
static volatile uint32_t s_feedback_seq;

/// 发送一帧协议数据，例如 "$spd:100,0,100,0#"（Send one protocol frame）.
static void Motor_Send(const char *frame) {
  HAL_UART_Transmit(MOTOR_UART, (const uint8_t *)frame, (uint16_t)strlen(frame),
                    MOTOR_UART_TIMEOUT_MS);
}

/// 把数值限制在指定范围内（Clamp a signed value to the given range）.
static int16_t Motor_Clamp(int32_t value, int32_t min, int32_t max) {
  if (value > max) {
    return (int16_t)max;
  }
  if (value < min) {
    return (int16_t)min;
  }
  return (int16_t)value;
}

/// 应用单轮方向校准（Apply the per-wheel direction calibration）.
static int16_t Motor_ApplyDirection(int16_t value, uint8_t invert) {
  return invert ? (int16_t)(-value) : value;
}

/// 跳过 "MAll:" 这类前缀，返回第一个数值的起点（Skip the prefix）.
static const char *Motor_FirstValue(const char *frame) {
  while (*frame != '\0' && *frame != ':') {
    frame++;
  }
  return (*frame == ':') ? frame + 1 : frame;
}

/// 读取下一个整数并移动游标（Read the next integer value）.
static int32_t Motor_NextInt(const char **cursor) {
  const char *p = *cursor;
  int32_t sign = 1;
  int32_t value = 0;

  while (*p == ' ') {
    p++;
  }
  if (*p == '-') {
    sign = -1;
    p++;
  }
  while (*p >= '0' && *p <= '9') {
    value = value * 10 + (*p - '0');
    p++;
  }
  *cursor = p;
  return sign * value;
}

/// 读取下一个小数并移动游标（Read the next decimal value）.
static float Motor_NextFloat(const char **cursor) {
  const char *p = *cursor;
  int32_t sign = 1;
  float value = 0.0f;
  float scale = 0.1f;

  while (*p == ' ') {
    p++;
  }
  if (*p == '-') {
    sign = -1;
    p++;
  }
  while (*p >= '0' && *p <= '9') {
    value = value * 10.0f + (float)(*p - '0');
    p++;
  }
  if (*p == '.') {
    p++;
    while (*p >= '0' && *p <= '9') {
      value += (float)(*p - '0') * scale;
      scale *= 0.1f;
      p++;
    }
  }
  *cursor = p;
  return (float)sign * value;
}

/// 解析 4 个逗号分隔的整数（Parse four comma-separated integers）.
static void Motor_ParseFourInts(const char *values, int32_t *out) {
  const char *p = values;

  for (uint8_t i = 0; i < MOTOR_CH_COUNT; i++) {
    out[i] = Motor_NextInt(&p);
    if (*p == ',') {
      p++;
    }
  }
}

/// 解析 4 个逗号分隔的小数（Parse four comma-separated decimals）.
static void Motor_ParseFourFloats(const char *values, float *out) {
  const char *p = values;

  for (uint8_t i = 0; i < MOTOR_CH_COUNT; i++) {
    out[i] = Motor_NextFloat(&p);
    if (*p == ',') {
      p++;
    }
  }
}

/// 刚收到的反馈帧类型（Feedback frame type）.
typedef enum {
  MOTOR_FRAME_TOTAL = 0,
  MOTOR_FRAME_DELTA,
  MOTOR_FRAME_SPEED
} MotorFrameType;

/// 通过双缓冲发布一帧完整数据（Publish a frame through the double buffer）.
static void Motor_Commit(MotorFrameType type, const int32_t *ints, const float *floats) {
  uint8_t next = (uint8_t)(s_feedback_active ^ 1U);

  /// 序号置为奇数，标记正在更新（Odd sequence = writing）.
  s_feedback_seq = s_feedback_seq + 1U;
  __DMB();

  /// 复制上一份数据，只更新本次帧的字段，然后切换索引（Copy, update, flip）.
  s_feedback[next] = s_feedback[s_feedback_active];
  for (uint8_t i = 0; i < MOTOR_CH_COUNT; i++) {
    if (type == MOTOR_FRAME_TOTAL) {
      s_feedback[next].total[i] = ints[i];
    } else if (type == MOTOR_FRAME_DELTA) {
      s_feedback[next].delta[i] = ints[i];
    } else {
      s_feedback[next].speed[i] = floats[i];
    }
  }
  __DMB();

  s_feedback_active = next;
  s_feedback_seq = s_feedback_seq + 1U;
}

/// 分发一帧完整的 "$...#' 数据（Dispatch one complete payload）.
static void Motor_ParseFrame(const char *frame) {
  int32_t ints[MOTOR_CH_COUNT] = {0};
  float floats[MOTOR_CH_COUNT] = {0.0f};

  if (strncmp(frame, "MAll", 4) == 0) {
    Motor_ParseFourInts(Motor_FirstValue(frame), ints);
    Motor_Commit(MOTOR_FRAME_TOTAL, ints, floats);
  } else if (strncmp(frame, "MTEP", 4) == 0) {
    Motor_ParseFourInts(Motor_FirstValue(frame), ints);
    Motor_Commit(MOTOR_FRAME_DELTA, ints, floats);
  } else if (strncmp(frame, "MSPD", 4) == 0) {
    Motor_ParseFourFloats(Motor_FirstValue(frame), floats);
    Motor_Commit(MOTOR_FRAME_SPEED, ints, floats);
  }
}

/// 把 '$' 与 '#' 之间的字节组装成一帧（Assemble bytes into one frame）.
static void Motor_ParseByte(uint8_t byte) {
  if (byte == '$') {
    s_rx_active = 1;
    s_rx_len = 0;
    return;
  }

  if (!s_rx_active) {
    return;
  }

  if (byte == '#') {
    s_rx_buf[s_rx_len] = '\0';
    s_rx_active = 0;
    Motor_ParseFrame(s_rx_buf);
    return;
  }

  if (s_rx_len < (MOTOR_RX_BUF_LEN - 1)) {
    s_rx_buf[s_rx_len++] = (char)byte;
  } else {
    s_rx_active = 0;
    s_rx_len = 0;
  }
}

/// 串口接收完成回调：逐字节解析并重新挂起接收（UART RX complete callback）.
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
  if (huart->Instance == USART3) {
    Motor_ParseByte(s_rx_byte);
    HAL_UART_Receive_IT(huart, &s_rx_byte, 1);
  }
}

/// 配置驱动板并让两轮保持停止（Configure the board and keep wheels stopped）.
void Motor_Init(void) {
  char frame[64];

  /// 配置期间先关闭上报（Disable reports while configuring）.
  Motor_Send("$upload:0,0,0#");
  HAL_Delay(50);

  snprintf(frame, sizeof(frame), "$mtype:%d#", MOTOR_TYPE);
  Motor_Send(frame);
  HAL_Delay(50);

  snprintf(frame, sizeof(frame), "$mphase:%d#", MOTOR_PHASE);
  Motor_Send(frame);
  HAL_Delay(50);

  snprintf(frame, sizeof(frame), "$mline:%d#", MOTOR_LINE);
  Motor_Send(frame);
  HAL_Delay(50);

  snprintf(frame, sizeof(frame), "$wdiameter:%.1f#", (double)MOTOR_WHEEL_DIAMETER);
  Motor_Send(frame);
  HAL_Delay(50);

  snprintf(frame, sizeof(frame), "$deadzone:%d#", MOTOR_DEADZONE);
  Motor_Send(frame);
  HAL_Delay(50);

  /// PID 指令会让驱动板重启，所以放最后并等待重启完成（PID last, wait reboot）.
  Motor_Set_Pid(0.8f, 0.06f, 0.5f);
  HAL_Delay(300);

  Motor_Stop();

  /// 先启动接收再打开上报，避免丢帧（Start RX before enabling reports）.
  HAL_UART_Receive_IT(MOTOR_UART, &s_rx_byte, 1);

  snprintf(frame, sizeof(frame), "$upload:%d,%d,%d#",
           MOTOR_UPLOAD_TOTAL_ENCODER, MOTOR_UPLOAD_DELTA_ENCODER, MOTOR_UPLOAD_SPEED);
  Motor_Send(frame);
}

/// 设置速度环 PID，发送后驱动板会重启并停转（Set speed PID）.
void Motor_Set_Pid(float p, float i, float d) {
  char frame[64];

  snprintf(frame, sizeof(frame), "$MPID:%.3f,%.3f,%.3f#", (double)p, (double)i, (double)d);
  Motor_Send(frame);
}

/// 设置左右轮闭环速度，范围 -MOTOR_SPEED_MAX~MOTOR_SPEED_MAX（Set speed）.
void Motor_Set_Speed(int16_t left, int16_t right) {
  char frame[48];
  int16_t l = Motor_ApplyDirection(Motor_Clamp(left, -MOTOR_SPEED_MAX, MOTOR_SPEED_MAX),
                                   MOTOR_LEFT_INVERT);
  int16_t r = Motor_ApplyDirection(Motor_Clamp(right, -MOTOR_SPEED_MAX, MOTOR_SPEED_MAX),
                                   MOTOR_RIGHT_INVERT);

  snprintf(frame, sizeof(frame), "$spd:%d,%d,%d,%d#",
           l, 0, r, 0);
  Motor_Send(frame);
}

/// 设置左右轮开环 PWM，范围 -MOTOR_PWM_MAX~MOTOR_PWM_MAX（Set PWM）.
void Motor_Set_Pwm(int16_t left, int16_t right) {
  char frame[48];
  int16_t l = Motor_ApplyDirection(Motor_Clamp(left, -MOTOR_PWM_MAX, MOTOR_PWM_MAX),
                                   MOTOR_LEFT_INVERT);
  int16_t r = Motor_ApplyDirection(Motor_Clamp(right, -MOTOR_PWM_MAX, MOTOR_PWM_MAX),
                                   MOTOR_RIGHT_INVERT);

  snprintf(frame, sizeof(frame), "$pwm:%d,%d,%d,%d#",
           l, 0, r, 0);
  Motor_Send(frame);
}

/// 停止两轮（Stop both wheels）.
void Motor_Stop(void) {
  Motor_Send("$spd:0,0,0,0#");
}

/// 读取累计编码器值（Last total encoder count）.
int32_t Motor_Get_Encoder(MotorChannel ch) {
  if (ch >= MOTOR_CH_COUNT) {
    return 0;
  }
  return s_feedback[s_feedback_active].total[ch];
}

/// 读取 10ms 编码器增量（Last 10 ms encoder delta）.
int32_t Motor_Get_Encoder_Delta(MotorChannel ch) {
  if (ch >= MOTOR_CH_COUNT) {
    return 0;
  }
  return s_feedback[s_feedback_active].delta[ch];
}

/// 读取速度，单位 mm/s（Last speed in mm/s）.
float Motor_Get_Speed(MotorChannel ch) {
  if (ch >= MOTOR_CH_COUNT) {
    return 0.0f;
  }
  return s_feedback[s_feedback_active].speed[ch];
}

/// 一次性原子复制所有通道数据，输出指针可为 NULL（Atomic snapshot）.
void Motor_Get_Snapshot(int32_t total[MOTOR_CH_COUNT],
                        int32_t delta[MOTOR_CH_COUNT],
                        float speed[MOTOR_CH_COUNT]) {
  Motor_Feedback snapshot;
  uint32_t seq1;
  uint32_t seq2;

  /// 如果复制期间中断发布了新数据，就重试（Retry if a new frame arrived）.
  for (;;) {
    seq1 = s_feedback_seq;
    if ((seq1 & 1U) != 0U) {
      continue;
    }

    __DMB();
    snapshot = s_feedback[s_feedback_active];
    __DMB();
    seq2 = s_feedback_seq;

    if (seq1 == seq2) {
      break;
    }
  }

  for (uint8_t i = 0; i < MOTOR_CH_COUNT; i++) {
    if (total != NULL) {
      total[i] = snapshot.total[i];
    }
    if (delta != NULL) {
      delta[i] = snapshot.delta[i];
    }
    if (speed != NULL) {
      speed[i] = snapshot.speed[i];
    }
  }
}



//  while (1)
//   {
//     /// Safe bench test: forward 2 s, stop 1 s, backward 2 s, stop 1 s.
//     uint32_t now = HAL_GetTick();

//     switch (test_phase)
//     {
//     case 0:
//       Motor_Set_Speed(150, 150);
//       test_tick = now;
//       test_phase = 1;
//       break;

//     case 1:
//       if ((now - test_tick) >= 2000U)
//       {
//         Motor_Stop();
//         test_tick = now;
//         test_phase = 2;
//       }
//       break;

//     case 2:
//       if ((now - test_tick) >= 1000U)
//       {
//         Motor_Set_Speed(-150, -150);
//         test_tick = now;
//         test_phase = 3;
//       }
//       break;

//     case 3:
//       if ((now - test_tick) >= 2000U)
//       {
//         Motor_Stop();
//         test_tick = now;
//         test_phase = 0;
//       }
//       break;

//     default:
//       test_phase = 0;
//       break;
//     }

//     /// Read all feedback at once so left/right come from the same frame.
//     int32_t enc_total[MOTOR_CH_COUNT];
//     int32_t enc_delta[MOTOR_CH_COUNT];
//     float speeds[MOTOR_CH_COUNT];

//     Motor_Get_Snapshot(enc_total, enc_delta, speeds);
//     test_left_enc = enc_total[MOTOR_M1];
//     test_right_enc = enc_total[MOTOR_M3];
//     test_left_delta = enc_delta[MOTOR_M1];
//     test_right_delta = enc_delta[MOTOR_M3];
//     test_left_speed = speeds[MOTOR_M1];
//     test_right_speed = speeds[MOTOR_M3];

//     /* USER CODE END WHILE */

//     /* USER CODE BEGIN 3 */
//   }
