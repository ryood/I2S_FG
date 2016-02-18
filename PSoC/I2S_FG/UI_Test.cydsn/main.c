/* ========================================
 *
 * Copyright YOUR COMPANY, THE YEAR
 * All Rights Reserved
 * UNPUBLISHED, LICENSED SOFTWARE.
 *
 * CONFIDENTIAL AND PROPRIETARY INFORMATION
 * WHICH IS THE PROPERTY OF your company.
 *
 * PSoC 5LP Prototyping Kitでは
 *  RX: P12[6]
 *  TX: P13[7]
 *
 * 2016.02.19 KeyPadで周波数の入力
 * 2016.02.19 LCD SW
 * 2016.02.15 Rotary Encoder
 * 2016.02.15 KeyPad
 * 2016.02.14 I2C LCD Test
 * 2016.02.14 Ext XTAL Test
 *
 * ========================================
*/
#include <project.h>
#include <stdio.h>

#include "SPLC792-I2C.h"

#define TITLE_STR1  ("I2S FG  ")
#define TITLE_STR2  ("20160219")

#define CMD_CLR     12
#define CMD_ENT     13
#define CMD_MENU    14

#define KEY_BUFFER_LENGTH   8

const int POW10[] = {
    1, 10, 100, 1000, 10000, 100000, 1000000, 10000000    
};

char uartBuff[80];

int keyBuffer[KEY_BUFFER_LENGTH];
int kbp;

int frequency;
int isLCDOn;

//------------------------------------------------------
// keyPadScan1(): 押されているキー
// return: キースキャンの結果 0..15
//         押されていない場合は -1
//------------------------------------------------------
int keyPadScan1()
{
    uint8 i, j, r;
    for (i = 0; i < 4; i++) {
        Pin_KeyPad_Row_Out_Write(1 << i);
        for (j = 0; j < 4; j++) {
            r = Pin_KeyPad_Col_In_Read();
            if (r & (1 << j)) {
                return (int)i * 4 + j;
            }
        }
    }
    return -1;
}

//------------------------------------------------------
// keyPadScan():
// return: -1: 変化なし
//         0 ..  9: 数字キー入力 結果はKeyBuffer1
//         10:      A (LCD On)
//         11:      B (LCD Off) 
//         12:      C (クリア)
//         13:      D (エンター)
//         14:      * (メニュー)
//         15:      # (未設定)
//
const int keyPadMatrix[] = {
     1,  2,  3, 10,
     4,  5,  6, 11,
     7,  8,  9, 12,
    14,  0, 15, 13 
};

int keyPadScan()
{
    static int prevKey = -1;
    int krd, kv;

    krd = keyPadScan1();
    if (krd != prevKey) {
        prevKey = krd;
        kv = -1;
        if (krd != -1) {
            kv = keyPadMatrix[krd];
            switch (kv) {
            case 0:
            case 1:
            case 2:
            case 3:
            case 4:
            case 5:
            case 6:
            case 7:
            case 8:
            case 9:
                if (kbp < KEY_BUFFER_LENGTH) {
                    keyBuffer[kbp] = kv;
                    kbp++;
                }
                break;
            case CMD_CLR:
                kbp = 0;
                break;
            case CMD_MENU:
                break;
            case CMD_ENT:
                break;
            }
        }
        return kv;
    }
    return -1;
}

//-------------------------------------------------
// ロータリーエンコーダの読み取り
// return: ロータリーエンコーダーの回転方向
//         0:変化なし 1:時計回り -1:反時計回り
//
int readRE()
{
    static uint8_t index;
    uint8_t rd = 0;
    int retval = 0;
    
    rd = Pin_RE_Read();

    index = (index << 2) | rd;
	index &= 0b1111;

    // Alps EC12E24208
    switch (index) {
    case 0b1101:
        retval = -1;
        break;
    case 0b1000:
        retval = 1;
        break;
    }
    return retval;
}

//-------------------------------------------------
// LCDのOn/Off
// parameter: sw: !0:on 0:off
//
void switchLCD(int swOn)
{
    if (0 != swOn) {
        Pin_LCD_SW_Write(1u);
        
        Pin_I2C_RST_Write(0u);
        CyDelay(1);
        Pin_I2C_RST_Write(1u);
        
        // akizuki AQM0802(3.3V)
        LCD_Init(0x3e, 32);        
        CyDelay(1);
    } else {
        Pin_LCD_SW_Write(0u);
        Pin_I2C_RST_Write(0u);
    }
}

int main()
{
    char buff1[80];
    char buff2[80];
    int i, v, key;
    
    CyGlobalIntEnable; /* Enable global interrupts. */

    UART_Start();
    UART_PutString("Start..\r\n");
    
    I2CM_LCD_Start();
    
    switchLCD(1);
    isLCDOn = 1;
    
    LCD_Puts(TITLE_STR1);
    LCD_SetPos(0, 1);
    LCD_Puts(TITLE_STR2);
    
    CyDelay(1000);
    
    for(;;)
    {
        key = keyPadScan();
        if ((0 <= key && key <= 9) || (key == CMD_CLR) || (key == CMD_ENT)) {
            v = 0;
            for (i = 0; i < kbp; i++) {
                v += keyBuffer[i] * POW10[kbp - 1 - i];
            }

            if (key == CMD_ENT) {
                frequency = v;
                kbp = 0;
            }
            sprintf(buff1, "%08d", v); 
        }
        if (key == CMD_MENU) {
            sprintf(buff1, "MENU?   ");
        }
        
        frequency += readRE();
        sprintf(buff2, "%8d", frequency);

        LCD_SetPos(0, 0);
        LCD_Puts(buff1);
        LCD_SetPos(0, 1);
        LCD_Puts(buff2);

        if (!isLCDOn && key == 10) {
            switchLCD(1);
            isLCDOn = 1;
        }
        if (isLCDOn && key == 11){
            switchLCD(0);
            isLCDOn = 0;
        }
        
        CyDelay(1);        
    }
}

/* [] END OF FILE */
