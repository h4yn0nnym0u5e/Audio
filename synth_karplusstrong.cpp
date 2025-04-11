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
	float frequency = noteFreq * maxBend;
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
}


void AudioSynthKarplusStrong::noteOff(float velocity) 
{
	state = silent; // first, to prevent update() using stale pointers
	theBuffer.release();
}


void AudioSynthKarplusStrong::setLevel(float level,int16_t* levelPtr)
{
	if (level > 1.0f) level = 1.0f;
	if (level < 0.0f) level = 0.0f;
	*levelPtr = (int16_t) (level * 32767);
}


/*
 * Code lifted from modulated waveform. It's no longer phase data, but
 * we keep the parameter name so it's easier to do a diff.
 *
 * Compute a list of period intervals into phasedata[], 
 * adjusted by the bend data supplied in bend[]
 */
void AudioSynthKarplusStrong::computeBendData(uint32_t* phasedata, int16_t* bp)
{
	for (int i=0; i < AUDIO_BLOCK_SAMPLES; i++) 
	{
		int32_t n = (*bp++) * modulation_factor; // n is # of octaves to mod
		int32_t ipart = n >> 27; // 4 integer bits
		n &= 0x7FFFFFF;          // 27 fractional bits
#ifdef IMPROVE_EXPONENTIAL_ACCURACY
		// exp2 polynomial suggested by Stefan Stenzel on "music-dsp"
		// mail list, Wed, 3 Sep 2014 10:08:55 +0200
		int32_t x = n << 3;
		n = multiply_accumulate_32x32_rshift32_rounded(536870912, x, 1494202713);
		int32_t sq = multiply_32x32_rshift32_rounded(x, x);
		n = multiply_accumulate_32x32_rshift32_rounded(n, sq, 1934101615);
		n = n + (multiply_32x32_rshift32_rounded(sq,
			multiply_32x32_rshift32_rounded(x, 1358044250)) << 1);
		n = n << 1;
#else
		// exp2 algorithm by Laurent de Soras
		// https://www.musicdsp.org/en/latest/Other/106-fast-exp2-approximation.html
		n = (n + 134217728) << 3;

		n = multiply_32x32_rshift32_rounded(n, n);
		n = multiply_32x32_rshift32_rounded(n, 715827883) << 3;
		n = n + 715827882;
#endif
		uint32_t scale = n >> (14 - ipart); // this is in 16.16 format
		int64_t per = (int64_t) baseLen * (int64_t) scale; // 24.8 * 16.16 = 40.24
		phasedata[i] = per >> 16;
	}
}


//-----------------------------------------------------------------------------
void AudioSynthKarplusStrong::update(void)
{
#if defined(KINETISK) || defined(__IMXRT1062__)
	audio_block_t *block, *input, *bend;
	
	// deal with drive and bend
	input = receiveReadOnly(0);	// do we have a drive block?
	bend  = receiveReadOnly(1); // or a bend block?

	if (state == silent) // not actually playing...
	{
		if (nullptr != input) release(input); // ...release any drive...
		if (nullptr != bend)  release(bend);  // ... and bend
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
	if (nullptr == bend) // no bend, just compute it
	{
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
	}
	else
	{
		uint32_t perData[AUDIO_BLOCK_SAMPLES];

		computeBendData(perData,bend->data); // compute look-back amount for each sample
		release(bend);
		for (int i=0; i < AUDIO_BLOCK_SAMPLES; i++) 
		{
			int16_t prior = theBuffer[bufferIndex - increment]; // frequency fixed at "baseLen" samples
			int16_t in = theBuffer[bufferIndex - perData[i]];
			int16_t out = (in * _feedbackLevel + prior * _feedbackLevel) >> 16;
			if (nullptr != drive)
				out += (*drive++ * _driveLevel) >> 16;
			*data++ = out;
			theBuffer[bufferIndex] = out; // store feedback data for next cycle
			bufferIndex += increment;
		}
	}
	bufferIndex = theBuffer.limitToBufferFrac(bufferIndex);

	transmit(block);
	release(block); 
	
	if (nullptr != input)
		release(input);
#endif
}


