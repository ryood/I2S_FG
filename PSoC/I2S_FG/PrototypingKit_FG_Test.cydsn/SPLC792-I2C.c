/* ========================================
 *
 * Copyright YOUR COMPANY, THE YEAR
 * All Rights Reserved
 * UNPUBLISHED, LICENSED SOFTWARE.
 *
 * CONFIDENTIAL AND PROPRIETARY INFORMATION
 * WHICH IS THE PROPERTY OF your company.
 *
 * 2015.11.09 PSoC5LP用にI2C Masterコンポーネント(UDB)を使用
 *
 * ========================================
*/
#include <project.h>
#include "SPLC792-I2C.h"

static uint8 LCD_I2C_Address;

uint32 LCD_Write(uint8 *buffer)
{
	uint32 status = LCD_I2C_TRANSFER_ERROR;
	
    I2CM_LCD_MasterWriteBuf(LCD_I2C_Address,
        buffer,
        LCD_I2C_PACKET_SIZE,
        I2CM_LCD_MODE_COMPLETE_XFER
    );
    while (0u == (I2CM_LCD_MasterStatus() & I2CM_LCD_MSTAT_WR_CMPLT))
    {
        /* Waits until master completes write transfer */
    }

    /* Displays transfer status */
    if (0u == (I2CM_LCD_MSTAT_ERR_XFER & I2CM_LCD_MasterStatus()))
    {
        status = LCD_I2C_TRANSFER_NOT_CMPLT;

        /* Check if all bytes was written */
        if(I2CM_LCD_MasterGetWriteBufSize() == LCD_I2C_BUFFER_SIZE)
        {
            status = LCD_I2C_TRANSFER_CMPLT;
			
			// １命令ごとに余裕を見て50usウェイトします。
			CyDelayUs(50);	
        }
    }
#if 0    
    else
    {
        RGB_LED_ON_RED;
    }
#endif
    (void) I2CM_LCD_MasterClearStatus();
	   
	return (status);
}

// コマンドを送信します。HD44780でいうRS=0に相当
void LCD_Cmd(uint8 cmd)
{
	uint8 buffer[LCD_I2C_BUFFER_SIZE];
	buffer[0] = 0b00000000;
	buffer[1] = cmd;
	(void) LCD_Write(buffer);
}

// データを送信します。HD44780でいうRS=1に相当
void LCD_Data(uint8 data)
{
	uint8 buffer[LCD_I2C_BUFFER_SIZE];
	buffer[0] = 0b01000000;
	buffer[1] = data;
	(void) LCD_Write(buffer);
}

void LCD_Init(uint8 address, uint8 contrast)
{
    LCD_I2C_Address = address;
    
	CyDelay(40);
	LCD_Cmd(0b00111000);	// function set
	LCD_Cmd(0b00111001);	// function set
	LCD_Cmd(0b00010100);	// interval osc
	LCD_Cmd(0b01110000 | (contrast & 0xF));	// contrast Low
	LCD_Cmd(0b01011100 | ((contrast >> 4) & 0x3)); // contast High/icon/power
	LCD_Cmd(0b01101100); // follower control
	CyDelay(300);
	
	LCD_Cmd(0b00111000); // function set
	LCD_Cmd(0b00001100); // Display On
}

void LCD_Clear()
{
	LCD_Cmd(0b00000001); // Clear Display
	CyDelay(2);	// Clear Displayは追加ウェイトが必要
}

void LCD_SetPos(uint32 x, uint32 y)
{
	LCD_Cmd(0b10000000 | (x + y * 0x40));
}

// （主に）文字列を連続送信します。
void LCD_Puts(char8 *s)
{
	while(*s) {
		LCD_Data((uint8)*s++);
	}
}

/* [] END OF FILE */
