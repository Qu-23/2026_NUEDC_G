#include "OLED_EX.h"
#include "OLED_Font.h"
#include "main.h"

/* ====== 引用OLED.c内部函数(未在OLED.h中声明) ====== */
extern void OLED_SetCursor(uint8_t Y, uint8_t X);
extern void OLED_WriteData(uint8_t Data);
extern void OLED_WriteCommand(uint8_t Command);

/* ====== 绘图显存缓冲区 8页 x 128列 = 1024字节 ====== */
static uint8_t OLED_Buffer[8][128];

/* ============================================================
 *                      浮点数显示
 * ============================================================ */

/**
  * @brief  计算浮点数显示所需的字符宽度
  * @param  Number    待显示的浮点数(已取绝对值)
  * @param  Decimals  小数位数
  * @retval 显示宽度(字符数)
  */
static uint8_t OLED_FloatWidth(float Number, uint8_t Decimals)
{
    uint8_t width = 0;
    uint32_t mul = 1;
    uint32_t scaled, int_part;
    uint8_t i;

    for (i = 0; i < Decimals; i++)
        mul *= 10;

    scaled = (uint32_t)(Number * (float)mul + 0.5f);
    int_part = scaled / mul;

    /* 整数部分位数(至少1位) */
    do {
        width++;
        int_part /= 10;
    } while (int_part);

    /* 小数点 + 小数部分 */
    if (Decimals > 0)
        width += 1 + Decimals;

    return width;
}

/**
  * @brief  OLED显示浮点数(无符号)
  * @param  Line     行位置 1~4
  * @param  Column   列位置 1~16
  * @param  Number   要显示的浮点数
  * @param  Decimals 小数位数 0~6
  * @retval 无
  * @note   Number*10^Decimals 不应超过4294967295(uint32_t上限)
  *         即: Number<4294 且 Decimals<=6，或 Number<42949 且 Decimals<=3
  */
void OLED_ShowFloat(uint8_t Line, uint8_t Column, float Number, uint8_t Decimals)
{
    uint8_t col = Column;
    uint8_t i;
    uint32_t mul = 1;
    uint32_t scaled, int_part, frac_part;
    uint8_t int_len;
    uint32_t temp;

    if (Decimals > 6)
        Decimals = 6;

    /* 处理负数 */
    if (Number < 0)
    {
        OLED_ShowChar(Line, col, '-');
        col++;
        Number = -Number;
    }

    /* 计算10的Decimals次方 */
    for (i = 0; i < Decimals; i++)
        mul *= 10;

    /* 缩放并四舍五入，避免浮点截断误差 */
    scaled = (uint32_t)(Number * (float)mul + 0.5f);
    int_part  = scaled / mul;
    frac_part = scaled % mul;

    /* 计算整数部分位数 */
    int_len = 0;
    temp = int_part;
    do {
        int_len++;
        temp /= 10;
    } while (temp);

    /* 显示整数部分 */
    OLED_ShowNum(Line, col, int_part, int_len);
    col += int_len;

    /* 显示小数点和小数部分 */
    if (Decimals > 0)
    {
        OLED_ShowChar(Line, col, '.');
        col++;
        OLED_ShowNum(Line, col, frac_part, Decimals);
    }
}

/**
  * @brief  OLED显示带符号浮点数
  * @param  Line     行位置 1~4
  * @param  Column   列位置 1~16
  * @param  Number   要显示的浮点数
  * @param  Decimals 小数位数 0~6
  * @retval 无
  */
void OLED_ShowSignedFloat(uint8_t Line, uint8_t Column, float Number, uint8_t Decimals)
{
    if (Number >= 0)
    {
        OLED_ShowFloat(Line, Column, Number, Decimals);
    }
    else
    {
        OLED_ShowChar(Line, Column, '-');
        OLED_ShowFloat(Line, Column + 1, -Number, Decimals);
    }
}

/* ============================================================
 *                    带标签数值显示
 * ============================================================ */

/**
  * @brief  显示"标签+整数+单位"，如 "V1:+012V"
  * @param  Line    行位置
  * @param  Column  起始列
  * @param  Label   标签字符串，如 "V1:"
  * @param  Value   整数值
  * @param  Length  整数显示长度(不含符号)
  * @param  Unit    单位字符串，如 "V"，传NULL则不显示
  * @retval 无
  */
void OLED_ShowLabelValue(uint8_t Line, uint8_t Column, char *Label,
                         int32_t Value, uint8_t Length, char *Unit)
{
    uint8_t col = Column;
    uint8_t i;

    /* 显示标签 */
    for (i = 0; Label[i] != '\0'; i++)
    {
        OLED_ShowChar(Line, col, Label[i]);
        col++;
    }

    /* 显示带符号数值 */
    OLED_ShowSignedNum(Line, col, Value, Length);
    col += Length + (Value < 0 ? 1 : 0);  /* 负数多占1位(负号) */

    /* 显示单位 */
    if (Unit != NULL)
    {
        OLED_ShowString(Line, col, Unit);
    }
}

/**
  * @brief  显示"标签+浮点数+单位"，如 "V2: 3.14V"
  * @param  Line     行位置
  * @param  Column   起始列
  * @param  Label    标签字符串
  * @param  Value    浮点数值
  * @param  Decimals 小数位数
  * @param  Unit     单位字符串，传NULL则不显示
  * @retval 无
  */
void OLED_ShowLabelFloat(uint8_t Line, uint8_t Column, char *Label,
                         float Value, uint8_t Decimals, char *Unit)
{
    uint8_t col = Column;
    uint8_t i;

    /* 显示标签 */
    for (i = 0; Label[i] != '\0'; i++)
    {
        OLED_ShowChar(Line, col, Label[i]);
        col++;
    }

    /* 显示浮点数 */
    OLED_ShowFloat(Line, col, Value, Decimals);
    col += OLED_FloatWidth((Value >= 0) ? Value : -Value, Decimals);
    if (Value < 0)
        col++;  /* 负号占一位 */

    /* 显示单位 */
    if (Unit != NULL)
    {
        OLED_ShowString(Line, col, Unit);
    }
}

/* ============================================================
 *                      反白(高亮)显示
 * ============================================================ */

/**
  * @brief  显示单个反白字符(黑底白字)
  * @param  Line    行位置 1~4
  * @param  Column  列位置 1~16
  * @param  Char    要显示的字符
  * @retval 无
  */
void OLED_ShowCharInv(uint8_t Line, uint8_t Column, char Char)
{
    uint8_t i;
    OLED_SetCursor((Line - 1) * 2, (Column - 1) * 8);
    for (i = 0; i < 8; i++)
    {
        OLED_WriteData(~OLED_F8x16[Char - ' '][i]);
    }
    OLED_SetCursor((Line - 1) * 2 + 1, (Column - 1) * 8);
    for (i = 0; i < 8; i++)
    {
        OLED_WriteData(~OLED_F8x16[Char - ' '][i + 8]);
    }
}

/**
  * @brief  显示反白字符串
  * @param  Line    行位置 1~4
  * @param  Column  起始列 1~16
  * @param  String  要显示的字符串
  * @retval 无
  */
void OLED_ShowStringInv(uint8_t Line, uint8_t Column, char *String)
{
    uint8_t i;
    for (i = 0; String[i] != '\0'; i++)
    {
        OLED_ShowCharInv(Line, Column + i, String[i]);
    }
}

/* ============================================================
 *                        GUI 组件
 * ============================================================ */

/**
  * @brief  显示复选框
  * @param  Line    行位置
  * @param  Column  列位置
  * @param  Checked 1=勾选[X], 0=未勾选[ ]
  * @retval 无
  */
void OLED_ShowCheckBox(uint8_t Line, uint8_t Column, uint8_t Checked)
{
    if (Checked)
        OLED_ShowString(Line, Column, "[X]");
    else
        OLED_ShowString(Line, Column, "[ ]");
}

/**
  * @brief  显示单选框
  * @param  Line     行位置
  * @param  Column   列位置
  * @param  Selected 1=选中(X), 0=未选( )
  * @retval 无
  */
void OLED_ShowRadioBox(uint8_t Line, uint8_t Column, uint8_t Selected)
{
    if (Selected)
        OLED_ShowString(Line, Column, "(X)");
    else
        OLED_ShowString(Line, Column, "( )");
}

/**
  * @brief  显示菜单列表，选中项反白高亮
  * @param  StartLine 起始行 1~4
  * @param  Column    列位置
  * @param  Items     菜单项字符串数组
  * @param  Count     菜单项数量
  * @param  Selected  当前选中项索引(0基)
  * @retval 无
  * @note   最多显示4项(受屏幕行数限制)
  */
void OLED_ShowMenu(uint8_t StartLine, uint8_t Column,
                   char *Items[], uint8_t Count, uint8_t Selected)
{
    uint8_t i;
    if (Count > 4)
        Count = 4;

    for (i = 0; i < Count; i++)
    {
        if (i == Selected)
            OLED_ShowStringInv(StartLine + i, Column, Items[i]);
        else
            OLED_ShowString(StartLine + i, Column, Items[i]);
    }
}

/**
  * @brief  显示水平进度条(占用一个文本行高度，16像素)
  * @param  Line     行位置 1~4
  * @param  Column    起始列 1~16
  * @param  Width     宽度(字符单位，每个8像素)
  * @param  Progress  进度 0~100
  * @retval 无
  */
void OLED_ShowProgressBar(uint8_t Line, uint8_t Column,
                          uint8_t Width, uint8_t Progress)
{
    uint8_t page_top = (Line - 1) * 2;
    uint8_t page_bot = page_top + 1;
    uint8_t x_start  = (Column - 1) * 8;
    uint8_t x_end   = x_start + Width * 8 - 1;
    uint8_t total_px = Width * 8;
    uint16_t fill_px = (uint16_t)Progress * total_px / 100;
    uint8_t x;
    uint8_t data;

    /* 绘制上半页 */
    OLED_SetCursor(page_top, x_start);
    for (x = x_start; x <= x_end; x++)
    {
        if (x == x_start || x == x_end)
            data = 0xFF;        /* 左右边框 */
        else if ((x - x_start) < fill_px)
            data = 0xFF;        /* 填充区域 */
        else
            data = 0x01;        /* 上边框线 */
        OLED_WriteData(data);
    }

    /* 绘制下半页 */
    OLED_SetCursor(page_bot, x_start);
    for (x = x_start; x <= x_end; x++)
    {
        if (x == x_start || x == x_end)
            data = 0xFF;        /* 左右边框 */
        else if ((x - x_start) < fill_px)
            data = 0xFF;        /* 填充区域 */
        else
            data = 0x80;        /* 下边框线 */
        OLED_WriteData(data);
    }
}

/* ============================================================
 *                      屏幕区域控制
 * ============================================================ */

/**
  * @brief  清除指定行
  * @param  Line 行位置 1~4
  * @retval 无
  */
void OLED_ClearLine(uint8_t Line)
{
    uint8_t i;
    OLED_SetCursor((Line - 1) * 2, 0);
    for (i = 0; i < 128; i++)
        OLED_WriteData(0x00);
    OLED_SetCursor((Line - 1) * 2 + 1, 0);
    for (i = 0; i < 128; i++)
        OLED_WriteData(0x00);
}

/**
  * @brief  开启OLED显示
  */
void OLED_DisplayOn(void)
{
    OLED_WriteCommand(0xAF);
}

/**
  * @brief  关闭OLED显示(不改变显存内容)
  */
void OLED_DisplayOff(void)
{
    OLED_WriteCommand(0xAE);
}

/**
  * @brief  设置OLED亮度(对比度)
  * @param  Brightness 亮度值 0~255
  */
void OLED_SetBrightness(uint8_t Brightness)
{
    OLED_WriteCommand(0x81);
    OLED_WriteCommand(Brightness);
}

/**
  * @brief  开启整屏反色显示
  */
void OLED_ReverseOn(void)
{
    OLED_WriteCommand(0xA7);
}

/**
  * @brief  关闭整屏反色显示(恢复正常)
  */
void OLED_ReverseOff(void)
{
    OLED_WriteCommand(0xA6);
}

/* ============================================================
 *                  基于显存的绘图函数
 * ============================================================ */

/**
  * @brief  清空绘图缓冲区
  */
void OLED_Buffer_Clear(void)
{
    uint8_t i, j;
    for (i = 0; i < 8; i++)
        for (j = 0; j < 128; j++)
            OLED_Buffer[i][j] = 0x00;
}

/**
  * @brief  在缓冲区中设置/清除像素点
  * @param  X     X坐标 0~127
  * @param  Y     Y坐标 0~63
  * @param  Color 颜色 1=亮, 0=灭
  */
void OLED_Buffer_SetPixel(uint8_t X, uint8_t Y, uint8_t Color)
{
    if (X >= OLED_WIDTH || Y >= OLED_HEIGHT)
        return;
    if (Color)
        OLED_Buffer[Y / 8][X] |= (1 << (Y % 8));
    else
        OLED_Buffer[Y / 8][X] &= ~(1 << (Y % 8));
}

/**
  * @brief  画水平线
  * @param  X     起始X坐标
  * @param  Y     Y坐标
  * @param  Width 线宽(像素)
  */
void OLED_Buffer_DrawHLine(uint8_t X, uint8_t Y, uint8_t Width)
{
    uint8_t i;
    for (i = 0; i < Width; i++)
        OLED_Buffer_SetPixel(X + i, Y, 1);
}

/**
  * @brief  画垂直线
  * @param  X      X坐标
  * @param  Y      起始Y坐标
  * @param  Height 线高(像素)
  */
void OLED_Buffer_DrawVLine(uint8_t X, uint8_t Y, uint8_t Height)
{
    uint8_t i;
    for (i = 0; i < Height; i++)
        OLED_Buffer_SetPixel(X, Y + i, 1);
}

/**
  * @brief  画任意直线(Bresenham算法)
  * @param  X1,Y1 起点坐标
  * @param  X2,Y2 终点坐标
  */
void OLED_Buffer_DrawLine(uint8_t X1, uint8_t Y1, uint8_t X2, uint8_t Y2)
{
    int16_t dx = (X2 > X1) ? (X2 - X1) : (X1 - X2);
    int16_t dy = (Y2 > Y1) ? (Y2 - Y1) : (Y1 - Y2);
    int16_t sx = (X1 < X2) ? 1 : -1;
    int16_t sy = (Y1 < Y2) ? 1 : -1;
    int16_t err = dx - dy;
    int16_t e2;

    while (1)
    {
        OLED_Buffer_SetPixel(X1, Y1, 1);
        if (X1 == X2 && Y1 == Y2)
            break;
        e2 = 2 * err;
        if (e2 > -dy) { err -= dy; X1 += sx; }
        if (e2 <  dx) { err += dx; Y1 += sy; }
    }
}

/**
  * @brief  画矩形框
  * @param  X,Y 左上角坐标
  * @param  W   宽度(像素)
  * @param  H   高度(像素)
  */
void OLED_Buffer_DrawRect(uint8_t X, uint8_t Y, uint8_t W, uint8_t H)
{
    if (W == 0 || H == 0)
        return;
    OLED_Buffer_DrawHLine(X, Y, W);
    OLED_Buffer_DrawHLine(X, Y + H - 1, W);
    OLED_Buffer_DrawVLine(X, Y, H);
    OLED_Buffer_DrawVLine(X + W - 1, Y, H);
}

/**
  * @brief  画填充矩形
  * @param  X,Y 左上角坐标
  * @param  W   宽度(像素)
  * @param  H   高度(像素)
  */
void OLED_Buffer_FillRect(uint8_t X, uint8_t Y, uint8_t W, uint8_t H)
{
    uint8_t i, j;
    for (j = 0; j < H; j++)
        for (i = 0; i < W; i++)
            OLED_Buffer_SetPixel(X + i, Y + j, 1);
}

/**
  * @brief  画圆形(中点画圆法)
  * @param  CX,CY 圆心坐标
  * @param  R      半径
  */
void OLED_Buffer_DrawCircle(uint8_t CX, uint8_t CY, uint8_t R)
{
    int16_t x = R;
    int16_t y = 0;
    int16_t err = 0;

    while (x >= y)
    {
        OLED_Buffer_SetPixel(CX + x, CY + y, 1);
        OLED_Buffer_SetPixel(CX + y, CY + x, 1);
        OLED_Buffer_SetPixel(CX - y, CY + x, 1);
        OLED_Buffer_SetPixel(CX - x, CY + y, 1);
        OLED_Buffer_SetPixel(CX - x, CY - y, 1);
        OLED_Buffer_SetPixel(CX - y, CY - x, 1);
        OLED_Buffer_SetPixel(CX + y, CY - x, 1);
        OLED_Buffer_SetPixel(CX + x, CY - y, 1);

        y++;
        if (err <= 0)
        {
            err += 2 * y + 1;
        }
        if (err > 0)
        {
            x--;
            err -= 2 * x + 1;
        }
    }
}

/**
  * @brief  将绘图缓冲区内容刷新到OLED屏幕
  */
void OLED_Buffer_Update(void)
{
    uint8_t page, col;
    for (page = 0; page < 8; page++)
    {
        OLED_SetCursor(page, 0);
        for (col = 0; col < 128; col++)
        {
            OLED_WriteData(OLED_Buffer[page][col]);
        }
    }
}

/* ============================================================
 *                    多尺寸字体显示
 * ============================================================ */

/**
  * @brief  在缓冲区中显示多尺寸字符
  * @param  X     X坐标(像素)
  * @param  Y     Y坐标(像素)
  * @param  Char  要显示的字符
  * @param  Size  字体尺寸: 8(6x8), 12(6x12), 16(8x16), 24(12x24)
  * @param  Mode  显示模式: 1=正常(白字黑底), 0=反白(黑字白底)
  */
void OLED_Buffer_ShowChar(uint8_t X, uint8_t Y, char Char, uint8_t Size, uint8_t Mode)
{
    uint8_t i, m, temp, size2, chr1;
    uint8_t x0 = X, y0 = Y;
    
    chr1 = Char - ' ';
    
    /* 计算字模占用的字节数 */
    if (Size == 8)
        size2 = 6;  /* 6x8: 6字节 */
    else
        size2 = (Size / 8 + ((Size % 8) ? 1 : 0)) * (Size / 2);
    
    for (i = 0; i < size2; i++)
    {
        /* 根据尺寸选择字模 */
        if (Size == 8)
            temp = OLED_F6x8[chr1][i];
        else if (Size == 12)
            temp = OLED_F6x12[chr1][i];
        else if (Size == 16)
            temp = OLED_F8x16[chr1][i];
        else if (Size == 24)
            temp = OLED_F12x24[chr1][i];
        else
            return;
        
        /* 逐位写入像素 */
        for (m = 0; m < 8; m++)
        {
            if (temp & 0x01)
                OLED_Buffer_SetPixel(X, Y, Mode);
            else
                OLED_Buffer_SetPixel(X, Y, !Mode);
            temp >>= 1;
            Y++;
        }
        X++;
        if ((Size != 8) && ((X - x0) == Size / 2))
        {
            X = x0;
            y0 = y0 + 8;
        }
        Y = y0;
    }
}

/**
  * @brief  在缓冲区中显示多尺寸字符串
  * @param  X       X坐标(像素)
  * @param  Y       Y坐标(像素)
  * @param  String  要显示的字符串
  * @param  Size    字体尺寸: 8, 12, 16, 24
  * @param  Mode    显示模式: 1=正常, 0=反白
  */
void OLED_Buffer_ShowString(uint8_t X, uint8_t Y, char *String, uint8_t Size, uint8_t Mode)
{
    uint8_t x0 = X;
    while (*String >= ' ' && *String <= '~')
    {
        OLED_Buffer_ShowChar(X, Y, *String, Size, Mode);
        if (Size == 8)
            X += 6;
        else
            X += Size / 2;
        String++;
    }
    (void)x0; /* 避免未使用警告 */
}

/* ============================================================
 *                    中文字符显示
 * ============================================================ */

/**
  * @brief  在缓冲区中显示中文字符(GB2312编码, 16x16)
  * @param  X     X坐标(像素)
  * @param  Y     Y坐标(像素)
  * @param  Code  中文字符GB2312编码(高字节<<8 | 低字节)
  * @param  Mode  显示模式: 1=正常, 0=反白
  */
void OLED_Buffer_ShowChinese(uint8_t X, uint8_t Y, uint16_t Code, uint8_t Mode)
{
    uint8_t temp;
    uint16_t idx, i;
    uint8_t row, col_byte, bit;
    uint8_t Size = 16;
    uint8_t bytes_per_row = 2;

    /* 在索引表中查找 */
    for (idx = 0; idx < OLED_Hzk16_Count; idx++)
    {
        if (OLED_Hzk16_Index[idx].code == Code)
            break;
    }
    if (idx >= OLED_Hzk16_Count)
        return;

    /* 行格式: 每行2字节, 共16行, MSB在前 */
    for (row = 0; row < Size; row++)
    {
        for (col_byte = 0; col_byte < bytes_per_row; col_byte++)
        {
            i = row * bytes_per_row + col_byte;
            temp = OLED_Hzk16_Data[idx][i];

            for (bit = 0; bit < 8; bit++)
            {
                if (temp & (0x80 >> bit))
                    OLED_Buffer_SetPixel(X + col_byte * 8 + bit, Y + row, Mode);
                else
                    OLED_Buffer_SetPixel(X + col_byte * 8 + bit, Y + row, !Mode);
            }
        }
    }
}

/**
  * @brief  在缓冲区中显示中英文混合字符串(自动识别中英文)
  * @param  X       X坐标(像素)
  * @param  Y       Y坐标(像素)
  * @param  Str     要显示的字符串(GB2312编码)
  * @param  Mode    显示模式: 1=正常, 0=反白
  * @note   中文16x16, 英文8x16, 自动识别
  */
void OLED_Buffer_ShowChineseStr(uint8_t X, uint8_t Y, const char *Str, uint8_t Mode)
{
    while (*Str != '\0')
    {
        if ((uint8_t)*Str >= 0x80)
        {
            /* 中文字符: 两个字节组成GB2312编码 */
            uint16_t code = ((uint8_t)*Str << 8) | (uint8_t)*(Str + 1);
            OLED_Buffer_ShowChinese(X, Y, code, Mode);
            X += 16;
            Str += 2;
        }
        else
        {
            /* ASCII字符: 8x16 */
            OLED_Buffer_ShowChar(X, Y, *Str, 16, Mode);
            X += 8;
            Str++;
        }
    }
}

/* ============================================================
 *                    位图显示
 * ============================================================ */

/**
  * @brief  在缓冲区中显示位图
  * @param  X      X坐标(像素)
  * @param  Y      Y坐标(像素)
  * @param  SizeX  位图宽度(像素)
  * @param  SizeY  位图高度(像素)
  * @param  BMP    位图数据数组指针
  * @param  Mode   显示模式: 1=正常, 0=反白
  */
void OLED_Buffer_ShowPicture(uint8_t X, uint8_t Y, uint8_t SizeX, uint8_t SizeY, const uint8_t *BMP, uint8_t Mode)
{
    uint16_t j = 0;
    uint8_t i, n, temp, m;
    uint8_t x0 = X, y0 = Y;
    
    SizeY = SizeY / 8 + ((SizeY % 8) ? 1 : 0);
    
    for (n = 0; n < SizeY; n++)
    {
        for (i = 0; i < SizeX; i++)
        {
            temp = BMP[j];
            j++;
            for (m = 0; m < 8; m++)
            {
                if (temp & 0x01)
                    OLED_Buffer_SetPixel(X, Y, Mode);
                else
                    OLED_Buffer_SetPixel(X, Y, !Mode);
                temp >>= 1;
                Y++;
            }
            X++;
            if ((X - x0) == SizeX)
            {
                X = x0;
                y0 = y0 + 8;
            }
            Y = y0;
        }
    }
}

/* ============================================================
 *                    屏幕旋转控制
 * ============================================================ */

/**
  * @brief  OLED屏幕旋转180度
  * @param  i  0=正常显示, 1=旋转180度
  */
void OLED_DisplayTurn(uint8_t i)
{
    if (i == 0)
    {
        OLED_WriteCommand(0xC8); /* 正常显示 */
        OLED_WriteCommand(0xA1);
    }
    else
    {
        OLED_WriteCommand(0xC0); /* 旋转180度 */
        OLED_WriteCommand(0xA0);
    }
}
