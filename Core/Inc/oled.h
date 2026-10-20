#ifndef OLED_H
#define OLED_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// 屏幕宽度，单位像素（Display width in pixels）.
#define OLED_WIDTH 128

/// 屏幕高度，单位像素（Display height in pixels）.
#define OLED_HEIGHT 64

/// 页数：一页 = 8 行像素（Page count, one page = 8 pixel rows）.
#define OLED_PAGE_COUNT (OLED_HEIGHT / 8)

/// 初始化 OLED 并清屏（Init the panel, then clear the screen）.
void OLED_Init(void);

/// 打开显示（Turn the display on）.
void OLED_Display_On(void);

/// 关闭显示，显存内容不变（Turn the display off, RAM is kept）.
void OLED_Display_Off(void);

/// 设置对比度/亮度，0~255（Set contrast / brightness）.
void OLED_Set_Contrast(uint8_t contrast);

/// 全屏清空（Clear the whole screen）.
void OLED_Clear(void);

/// 清空一页，page 取值 0~7（Clear one page）.
void OLED_Clear_Page(uint8_t page);

/// 设置写入位置：页 0~7、列 0~127（Set the write cursor）.
void OLED_Set_Cursor(uint8_t page, uint8_t column);

/// 在指定位置显示一个 ASCII 字符，scale 为放大倍数 1~4.
///
/// 字符占 8*scale 列、scale 页，超出屏幕的部分自动裁掉（Show one ASCII char）.
void OLED_Show_Char(uint8_t page, uint8_t column, char ch, uint8_t scale);

/// 在指定位置显示字符串，scale 为放大倍数 1~4（Show a string）.
void OLED_Show_String(uint8_t page, uint8_t column, const char *str, uint8_t scale);

/// 显示有符号整数，width 为最小宽度，不足时左侧补空格（Show a signed integer）.
void OLED_Show_Int(uint8_t page, uint8_t column, int32_t value, uint8_t width,
                   uint8_t scale);

/// 显示浮点数，decimals 为小数位数 0~4（Show a float number）.
void OLED_Show_Float(uint8_t page, uint8_t column, float value, uint8_t decimals,
                     uint8_t width, uint8_t scale);

/// 格式化显示，用法同 printf，输出最长 31 字符（Formatted output like printf）.
void OLED_Printf(uint8_t page, uint8_t column, uint8_t scale, const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif /* OLED_H */
