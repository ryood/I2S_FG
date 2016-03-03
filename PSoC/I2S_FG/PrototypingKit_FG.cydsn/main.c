/* ========================================
 *
 * Copyright YOUR COMPANY, THE YEAR
 * All Rights Reserved
 * UNPUBLISHED, LICENSED SOFTWARE.
 *
 * CONFIDENTIAL AND PROPRIETARY INFORMATION
 * WHICH IS THE PROPERTY OF your company.
 *
 * 2016.03.03 UIを整理
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
#define TITLE_STR2  ("20160303")

// Defines for DDS
#define SAMPLE_CLOCK    312500u
#define FREQUENCY_INIT  1000

#define TABLE_SIZE      32768
#define BUFFER_SIZE     4     

/* Defines for DMA_0 */
#define DMA_0_BYTES_PER_BURST 1
#define DMA_0_REQUEST_PER_BURST 1
#define DMA_0_SRC_BASE (CYDEV_SRAM_BASE)
#define DMA_0_DST_BASE (CYDEV_PERIPH_BASE)

// Defines for UI
#define MODE_FIRSTTIME  -1
#define MODE_NORMAL		0
#define MODE_FREQUENCY	1
#define MODE_WAVEFORM	2
#define MODE_CONFIG		3
#define MODE_N			4

#define CMD_LCD_ON  10
#define CMD_LCD_OFF 11
#define CMD_CLR     12
#define CMD_ENT     13
#define CMD_MODE    14
#define CMD_CANCEL  15

#define WAVEFORM_SIN	0
#define WAVEFORM_TRI	1
#define WAVEFORM_SW1	2
#define WAVEFORM_SW2	3
#define WAVEFORM_SQR	4
#define WAVEFORM_N		5

#define KEY_BUFFER_LENGTH   8

#define LCD_ON  1
#define LCD_OFF 0
#define LCD_CONTRAST_INIT   32
#define LCD_CONTRAST_LIMIT  63

#define ATTENUATE_LIMIT 8

/* Variable declarations for DMA_0 */
/* Move these variable declarations to the top of the function */
uint8 DMA_0_Chan;
uint8 DMA_0_TD[1];

volatile uint8 waveBuffer_0[BUFFER_SIZE];
volatile uint32 tuningWord_0;
volatile uint32 phaseRegister_0 = 0;

volatile int frequency;
volatile int8 attenuate;
volatile int waveForm;

const char *strWaveForm[] = {
	"SIN ", "TRI ", "SW1 ", "SW2 ", "SQR "
};

const int POW10[] = {
    1, 10, 100, 1000, 10000, 100000, 1000000, 10000000    
};

int currentMode;

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
//         0 ..  9: 数字キー入力
//         10.. 15: CMD
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

    kv = -1;    
    if (tmp == krd && krd != prevKey) {
        prevKey = krd;
		if (krd != -1) {
            kv = keyPadMatrix[krd];
        }
    }
    return kv;
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

//-------------------------------------------------
// KeyBufferの周波数値の増減
// parameter: val: -1 または　1 それ以外は作用なし
// 
// ＊＊＊＊＊ 範囲制限にBugあり ＊＊＊＊＊
//
void incDecKeyBuffer(int val)
{
	int i, v;
	
	if (val != -1 && val != 1) {
		return;
	}
	
	v = keyBuffer[0];
	v += val;
	if (v >= 10) {
		if (kbp < KEY_BUFFER_LENGTH) {
			kbp++;
			keyBuffer[0] = 1;
		}
	} else if (v <= 0) {
		if (kbp > 0) {
			kbp--;
			keyBuffer[0] = 9;
		}
	} else {
		keyBuffer[0] = v;
	}
 	// 下位を0で埋める
    for (i = 1; i < kbp - 1; i++) {
		keyBuffer[i] = 0;
	}
}
#endif // DDS_ONLY

//------------------------------------------------------
// メイン・ルーチン
//
int keyBuffer2int()
{
	int i, v;
    
    v = 0;
	for (i = 0; i < kbp; i++) {
		v += keyBuffer[i] * POW10[kbp - 1 - i];
	}
    return v;
}

void setKeyBufferWithInt(int v)
{
    int buff[KEY_BUFFER_LENGTH];
    int i;
    
    for (i = 0; v > 0 && i < KEY_BUFFER_LENGTH; i++) {
        buff[i] = v % 10;
        v /= 10;
    }
    kbp = i;
    for (i = 0; i < kbp; i++) {
        keyBuffer[i] = buff[kbp - 1 - i];
    }
}

int constrain(int x, int min, int max)
{
	if (x < min) {
		x = min;
	} else if (x > max) {
		x = max;
	}
	return x;
}

int main()
{
    char buff1[10];
    char buff2[10];
    int key;
    int re_v;
    int isDirty;
    
    frequency = FREQUENCY_INIT;
    setKeyBufferWithInt(frequency);
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

    currentMode = MODE_FIRSTTIME;
    
    for(;;)
    {
#if !DDS_ONLY     
        isDirty = 0;
	
    	// キーパッドの処理
    	//
    	key = keyPadScan();
        
        /*
        LCD_SetPos(0, 0);
        sprintf(buff1, "%d ", key);
    	LCD_Puts(buff1);
        CyDelay(100);
        */
        
    	switch (key) {
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
    		switch (currentMode) {
    		case MODE_NORMAL:
    			currentMode = MODE_FREQUENCY;
                kbp = 0;
    		case MODE_FREQUENCY:
    			if (kbp < KEY_BUFFER_LENGTH) {
    				keyBuffer[kbp] = key;
    				kbp++;
    			}
    			isDirty = 1;
                LCD_SetCursor(0, 1);
    			break;
    		case MODE_WAVEFORM:
    			if (0 <= key && key < WAVEFORM_N) {
    				waveForm = key;
    			}
    			isDirty = 1;
    			break;
    		}
    		break;
    	case CMD_CLR:
    		if (currentMode == MODE_FREQUENCY) {
    			kbp = 0;
    		}
            isDirty = 1;
    		break;
    	case CMD_ENT:
    		currentMode = MODE_NORMAL;
    		frequency = keyBuffer2int();
    		setDDSParameter_0(frequency);
    		//kbp = 0;
            LCD_SetCursor(0, 0);
    		break;
    	case CMD_CANCEL:
    		switch (currentMode) {
    		case MODE_FREQUENCY:
    			currentMode = MODE_NORMAL;
    			//kbp = 0;
    			break;
    		case MODE_WAVEFORM:
    			currentMode = MODE_NORMAL;
    			break;
    		case MODE_CONFIG:
    			currentMode = MODE_NORMAL;
    			break;
    		}
    		isDirty = 1;
    		break;
    	case CMD_MODE:
    		switch (currentMode) {
    		case MODE_NORMAL:
    			currentMode = MODE_WAVEFORM;
    			break;
    		case MODE_WAVEFORM:
    			currentMode = MODE_CONFIG;
    			break;
    		case MODE_CONFIG:
    			currentMode = MODE_NORMAL;
    			break;
    		}
    		isDirty = 1;
    		break;
        /*
         * 動作不良のため保留
         *
    	case CMD_LCD_ON:
    		if (lcdStat != LCD_ON) {
    			switchLCD(LCD_ON);
    			lcdStat = LCD_ON;
    		}
    		break;
    	case CMD_LCD_OFF:
    		if (lcdStat != LCD_OFF) {
    			switchLCD(LCD_OFF);
    			lcdStat = LCD_OFF;
    		}
    		break;
        */
    	}
    	
    	// ロータリーエンコーダーの処理
    	//
    	re_v = readRE();
    	if (re_v != 0) {
    		switch (currentMode) {
    		case MODE_NORMAL:
    			// 周波数増減
                incDecKeyBuffer(re_v);
                frequency = keyBuffer2int();
    		    setDDSParameter_0(frequency);
                isDirty = 1;
    			break;
    		case MODE_FREQUENCY:
    			// do nothing
    			break;
    		case MODE_WAVEFORM:
    			// attenuate
    			attenuate -= re_v;
    			attenuate = constrain(attenuate, 0, ATTENUATE_LIMIT);
    			isDirty = 1;
    			break;
    		case MODE_CONFIG:
    			lcdContrast += re_v;
    			constrain(lcdContrast, 0, LCD_CONTRAST_LIMIT);
    			LCD_SetContrast(lcdContrast);
    			isDirty = 1;
    			break;
    		}
    	}
    	
    	// 電源電圧測定
    	//
    	supplyVoltage = measureSupplyVoltage();
    	// ToDo: 下限値以下に成った場合アラート。
    	
    	// 表示文字列
    	//
    	if (((lcdStat != LCD_OFF) && isDirty) || (currentMode == MODE_FIRSTTIME)) {
    		switch (currentMode) {
            case MODE_FIRSTTIME:
                 currentMode = MODE_NORMAL;
    		case MODE_NORMAL:
    			sprintf(buff1, "%6dHz", frequency);
    			sprintf(buff2, "%s A:%d", strWaveForm[waveForm], attenuate);
    			break;
    		case MODE_FREQUENCY:
    			sprintf(buff1, "%6dHz", keyBuffer2int());
    			sprintf(buff2, "%s A:%d", strWaveForm[waveForm], attenuate);
    			break;
    		case MODE_WAVEFORM:
    			sprintf(buff1, "WAV:%s", strWaveForm[waveForm]);
    			sprintf(buff2, "ATT:%d   ", attenuate);
    			break;
    		case MODE_CONFIG:
    			sprintf(buff1, "%6ldmV", supplyVoltage);
    			sprintf(buff2, "CONT:%d", lcdContrast);
    			break;
    		}
    		
            //sprintf(buff2, "%8d", kbp);
    		LCD_SetPos(0, 0);
    		LCD_Puts(buff1);
    		LCD_SetPos(0, 1);
    		LCD_Puts(buff2);
            
            if (currentMode == MODE_FREQUENCY) {
                LCD_SetPos(5, 0);
            }
        }

        //CyDelay(1);
        
#endif // DDS_ONLY        
    }
}

/* [] END OF FILE */
