/* ========================================
 *
 * Copyright YOUR COMPANY, THE YEAR
 * All Rights Reserved
 * UNPUBLISHED, LICENSED SOFTWARE.
 *
 * CONFIDENTIAL AND PROPRIETARY INFORMATION
 * WHICH IS THE PROPERTY OF your company.
 *
 * 2016.02.11 32bit Sine Table
 * 2016.01.31 32bit/384kHz
 * 2016.01.31 24bit/384kHz 
 *
 * ========================================
*/
#include <project.h>
#include "wavetable32_0_9_32768.h"

#define SAMPLE_CLOCK    192000u

#define TABLE_SIZE      32768
#define BUFFER_SIZE     4     

/* Defines for DMA_0 */
#define DMA_0_BYTES_PER_BURST 1
#define DMA_0_REQUEST_PER_BURST 1
#define DMA_0_SRC_BASE (CYDEV_SRAM_BASE)
#define DMA_0_DST_BASE (CYDEV_PERIPH_BASE)
/* Defines for DMA_1 */
/*
#define DMA_1_BYTES_PER_BURST 1
#define DMA_1_REQUEST_PER_BURST 1
#define DMA_1_SRC_BASE (CYDEV_SRAM_BASE)
#define DMA_1_DST_BASE (CYDEV_PERIPH_BASE)
*/

/* Variable declarations for DMA_0 */
/* Move these variable declarations to the top of the function */
uint8 DMA_0_Chan;
uint8 DMA_0_TD[1];
/* Variable declarations for DMA_1 */
/* Move these variable declarations to the top of the function */
/*
uint8 DMA_1_Chan;
uint8 DMA_1_TD[1];
*/

volatile uint8 waveBuffer_0[BUFFER_SIZE];
//volatile uint8 waveBuffer_1[BUFFER_SIZE];

volatile uint32 tuningWord_0;
//volatile uint32 tuningWord_1;
volatile uint32 phaseRegister_0 = 0;
//volatile uint32 phaseRegister_1 = 0;

const uint32 frequencyMajTable[] = {
    1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 
    10000, 20000, 50000, 100000, 200000, 500000
};
const int16 frequencyMnrStep[] = {
    1, 1, 1,  1,  2,  5,  10,  20,  50,  100,  200,  500,
    1000,   2000,  5000,  10000,  20000,  50000
};
#define FREQUENCY_INIT  50000
#define FREQUENCY_MAJ_INDEX_INIT    14
#define FREQUENCY_TABLE_LENGTH \
    ((int)(sizeof(frequencyMajTable)/sizeof(frequencyMajTable[1])))

void setDDSParameter_0(uint32 frequency)
{
    tuningWord_0 = (((uint64)frequency << 32) / SAMPLE_CLOCK);
}

/*
void setDDSParameter_1(uint16 frequency)
{
    tuningWord_1 = (((uint64)frequency << 32) / SAMPLE_CLOCK);
}
*/
void generateWave_0()
{
    int i, index, v;
    uint8* p8;
    
    // 波形をバッファに転送
    for (i = 0; i < BUFFER_SIZE; i+=4) {
        phaseRegister_0 += tuningWord_0;
        // テーブルの要素数=2^n として 32 - n で右シフト
        // 1024  = 2^10 : 32 - 10 = 22
        // 32768 = 2^15 : 32 - 15 = 17 
        index = phaseRegister_0 >> 17;
        
        p8 = (uint8 *)(sineTable + index);
        //v = (sineTable[index] >> 1);
        //p8 = (uint8 *)(&v);
        waveBuffer_0[i]   = *(p8 + 3);
        waveBuffer_0[i+1] = *(p8 + 2);
        waveBuffer_0[i+2] = *(p8 + 1);
        waveBuffer_0[i+3] = *p8;
    }
}

/*
void generateWave_1()
{
    int i, index;
    uint8* p8;
    
    // 波形をバッファに転送
    for (i = 0; i < BUFFER_SIZE; i+=2) {
        phaseRegister_1 += tuningWord_1;
        index = phaseRegister_1 >> 22;
        
        p8 = (uint8 *)(sineTable + index);
        waveBuffer_1[i]   = *(p8 + 1);
        waveBuffer_1[i+1] = *p8;
    }
}
*/
CY_ISR (dma_0_done_handler)
{
    Pin_Check_0_Write(1u);
    generateWave_0();
    Pin_Check_0_Write(0u);
}

/*
CY_ISR (dma_1_done_handler)
{
    Pin_Check_1_Write(1u);
    generateWave_1();
    Pin_Check_1_Write(0u);
}
*/

CY_ISR (i2s_1_tx_handler)
{
    if (I2S_1_ReadTxStatus() & I2S_1_TX_FIFO_UNDERFLOW) {
        // Underflow Alert
        Pin_Check_1_Write(1u);
        Pin_Check_1_Write(0u);
    }
}

//-------------------------------------------------------------
// Rotary Encoder
//
//-------------------------------------------------------------
//-------------------------------------------------
// ロータリーエンコーダの読み取り
// return: ロータリーエンコーダーの回転方向
//         0:変化なし 1:時計回り -1:反時計回り
//
int readRE(int RE_n)
{
    static uint8_t index[3];
    uint8_t rd = 0;
    int retval = 0;
    
    switch (RE_n) {
    case 0:
        rd = Pin_RE0_Read();
        break;
    case 1:
        rd = Pin_RE1_Read();
        break;
    case 2:
        //rd = Pin_RE2_Read();
        break;
    default:
        //error(ERR_RE_OUT_OF_BOUNDS, RE_n);
        ;
    }

    index[RE_n] = (index[RE_n] << 2) | rd;
	index[RE_n] &= 0b1111;

	switch (index[RE_n]) {
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
    return retval;
}

int main()
{
    int16 frequencyMajIndex = FREQUENCY_MAJ_INDEX_INIT;
    int32 frequency = FREQUENCY_INIT;
    int re_val;
    
    setDDSParameter_0(frequency);
    //setDDSParameter_1(1000);
    generateWave_0();
    //generateWave_1();
    
    CyGlobalIntEnable; /* Enable global interrupts. */

    I2S_1_Start();

    /* DMA Configuration for DMA_0 */
    DMA_0_Chan = DMA_0_DmaInitialize(DMA_0_BYTES_PER_BURST, DMA_0_REQUEST_PER_BURST, 
        HI16(DMA_0_SRC_BASE), HI16(DMA_0_DST_BASE));
    DMA_0_TD[0] = CyDmaTdAllocate();
    CyDmaTdSetConfiguration(DMA_0_TD[0], BUFFER_SIZE, DMA_0_TD[0], DMA_0__TD_TERMOUT_EN | TD_INC_SRC_ADR);
    CyDmaTdSetAddress(DMA_0_TD[0], LO16((uint32)waveBuffer_0), LO16((uint32)I2S_1_TX_CH0_F0_PTR));
    CyDmaChSetInitialTd(DMA_0_Chan, DMA_0_TD[0]);
    CyDmaChEnable(DMA_0_Chan, 1);

    /* DMA Configuration for DMA_1 */
    /*
    DMA_1_Chan = DMA_1_DmaInitialize(DMA_1_BYTES_PER_BURST, DMA_1_REQUEST_PER_BURST, 
        HI16(DMA_1_SRC_BASE), HI16(DMA_1_DST_BASE));
    DMA_1_TD[0] = CyDmaTdAllocate();
    CyDmaTdSetConfiguration(DMA_1_TD[0], BUFFER_SIZE, DMA_1_TD[0], DMA_1__TD_TERMOUT_EN | TD_INC_SRC_ADR);
    CyDmaTdSetAddress(DMA_1_TD[0], LO16((uint32)waveBuffer_0), LO16((uint32)I2S_1_TX_CH0_F1_PTR));
    CyDmaChSetInitialTd(DMA_1_Chan, DMA_1_TD[0]);
    CyDmaChEnable(DMA_1_Chan, 1);
    */

    ISR_DMA_0_Done_StartEx(dma_0_done_handler);
    //ISR_DMA_1_Done_StartEx(dma_1_done_handler);
    ISR_I2S_1_TX_StartEx(i2s_1_tx_handler);
    
    while(0u != (I2S_1_ReadTxStatus() & (I2S_1_TX_FIFO_0_NOT_FULL | I2S_1_TX_FIFO_1_NOT_FULL)))
    {
        /* Wait for TxDMA to fill Tx FIFO */
    }
    CyDelay(1);

    I2S_1_EnableTx();
    
    for(;;)
    {
        re_val = readRE(0);
        if (0 != re_val) {
            frequencyMajIndex += re_val;
            if (frequencyMajIndex < 0) {
                frequencyMajIndex = 0;
            } else if (frequencyMajIndex >= FREQUENCY_TABLE_LENGTH) {
                frequencyMajIndex = FREQUENCY_TABLE_LENGTH - 1;
            }
            frequency = frequencyMajTable[frequencyMajIndex];
        } else {
            frequency += readRE(1) * frequencyMnrStep[frequencyMajIndex];
            if (frequency <= 0) frequency = 1;
            else if ((uint32)frequency >= SAMPLE_CLOCK / 2) frequency = SAMPLE_CLOCK / 2;
        }
        setDDSParameter_0(frequency);
        CyDelay(1);
    }
}

/* [] END OF FILE */
