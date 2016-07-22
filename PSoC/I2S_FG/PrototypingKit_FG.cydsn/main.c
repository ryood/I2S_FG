/* ========================================
 *
 * Copyright YOUR COMPANY, THE YEAR
 * All Rights Reserved
 * UNPUBLISHED, LICENSED SOFTWARE.
 *
 * CONFIDENTIAL AND PROPRIETARY INFORMATION
 * WHICH IS THE PROPERTY OF your company.
 *
 * 2016.03.30 UIを修正
 * 2016.03.27 ノコギリ波の出力
 * 2016.03.27 Sampling Rateを382kHzに (ダイ温度測定)
 * 2016.03.03 UIを整理
 * 2016.03.01 created
 *
 * ========================================
*/
#include <project.h>
#include <stdio.h>
#include "wavetable32_0_9_8192.h"
#include "SPLC792-I2C.h"

#define DDS_ONLY    0

#define TITLE_STR1  ("I2S FG  ")
#define TITLE_STR2  ("20160714")

// ロータリーエンコーダによる周波数増減を上位1桁または上位2桁に切り替える閾値
#define INC_DEC_THRESHOLD   100000 

// Defines for DDS
#define SAMPLE_CLOCK    384000u
#define FREQUENCY_INIT  1000

#define TABLE_SIZE      8192
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
#define MODE_STATUS     3
#define MODE_CONTRAST	4
#define MODE_N			5

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
#define ATTENUATE_INIT 2

#define USED_EEPROM_SECTOR      (1u)
#define EEPROM_FREQUENCY        ((USED_EEPROM_SECTOR * CYDEV_EEPROM_SECTOR_SIZE) + 0x00)
#define EEPROM_ATTENUATE        ((USED_EEPROM_SECTOR * CYDEV_EEPROM_SECTOR_SIZE) + 0x04)
#define EEPROM_WAVEFORM         ((USED_EEPROM_SECTOR * CYDEV_EEPROM_SECTOR_SIZE) + 0x05)

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
    // "SIN ", "TRI ", "SW1 ", "SW2 ", "SQR "
    "SIN ", "SIN ", "SAW ", "SAW ", "SIN "
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
int16 dieTemp;
cystatus dieTempStatus;

void setDDSParameter_0(uint32 frequency)
{
    tuningWord_0 = (((uint64)frequency << 32) / SAMPLE_CLOCK);
}

void generateWave_0()
{
    int i, index;
    int32 v;
    const int32 *table_p;
    
    switch (waveForm) {
    case WAVEFORM_SW1:
    case WAVEFORM_SW2:
        table_p = sawupTable;
        break;
    default:
        table_p = sineTable;
    }
    
    // 波形をバッファに転送
    // Todo: BUFFER_SIZEが4の場合はforループを外す。
    for (i = 0; i < BUFFER_SIZE; i+=4) {
        phaseRegister_0 += tuningWord_0;
        // テーブルの要素数 = 2^n として 32 - n で右シフト
        // ex)
        //  1024  = 2^10 : 32 - 10 = 22
        //  32768 = 2^15 : 32 - 15 = 17 
        index = phaseRegister_0 >> 19;

        // 右シフトで出力レベルを減衰
        v = (table_p[index] >> attenuate);
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
// ユーティリティ
//
int constrain(int x, int min, int max)
{
	if (x < min) {
		x = min;
	} else if (x > max) {
		x = max;
	}
	return x;
}

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
// parameter: val:増減値 -1 または 1。
//                それ以外は動作不定
//
void incDecFrequency(int val)
{
	int v, cnt, rem;

	// 桁数を求める。
	v = frequency;
	cnt = 0;
	rem = 0;
	while (v) {
		rem = v % 10;
		v /= 10;
		cnt++;
	}

	v = rem + val;
	printf("v: %d\t", v);
	if (v > 0) {
		frequency = POW10[cnt - 1] * v;
	}
	else {
		frequency = POW10[cnt - 2] * (9 + v);
	}
	frequency = constrain(frequency, 1, SAMPLE_CLOCK / 2);
}

// 上位2桁を増減
//
void incDecFrequencyUpper2Digit(int val)
{
	int v, cnt, rem;
	int upper2digit;

	// 周波数が１桁の場合は最上位を増減
	if (frequency < 10) {
		incDecFrequency(val);
		return;
	}

	// 桁数を求める。
	v = frequency;
	cnt = 0;
	rem = 0;
	while (v) {
		rem = v % 10;
		v /= 10;
		cnt++;
	}

	upper2digit = frequency / POW10[cnt - 2];
	v = upper2digit + val;
	frequency = POW10[cnt - 2] * v;
	frequency = constrain(frequency, 1, SAMPLE_CLOCK / 2);
}

#endif // DDS_ONLY


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
    uint8 tmp;
    
    LCD_SetPos(0, 0);
    LCD_Puts("loading");
    LCD_SetPos(0, 1);
    LCD_Puts("EEPROM");
    
    tmp = EEPROM_ReadByte(EEPROM_FREQUENCY);
    if (tmp != 0xFF) {
        frequency = eepromReadInt32(EEPROM_FREQUENCY);
        attenuate = EEPROM_ReadByte(EEPROM_ATTENUATE);
        waveForm  = eepromReadInt32(EEPROM_WAVEFORM); 
    }

    CyDelay(500);
}

//------------------------------------------------------
// メイン・ルーチン
//

int main()
{
    char buff1[10];
    char buff2[10];
    int key;
    int re_v;
    int isDirty;
    
    frequency = FREQUENCY_INIT;
    setKeyBufferWithInt(frequency);
    attenuate = ATTENUATE_INIT;
    
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
    
    // Load Prameters from EEPROM
    EEPROM_Start();
    loadParametersFromEEPROM();
    
    // Init ADC
    //
    ADC_DelSig_Supply_Start();
    
#endif // DDS_ONLY

    setDDSParameter_0(frequency);
    generateWave_0();
    
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
            frequency = constrain(frequency, 1, SAMPLE_CLOCK / 2);
    		setDDSParameter_0(frequency);
    		//kbp = 0;
            LCD_SetCursor(0, 0);
            isDirty = 1;
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
			case MODE_STATUS:
    			currentMode = MODE_NORMAL;
    			break;    
    		case MODE_CONTRAST:
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
    			currentMode = MODE_STATUS;
    			break;
            case MODE_STATUS:
                currentMode = MODE_CONTRAST;
                break;			
    		case MODE_CONTRAST:
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
                incDecFrequencyUpper2Digit(re_v);
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
    		case MODE_CONTRAST:
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
        if (currentMode == MODE_STATUS) {
            // ダイ温度測定
            // 測定に時間がかかるためMODE_STATUSの場合のみ測定
            dieTempStatus = DieTemp_GetTemp(&dieTemp);
			isDirty = 1;
        }
        	
    	// 表示文字列
    	//
    	if (((lcdStat != LCD_OFF) && isDirty) || (currentMode == MODE_FIRSTTIME)) {
            // ParameterをEEPROMに保存
            #if !DDS_ONLY
            saveParametersToEEPROM();
            #endif
            
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
            case MODE_STATUS:
                sprintf(buff1, "%6ldmV", supplyVoltage);
                if (dieTempStatus == CYRET_SUCCESS) {
    			    sprintf(buff2, "%6d C", dieTemp);
                    buff2[6] = 0b11110010;  // [°]
                } else {
                    sprintf(buff2, "ST:%d", (int8)dieTempStatus);
                }
                break;
    		case MODE_CONTRAST:
    			sprintf(buff1, "CONT:%3d", lcdContrast);
                sprintf(buff2, "        ");
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
