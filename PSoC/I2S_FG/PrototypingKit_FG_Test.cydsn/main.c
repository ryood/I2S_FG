/* ========================================
 *
 * Copyright YOUR COMPANY, THE YEAR
 * All Rights Reserved
 * UNPUBLISHED, LICENSED SOFTWARE.
 *
 * CONFIDENTIAL AND PROPRIETARY INFORMATION
 * WHICH IS THE PROPERTY OF your company.
 *
 * 2016.02.19 DDSとUIを結合
 *
 * ========================================
*/
#include <project.h>
#include "wavetable32_0_9_32768.h"

#define SAMPLE_CLOCK    375000u
#define FREQUENCY_INIT  1000

#define TABLE_SIZE      32768
#define BUFFER_SIZE     4     

/* Defines for DMA_0 */
#define DMA_0_BYTES_PER_BURST 1
#define DMA_0_REQUEST_PER_BURST 1
#define DMA_0_SRC_BASE (CYDEV_SRAM_BASE)
#define DMA_0_DST_BASE (CYDEV_PERIPH_BASE)

/* Variable declarations for DMA_0 */
/* Move these variable declarations to the top of the function */
uint8 DMA_0_Chan;
uint8 DMA_0_TD[1];

volatile uint8 waveBuffer_0[BUFFER_SIZE];

volatile uint32 tuningWord_0;
volatile uint32 phaseRegister_0 = 0;

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
        // テーブルの要素数=2^n として 32 - n で右シフト
        // 1024  = 2^10 : 32 - 10 = 22
        // 32768 = 2^15 : 32 - 15 = 17 
        index = phaseRegister_0 >> 17;
        
        //p8 = (uint8 *)(sineTable + index);
        // 右シフトで出力レベルを減衰
        v = (sineTable[index] >> 0);
        *((uint32 *)waveBuffer_0) = __REV(v);
    }
}

CY_ISR (dma_0_done_handler)
{
    Pin_Check_0_Write(1u);
    generateWave_0();
    Pin_Check_0_Write(0u);
}

CY_ISR (i2s_1_tx_handler)
{
    if (I2S_1_ReadTxStatus() & I2S_1_TX_FIFO_UNDERFLOW) {
        // Underflow Alert
        Pin_Check_1_Write(1u);
        Pin_Check_1_Write(0u);
    }
}

int main()
{
    int32 frequency = FREQUENCY_INIT;
    
    setDDSParameter_0(frequency);
    generateWave_0();
    
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

    ISR_DMA_0_Done_StartEx(dma_0_done_handler);

    ISR_I2S_1_TX_StartEx(i2s_1_tx_handler);
    
    while(0u != (I2S_1_ReadTxStatus() & (I2S_1_TX_FIFO_0_NOT_FULL)))
    {
        /* Wait for TxDMA to fill Tx FIFO */
    }
    CyDelay(1);

    I2S_1_EnableTx();
    
    for(;;)
    {
    }
}

/* [] END OF FILE */
