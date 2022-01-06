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
#include "DynMixer.h"
#include "utility/dspinst.h"

#if defined(__ARM_ARCH_7EM__)

static void applyGain(int16_t *data, int32_t mult)
{
	uint32_t *p = (uint32_t *)data;
	const uint32_t *end = (uint32_t *)(data + AUDIO_BLOCK_SAMPLES);

	do {
		uint32_t tmp32 = *p; // read 2 samples from *data
		int32_t val1 = signed_multiply_32x16b(mult, tmp32);
		int32_t val2 = signed_multiply_32x16t(mult, tmp32);
		val1 = signed_saturate_rshift(val1, 16, 0);
		val2 = signed_saturate_rshift(val2, 16, 0);
		*p++ = pack_16b_16b(val2, val1);
	} while (p < end);
}

static void applyGainThenAdd(int16_t *data, const int16_t *in, int32_t mult)
{
	uint32_t *dst = (uint32_t *)data;
	const uint32_t *src = (uint32_t *)in;
	const uint32_t *end = (uint32_t *)(data + AUDIO_BLOCK_SAMPLES);

	if (mult == MULTI_UNITYGAIN) {
		do {
			uint32_t tmp32 = *dst;
			*dst++ = signed_add_16_and_16(tmp32, *src++);
			tmp32 = *dst;
			*dst++ = signed_add_16_and_16(tmp32, *src++);
		} while (dst < end);
	} else {
		do {
			uint32_t tmp32 = *src++; // read 2 samples from *data
			int32_t val1 = signed_multiply_32x16b(mult, tmp32);
			int32_t val2 = signed_multiply_32x16t(mult, tmp32);
			val1 = signed_saturate_rshift(val1, 16, 0);
			val2 = signed_saturate_rshift(val2, 16, 0);
			tmp32 = pack_16b_16b(val2, val1);
			uint32_t tmp32b = *dst;
			*dst++ = signed_add_16_and_16(tmp32, tmp32b);
		} while (dst < end);
	}
}

#elif defined(KINETISL)

static void applyGain(int16_t *data, int32_t mult)
{
	const int16_t *end = data + AUDIO_BLOCK_SAMPLES;

	do {
		int32_t val = *data * mult;
		*data++ = signed_saturate_rshift(val, 16, 0);
	} while (data < end);
}

static void applyGainThenAdd(int16_t *dst, const int16_t *src, int32_t mult)
{
	const int16_t *end = dst + AUDIO_BLOCK_SAMPLES;

	if (mult == MULTI_UNITYGAIN) {
		do {
			int32_t val = *dst + *src++;
			*dst++ = signed_saturate_rshift(val, 16, 0);
		} while (dst < end);
	} else {
		do {
			int32_t val = *dst + ((*src++ * mult) >> 8); // overflow possible??
			*dst++ = signed_saturate_rshift(val, 16, 0);
		} while (dst < end);
	}
}

#endif

void AudioMixer::update(void)
{
	audio_block_t *in, *out=NULL;
	unsigned int channel;

	// use actual number of channels available
	for (channel=0; channel < num_inputs; channel++) {
		if (0 != multiplier[channel])
		{
			if (NULL != out) {
				in = receiveReadOnly(channel);
				if (in == NULL) continue;
				applyGainThenAdd(out->data, in->data, multiplier[channel]);
				release(in);
			} else {
				out = receiveWritable(channel);
				if (NULL == out) continue;
				int32_t mult = multiplier[channel];
				if (mult == MULTI_UNITYGAIN) continue;
				applyGain(out->data, mult);
			}
		}
	}
	if (NULL == out) return;
    transmit(out);
	release(out);
}

void AudioMixerStereo::update(void)
{
	audio_block_t *in, *outL=NULL, *outR=NULL;
	unsigned int channel;

	// use actual number of channels available
	for (channel=0; channel < num_inputs; channel++) 
	{
		in = receiveReadOnly(channel); // we need two copies, and this NULLs the inputQueue pointer
		
		if (NULL != in)
		{
			if (0 != multiplier[channel].mL)
			{
				if (NULL != outL) {				
						applyGainThenAdd(outL->data, in->data, multiplier[channel].mL);
				} else {
					outL = allocate();
					if (NULL != outL)
					{
						int32_t mult = multiplier[channel].mL;
						memcpy(outL->data, in->data, sizeof(outL->data));
						if (mult != MULTI_UNITYGAIN)
							applyGain(outL->data, mult);
					}
				}
			}
			
			if (0 != multiplier[channel].mR)
			{
				if (NULL != outR) {				
					applyGainThenAdd(outR->data, in->data, multiplier[channel].mR);
				} else {
					outR = allocate();
					if (NULL != outR)
					{
						int32_t mult = multiplier[channel].mR;
						memcpy(outR->data, in->data, sizeof(outR->data));
						if (mult != MULTI_UNITYGAIN)
							applyGain(outR->data, mult);
					}
				}
			}		
			release(in); 
		}
	}
	if (NULL != outL)
	{
		transmit(outL);
		release(outL);
	}
	if (NULL != outR)
	{
		transmit(outR,1);
		release(outR);
	}
}
