/* Audio Library for Teensy 3.X
 * Copyright (c) 2014, Paul Stoffregen, paul@pjrc.com
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
#include "analyze_fft1024.h"
#include "sqrt_integer.h"
#include "utility/dspinst.h"


// 140312 - PAH - slightly faster copy
// 2025-02: JRO - merge copy and windowing code for better efficiency
#if defined(__ARM_ARCH_7EM__)
static void window_block_to_fft_buffer(audio_block_t* block, int blockNum, void *buffer, const void *window)
{
	uint32_t *buf = (uint32_t*)(((int16_t *)buffer) + AUDIO_BLOCK_SAMPLES*blockNum*2);
	const int16_t *win = (int16_t *)window + AUDIO_BLOCK_SAMPLES*blockNum;

	if (nullptr != window) // only window if we have one set!
	{
		for (int i=0; i < AUDIO_BLOCK_SAMPLES; i++) 
		{
			int32_t val = block->data[i] * *win++;
			*buf++ = val >> 15;
		}
	}
	else
	{
		for (int i=0; i < AUDIO_BLOCK_SAMPLES; i++) 
			*buf++ = block->data[i];
	}
}
#endif

void AudioAnalyzeFFT1024::update(void)
{
	const uint8_t HALF_FULL = NUM_BINS/AUDIO_BLOCK_SAMPLES;
	audio_block_t *block;

	block = receiveReadOnly();
	if (!block) return;

#if defined(__ARM_ARCH_7EM__)
	if (state < HALF_FULL) 
	{
		// startup - just store blocks
		blocklist[state] = block;
		state++;
	}
	else
	{
		// running
		// window old block to buffer and release it
		window_block_to_fft_buffer(blocklist[state-HALF_FULL], state-HALF_FULL, buffer, window);
		release(blocklist[state-HALF_FULL]);

		// window new block to buffer and retain
		window_block_to_fft_buffer(block, state, buffer, window);
		blocklist[state-HALF_FULL] = block;

		// wait for a full set of blocks
		if (HALF_FULL*2-1 != state)
		{
			state++;
		}
		else
		{
			// compute new FFT every HALF_FULL blocks received:
			arm_cfft_radix4_q15(&fft_inst, buffer);
			// TODO: support averaging multiple copies
			for (unsigned int i=0; i < NUM_BINS; i++) {
				uint32_t tmp = *((uint32_t *)buffer + i); // real & imag
				uint32_t magsq = multiply_16tx16t_add_16bx16b(tmp, tmp);
				output[i] = sqrt_uint32_approx(magsq);
			}
			outputflag = true;
			state = HALF_FULL;
		}
	}
#else
	release(block);
#endif
}


