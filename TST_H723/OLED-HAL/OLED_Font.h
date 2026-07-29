#ifndef __OLED_FONT_H
#define __OLED_FONT_H

#include <stdint.h>

/* ASCII 字模 */
extern const uint8_t OLED_F8x16[][16];
extern const uint8_t OLED_F6x8[][6];
extern const uint8_t OLED_F6x12[][12];
extern const uint8_t OLED_F12x24[][36];

/* 中文汉字字模 16x16 (GB2312编码索引) */
typedef struct {
    uint16_t code;  /* GB2312编码 (高字节<<8 | 低字节) */
} OLED_HzkHead_t;

extern const OLED_HzkHead_t OLED_Hzk16_Index[];
extern const uint8_t OLED_Hzk16_Data[][32];
extern const uint16_t OLED_Hzk16_Count;

#endif /* __OLED_FONT_H */
