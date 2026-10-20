#ifndef KEY_H
#define KEY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// 按键事件（Key events）.
typedef enum {
  KEY_EVENT_NONE = 0,   ///< 无事件（Nothing happened）.
  KEY_EVENT_PRESS,      ///< 消抖后的按下（Stable press）.
  KEY_EVENT_RELEASE,    ///< 消抖后的松开（Stable release）.
  KEY_EVENT_LONG_PRESS, ///< 按住达到长按时间（Long press reached）.
  KEY_EVENT_REPEAT      ///< 长按之后周期连发（Auto repeat while held）.
} KeyEvent;

/// 初始化按键引脚 PA5（Configure the key pin）.
void Key_Init(void);

/// 扫描按键，必须在主循环里反复调用（Poll the key, call it in the main loop）.
///
/// 内部按 HAL_GetTick() 计时做软件消抖，调用周期建议 5~10 ms；
/// 返回本次采样产生的事件，没有事件时返回 KEY_EVENT_NONE。
KeyEvent Key_Scan(void);

/// 当前消抖后的按键状态，1 = 按下（Current debounced state）.
uint8_t Key_Is_Pressed(void);

#ifdef __cplusplus
}
#endif

#endif /* KEY_H */
