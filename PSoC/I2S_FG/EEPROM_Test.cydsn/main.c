/* ========================================
 *
 * Copyright YOUR COMPANY, THE YEAR
 * All Rights Reserved
 * UNPUBLISHED, LICENSED SOFTWARE.
 *
 * CONFIDENTIAL AND PROPRIETARY INFORMATION
 * WHICH IS THE PROPERTY OF your company.
 *
 * ========================================
*/
#include <project.h>
#include <stdio.h>

#define USED_EEPROM_SECTOR      (1u)
#define EEPROM_FREQUENCY        ((USED_EEPROM_SECTOR * CYDEV_EEPROM_SECTOR_SIZE) + 0x00)
#define EEPROM_ATTENUATE        ((USED_EEPROM_SECTOR * CYDEV_EEPROM_SECTOR_SIZE) + 0x04)
#define EEPROM_WAVEFORM         ((USED_EEPROM_SECTOR * CYDEV_EEPROM_SECTOR_SIZE) + 0x05)

volatile int frequency;
volatile int8 attenuate;
volatile int waveForm;

//------------------------------------------------------
// ParameterをEEPROMに読み書き
//
void eepromWriteInt32(int v, uint16 address)
{
    EEPROM_WriteByte( v        & 0xff, address    );
    EEPROM_WriteByte((v >>  8) & 0xff, address + 1);
    EEPROM_WriteByte((v >> 16) & 0xff, address + 2);
    EEPROM_WriteByte((v >> 24) & 0xff, address + 3);
}

int eepromReadInt32(uint16 address)
{
    int v;
    uint8 tmp;

    tmp = EEPROM_ReadByte(address);
    v   = (int)tmp;   
    tmp = EEPROM_ReadByte(address + 1);
    v  |= (int)tmp << 8;
    tmp = EEPROM_ReadByte(address + 2);
    v  |= (int)tmp << 16;
    tmp = EEPROM_ReadByte(address + 3);
    v  |= (int)tmp << 24;

    return v;
}

void saveParametersToEEPROM()
{
    eepromWriteInt32(frequency, EEPROM_FREQUENCY);
    EEPROM_WriteByte(attenuate, EEPROM_ATTENUATE);
    eepromWriteInt32(waveForm,  EEPROM_WAVEFORM);
}

void loadParametersFromEEPROM()
{
    uint8 tmp = EEPROM_ReadByte(EEPROM_FREQUENCY);
    
    if (tmp != 0xFF) {
        frequency = eepromReadInt32(EEPROM_FREQUENCY);
        attenuate = EEPROM_ReadByte(EEPROM_ATTENUATE);
        waveForm  = eepromReadInt32(EEPROM_WAVEFORM); 
    }

    CyDelay(500);
}

int main()
{
    char buff[80];
    int cnt;
    
    CyGlobalIntEnable; /* Enable global interrupts. */

    UART_Start();
    UART_PutString("******************* Start.. ***********************\r\n");
    
    // Load Prameters from EEPROM
    EEPROM_Start();
    UART_PutString("load parameters from EEPROM.\r\n");
    loadParametersFromEEPROM();

    cnt = 0;
    for(;;)
    {
        sprintf(buff, "%d\r\n", cnt);
        UART_PutString(buff);
        cnt++;
        
        sprintf(buff, "%d\t%u\t%d\r\n", frequency, attenuate, waveForm);
        UART_PutString(buff);
        
        frequency++;
        attenuate++;
        waveForm++;

        UART_PutString("save parameters to EEPROM.\r\n");
        saveParametersToEEPROM();

        CyDelay(500);
    }
}

/* [] END OF FILE */
