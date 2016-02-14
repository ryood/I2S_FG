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
#define TITLE_STR2  ("20160214")

int keyPadScan()
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

    /*
	switch (index) {
	// 時計回り
	case 0b0001:	// 00 -> 01
	case 0b1110:	// 11 -> 10
	    retval = 1;
	    break;
	// 反時計回り
	case 0b0010:	// 00 -> 10
	case 0b1101:	// 11 -> 01
	    retval = -1;
	    break;
    }
    */
    
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

int main()
{
    char buff1[80];
    char buff2[80];
    char buff3[80];
    int cnt, v;
    
    CyGlobalIntEnable; /* Enable global interrupts. */

    UART_Start();
    UART_PutString("Start..\r\n");
    
    
    Pin_I2C_RST_Write(0u);
    CyDelay(1);
    Pin_I2C_RST_Write(1u);
    
    I2CM_LCD_Start();
    // akizuki AQM0802(3.3V)
    LCD_Init(0x3e, 32);
    
    CyDelay(100);

    LCD_Puts(TITLE_STR1);
    LCD_SetPos(0, 1);
    LCD_Puts(TITLE_STR2);
    
    CyDelay(1000);
    
    cnt = 0;
    v = 0;
    for(;;)
    {
        //Pin_LED_Write(Pin_LED_Read() ? 0 : 1);

        cnt++;
        v += readRE();

        sprintf(buff1, "%08d", cnt);
        sprintf(buff2, "%d   ", keyPadScan());
        sprintf(buff3, "%d   ", v);

        LCD_SetPos(0, 0);
        LCD_Puts(buff1);
        LCD_SetPos(0, 1);
        LCD_Puts(buff2);
        LCD_SetPos(4, 1);
        LCD_Puts(buff3);
        
        CyDelay(1);        
    }
}

/* [] END OF FILE */
