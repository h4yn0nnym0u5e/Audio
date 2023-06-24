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
#include "extmem.h"
#include "effect_delay_ext.h"
#include "utility/dspinst.h"


void AudioEffectDelayExternal::update(void)
{
	audio_block_t *block, *modBlock;
	uint32_t n, channel, read_offset;

	// grab incoming data and put it into the memory
	block = receiveReadOnly();
	if (memory_type >= AUDIO_MEMORY_UNDEFINED
	 || !initialisationDone) {
		// ignore input and do nothing if undefined memory type
		if (nullptr != block)
			release(block);
		return;
	}
	if (block) {
		if (head_offset + AUDIO_BLOCK_SAMPLES <= memory_length) {
			// a single write is enough
			write(head_offset, AUDIO_BLOCK_SAMPLES, block->data);
			head_offset += AUDIO_BLOCK_SAMPLES;
		} else {
			// write wraps across end-of-memory
			n = memory_length - head_offset;
			write(head_offset, n, block->data);
			head_offset = AUDIO_BLOCK_SAMPLES - n;
			write(0, head_offset, block->data + n);
		}
		release(block);
	} else {
		// if no input, store zeros, so later playback will
		// not be random garbage previously stored in memory
		if (head_offset + AUDIO_BLOCK_SAMPLES <= memory_length) {
			zero(head_offset, AUDIO_BLOCK_SAMPLES);
			head_offset += AUDIO_BLOCK_SAMPLES;
		} else {
			n = memory_length - head_offset;
			zero(head_offset, n);
			head_offset = AUDIO_BLOCK_SAMPLES - n;
			zero(0, head_offset);
		}
	}

	// transmit the delayed outputs
	for (channel = 0; channel < 8; channel++) {
		modBlock = receiveReadOnly(channel+1); // get modulation signal, if any
		do {
			if (0 == (activemask & (1<<channel)))
				break;
			block = allocate();
			if (!block) 
				break;
			
			// compute the delayed location where we read
			if (delay_length[channel] <= head_offset) {
				read_offset = head_offset - delay_length[channel];
			} else {
				read_offset = memory_length + head_offset - delay_length[channel];
			}
			
			if (nullptr == modBlock 		// no modulation, all samples delayed the same
			 || 0 == mod_depth[channel])
			{
				if (read_offset + AUDIO_BLOCK_SAMPLES <= memory_length) {
					// a single read will do it
					read(read_offset, AUDIO_BLOCK_SAMPLES, block->data);
				} else {
					// read wraps across end-of-memory
					n = memory_length - read_offset;
					read(read_offset, n, block->data);
					read(0, AUDIO_BLOCK_SAMPLES - n, block->data + n);
				}
			}
			else
			{
				// Create offsets from the start of delay memory.
				// These are fixed-point integers in samples*256, so e.g.
				// 44.25 samples = 44.5*256 = 11392
				int offsets[AUDIO_BLOCK_SAMPLES], depth = mod_depth[channel];
				uint32_t* p = (uint32_t*) modBlock->data;
				uint32_t read_offset256 = read_offset<<SIG_SHIFT; // scale the read offset the same way
				
				// Fill in the offsets. Even with zero modulation we stiil expect to increment
				// through the delay memory one sample at a time, so we add that to the
				// read offset as we go.
				for (int i=0;i<AUDIO_BLOCK_SAMPLES;i+=2)
				{
					uint32_t modhl = *p++;
					offsets[i+0] = signed_multiply_32x16b(depth,modhl) + read_offset256;
					offsets[i+1] = signed_multiply_32x16t(depth,modhl) + read_offset256 + SIG_MULT;
					read_offset256 += SIG_MULT + SIG_MULT;
				}
				
				// offsets[] now indexes the samples we want, most probably
				// fractional ones. Retrieve what we want from delay memory
				// and interpolate. This could get really inefficient with
				// super-high modulation rates or depths!
				//
				// For now we only do linear interpolation.
				
			}
			
			transmit(block, channel);
			release(block);
			
		} while (0);
		if (nullptr != modBlock)
			release(modBlock);
	}
}

