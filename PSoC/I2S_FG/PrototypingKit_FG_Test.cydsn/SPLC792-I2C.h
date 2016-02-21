/* ========================================
 *
 * Copyright YOUR COMPANY, THE YEAR
 * All Rights Reserved
 * UNPUBLISHED, LICENSED SOFTWARE.
 *
 * CONFIDENTIAL AND PROPRIETARY INFORMATION
 * WHICH IS THE PROPERTY OF your company.
 *
 * "I2CM_LCD"という名前で"I2C(SCB mode)"コンポーネントを作成してください。
 *
 * 2015.11.09 PSoC5LP用にI2C Masterコンポーネント(UDB)を使用
 *
 * ========================================
*/
#ifndef _SPLC792_I2C_H_
#define _SPLC792_I2C_H_
    
#include <project.h>
    
#define LCD_I2C_BUFFER_SIZE     (2u)
#define LCD_I2C_PACKET_SIZE     (LCD_I2C_BUFFER_SIZE)
    
// I2C Command valid status
//
#define LCD_I2C_TRANSFER_CMPLT      (0x00u)
#define LCD_I2C_TRANSFER_NOT_CMPLT  (0x80u)
#define LCD_I2C_TRANSFER_ERROR      (0xFFu)
    
// LCD初期化
// parameter: address: LCDのI2Cアドレス SPLC792-I2Cの場合は"0x3e"を指定
//            contrast: LCDのコントラスト(0..63) 最初は最大にして表示を確認する
//
void LCD_Init(uint8 address, uint8 contrast);
void LCD_Clear();
void LCD_SetPos(uint32 x, uint32 y);
void LCD_Puts(char8 *s);

#endif  //_SPLC792_I2C_H_

/* [] END OF FILE */
