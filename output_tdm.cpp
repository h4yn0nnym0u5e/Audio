/* Audio Library for Teensy 3.X
 * Copyright (c) 2017, Paul Stoffregen, paul@pjrc.com
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

#include <Arduino.h>

#if !defined(KINETISL)

#include "output_tdm.h"
#include "memcpy_audio.h"
#include "utility/imxrt_hw.h"

audio_block_t* AudioOutputTDMbase::block_input[MAX_TDM_OUTPUTS] = {nullptr};
int AudioOutputTDMbase::pin_mask = 0;
bool AudioOutputTDMbase::update_responsibility = false;
volatile AudioOutputTDMbase::TDMstate_e AudioOutputTDMbase::state = INACTIVE;
DMAChannel AudioOutputTDMbase::dma(false);
#if defined(KINETISK)
	DMAMEM __attribute__((aligned(32)))
	static uint32_t tdm_tx_buffer[AUDIO_BLOCK_SAMPLES*16];
#elif defined(__IMXRT1062__)	
	uint32_t* AudioOutputTDMbase::tdm_tx_malloc = nullptr;
	uint32_t* AudioOutputTDMbase::tdm_tx_buffer = nullptr;
	uint32_t  AudioOutputTDMbase::tdm_txbuf_len = 0;
#endif // hardware-dependent


void AudioOutputTDMbase::begin(int pin) //!< pin number, range 1-4
{
	if (INACTIVE == state) // never been called before
	{
		dma.begin(true); // Allocate the DMA channel first
		for (int i=0; i < MAX_TDM_OUTPUTS; i++) 
			block_input[i] = nullptr;
	}
	else
	{
		elapsedMillis timeout = 0;
		
		state = STOPPING;
		while (STOPPING == state && timeout<20)
			;
if (timeout >= 20) Serial.println("Timed out stopping input");		
		// ISR has disabled the DMA now, so we should be safe to
		// reallocate the TX buffer
	}
	
#if defined(KINETISK)
	memset(tdm_tx_buffer, 0, sizeof(tdm_tx_buffer));
#elif defined(__IMXRT1062__)
	// Each pin transmits 16x 16-bit channels, and we double buffer, so:
	uint32_t tx_buf_needed = pin*sizeof(uint32_t)*AUDIO_BLOCK_SAMPLES*16;
	if (tx_buf_needed > tdm_txbuf_len) // buffer isn't big enough
	{
		void* buf = realloc(tdm_tx_malloc, tx_buf_needed + 32); // need to 32-byte align
		if (nullptr != buf) // got a new buffer
		{
			tdm_tx_malloc = (uint32_t*) buf;  // actual memory allocated, in case we need to realloc()
			tdm_tx_buffer = (uint32_t*)(((uint32_t) buf+32) & -32U);
			tdm_txbuf_len = tx_buf_needed;
			pin_mask |= (1<<(pin))-1; // enable all pins up to and including this one
		}		
	}
	setDMA(dma, tdm_tx_buffer, tdm_txbuf_len, true);
	memset(tdm_tx_buffer, 0, tdm_txbuf_len);
#endif // hardware-dependent

	// TODO: should we set & clear the I2S_TCSR_SR bit here?
	config_tdm(pin, -1);  // leave Rx pins, configure Tx pins
#if defined(KINETISK)
	CORE_PIN22_CONFIG = PORT_PCR_MUX(6); // pin 22, PTC1, I2S0_TXD0

	dma.TCD->SADDR = tdm_tx_buffer;
	dma.TCD->SOFF = 4;
	dma.TCD->ATTR = DMA_TCD_ATTR_SSIZE(2) | DMA_TCD_ATTR_DSIZE(2);
	dma.TCD->NBYTES_MLNO = 4;
	dma.TCD->SLAST = -sizeof(tdm_tx_buffer);
	dma.TCD->DADDR = &I2S0_TDR0;
	dma.TCD->DOFF = 0;
	dma.TCD->CITER_ELINKNO = sizeof(tdm_tx_buffer) / 4;
	dma.TCD->DLASTSGA = 0;
	dma.TCD->BITER_ELINKNO = sizeof(tdm_tx_buffer) / 4;
	dma.TCD->CSR = DMA_TCD_CSR_INTHALF | DMA_TCD_CSR_INTMAJOR;
	dma.triggerAtHardwareEvent(DMAMUX_SOURCE_I2S0_TX);

	update_responsibility = update_setup();
	dma.enable();

	I2S0_TCSR = I2S_TCSR_SR;
	I2S0_TCSR = I2S_TCSR_TE | I2S_TCSR_BCE | I2S_TCSR_FRDE;
#elif defined(__IMXRT1062__)
	switch (pin_mask)
	{
	  case 0x0F:
		CORE_PIN6_CONFIG  = 3;  //1:TX_DATA3
	  case 0x07:
		CORE_PIN9_CONFIG  = 3;  //1:TX_DATA2
	  case 0x03:
		CORE_PIN32_CONFIG = 3;  //1:TX_DATA1
	  case 0x01:
		CORE_PIN7_CONFIG  = 3;  //1:TX_DATA0
		break;
	}

	if (STOPPED != state) // first call to begin(): set everything up
	{
		dma.TCD->SADDR = tdm_tx_buffer;
		dma.TCD->SOFF = 4;
		dma.TCD->ATTR = DMA_TCD_ATTR_SSIZE(2) | DMA_TCD_ATTR_DSIZE(2);
		dma.TCD->NBYTES_MLNO = 4;
		dma.TCD->SLAST = -tdm_txbuf_len;
		dma.TCD->DADDR = &I2S1_TDR0;
		dma.TCD->DOFF = 0;
		dma.TCD->CITER_ELINKNO = tdm_txbuf_len / 4;
		dma.TCD->DLASTSGA = 0;
		dma.TCD->BITER_ELINKNO = tdm_txbuf_len / 4;
		dma.TCD->CSR = DMA_TCD_CSR_INTHALF | DMA_TCD_CSR_INTMAJOR;
		dma.triggerAtHardwareEvent(DMAMUX_SOURCE_SAI1_TX);

		if (!update_responsibility)
			update_responsibility = update_setup();

		dma.enable();
		dma.attachInterrupt(isr);
	}
	else // second or later call: minor changes only
	{
		//*
		zapDMA();
		/*/
		dma.disable();
		
		I2S1_TCSR |= I2S_TCSR_SR; // must reset SAI1, or we lose sync
		
		dma.TCD->SADDR = tdm_tx_buffer;
		dma.TCD->SLAST = -tdm_txbuf_len;
		dma.TCD->CITER_ELINKNO = tdm_txbuf_len / 4;
		dma.TCD->BITER_ELINKNO = tdm_txbuf_len / 4;
		dma.enable();
		//*/
	}
	I2S1_RCSR |= I2S_RCSR_RE | I2S_RCSR_BCE;
	I2S1_TCSR = I2S_TCSR_TE | I2S_TCSR_BCE | I2S_TCSR_FRDE;
#endif

	state = ACTIVE;
}


void AudioOutputTDMbase::isr(void)
{
	uint32_t *dest;
	//const uint32_t *src1, *src2;
	uint32_t i, saddr, nch;
	
	switch (pin_mask) // figure out how many pins we're using
	{
		default:
		case 0x01: nch = 1; break;
		case 0x03: nch = 2; break;
		case 0x07: nch = 3; break;
		case 0x0F: nch = 4; break;
	}
#if defined(KINETISK)
	uint32_t tdm_txbuf_len = sizeof(tdm_tx_buffer);
#endif	
	
#if defined(KINETISK) || defined(__IMXRT1062__)
	saddr = (uint32_t)(dma.TCD->SADDR);
#endif
	dma.clearInterrupt();
	if (STOPPING == state) // begin() called, signal it's OK to make changes
		state = STOPPED;
		
	if (saddr < (uint32_t)tdm_tx_buffer + tdm_txbuf_len / 2) {
		// DMA is transmitting the first half of the buffer
		// so we must fill the second half
		dest = tdm_tx_buffer + AUDIO_BLOCK_SAMPLES*8*nch;
	} else {
		// DMA is transmitting the second half of the buffer
		// so we must fill the first half
		dest = tdm_tx_buffer;
	}
	if (update_responsibility) AudioStream::update_all();

	#if IMXRT_CACHE_ENABLED >= 2
	uint32_t *dc = dest;
	#endif
	
	/*
	 * For 1 pin:  C01 C00 C03 C02 C05 C04 ...
	 * For 2 pins: C01 C00 C17 C16 C03 C02 ...
	 * For 3 pins: C01 C00 C17 C16 C33 C32 C03 C02 ...
	 */
	for (uint32_t ch=0;ch<nch;ch++)
	{
		uint32_t choff = ch*16;
		for (i=0; i < 16; i++) 
		{
			int16_t* dest16 = ((int16_t*) dest)+((i^1)&1)+ch*2+((i&-2)*nch); // need to swap MSW and LSW
			if (nullptr == block_input[i+choff])
				for (int j=0;j<AUDIO_BLOCK_SAMPLES; j++, dest16 += 16*nch) *dest16 = 0;
			else
				for (int j=0;j<AUDIO_BLOCK_SAMPLES; j++, dest16 += 16*nch) *dest16 = block_input[i+choff]->data[j];
		}
	}			
	#if IMXRT_CACHE_ENABLED >= 2
	arm_dcache_flush_delete(dc, tdm_txbuf_len / 2 );
	#endif
	
	for (i=0; i < 16*nch; i++) {
		if (block_input[i]) {
			release(block_input[i]);
			block_input[i] = nullptr;
		}
	}
}


void AudioOutputTDM16::update(void)
{
	audio_block_t *prev[channels];
	int i;
	unsigned int choff = (pin-1)*channels; // offset into block_channels

	__disable_irq();
	for (i=0; i < channels; i++) {
		prev[i] = block_input[i+choff];
		block_input[i+choff] = receiveReadOnly(i);
	}
	__enable_irq();
	for (i=0; i < channels; i++) {
		if (prev[i]) release(prev[i]);
	}
}

#if defined(KINETISK)
// MCLK needs to be 48e6 / 1088 * 512 = 22.588235 MHz -> 44.117647 kHz sample rate
//
#if F_CPU == 96000000 || F_CPU == 48000000 || F_CPU == 24000000
  // PLL is at 96 MHz in these modes
  #define MCLK_MULT 4
  #define MCLK_DIV  17
#elif F_CPU == 72000000
  #define MCLK_MULT 16
  #define MCLK_DIV  51
#elif F_CPU == 120000000
  #define MCLK_MULT 16
  #define MCLK_DIV  85
#elif F_CPU == 144000000
  #define MCLK_MULT 8
  #define MCLK_DIV  51
#elif F_CPU == 168000000
  #define MCLK_MULT 16
  #define MCLK_DIV  119
#elif F_CPU == 180000000
  #define MCLK_MULT 32
  #define MCLK_DIV  255
  #define MCLK_SRC  0
#elif F_CPU == 192000000
  #define MCLK_MULT 2
  #define MCLK_DIV  17
#elif F_CPU == 216000000
  #define MCLK_MULT 12
  #define MCLK_DIV  17
  #define MCLK_SRC  1
#elif F_CPU == 240000000
  #define MCLK_MULT 2
  #define MCLK_DIV  85
  #define MCLK_SRC  0
#elif F_CPU == 256000000
  #define MCLK_MULT 12
  #define MCLK_DIV  17
  #define MCLK_SRC  1
#else
  #error "This CPU Clock Speed is not supported by the Audio library";
#endif

#ifndef MCLK_SRC
#if F_CPU >= 20000000
  #define MCLK_SRC  3  // the PLL
#else
  #define MCLK_SRC  0  // system clock
#endif
#endif
#endif

void static_config_tdm(int pinTx, /* = -1, */ 	// transmit pin number, or -1 for unchanged
									int pinRx, /* = -1, */	// receive pin number, or -1 for unchanged
									int clks_per_frame /* = 256 */) // bit clocks per sample frame
{
#if defined(KINETISK)
	SIM_SCGC6 |= SIM_SCGC6_I2S;
	SIM_SCGC7 |= SIM_SCGC7_DMA;
	SIM_SCGC6 |= SIM_SCGC6_DMAMUX;

	// if either transmitter or receiver is enabled, do nothing
	if (I2S0_TCSR & I2S_TCSR_TE) return;
	if (I2S0_RCSR & I2S_RCSR_RE) return;

	// enable MCLK output
	I2S0_MCR = I2S_MCR_MICS(MCLK_SRC) | I2S_MCR_MOE;
	while (I2S0_MCR & I2S_MCR_DUF) ;
	I2S0_MDR = I2S_MDR_FRACT((MCLK_MULT-1)) | I2S_MDR_DIVIDE((MCLK_DIV-1));

	// configure transmitter
	I2S0_TMR = 0;
	I2S0_TCR1 = I2S_TCR1_TFW(4);
	I2S0_TCR2 = I2S_TCR2_SYNC(0) | I2S_TCR2_BCP | I2S_TCR2_MSEL(1)
		| I2S_TCR2_BCD | I2S_TCR2_DIV(0);
	I2S0_TCR3 = I2S_TCR3_TCE;
	I2S0_TCR4 = I2S_TCR4_FRSZ(7) | I2S_TCR4_SYWD(0) | I2S_TCR4_MF
		| I2S_TCR4_FSE | I2S_TCR4_FSD;
	I2S0_TCR5 = I2S_TCR5_WNW(31) | I2S_TCR5_W0W(31) | I2S_TCR5_FBT(31);

	// configure receiver (sync'd to transmitter clocks)
	I2S0_RMR = 0;
	I2S0_RCR1 = I2S_RCR1_RFW(4);
	I2S0_RCR2 = I2S_RCR2_SYNC(1) | I2S_TCR2_BCP | I2S_RCR2_MSEL(1)
		| I2S_RCR2_BCD | I2S_RCR2_DIV(0);
	I2S0_RCR3 = I2S_RCR3_RCE;
	I2S0_RCR4 = I2S_RCR4_FRSZ(7) | I2S_RCR4_SYWD(0) | I2S_RCR4_MF
		| I2S_RCR4_FSE | I2S_RCR4_FSD;
	I2S0_RCR5 = I2S_RCR5_WNW(31) | I2S_RCR5_W0W(31) | I2S_RCR5_FBT(31);

	// configure pin mux for 3 clock signals
	CORE_PIN23_CONFIG = PORT_PCR_MUX(6); // pin 23, PTC2, I2S0_TX_FS (LRCLK) - 44.1kHz
	CORE_PIN9_CONFIG  = PORT_PCR_MUX(6); // pin  9, PTC3, I2S0_TX_BCLK  - 11.2 MHz
	CORE_PIN11_CONFIG = PORT_PCR_MUX(6); // pin 11, PTC6, I2S0_MCLK - 22.5 MHz

#elif defined(__IMXRT1062__)
	// retrieve existing pin mask values
	uint32_t txPinMask = (I2S1_TCR3 / I2S_TCR3_TCE) & 0x0F;
	uint32_t rxPinMask = (I2S1_RCR3 / I2S_RCR3_RCE) & 0x0F;
	
	// create updated masks, if needed
	if (pinTx > 0) txPinMask |= (1<<pinTx)-1;
	if (pinRx > 0) rxPinMask |= (1<<pinRx)-1;
	
	CCM_CCGR5 |= CCM_CCGR5_SAI1(CCM_CCGR_ON);

	// if either transmitter or receiver is enabled, do nothing...
	if ((I2S1_TCSR & I2S_TCSR_TE) 
	 || (I2S1_RCSR & I2S_RCSR_RE))
	 { 
		// Documentation (e.g. 38.5.1.7.4) says we should only reset FIFOs when
		// a channel is disabled, but this seems to work OK as-is. Consider
		// changing to a temporary disable if there are issues. Note that
		// the calling code should do a soft reset anyway, which may be enough.
		I2S1_TCR3  = (I2S_TCR3_CFR | I2S_TCR3_TCE) * txPinMask; // ...except set channel flags
		I2S1_TCR4 |= txPinMask > 0x01?I2S_TCR4_FCOMB_ENABLED_ON_WRITES:0;
		I2S1_RCR3  = (I2S_RCR3_CFR | I2S_RCR3_RCE) * rxPinMask; 
		I2S1_RCR4 |= rxPinMask > 0x01?I2S_RCR4_FCOMB_ENABLED_ON_READS:0;
		return;
	 }
//PLL:
	int fs = AUDIO_SAMPLE_RATE_EXACT;
	// PLL between 27*24 = 648MHz und 54*24=1296MHz
	int n1 = 4; //SAI prescaler 4 => (n1*n2) = multiple of 4
	int n2 = 1 + (24000000 * 27) / (fs * 256 * n1);

	double C = ((double)fs * 256 * n1 * n2) / 24000000;
	int c0 = C;
	int c2 = 10000;
	int c1 = C * c2 - (c0 * c2);
	set_audioClock(c0, c1, c2);
	// clear SAI1_CLK register locations
	CCM_CSCMR1 = (CCM_CSCMR1 & ~(CCM_CSCMR1_SAI1_CLK_SEL_MASK))
		   | CCM_CSCMR1_SAI1_CLK_SEL(2); // &0x03 // (0,1,2): PLL3PFD0, PLL5, PLL4

	n1 = n1 / 2; //Double Speed for TDM

	CCM_CS1CDR = (CCM_CS1CDR & ~(CCM_CS1CDR_SAI1_CLK_PRED_MASK | CCM_CS1CDR_SAI1_CLK_PODF_MASK))
		   | CCM_CS1CDR_SAI1_CLK_PRED(n1-1) // &0x07
		   | CCM_CS1CDR_SAI1_CLK_PODF(n2-1); // &0x3f

	IOMUXC_GPR_GPR1 = (IOMUXC_GPR_GPR1 & ~(IOMUXC_GPR_GPR1_SAI1_MCLK1_SEL_MASK))
			| (IOMUXC_GPR_GPR1_SAI1_MCLK_DIR | IOMUXC_GPR_GPR1_SAI1_MCLK1_SEL(0));	//Select MCLK

	// configure transmitter
	int rsync = 0;
	int tsync = 1;
	int frame_size = clks_per_frame/32;  // => 4 or 8 32-bit words per frame

	I2S1_TMR = 0;
	I2S1_TCR1 = I2S_TCR1_RFW(4);
	I2S1_TCR2 = I2S_TCR2_SYNC(tsync) | I2S_TCR2_BCP | I2S_TCR2_MSEL(1)
		| I2S_TCR2_BCD | I2S_TCR2_DIV(0);
	I2S1_TCR3 = I2S_TCR3_TCE * txPinMask;
	I2S1_TCR4 = I2S_TCR4_FRSZ(frame_size-1) | I2S_TCR4_SYWD(0) | I2S_TCR4_MF
		| I2S_TCR4_FSE | I2S_TCR4_FSD |
		(txPinMask > 0x01?I2S_TCR4_FCOMB_ENABLED_ON_WRITES:0);
	I2S1_TCR5 = I2S_TCR5_WNW(31) | I2S_TCR5_W0W(31) | I2S_TCR5_FBT(31);

	I2S1_RMR = 0;
	I2S1_RCR1 = I2S_RCR1_RFW(4);
	I2S1_RCR2 = I2S_RCR2_SYNC(rsync) | I2S_TCR2_BCP | I2S_RCR2_MSEL(1)
		| I2S_RCR2_BCD | I2S_RCR2_DIV(0);
	I2S1_RCR3 = I2S_RCR3_RCE * rxPinMask;
	I2S1_RCR4 = I2S_RCR4_FRSZ(frame_size-1) | I2S_RCR4_SYWD(0) | I2S_RCR4_MF
		| I2S_RCR4_FSE | I2S_RCR4_FSD |
		(rxPinMask > 0x01?I2S_RCR4_FCOMB_ENABLED_ON_READS:0);
	I2S1_RCR5 = I2S_RCR5_WNW(31) | I2S_RCR5_W0W(31) | I2S_RCR5_FBT(31);

	CORE_PIN23_CONFIG = 3;  //1:MCLK
	CORE_PIN21_CONFIG = 3;  //1:RX_BCLK
	CORE_PIN20_CONFIG = 3;  //1:RX_SYNC
#endif
}

AudioHardwareTDM::DMAsettings AudioHardwareTDM::TxDMA = {0}, AudioHardwareTDM::RxDMA = {0};

void AudioHardwareTDM::config_tdm(int pinTx, /* = -1, */ 	// transmit pin number, or -1 for unchanged
									int pinRx, /* = -1, */	// receive pin number, or -1 for unchanged
									int clks_per_frame /* = 256 */) // bit clocks per sample frame
{
	static_config_tdm(pinTx,pinRx,clks_per_frame);
}

void AudioHardwareTDM::setDMA(DMAChannel& dma, uint32_t* buf, uint32_t buflen, bool which)
{
	DMAsettings* settings = which?&TxDMA:&RxDMA;
	
	settings->dma = &dma;
	settings->buf = buf;
	settings->buflen = buflen;
}

void AudioHardwareTDM::zapDMA(void)
{
	if (nullptr != TxDMA.dma)
	{
Serial.printf("Zapping TxDMA at %dms\n", millis());	
		TxDMA.dma->disable();
		
		I2S1_TCSR |= I2S_TCSR_SR; // must reset SAI1, or we lose sync
		
		TxDMA.dma->TCD->SADDR = TxDMA.buf;
		TxDMA.dma->TCD->SLAST = -TxDMA.buflen;
		TxDMA.dma->TCD->CITER_ELINKNO = TxDMA.buflen / 4;
		TxDMA.dma->TCD->BITER_ELINKNO = TxDMA.buflen / 4;
		TxDMA.dma->enable();

		I2S1_TCSR = I2S_TCSR_TE | I2S_TCSR_BCE | I2S_TCSR_FRDE;
	}
else Serial.println("Not zapping TxDMA - null pointer");		
delay(10);
	
	if (nullptr != RxDMA.dma)
	{
Serial.printf("Zapping RxDMA at %dms\n", millis());	
		RxDMA.dma->disable();
		
		I2S1_RCSR |= I2S_RCSR_SR; // must reset SAI1, or we lose sync
		
		RxDMA.dma->TCD->DADDR = RxDMA.buf;
		RxDMA.dma->TCD->DLASTSGA = -RxDMA.buflen;
		RxDMA.dma->TCD->CITER_ELINKNO = RxDMA.buflen / 4;
		RxDMA.dma->TCD->BITER_ELINKNO = RxDMA.buflen / 4;
		RxDMA.dma->enable();
		
		I2S1_RCSR = I2S_RCSR_RE | I2S_RCSR_BCE | I2S_RCSR_FRDE | I2S_RCSR_FR;		
	}
else Serial.println("Not zapping RxDMA - null pointer");		
delay(10);
	
}

void AudioHardwareTDM::printSettings()
{
	for (int which=0;which<2;which++)
	{
		DMAsettings* settings = which?&TxDMA:&RxDMA;
		
		Serial.printf("%s: ", which?"TxDMA":"RxDMA");
		Serial.printf("%08X; %08X, %d\n",
			(uint32_t) settings->dma,
			(uint32_t) settings->buf,
					   settings->buflen);
	}
	Serial.printf("BCLK is%s inverted\n",getBCLKinverted()?"":" not");
}


#endif
