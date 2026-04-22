/* Audio Library for Teensy 3.X
 * Copyright (c) 2019, Paul Stoffregen, paul@pjrc.com
 *
 * Development of this audio library was funded by PJRC.COM, LLC by sales of
 * Teensy and Audio Adaptor boards.  Please support PJRC's efforts to develop
 * open source software by purchasing Teensy or other PJRC products.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice, development funding notice, and this permission
 * notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
/*
 (c) Frank B, Jonathan O
*/

#include "imxrt_hw.h"

#if defined(__IMXRT1052__) || defined(__IMXRT1062__)

FLASHMEM
void set_audioClock(int nfact, int32_t nmult, uint32_t ndiv, bool force) // sets PLL4
{
	if (!force && (CCM_ANALOG_PLL_AUDIO & CCM_ANALOG_PLL_AUDIO_ENABLE)) return;

	CCM_ANALOG_PLL_AUDIO = CCM_ANALOG_PLL_AUDIO_BYPASS | CCM_ANALOG_PLL_AUDIO_ENABLE
			     | CCM_ANALOG_PLL_AUDIO_POST_DIV_SELECT(2) // page 1105
			     | CCM_ANALOG_PLL_AUDIO_DIV_SELECT(nfact);

	CCM_ANALOG_PLL_AUDIO_NUM   = nmult & CCM_ANALOG_PLL_AUDIO_NUM_MASK;
	CCM_ANALOG_PLL_AUDIO_DENOM = ndiv & CCM_ANALOG_PLL_AUDIO_DENOM_MASK;
	
	CCM_ANALOG_PLL_AUDIO &= ~CCM_ANALOG_PLL_AUDIO_POWERDOWN;//Switch on PLL
	while (!(CCM_ANALOG_PLL_AUDIO & CCM_ANALOG_PLL_AUDIO_LOCK)) {}; //Wait for pll-lock
	
	const int div_post_pll = 1; // other values: 2,4
	CCM_ANALOG_MISC2 &= ~(CCM_ANALOG_MISC2_DIV_MSB | CCM_ANALOG_MISC2_DIV_LSB);
	if(div_post_pll>1) CCM_ANALOG_MISC2 |= CCM_ANALOG_MISC2_DIV_LSB;
	if(div_post_pll>3) CCM_ANALOG_MISC2 |= CCM_ANALOG_MISC2_DIV_MSB;
	
	CCM_ANALOG_PLL_AUDIO &= ~CCM_ANALOG_PLL_AUDIO_BYPASS;//Disable Bypass
}


FLASHMEM
void SAIconfig::configSAI(SAIcfg cfg, 		//! type of hardware: I²S, TDM etc.
						  double fs, 		//! sample rate
						  bool only_bclk, 	//! true if we only want to set bit clock
						  int channels, 	//! number of channels: 2-8 for I²S, 16 for TDM
						  bool rx, 			//! true if setting up receiver
						  uint32_t extra) 	//! extra hardware-dependent info, e.g. for PT8211
{
	uint32_t tcr3 = sai.TCR3, rcr3 = sai.RCR3; // keep existing values

	switch (which)
	{
		case 1:
			CCM_CCGR5 |= CCM_CCGR5_SAI1(CCM_CCGR_ON);
			break;
		case 2:
			CCM_CCGR5 |= CCM_CCGR5_SAI2(CCM_CCGR_ON);
			break;
	}					

	//PLL:
	//int fs = AUDIO_SAMPLE_RATE_EXACT;
	// PLL between 27*24 = 648MHz und 54*24=1296MHz
	int n1 = 4; //SAI prescaler 4 => (n1*n2) = multiple of 4
	int n2 = 1 + (24000000 * 27) / (fs * 256 * n1);

	double C = ((double)fs * 256 * n1 * n2) / 24000000;
	int c0 = C;
	int c2 = 10000;
	int c1 = C * c2 - (c0 * c2);
	set_audioClock(c0, c1, c2);

	if (SAIcfg::TDM == cfg)
		n1 = n1 / 2; //Double Speed for TDM

	// clear SAIn_CLK register locations
	switch (which)
	{
		case 1:
			CCM_CSCMR1 = (CCM_CSCMR1 & ~(CCM_CSCMR1_SAI1_CLK_SEL_MASK))
				| CCM_CSCMR1_SAI1_CLK_SEL(2); // &0x03 // (0,1,2): PLL3PFD0, PLL5, PLL4
			CCM_CS1CDR = (CCM_CS1CDR & ~(CCM_CS1CDR_SAI1_CLK_PRED_MASK | CCM_CS1CDR_SAI1_CLK_PODF_MASK))
				| CCM_CS1CDR_SAI1_CLK_PRED(n1-1) // &0x07
				| CCM_CS1CDR_SAI1_CLK_PODF(n2-1); // &0x3f
			// Select MCLK
			IOMUXC_GPR_GPR1 = (IOMUXC_GPR_GPR1
				& ~(IOMUXC_GPR_GPR1_SAI1_MCLK1_SEL_MASK))
				| (IOMUXC_GPR_GPR1_SAI1_MCLK_DIR | IOMUXC_GPR_GPR1_SAI1_MCLK1_SEL(0));
			break;

		case 2:
			CCM_CSCMR1 = (CCM_CSCMR1 & ~(CCM_CSCMR1_SAI2_CLK_SEL_MASK))
				| CCM_CSCMR1_SAI2_CLK_SEL(2); // &0x03 // (0,1,2): PLL3PFD0, PLL5, PLL4,
			CCM_CS2CDR = (CCM_CS2CDR & ~(CCM_CS2CDR_SAI2_CLK_PRED_MASK | CCM_CS2CDR_SAI2_CLK_PODF_MASK))
				| CCM_CS2CDR_SAI2_CLK_PRED(n1-1)
				| CCM_CS2CDR_SAI2_CLK_PODF(n2-1);

			IOMUXC_GPR_GPR1 = (IOMUXC_GPR_GPR1 & ~(IOMUXC_GPR_GPR1_SAI2_MCLK3_SEL_MASK))
					| (IOMUXC_GPR_GPR1_SAI2_MCLK_DIR | IOMUXC_GPR_GPR1_SAI2_MCLK3_SEL(0));	//Select MCLK
			break;
	}
	/*
	Use I2S as baseline:
	Different channel counts 
		affect TCR3 / RCR3 enabled channels

	Using TDM affects
		TCR2: DIV (bit clock divide)
		TCR4: FRSZ (frame size); SYWD (sync width); FSP (frame sync polarity)

	PT8211:
		TCR2: DIV (bit clock divide)
		TCR4: SYWD (sync width); FSE (frame sync early)
		TCR5: all (16-bit word width rather than 32)

	S/PDIF:
		TCR2: BCP (bit clock polarity); DIV (bit clock divide)
		TCR4: FRSZ (frame size); SYWD (sync width); FSE (frame sync early)

	PDM:
		RCR2: DIV (bit clock divide)
		RCR4: FRSZ (frame size); SYWD (sync width); FSP (frame sync polarity)
	*/


	int rsync = which == 1?0:1; // for some reason; does it matter?
	int tsync = 1 - rsync;
	bool txOnly = false, rxOnly = false;

	settings_t settingsTCR;
	switch (cfg)
	{
		default:
			settingsTCR = settings_t{0,0,  0,0,0,0, 0};
			break;
			
		case SAIcfg::I2S:	   	//   TCR2  TCR4          TCR5	clocks
 // Serial.printf("I2S; %d channels; %s\n", channels, rx?"Rx":"Tx");
			settingsTCR = settings_t{1,1,  2-1,32-1,1,1, 32-1,  1,1,1};
			break;
			
		case SAIcfg::TDM:		//   TCR2  TCR4          TCR5  clocks
			settingsTCR = settings_t{0,1,  8-1, 1-1,1,0, 32-1, 1,1,1};
			break;
			
		case SAIcfg::SPDIF:		//   TCR2  TCR4       TCR5  (no clocks)
			settingsTCR = settings_t{0,0,  4-1,0,0,1, 32-1};
			txOnly = true;
			break;
			
		case SAIcfg::PT8211: 
		{
			uint32_t div = extra; // extra info is DIV value, for oversampling or not
								//   TCR2  	 TCR4          TCR5	 clocks
			settingsTCR = settings_t{div,1,  2-1,16-1,0,1, 16-1, 0,1,1};
		}
			txOnly = true;
			break;
			
		case SAIcfg::PDM:	//    	 TCR2  TCR4          TCR5  clocks	
			settingsTCR = settings_t{1,1,  2-1,32-1,0,1, 32-1, 0,1,0};
			rsync = 0;
			rxOnly = true;
			break;
	}

	// We note that RCRn settings are typically identical to the TCRn ones...
	settings_t settingsRCR = settingsTCR;
	// ... but we could tweak them here

	switch (which)
	{ 
		case 1:
			if (!only_bclk)
			{
				if (settingsTCR.mclk)  CORE_PIN23_CONFIG = 3;  //1:MCLK
				if (settingsTCR.lrclk) CORE_PIN20_CONFIG = 3;  //1:RX_SYNC  (LRCLK)
			}
			if (settingsTCR.bclk) CORE_PIN21_CONFIG = 3;  //1:RX_BCLK
			break;

		case 2:			
			if (!only_bclk)
			{
				if (settingsTCR.mclk)  CORE_PIN33_CONFIG = 2;  //EMC_07, 2=SAI2_MCLK
				if (settingsTCR.lrclk)  CORE_PIN4_CONFIG  = 2;  //EMC_06, 2=SAI2_TX_BCLK
			}
			if (settingsTCR.bclk)  CORE_PIN3_CONFIG  = 2;  //EMC_05, 2=SAI2_TX_SYNC, page 429
			break;
	}

	// Enable channels. We assume everything is in pairs going
	// down a single wire (apart from TDM)
	if (rx) // configuring receive
	{
		switch (channels)
		{
			default: break;
			case 16: // TDM: one wire with 8 32-bit words in it, or 16 16-bit ones...
			case 2:	rcr3 = I2S_RCR3_RCE; break;
			case 4:	rcr3 = I2S_RCR3_RCE_2CH; break;
			case 6: rcr3 = I2S_RCR3_RCE_3CH; break;
			case 8:	rcr3 = I2S_RCR3_RCE_4CH; break;
		}
	}
	else
	{
		switch (channels)
		{
			default: break;
			case 16: // TDM: one wire with 8 32-bit words in it, or 16 16-bit ones...
			case 2:	tcr3 = I2S_TCR3_TCE; break;
			case 4:	tcr3 = I2S_TCR3_TCE_2CH; break;
			case 6: tcr3 = I2S_TCR3_TCE_3CH; break;
			case 8:	tcr3 = I2S_TCR3_TCE_4CH; break;
		}
	}


	uint32_t tcsr = sai.TCSR & I2S_TCSR_TE, 
			 rcsr = sai.RCSR & I2S_RCSR_RE;

	sai.TCSR &= ~I2S_TCSR_TE; // disable tx
	sai.RCSR &= ~I2S_RCSR_RE; // and rx

	if ((!rx || 0 == tcsr) && !rxOnly) // input only affects output if not previously configured
	{
		sai.TMR = 0;
		//sai.TCSR = (1<<25); //Reset
		sai.TCR1 = I2S_TCR1_RFW(FIFOwatermark);
		sai.TCR2 = I2S_TCR2_SYNC(tsync) | I2S_TCR2_MSEL(1) // sync=0; tx is async;
				| (settingsTCR.bcp? I2S_TCR2_BCP :0) 
				| I2S_TCR2_BCD | I2S_TCR2_DIV(settingsTCR.div) ;
		sai.TCR3 = tcr3;
		sai.TCR4 = I2S_TCR4_FRSZ(settingsTCR.frsz) | I2S_TCR4_SYWD(settingsTCR.sywd) 
				| I2S_TCR4_MF | I2S_TCR4_FSD 
				| (settingsTCR.fse? I2S_TCR4_FSE :0)  
				| (settingsTCR.fsp? I2S_TCR4_FSP :0) ;
		sai.TCR5 = I2S_TCR5_WNW(settingsTCR.wnw) | I2S_TCR5_W0W(settingsTCR.wnw) | I2S_TCR5_FBT(settingsTCR.wnw);
	}

	if ((rx || 0 == rcsr) && !txOnly) // output only affects input if not previously configured
	{
		sai.RMR = 0;
		//sai.RCSR = (1<<25); //Reset
		sai.RCR1 = I2S_RCR1_RFW(FIFOwatermark);
		sai.RCR2 = I2S_RCR2_SYNC(rsync) | I2S_RCR2_MSEL(1)  // sync=0; rx is async;
				| (settingsRCR.bcp? I2S_RCR2_BCP :0)
				| I2S_RCR2_BCD | I2S_RCR2_DIV(settingsRCR.div) ;
		sai.RCR3 = rcr3;
		sai.RCR4 = I2S_RCR4_FRSZ(settingsRCR.frsz) | I2S_RCR4_SYWD(settingsRCR.sywd) 
				| I2S_RCR4_MF | I2S_RCR4_FSD
				| (settingsRCR.fse? I2S_RCR4_FSE :0)
				| (settingsRCR.fsp? I2S_RCR4_FSP :0);
		sai.RCR5 = I2S_RCR5_WNW(settingsRCR.wnw) | I2S_RCR5_W0W(settingsRCR.wnw) | I2S_RCR5_FBT(settingsRCR.wnw);
	}

	// re-enable tx and rx, if previously enabled
	sai.TCSR |= tcsr;
	sai.RCSR |= rcsr;
}

#endif // defined(__IMXRT1052__) || defined(__IMXRT1062__)

#if defined(__IMXRT1052__) || defined(__IMXRT1062__) || defined(KINETISK)
SAIbase::DMAisrInfo_t SAIbase::DMAisrInfo[2]{0};

FLASHMEM
void SAIbase::configDMA(
				void* instance,			//! which instance is using this: needed for DMA ISR
				void (*isr)(void*),		//! object's DMA ISR function
				size_t bufSz, 			//! required buffer size
				volatile void* regAddr,	//! SAI register to transfer to / from
				int regSz, 				//! register width (2 / 4 bytes)
				int regStep, 			//! register step for e.g. quad / hex/ oct I²S
				bool rx)				//! configure as receiver (reg -> buffer)
{
	buffer = (uint32_t*) aligned_alloc(32, bufSz);
	dma.begin(true); // Allocate the DMA channel first

 /*
	dma.TCD->SADDR = tdm_tx_buffer;
	dma.TCD->SOFF = 4;
	dma.TCD->ATTR = DMA_TCD_ATTR_SSIZE(2) | DMA_TCD_ATTR_DSIZE(2);
	dma.TCD->NBYTES_MLNO = 4;
	dma.TCD->SLAST = -txBufSz; // (tdmsizeof_tx_buffer);
	dma.TCD->DADDR = &I2S2_TDR0;
	dma.TCD->DOFF = 0;
	dma.TCD->CITER_ELINKNO = txBufSz / 4; // sizeof(tdm_tx_buffer) / 4;
	dma.TCD->DLASTSGA = 0;
	dma.TCD->BITER_ELINKNO = txBufSz / 4; // sizeof(tdm_tx_buffer) / 4;
	dma.TCD->CSR = DMA_TCD_CSR_INTHALF | DMA_TCD_CSR_INTMAJOR;
	dma.triggerAtHardwareEvent(DMAMUX_SOURCE_SAI2_TX);
 */
	if (rx)
	{
		switch (regSz)
		{
			case 2: // int16_t samples
				dma.destinationBuffer((uint16_t*) buffer,bufSz);
				dma.source(*((uint16_t*) regAddr));
				break;

			case 4: // int16_t samples
				dma.destinationBuffer(buffer,bufSz);
				dma.source(*((uint32_t*) regAddr));
				break;
		}
		#if defined(__IMXRT1062__)
			dma.triggerAtHardwareEvent(which == 1
											?DMAMUX_SOURCE_SAI1_RX
											:DMAMUX_SOURCE_SAI2_RX);
		#elif defined(KINETISK)
			dma.triggerAtHardwareEvent(DMAMUX_SOURCE_I2S0_RX);
		#endif // hardware type
	}
	else
	{
		switch (regSz)
		{
			case 2: // int16_t samples
				dma.sourceBuffer((uint16_t*) buffer,bufSz);
				dma.destination(*((uint16_t*) regAddr));
				break;

			case 4: // int16_t samples
				dma.sourceBuffer(buffer,bufSz);
				dma.destination(*((uint32_t*) regAddr));
				break;
		}
		#if defined(__IMXRT1062__)
			dma.triggerAtHardwareEvent(which == 1
											?DMAMUX_SOURCE_SAI1_TX
											:DMAMUX_SOURCE_SAI2_TX);
		#elif defined(KINETISK)
			dma.triggerAtHardwareEvent(DMAMUX_SOURCE_I2S0_TX);
		#endif // hardware type
	}
	dma.interruptAtCompletion();
	dma.interruptAtHalf();

	dma.attachInterrupt(which == 1
							?isr1
							:isr2);
	dma.enable();

	// allow ISR to find the target object
	DMAisrInfo[which-1] = {instance, isr};
}

// static
void SAIbase::isr1(void)
{
	(DMAisrInfo[0].isr)(DMAisrInfo[0].instance);
}

// static
void SAIbase::isr2(void)
{
	(DMAisrInfo[1].isr)(DMAisrInfo[1].instance);
}

#endif // defined(__IMXRT1052__) || defined(__IMXRT1062__) || defined(KINETISK)
