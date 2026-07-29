#ifndef __OLED_EX_H
#define __OLED_EX_H

#include "OLED.h"
//#include <stdint.h>

/* ======================== 显示规格定义 ======================== */
#define OLED_WIDTH      128     /* 像素宽度 */
#define OLED_HEIGHT     64      /* 像素高度 */
#define OLED_LINES      4       /* 文本行数 (8x16字体) */
#define OLED_COLUMNS    16      /* 文本列数 (8x16字体) */

/* ======================== 浮点数显示 ======================== */
/* 在指定位置显示浮点数(无符号)，Decimals为小数位数 */
void OLED_ShowFloat(uint8_t Line, uint8_t Column, float Number, uint8_t Decimals);
/* 在指定位置显示带符号浮点数，自动添加+/-号 */
void OLED_ShowSignedFloat(uint8_t Line, uint8_t Column, float Number, uint8_t Decimals);

/* ======================== 带标签数值显示 ======================== */
/* 显示 "Label+Value+Unit" 格式，如 "V1:+3.14V" */
void OLED_ShowLabelValue(uint8_t Line, uint8_t Column, char *Label,
                         int32_t Value, uint8_t Length, char *Unit);
/* 显示 "Label+Float+Unit" 格式，如 "V2: 3.14V" */
void OLED_ShowLabelFloat(uint8_t Line, uint8_t Column, char *Label,
                         float Value, uint8_t Decimals, char *Unit);

/* ======================== 反白(高亮)显示 ======================== */
/* 显示单个反白字符(黑底白字)，用于菜单高亮 */
void OLED_ShowCharInv(uint8_t Line, uint8_t Column, char Char);
/* 显示反白字符串 */
void OLED_ShowStringInv(uint8_t Line, uint8_t Column, char *String);

/* ======================== GUI 组件 ======================== */
/* 显示复选框 [X] 或 [ ] */
void OLED_ShowCheckBox(uint8_t Line, uint8_t Column, uint8_t Checked);
/* 显示单选框 (X) 或 ( ) */
void OLED_ShowRadioBox(uint8_t Line, uint8_t Column, uint8_t Selected);
/* 显示菜单列表，Selected项反白高亮，Count最多4项 */
void OLED_ShowMenu(uint8_t StartLine, uint8_t Column,
                   char *Items[], uint8_t Count, uint8_t Selected);
/* 显示水平进度条，Width为字符宽度，Progress:0-100 */
void OLED_ShowProgressBar(uint8_t Line, uint8_t Column,
                          uint8_t Width, uint8_t Progress);

/* ======================== 屏幕区域控制 ======================== */
/* 清除指定行(1~4) */
void OLED_ClearLine(uint8_t Line);
/* 开启/关闭显示(不改变显存内容) */
void OLED_DisplayOn(void);
void OLED_DisplayOff(void);
/* 设置亮度(对比度) 0~255 */
void OLED_SetBrightness(uint8_t Brightness);
/* 开启/关闭反色显示(整个屏幕) */
void OLED_ReverseOn(void);
void OLED_ReverseOff(void);

/* ======================== 基于显存的绘图函数 ======================== */
/* 注意: 使用绘图函数前应先 OLED_Buffer_Clear()，绘图后调用 OLED_Buffer_Update() 刷新。
   绘图函数与直接文本函数(OLED_ShowChar等)混用会导致显存不同步。 */

/* 清空绘图缓冲区 */
void OLED_Buffer_Clear(void);
/* 设置/清除单个像素点，Color: 1=亮, 0=灭 */
void OLED_Buffer_SetPixel(uint8_t X, uint8_t Y, uint8_t Color);
/* 画水平线 */
void OLED_Buffer_DrawHLine(uint8_t X, uint8_t Y, uint8_t Width);
/* 画垂直线 */
void OLED_Buffer_DrawVLine(uint8_t X, uint8_t Y, uint8_t Height);
/* 画任意直线(Bresenham算法) */
void OLED_Buffer_DrawLine(uint8_t X1, uint8_t Y1, uint8_t X2, uint8_t Y2);
/* 画矩形框 */
void OLED_Buffer_DrawRect(uint8_t X, uint8_t Y, uint8_t W, uint8_t H);
/* 画填充矩形 */
void OLED_Buffer_FillRect(uint8_t X, uint8_t Y, uint8_t W, uint8_t H);
/* 画圆形(中点画圆法) */
void OLED_Buffer_DrawCircle(uint8_t CX, uint8_t CY, uint8_t R);
/* 将绘图缓冲区内容刷新到OLED屏幕 */
void OLED_Buffer_Update(void);

/* ======================== 多尺寸字体显示 ======================== */
/* 注意: 以下函数基于绘图缓冲区操作，需先 OLED_Buffer_Clear()，后 OLED_Buffer_Update() */
/* 在缓冲区指定像素位置显示多尺寸字符 */
void OLED_Buffer_ShowChar(uint8_t X, uint8_t Y, char Char, uint8_t Size, uint8_t Mode);
/* 在缓冲区显示多尺寸字符串 */
void OLED_Buffer_ShowString(uint8_t X, uint8_t Y, char *String, uint8_t Size, uint8_t Mode);
/* Size取值: 8(6x8), 12(6x12), 16(8x16), 24(12x24) */
/* Mode: 1=正常显示, 0=反色显示 */

/* ======================== 中文字符显示 ======================== */
/* 注意: 源文件需保存为GB2312编码, 否则中文会乱码 */
/* 在缓冲区显示中文字符(16x16, GB2312编码) */
void OLED_Buffer_ShowChinese(uint8_t X, uint8_t Y, uint16_t Code, uint8_t Mode);
/* Code: GB2312编码(高字节<<8|低字节), 如 "中"=0xD6D0 */
/* Mode: 1=正常显示, 0=反色显示 */

/* 在缓冲区显示中英文混合字符串(自动识别中英文) */
void OLED_Buffer_ShowChineseStr(uint8_t X, uint8_t Y, const char *Str, uint8_t Mode);
/* Str: GB2312编码字符串, 中英文自动识别 */
/* Mode: 1=正常显示, 0=反色显示 */

/* ======================== 位图显示 ======================== */
/* 在缓冲区显示位图 */
void OLED_Buffer_ShowPicture(uint8_t X, uint8_t Y, uint8_t SizeX, uint8_t SizeY, const uint8_t *BMP, uint8_t Mode);
/* SizeX,SizeY: 图片宽高(像素); BMP: 图片数据数组; Mode: 1=正常, 0=反色 */

/* ======================== 屏幕旋转 ======================== */
/* 设置屏幕旋转180度 */
void OLED_DisplayTurn(uint8_t i);
/* i=0: 正常显示, i=1: 旋转180度 */

#endif /* __OLED_EX_H */
