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
	 || !initialisationDone) 
	{
		// ignore input and do nothing if undefined memory type
		if (nullptr != block)
			release(block);
		return;
	}
	
	if (block) 
	{
		writeWrap(head_offset, AUDIO_BLOCK_SAMPLES, block->data);
		release(block);
	} 
	else 
	{
		// if no input, store zeros, so later playback will
		// not be random garbage previously stored in memory
		zero(head_offset, AUDIO_BLOCK_SAMPLES);
	}
	head_offset += AUDIO_BLOCK_SAMPLES;
	if (head_offset >= memory_length)
		head_offset -= memory_length;

	// transmit the delayed outputs
	for (channel = 0; channel < CHANNEL_COUNT; channel++) 
	{
		modBlock = receiveReadOnly(channel+1); // get modulation signal, if any
		do 
		{
			if (0 == (activemask & (1<<channel)))
				break;
			block = allocate();
			if (!block) 
				break;
			
			// compute the delayed location where we read
			read_offset = head_offset - delay_length[channel];
			if (delay_length[channel] > head_offset) 
				read_offset += memory_length;
			
			if (nullptr == modBlock 		// no modulation, all samples delayed the same
			 || 0 == mod_depth[channel])
			{
				readWrap(read_offset, AUDIO_BLOCK_SAMPLES, block->data); // read in delayed samples, wrapping as needed
			}
			else
			{
				// Create offsets from the start of delay memory.
				// These are fixed-point integers in samples*256, so e.g.
				// 44.25 samples = 44.5*256 = 11392
				uint32_t offsets[AUDIO_BLOCK_SAMPLES], depth = mod_depth[channel];
				uint32_t* pl = (uint32_t*) modBlock->data;
				uint32_t read_offset256   = read_offset<<SIG_SHIFT; 	// scale the read offset the same way
				uint32_t memory_length256 = memory_length<<SIG_SHIFT;	// biggest allowed offset
				
				// Fill in the offsets. Even with zero modulation we stiil expect to increment
				// through the delay memory one sample at a time, so we add that to the
				// read offset as we go.
				for (int i=0;i<AUDIO_BLOCK_SAMPLES;i+=2)
				{
					uint32_t modhl = *pl++;
					offsets[i+0] = signed_multiply_32x16b(depth,modhl) + read_offset256;
					offsets[i+1] = signed_multiply_32x16t(depth,modhl) + read_offset256 + SIG_MULT;
					
					// correct for wrapping: weird-looking because it's unsigned
					if (offsets[i+0] > memory_length256) offsets[i+0] -= memory_length256;
					if (offsets[i+0] > memory_length256) offsets[i+0] += memory_length256<<1;
					if (offsets[i+1] > memory_length256) offsets[i+1] -= memory_length256;
					if (offsets[i+1] > memory_length256) offsets[i+1] += memory_length256<<1;
					read_offset256 += SIG_MULT + SIG_MULT;
				}
				
				// offsets[] now indexes the samples we want, most probably
				// fractional ones. Retrieve what we want from delay memory
				// and interpolate. This could get really inefficient with
				// super-high modulation rates or depths!
				//
				// For now we only do linear interpolation.
#define BUF_COUNT (AUDIO_BLOCK_SAMPLES*2)
				int16_t samples[BUF_COUNT+2]; // extra at the end for last sample interpolation
				int16_t* p = block->data;
				int i = 0;
				int maxDiff = BUF_COUNT << SIG_SHIFT;
				
				while (i < AUDIO_BLOCK_SAMPLES)
				{
					// divide memory block reads into chunks that fit our on-stack buffer
					uint32_t min = offsets[i], max = min;
					int j;
					for (j = i+1;j < AUDIO_BLOCK_SAMPLES; j++)
					{
						if (offsets[j] > max)
						{
							if (offsets[j] >  min + maxDiff)
								break;
							max = offsets[j];
						}
						if (offsets[j] < min) 
						{
							if (max > maxDiff + offsets[j])
								break;
							min = offsets[j];
						}
					}
					
					// min and max now define the delay memory zone we need
					// to load in order to process output samples i to j-1
					min >>= SIG_SHIFT;
					max >>= SIG_SHIFT;
					
					readWrap(min,max-min+2,samples);
					for(;i<j;i++)
					{
						// linear interpolation between two samples
						int idx = (offsets[i]>>SIG_SHIFT) - min;
						int sl = samples[idx]   * (SIG_MULT - (offsets[i] & SIG_MASK)),
							sh = samples[idx+1] *             (offsets[i] & SIG_MASK);
						sl = (sl + sh)>>SIG_SHIFT;
						p[i] = sl;
					}
				}
			}
			
			transmit(block, channel);
			release(block);
			
		} while (0);
		
		if (nullptr != modBlock)
			release(modBlock);
	}
}

