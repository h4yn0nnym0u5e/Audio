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
#include "input_tdm.h"
#if defined(KINETISK) || defined(__IMXRT1062__)
#include "utility/imxrt_hw.h"

audio_block_t* AudioInputTDMbase::block_incoming[MAX_TDM_INPUTS] = {nullptr};
int AudioInputTDMbase::clks_per_frame = 256;
int AudioInputTDMbase::pin_mask = 0;
bool AudioInputTDMbase::update_responsibility = false;
volatile AudioInputTDMbase::TDMstate_e AudioInputTDMbase::state = INACTIVE;
DMAChannel AudioInputTDMbase::dma(false);
#if defined(KINETISK)
DMAMEM __attribute__((aligned(32)))
static uint32_t tdm_rx_buffer[AUDIO_BLOCK_SAMPLES*16];
#elif defined(__IMXRT1062__)	
	uint32_t* AudioInputTDMbase::tdm_rx_malloc = nullptr;
	uint32_t* AudioInputTDMbase::tdm_rx_buffer = nullptr;
	uint32_t  AudioInputTDMbase::tdm_rxbuf_len = 0;
#endif // hardware-dependent


void AudioInputTDMbase::begin(int pin, //!< pin number, range 1-4
							  int cpf /* = 256 */) //!< clocks per frame => single/dual pins
{
	if (INACTIVE == state) // never been called before
	{
		clks_per_frame = cpf;
		dma.begin(true); // Allocate the DMA channel first
		for (int i=0; i < MAX_TDM_INPUTS; i++) 
			block_incoming[i] = nullptr;
	}
	else
	{
		elapsedMillis timeout = 0;
		
		state = STOPPING;
		while (STOPPING == state && timeout<20)
			;
		// ISR has disabled the DMA now, so we should be safe to
		// reallocate the TX buffer
	}

#if defined(__IMXRT1062__)
	// Each pin receives 16x 16-bit channels, and we double buffer, so:
	uint32_t rx_buf_needed = (clks_per_frame == 256)
							?pin*sizeof(uint32_t)*AUDIO_BLOCK_SAMPLES*16  // 16x 16-bit channels, or...
							:pin*sizeof(uint64_t)*AUDIO_BLOCK_SAMPLES* 4; //  4x 32-bit channels
	if (rx_buf_needed > tdm_rxbuf_len) // buffer isn't big enough
	{
		void* buf = realloc(tdm_rx_malloc, rx_buf_needed + 32); // need to 32-byte align
		if (nullptr != buf) // got a new buffer
		{
			tdm_rx_malloc = (uint32_t*) buf;  // actual memory allocated, in case we need to realloc()
			tdm_rx_buffer = (uint32_t*)(((uint32_t) buf+32) & -32U);
			tdm_rxbuf_len = rx_buf_needed;
			pin_mask |= (1<<(pin))-1; // enable all pins up to and including this one
		}		
	}
	setDMA(dma, tdm_rx_buffer, tdm_rxbuf_len, false);
#endif // defined(__IMXRT1062__)

	// TODO: should we set & clear the I2S_RCSR_SR bit here?
	config_tdm(-1, pin, clks_per_frame);  // leave Tx pins, configure Rx pins
#if defined(KINETISK)
	CORE_PIN13_CONFIG = PORT_PCR_MUX(4); // pin 13, PTC5, I2S0_RXD0
	dma.TCD->SADDR = &I2S0_RDR0;
	dma.TCD->SOFF = 0;
	dma.TCD->ATTR = DMA_TCD_ATTR_SSIZE(2) | DMA_TCD_ATTR_DSIZE(2);
	dma.TCD->NBYTES_MLNO = 4;
	dma.TCD->SLAST = 0;
	dma.TCD->DADDR = tdm_rx_buffer;
	dma.TCD->DOFF = 4;
	dma.TCD->CITER_ELINKNO = sizeof(tdm_rx_buffer) / 4;
	dma.TCD->DLASTSGA = -sizeof(tdm_rx_buffer);
	dma.TCD->BITER_ELINKNO = sizeof(tdm_rx_buffer) / 4;
	dma.TCD->CSR = DMA_TCD_CSR_INTHALF | DMA_TCD_CSR_INTMAJOR;
	dma.triggerAtHardwareEvent(DMAMUX_SOURCE_I2S0_RX);
	update_responsibility = update_setup();
	dma.enable();

	I2S0_RCSR |= I2S_RCSR_RE | I2S_RCSR_BCE | I2S_RCSR_FRDE | I2S_RCSR_FR;
	I2S0_TCSR |= I2S_TCSR_TE | I2S_TCSR_BCE; // TX clock enable, because sync'd to TX
	dma.attachInterrupt(isr);
#elif defined(__IMXRT1062__)
	switch (pin_mask)
	{
	  case 0x0F:
		CORE_PIN32_CONFIG  = 3;  //1:RX_DATA3
	  case 0x07:
		CORE_PIN9_CONFIG  = 3;  //1:RX_DATA2
	  case 0x03:
		CORE_PIN6_CONFIG = 3;  //1:RX_DATA1
	  case 0x01:
		CORE_PIN8_CONFIG  = 3;  //1:RX_DATA0
		break;
	}

	if (STOPPED != state) // first call to begin(): set everything up
	{
		IOMUXC_SAI1_RX_DATA0_SELECT_INPUT = 2;
		dma.TCD->SADDR = &I2S1_RDR0;
		dma.TCD->SOFF = 0;
		dma.TCD->ATTR = DMA_TCD_ATTR_SSIZE(2) | DMA_TCD_ATTR_DSIZE(2);
		dma.TCD->NBYTES_MLNO = 4;
		dma.TCD->SLAST = 0;
		dma.TCD->DADDR = tdm_rx_buffer;
		dma.TCD->DOFF = 4;
		dma.TCD->CITER_ELINKNO = tdm_rxbuf_len / 4;
		dma.TCD->DLASTSGA = -tdm_rxbuf_len;
		dma.TCD->BITER_ELINKNO = tdm_rxbuf_len / 4;
		dma.TCD->CSR = DMA_TCD_CSR_INTHALF | DMA_TCD_CSR_INTMAJOR;
		dma.triggerAtHardwareEvent(DMAMUX_SOURCE_SAI1_RX);
		if (!update_responsibility)
			update_responsibility = update_setup();

		dma.attachInterrupt(isr);	
	}
	zapDMA();  // (re)start hardware and DMA for this and any output objects

	I2S1_RCSR = I2S_RCSR_RE | I2S_RCSR_BCE | I2S_RCSR_FRDE | I2S_RCSR_FR;
#endif	
	state = ACTIVE;
}


void AudioInputTDMbase::isr(void)
{
	uint32_t daddr, nch;
	const uint32_t* src;
	unsigned int i;
	
	switch (pin_mask) // figure out how many pins we're using
	{
		default:
		case 0x01: nch = 1; break;
		case 0x03: nch = 2; break;
		case 0x07: nch = 3; break;
		case 0x0F: nch = 4; break;
	}
#if defined(KINETISK)
	uint32_t tdm_rxbuf_len = sizeof(tdm_rx_buffer);
#endif	
	

	daddr = (uint32_t)(dma.TCD->DADDR);
	dma.clearInterrupt();

	if (daddr < (uint32_t)tdm_rx_buffer + tdm_rxbuf_len / 2) {
		// DMA is receiving to the first half of the buffer
		// need to remove data from the second half
		src = (uint32_t*)((uint32_t)tdm_rx_buffer + tdm_rxbuf_len / 2);
	} else {
		// DMA is receiving to the second half of the buffer
		// need to remove data from the first half
		src = &tdm_rx_buffer[0];
	}
	
	if (block_incoming[0] != nullptr) // first block not present means none are present: drop the mic
	{
		#if IMXRT_CACHE_ENABLED >=1
		arm_dcache_delete((void*)src, tdm_rxbuf_len / 2);
		#endif
		if (256 == clks_per_frame)
		{
			/*
			 * For 1 pin:  C01 C00 C03 C02 C05 C04 ...
			 * For 2 pins: C01 C00 C17 C16 C03 C02 ...
			 * For 3 pins: C01 C00 C17 C16 C33 C32 C03 C02 ...
			 */
			for (uint32_t pin=0;pin<nch;pin++)
			{
				uint32_t choff = pin*16;
				for (i=0; i < 16; i++) 
				{
					int16_t* src16 = ((int16_t*) src)+((i^1)&1)+pin*2+((i&-2)*nch); // need to swap MSW and LSW
					for (int j=0;j<AUDIO_BLOCK_SAMPLES; j++, src16 += 16*nch) block_incoming[i+choff]->data[j] = *src16;
				}
			}
		}
		else
		{
			for (uint32_t pin=0;pin<nch;pin++) // nch = 2 or 4
			{
				uint32_t choff = pin*4;
				for (i=0; i < 4; i++) 
				{
					int32_t* src32 = (int32_t*) src+i*nch+pin;
					for (int j=0;j<AUDIO_BLOCK_SAMPLES; j++, src32 += 4*nch) block_incoming[i+choff]->data[j] = (*src32)/65536;
				}
			}
		}
	}
	if (update_responsibility) update_all();
}


void AudioInputTDM16::update(void)
{
	unsigned int i, j;
	audio_block_t* new_block[16];
	audio_block_t* out_block[16];
	audio_block_t** incoming = block_incoming + (pin-1)*16;

	// allocate 16 new blocks.  If any fails, allocate none
	for (i=0; i < 16; i++) 
	{
		new_block[i] = allocate();
		if (new_block[i] == nullptr) 
		{
			for (j=0; j < i; j++) 
				release(new_block[j]);
			memset(new_block, 0, sizeof(new_block));
			break;
		}
	}
	__disable_irq();
	memcpy(out_block, incoming, sizeof(out_block)); // copy blocks to transmit
	memcpy(incoming, new_block, sizeof(new_block)); // queue new, empty blocks ready for filling	
	__enable_irq();
	if (out_block[0] != nullptr) {		
		// if we got 1 block, all 16 are filled
		for (i=0; i < 16; i++) {
			transmit(out_block[i], i);
			release(out_block[i]);
		}
	}
}


void AudioInputTDM8::update(void)
{
	unsigned int i, j;
	audio_block_t* new_block[8];
	audio_block_t* out_block[8];
	audio_block_t** incoming = block_incoming + (pin/2-1)*8;

	// allocate 8 new blocks.  If any fails, allocate none
	for (i=0; i < 8; i++) 
	{
		new_block[i] = allocate();
		if (new_block[i] == nullptr) 
		{
			for (j=0; j < i; j++) 
				release(new_block[j]);
			memset(new_block, 0, sizeof(new_block));
			break;
		}
	}
	__disable_irq();
	memcpy(out_block, incoming, sizeof(out_block)); // copy blocks to transmit
	memcpy(incoming, new_block, sizeof(new_block)); // queue new, empty blocks ready for filling	
	__enable_irq();
	if (out_block[0] != nullptr) {		
		// if we got 1 block, all 8 are filled
		for (i=0; i < 8; i++) {
			transmit(out_block[i], i);
			release(out_block[i]);
		}
	}
}
#endif
