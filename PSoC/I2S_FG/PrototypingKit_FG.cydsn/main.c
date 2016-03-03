/* ========================================
 *
 * Copyright YOUR COMPANY, THE YEAR
 * All Rights Reserved
 * UNPUBLISHED, LICENSED SOFTWARE.
 *
 * CONFIDENTIAL AND PROPRIETARY INFORMATION
 * WHICH IS THE PROPERTY OF your company.
 *
 * 2016.03.01 created
 *
 * ========================================
*/
#include <project.h>
#include <stdio.h>
#include "wavetable32_0_9_32768.h"
#include "SPLC792-I2C.h"

#define DDS_ONLY    0

#define TITLE_STR1  ("I2S FG  ")
#define TITLE_STR2  ("20160301")

// Defines for DDS
#define SAMPLE_CLOCK    312500u
#define FREQUENCY_INIT  10000

#define TABLE_SIZE      32768
#define BUFFER_SIZE     4     

/* Defines for DMA_0 */
#define DMA_0_BYTES_PER_BURST 1
#define DMA_0_REQUEST_PER_BURST 1
#define DMA_0_SRC_BASE (CYDEV_SRAM_BASE)
#define DMA_0_DST_BASE (CYDEV_PERIPH_BASE)

// Defines for UI
#define CMD_LCD_ON  10
#define CMD_LCD_OFF 11
#define CMD_CLR     12
#define CMD_ENT     13
#define CMD_MENU    14
#define CMD_BATTERY 15

#define KEY_BUFFER_LENGTH   8

#define LCD_ON  1
#define LCD_OFF 0
#define LCD_CONTRAST_INIT   32
#define LCD_CONTRAST_LIMIT  63

#define ATTENUATE_LIMIT 16

/* Variable declarations for DMA_0 */
/* Move these variable declarations to the top of the function */
uint8 DMA_0_Chan;
uint8 DMA_0_TD[1];

volatile uint8 waveBuffer_0[BUFFER_SIZE];
volatile uint32 tuningWord_0;
volatile uint32 phaseRegister_0 = 0;

volatile int frequency;
volatile int8 attenuate;

const int POW10[] = {
    1, 10, 100, 1000, 10000, 100000, 1000000, 10000000    
};

int keyBuffer[KEY_BUFFER_LENGTH] = { 0 };
int kbp = 0;
int lcdStat = 0;
int lcdContrast = LCD_CONTRAST_INIT;

int32 supplyVoltage;

void setDDSParameter_0(uint32 frequency)
{
    tuningWord_0 = (((uint64)frequency << 32) / SAMPLE_CLOCK);
}

void generateWave_0()
{
    int i, index;
    int32 v;
    
    // 波形をバッファに転送
    // Todo: BUFFER_SIZEが4の場合はforループを外す。
    for (i = 0; i < BUFFER_SIZE; i+=4) {
        phaseRegister_0 += tuningWord_0;
        // テーブルの要素数 = 2^n として 32 - n で右シフト
        // ex)
        //  1024  = 2^10 : 32 - 10 = 22
        //  32768 = 2^15 : 32 - 15 = 17 
        index = phaseRegister_0 >> 17;

        // 右シフトで出力レベルを減衰
        v = (sineTable[index] >> attenuate);
        *((uint32 *)waveBuffer_0) = __REV(v);
    }
}

CY_ISR (dma_0_done_handler)
{
    //Pin_Check_0_Write(1u);
    generateWave_0();
    //Pin_Check_0_Write(0u);
}

#if !DDS_ONLY
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
        LCD_Init(0x3e, lcdContrast);        
        CyDelay(1);
        lcdStat = LCD_ON;
    } else {
        Pin_LCD_SW_Write(0u);
        Pin_I2C_RST_Write(0u);
        lcdStat = LCD_OFF;
    }
}

//-------------------------------------------------
// readRE(): ロータリーエンコーダの読み取り
// return: ロータリーエンコーダーの回転方向
//         0:変化なし 1:時計回り -1:反時計回り
//
int readRE()
{
    static uint8_t index;
    uint8_t rd = 0;
    int retval = 0;
    
    rd = Pin_RE1_Read();

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

//------------------------------------------------------
// keyPadScan1(): 押されているキー
// return: キースキャンの結果 0..15
//         押されていない場合は -1
//
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
//         0 ..  9: 数字キー入力 結果はKeyBuffer
//         10:      A (LCD On)
//         11:      B (LCD Off) 
//         12:      C (クリア)
//         13:      D (エンター)
//         14:      * (メニュー)
//         15:      # (電源電圧)
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
    int tmp, krd, kv;

    // チャタリング防止
    tmp = keyPadScan1();
    CyDelayUs(100);
    krd = keyPadScan1();
    
    if (tmp == krd && krd != prevKey) {
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
            case CMD_BATTERY:
                break;
            }
        }
        return kv;
    }
    return -1;
}

//-------------------------------------------------
// measureSupplyVoltage(): 電源電圧の測定
// return: 電圧値(mV)
//
int measureSupplyVoltage()
{
    int16 sample, mv;
    sample = ADC_DelSig_Supply_Read16();
    mv = ADC_DelSig_Supply_CountsTo_mVolts(sample * 2);
    return mv;
}

#endif // DDS_ONLY

//------------------------------------------------------
// メイン・ルーチン
//
int main()
{
    char buff1[10];
    char buff2[10];
    int i, v, key, re_v;
    
    frequency = FREQUENCY_INIT;
    attenuate = 0;
    
    setDDSParameter_0(frequency);
    generateWave_0();
    
    CyGlobalIntEnable; /* Enable global interrupts. */

#if !DDS_ONLY
    // Init I2C LCD
    //
    I2CM_LCD_Start();
    switchLCD(LCD_ON);
    
    LCD_Puts(TITLE_STR1);
    LCD_SetPos(0, 1);
    LCD_Puts(TITLE_STR2);
    
    CyDelay(1000);
    LCD_Clear();
    
    // Init ADC
    //
    ADC_DelSig_Supply_Start();
    
#endif // DDS_ONLY

    // Init I2S DAC
    //
    I2S_1_Start();

    /* DMA Configuration for DMA_0 */
    DMA_0_Chan = DMA_0_DmaInitialize(DMA_0_BYTES_PER_BURST, DMA_0_REQUEST_PER_BURST, 
        HI16(DMA_0_SRC_BASE), HI16(DMA_0_DST_BASE));
    DMA_0_TD[0] = CyDmaTdAllocate();
    CyDmaTdSetConfiguration(DMA_0_TD[0], BUFFER_SIZE, DMA_0_TD[0], DMA_0__TD_TERMOUT_EN | TD_INC_SRC_ADR);
    CyDmaTdSetAddress(DMA_0_TD[0], LO16((uint32)waveBuffer_0), LO16((uint32)I2S_1_TX_CH0_F0_PTR));
    CyDmaChSetInitialTd(DMA_0_Chan, DMA_0_TD[0]);
    CyDmaChEnable(DMA_0_Chan, 1);

    ISR_DMA_0_Done_StartEx(dma_0_done_handler);
    
    while(0u != (I2S_1_ReadTxStatus() & (I2S_1_TX_FIFO_0_NOT_FULL)))
    {
        /* Wait for TxDMA to fill Tx FIFO */
    }
    CyDelay(1);

    I2S_1_EnableTx();

    for(;;)
    {
#if !DDS_ONLY        
        // ToDo: keyPadScan()と分岐を整理
        key = keyPadScan();
        if ((0 <= key && key <= 9) || (key == CMD_CLR) || (key == CMD_ENT)) {
            v = 0;
            for (i = 0; i < kbp; i++) {
                v += keyBuffer[i] * POW10[kbp - 1 - i];
            }

            if (key == CMD_ENT) {
                frequency = v;
                setDDSParameter_0(frequency);
                kbp = 0;
            }
            sprintf(buff1, "%08d", v); 
        }
        
        if (key == CMD_MENU) {
            sprintf(buff1, "MENU?   ");
        }
        
        supplyVoltage = measureSupplyVoltage();
        if (key == CMD_BATTERY) {
            sprintf(buff1, "%6ldmV", supplyVoltage);
        }
        sprintf(buff2, "%2d%6d", lcdContrast, frequency);
        /*
        attenuate -= readRE();
        if (attenuate < 0) {
            attenuate = 0;
        } else if (attenuate > ATTENUATE_LIMIT) {
            attenuate = ATTENUATE_LIMIT;
        }
        */

        re_v = readRE();
        if (re_v != 0) {
            lcdContrast += re_v;
            if (lcdContrast < 0) {
                lcdContrast = 0;
            } else if (lcdContrast > LCD_CONTRAST_LIMIT) {
                lcdContrast = LCD_CONTRAST_LIMIT;
            }
            LCD_SetContrast(lcdContrast);
        }
        
        if (lcdStat != LCD_ON && key == CMD_LCD_ON) {
            switchLCD(LCD_ON);
            lcdStat = LCD_ON;
        }
        if (lcdStat != LCD_OFF && key == CMD_LCD_OFF){
            switchLCD(LCD_OFF);
            lcdStat = LCD_OFF;
        }

        // ToDo: LCD書き換えの条件を修正: attenuateの更新時
        if (lcdStat != LCD_OFF && key != -1) {
            LCD_SetPos(0, 0);
            LCD_Puts(buff1);
            LCD_SetPos(0, 1);
            LCD_Puts(buff2);
        }
        //CyDelay(100);
#endif // DDS_ONLY        
    }
}

/* [] END OF FILE */
