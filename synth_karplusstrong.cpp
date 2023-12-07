/* Audio Library for Teensy 3.X
 * Copyright (c) 2016, Paul Stoffregen, paul@pjrc.com
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
#include "synth_karplusstrong.h"

#if defined(KINETISK) || defined(__IMXRT1062__)
static uint32_t pseudorand(uint32_t lo)
{
	uint32_t hi;

	hi = multiply_16bx16t(16807, lo); // 16807 * (lo >> 16)
	lo = 16807 * (lo & 0xFFFF);
	lo += (hi & 0x7FFF) << 16;
	lo += hi >> 15;
	lo = (lo & 0x7FFFFFFF) + (lo >> 31);
	return lo;
}
#endif


void AudioSynthKarplusStrong::noteOn(float frequency, float velocity) 
{
	if (velocity > 1.0f) {
		velocity = 0.0f;
	} else if (velocity <= 0.0f) {
		noteOff(1.0f);
		return;
	}
	magnitude = velocity * 65535.0f;
	if (state != silent) 	// already playing...
		noteOff(1.0f); 	// ... release buffers
	
	if (frequency < lowestFreq)
		frequency = lowestFreq;
	bufferLen = (AUDIO_SAMPLE_RATE_EXACT / frequency) + 0.5f; // length of one cycle
	bufferCount = bufferLen / AUDIO_BLOCK_SAMPLES + 1; // one cycle, rounded up
	bufferIndexLimit = bufferLen - (bufferCount - 1)*AUDIO_BLOCK_SAMPLES;
	bufferIndex = 0;
	bufferNum = 0;
	
	size_t i;
	for (i=0; i < bufferCount; i++)
	{
		buffers[i] = allocate();
		if (nullptr == buffers[i])
		{
			bufferLen = 0;
			break;
		}
	}
	
	if (0 == bufferLen) // couldn't allocate, stay silent
	{
		for (i=0; i < bufferCount; i++)
		{
			if (nullptr == buffers[i]) // nothing beyond here, quite
				break;
			else
			{
				release(buffers[i]);
				buffers[i] = nullptr;
			}
		}
	}
	else
		state = started; // allocated, we're playing
Serial.printf("buffers: %d; state: %d\n",bufferCount,state);	
}


void AudioSynthKarplusStrong::noteOff(float velocity) 
{
	state = silent; // first, to prevent update() using stale pointers
	for (size_t i=0;i<maxBufferCount;i++)
	{
		if (nullptr == buffers[i])
			break;
		else
		{
			release(buffers[i]);
			buffers[i] = nullptr;
		}
	}
}


void AudioSynthKarplusStrong::setLevel(float level,int16_t* levelPtr)
{
	if (level > 1.0f)
		level = 1.0f;
	*levelPtr = (int16_t) (level * 32767);
}


void AudioSynthKarplusStrong::update(void)
{
#if defined(KINETISK) || defined(__IMXRT1062__)
	audio_block_t *block, *input;
	
	// deal with drive
	input = receiveReadOnly();	// do we have a drive block?

	if (state == silent) 
	{
		if (nullptr != input)
			release(input);
		return;
	}
	
	block = allocate();
	if (nullptr == block)
	{
		state = 0;
		return;
	}

	int16_t *data = block->data;
	int16_t* drive = nullptr;
	if (nullptr != input)
		drive = input->data;

	if (state == started) 
	{
		uint32_t lo = seed;
		int samples = bufferLen;
		
		for (size_t i=0; i < bufferCount; i++) 		
		{
			int16_t* buffer = buffers[i]->data;
			for (size_t j=0; j < AUDIO_BLOCK_SAMPLES; j++)
			{
				lo = pseudorand(lo);
				buffer[j] = signed_multiply_32x16b(magnitude, lo);
				if (0 >= --samples)
					break;
			}
		}
		seed = lo;
		state = playing;
	}

	int16_t prior;
	if (bufferIndex > 0) 
		prior = buffers[bufferNum]->data[bufferIndex - 1];
	else 
	{
		if (bufferNum > 0)
			prior = buffers[bufferNum - 1]->data[AUDIO_BLOCK_SAMPLES - 1];
		else
			prior = buffers[bufferCount - 1]->data[bufferIndexLimit - 1];
	}
	
	int16_t* buffer = buffers[bufferNum]->data;
	size_t limit = (bufferNum == bufferCount - 1)
							?bufferIndexLimit
							:AUDIO_BLOCK_SAMPLES;
							
	// remember where feedback data gets stored, in case
	// we want to overwite it
	fbkNum = bufferNum;
	fbkIndex = bufferIndex;
	
	for (int i=0; i < AUDIO_BLOCK_SAMPLES; i++) 
	{
		int16_t in = buffer[bufferIndex];
		int16_t out = (in * _feedbackLevel + prior * _feedbackLevel) >> 16;
		if (nullptr != drive)
			out += (*drive++ * _driveLevel) >> 16;
		*data++ = out;
		buffer[bufferIndex] = out; // store feedback data for next cycle
		prior = in;
		
		if (++bufferIndex >= limit) // reached the end of this audio block
		{
			bufferIndex = 0;
			bufferNum++;
			if (bufferNum >= bufferCount) // end of all blocks
				bufferNum = 0;
			
			buffer = buffers[bufferNum]->data;
			limit = (bufferNum == bufferCount - 1)
							?bufferIndexLimit
							:AUDIO_BLOCK_SAMPLES;
		}
	}

	transmit(block);
	release(block); 
	
	if (nullptr != input)
		release(input);
#endif
}


uint32_t AudioSynthKarplusStrong::seed = 1;

