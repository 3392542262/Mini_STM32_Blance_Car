#include "main.h"
#include "key.h"

/// 按键引脚：PA5（Key pin on the B570 control board）.
#define KEY_PORT GPIOA
#define KEY_PIN GPIO_PIN_5

/// 按下时的电平：0 = 按下为低（内部上拉），1 = 按下为高（内部下拉）.
///
/// 板子上按键一端接 GND 时为 0；接 3V3 时改成 1（Active level of the key）.
#define KEY_ACTIVE_LEVEL 0

/// 消抖时间，单位 ms（Debounce time）.
#define KEY_DEBOUNCE_MS 20U

/// 长按判定时间，单位 ms（Long press threshold）.
#define KEY_LONG_PRESS_MS 800U

/// 长按后的连发周期，单位 ms（Auto repeat period）.
#define KEY_REPEAT_MS 150U

/// 消抖后的稳定状态，1 = 按下（Debounced state）.
static uint8_t s_pressed;

/// 最近一次原始采样（Last raw sample）.
static uint8_t s_sample;

/// 采样发生变化的时间戳（Timestamp of the last raw change）.
static uint32_t s_change_tick;

/// 按下时刻，用于长按计时（Timestamp of the stable press）.
static uint32_t s_press_tick;

/// 上一次连发的时刻（Timestamp of the last auto repeat）.
static uint32_t s_repeat_tick;

/// 长按事件是否已经上报（Whether the long press was reported）.
static uint8_t s_long_reported;

/// 读取引脚的原始状态，已按有效电平折算成 1 = 按下（Read the raw pin state）.
static uint8_t Key_Read_Raw(void) {
#if KEY_ACTIVE_LEVEL
  return (uint8_t)(HAL_GPIO_ReadPin(KEY_PORT, KEY_PIN) == GPIO_PIN_SET);
#else
  return (uint8_t)(HAL_GPIO_ReadPin(KEY_PORT, KEY_PIN) == GPIO_PIN_RESET);
#endif
}

void Key_Init(void) {
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  uint32_t now;

  __HAL_RCC_GPIOA_CLK_ENABLE();

  GPIO_InitStruct.Pin = KEY_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
#if KEY_ACTIVE_LEVEL
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
#else
  GPIO_InitStruct.Pull = GPIO_PULLUP;
#endif
  HAL_GPIO_Init(KEY_PORT, &GPIO_InitStruct);

  /// 以上电瞬间的状态作为初始值，避免误报一次按下（Seed the state machine）.
  now = HAL_GetTick();
  s_pressed = Key_Read_Raw();
  s_sample = s_pressed;
  s_change_tick = now;
  s_press_tick = now;
  s_repeat_tick = now;
  s_long_reported = 0;
}

KeyEvent Key_Scan(void) {
  uint32_t now = HAL_GetTick();
  uint8_t raw = Key_Read_Raw();

  /// 1. 原始电平变化就重新计时，抖动期间不会产生事件（Restart the debounce timer）.
  if (raw != s_sample) {
    s_sample = raw;
    s_change_tick = now;
  }

  /// 2. 电平稳定超过消抖时间才认为状态真的变了（Accept the new state）.
  if ((now - s_change_tick) >= KEY_DEBOUNCE_MS && s_pressed != s_sample) {
    s_pressed = s_sample;
    if (s_pressed) {
      s_press_tick = now;
      s_repeat_tick = now;
      s_long_reported = 0;
      return KEY_EVENT_PRESS;
    }
    return KEY_EVENT_RELEASE;
  }

  /// 电平刚变化、还没稳定满消抖时间时不产生长按/连发事件，
  /// 否则手指松开的那 20 ms 抖动窗口里可能多报一次 REPEAT
  /// （Wait for the debounce to settle before long press / repeat）.
  if (s_sample != s_pressed) {
    return KEY_EVENT_NONE;
  }

  if (!s_pressed) {
    return KEY_EVENT_NONE;
  }

  /// 3. 按住超过长按门限先报一次长按（Report the long press once）.
  if (!s_long_reported) {
    if ((now - s_press_tick) >= KEY_LONG_PRESS_MS) {
      s_long_reported = 1;
      s_repeat_tick = now;
      return KEY_EVENT_LONG_PRESS;
    }
    return KEY_EVENT_NONE;
  }

  /// 4. 长按之后按固定周期连发，便于连续调节参数（Auto repeat）.
  if ((now - s_repeat_tick) >= KEY_REPEAT_MS) {
    s_repeat_tick = now;
    return KEY_EVENT_REPEAT;
  }

  return KEY_EVENT_NONE;
}

uint8_t Key_Is_Pressed(void) {
  return s_pressed;
}
