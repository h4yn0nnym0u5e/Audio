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


bool AudioOutputTDM_Base::update_responsibility = false;
DMAMEM __attribute__((aligned(32)))
static uint32_t zeros[AUDIO_BLOCK_SAMPLES/2];

void AudioOutputTDM_Base::begin(void)
{

	for (int i=0; i < 16; i++) {
		block_input[i] = nullptr;
	}
	memset(zeros, 0, sizeof(zeros));

	// TODO: should we set & clear the I2S_TCSR_SR bit here?
#if defined(KINETISK)
	configDMAtx(this, DMAisr, txBufSz,
				&I2S0_TDR0, 4, 0);

	config_tdm();
	CORE_PIN22_CONFIG = PORT_PCR_MUX(6); // pin 22, PTC1, I2S0_TXD0
	update_responsibility = update_setup();

	I2S0_TCSR = I2S_TCSR_SR;
	I2S0_TCSR = I2S_TCSR_TE | I2S_TCSR_BCE | I2S_TCSR_FRDE;

	#elif defined(__IMXRT1062__)

	configDMAtx(this, DMAisr, txBufSz,
				&I2S1_TDR0, 4, 0);

	configSAItx(AUDIO_SAMPLE_RATE_EXACT, false, 16);
	CORE_PIN7_CONFIG  = 3;  //1:TX_DATA0

	update_responsibility = update_setup();

	I2S1_RCSR |= I2S_RCSR_RE | I2S_RCSR_BCE;
	I2S1_TCSR = I2S_TCSR_TE | I2S_TCSR_BCE | I2S_TCSR_FRDE;

#endif
}

// TODO: needs optimization...
static void memcpy_tdm_tx(uint32_t *dest, const uint32_t *src1, const uint32_t *src2)
{
	uint32_t i, in1, in2, out1, out2;

	for (i=0; i < AUDIO_BLOCK_SAMPLES/4; i++) {

		in1 = *src1++;
		in2 = *src2++;
		out1 = (in1 << 16) | (in2 & 0xFFFF);
		out2 = (in1 & 0xFFFF0000) | (in2 >> 16);
		*dest = out1;
		*(dest + 8) = out2;

		in1 = *src1++;
		in2 = *src2++;
		out1 = (in1 << 16) | (in2 & 0xFFFF);
		out2 = (in1 & 0xFFFF0000) | (in2 >> 16);
		*(dest + 16)= out1;
		*(dest + 24) = out2;

		dest += 32;
	}
}

// static
void AudioOutputTDM_Base::DMAisr(void* instance)
{
	((AudioOutputTDM_Base*) instance)->isr();
}


void AudioOutputTDM_Base::isr(void)
{
	uint32_t *dest;
	const uint32_t *src1, *src2;
	uint32_t i, saddr;

#if defined(KINETISK) || defined(__IMXRT1062__)
	saddr = (uint32_t)(dma.TCD->SADDR);
#endif
	dma.clearInterrupt();
	if (saddr < (uint32_t) buffer + txBufSz / 2) {
		// DMA is transmitting the first half of the buffer
		// so we must fill the second half
		dest = buffer + AUDIO_BLOCK_SAMPLES*8;
	} else {
		// DMA is transmitting the second half of the buffer
		// so we must fill the first half
		dest = buffer;
	}
	if (update_responsibility) AudioStream::update_all();

	#if IMXRT_CACHE_ENABLED >= 2
	uint32_t *dc = dest;
	#endif
	
	for (i=0; i < 16; i += 2) {
		src1 = block_input[i] ? (uint32_t *)(block_input[i]->data) : zeros;
		src2 = block_input[i+1] ? (uint32_t *)(block_input[i+1]->data) : zeros;
		memcpy_tdm_tx(dest, src1, src2);
		dest++;
	}

	#if IMXRT_CACHE_ENABLED >= 2
	arm_dcache_flush_delete(dc, txBufSz / 2 );
	#endif

	for (i=0; i < 16; i++) {
		if (block_input[i]) {
			release(block_input[i]);
			block_input[i] = nullptr;
		}
	}
}


void AudioOutputTDM_Base::update(void)
{
	audio_block_t *prev[16];
	unsigned int i;

	__disable_irq();
	for (i=0; i < 16; i++) {
		prev[i] = block_input[i];
		block_input[i] = receiveReadOnly(i);
	}
	__enable_irq();
	for (i=0; i < 16; i++) {
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

void AudioOutputTDM_Base::config_tdm(void)
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
	I2S0_TCR1 = I2S_TCR1_TFW(FIFOwatermark);
	I2S0_TCR2 = I2S_TCR2_SYNC(0) | I2S_TCR2_BCP | I2S_TCR2_MSEL(1)
		| I2S_TCR2_BCD | I2S_TCR2_DIV(0);
	I2S0_TCR3 = I2S_TCR3_TCE;
	I2S0_TCR4 = I2S_TCR4_FRSZ(7) | I2S_TCR4_SYWD(0) | I2S_TCR4_MF
		| I2S_TCR4_FSE | I2S_TCR4_FSD;
	I2S0_TCR5 = I2S_TCR5_WNW(31) | I2S_TCR5_W0W(31) | I2S_TCR5_FBT(31);

	// configure receiver (sync'd to transmitter clocks)
	I2S0_RMR = 0;
	I2S0_RCR1 = I2S_RCR1_RFW(FIFOwatermark);
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
 
#endif
}

#endif
