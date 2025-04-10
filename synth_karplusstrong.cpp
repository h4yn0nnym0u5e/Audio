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

//=============================================================================
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

uint32_t AudioSynthKarplusStrong::IndexableBuffer::seed = 1;
bool AudioSynthKarplusStrong::IndexableBuffer::allocate(uint16_t count)
{
	bool result = true;
	size_t i;
	for (i=0; i < count && result; i++)
	{
		buffers[i] = AudioStream::allocate();
		if (nullptr == buffers[i])
			result = false;
	}
	bufferCount = i;
	sampleCount = bufferCount*AUDIO_BLOCK_SAMPLES;

	return result;
}

void AudioSynthKarplusStrong::IndexableBuffer::release(void)
{
	size_t i;
	for (i=0; i < maxBufferCount; i++)
	{
		if (nullptr != buffers[i])
		{
		 	AudioStream::release(buffers[i]);
			buffers[i] = nullptr;
		}
	}
	bufferCount = 0;
}

void AudioSynthKarplusStrong::IndexableBuffer::prefill(int samples, int32_t magnitude)
{
	uint32_t lo = seed;

	// fill one full cycle with pseudo-noise
	for (size_t i=0; i < bufferCount; i++) 		
	{
		int16_t* buffer = buffers[i]->data;
		for (size_t j=0; j < AUDIO_BLOCK_SAMPLES; j++)
		{
			lo = pseudorand(lo);
			buffer[j] = signed_multiply_32x16b(magnitude, lo);
			//if (0 >= --samples)
			//	break;
		}
	}
	seed = lo; // re-seed for different noise next time
}


//=============================================================================
void AudioSynthKarplusStrong::noteOn(float noteFreq, float velocity) 
{
	int bufferNum;
	int32_t bufferLen;

	if (velocity > 1.0f) {
		velocity = 0.0f;
	} else if (velocity <= 0.0f) {
		noteOff(1.0f);
		return;
	}
	magnitude = velocity * 65535.0f;
	if (state != silent) 	// already playing...
		noteOff(1.0f); 	// ... release buffers
	
	// pitch bend requires ability to reach lower frequency, 
	// so adjust requested frequency accordingly
	float frequency = noteFreq * maxShift;
	if (frequency < lowestFreq)
		frequency = lowestFreq;
	bufferLen = (AUDIO_SAMPLE_RATE_EXACT / frequency) + 0.5f; // length of one cycle
	bufferNum = bufferLen / AUDIO_BLOCK_SAMPLES + 1; // one cycle, rounded up
	
	if (!theBuffer.allocate(bufferNum)) // couldn't allocate, stay silent
		theBuffer.release();
	else
	{
		bufferIndex = 0;	
		state = started; // allocated, we're playing
	}

	// actual number of samples for requested note
	baseLen = AUDIO_SAMPLE_RATE_EXACT*increment / noteFreq;

Serial.printf("buffers: %d; state: %d; length: %d\n",
				theBuffer.bufferCount,state, baseLen);	
}


void AudioSynthKarplusStrong::noteOff(float velocity) 
{
	state = silent; // first, to prevent update() using stale pointers
	theBuffer.release();
}


void AudioSynthKarplusStrong::setLevel(float level,int16_t* levelPtr)
{
	if (level > 1.0f)
		level = 1.0f;
	*levelPtr = (int16_t) (level * 32767);
}


//-----------------------------------------------------------------------------
void AudioSynthKarplusStrong::update(void)
{
#if defined(KINETISK) || defined(__IMXRT1062__)
	audio_block_t *block, *input;
	
	// deal with drive
	input = receiveReadOnly();	// do we have a drive block?

	if (state == silent) // not actually playing...
	{
		if (nullptr != input)
			release(input); // ...release any drive block
		return;
	}
	
	// prepare to output
	block = allocate();
	if (nullptr == block)
	{
		state = silent; // darn: give up
		return;
	}

	// prepare audio data pointers, in and out
	int16_t *data = block->data;
	int16_t* drive = nullptr;
	if (nullptr != input)
		drive = input->data;

	// if just started, provide the initial stimulus		
	if (state == started) 
	{
		theBuffer.prefill(theBuffer.sampleCount, magnitude);
		state = playing;
	}

	// finally, create new audio data
	for (int i=0; i < AUDIO_BLOCK_SAMPLES; i++) 
	{
		int16_t prior = theBuffer[bufferIndex - increment]; // frequency fixed at "baseLen" samples
		int16_t in = theBuffer[bufferIndex - baseLen];
		int16_t out = (in * _feedbackLevel + prior * _feedbackLevel) >> 16;
		if (nullptr != drive)
			out += (*drive++ * _driveLevel) >> 16;
		*data++ = out;
		theBuffer[bufferIndex] = out; // store feedback data for next cycle
		bufferIndex += increment;
	}
	bufferIndex = theBuffer.limitToBufferFrac(bufferIndex);

	transmit(block);
	release(block); 
	
	if (nullptr != input)
		release(input);
#endif
}


