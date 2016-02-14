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

int main()
{
    char buff1[80];
    char buff2[80];
    int cnt;
    
    CyGlobalIntEnable; /* Enable global interrupts. */

    UART_Start();
    UART_PutString("Start..\r\n");
    
    
    Pin_I2C_RST_Write(0u);
    CyDelay(1);
    Pin_I2C_RST_Write(1u);
    
    I2CM_LCD_Start();
    // akizuki AQM1602(3.3V)
    LCD_Init(0x3e, 32);
    
    CyDelay(100);

    LCD_Puts(TITLE_STR1);
    LCD_SetPos(0, 1);
    LCD_Puts(TITLE_STR2);
    
    CyDelay(1000);
    
    cnt = 0;
    for(;;)
    {
        Pin_LED_Write(Pin_LED_Read() ? 0 : 1);
        
        sprintf(buff1, "%08d\r\n", cnt);
        UART_PutString(buff1);
        cnt++;
        
        sprintf(buff2, "%d        \r\n", keyPadScan());
        LCD_SetPos(0, 0);
        LCD_Puts(buff1);
        LCD_SetPos(0, 1);
        LCD_Puts(buff2);
        
        CyDelay(100);        
    }
}

/* [] END OF FILE */
