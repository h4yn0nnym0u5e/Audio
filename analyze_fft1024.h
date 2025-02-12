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

#ifndef analyze_fft1024_h_
#define analyze_fft1024_h_

#include <Arduino.h>     // github.com/PaulStoffregen/cores/blob/master/teensy4/Arduino.h
#include <AudioStream.h> // github.com/PaulStoffregen/cores/blob/master/teensy4/AudioStream.h
#include <arm_math.h>    // github.com/PaulStoffregen/cores/blob/master/teensy4/arm_math.h
#include "utility/dspinst.h"

// windows.c - FFT256
extern "C" {
extern const int16_t AudioWindowHanning256[];
extern const int16_t AudioWindowBartlett256[];
extern const int16_t AudioWindowBlackman256[];
extern const int16_t AudioWindowFlattop256[];
extern const int16_t AudioWindowBlackmanHarris256[];
extern const int16_t AudioWindowNuttall256[];
extern const int16_t AudioWindowBlackmanNuttall256[];
extern const int16_t AudioWindowWelch256[];
extern const int16_t AudioWindowHamming256[];
extern const int16_t AudioWindowCosine256[];
extern const int16_t AudioWindowTukey256[];
}

// windows.c - FFT1024
extern "C" {
extern const int16_t AudioWindowHanning1024[];
extern const int16_t AudioWindowBartlett1024[];
extern const int16_t AudioWindowBlackman1024[];
extern const int16_t AudioWindowFlattop1024[];
extern const int16_t AudioWindowBlackmanHarris1024[];
extern const int16_t AudioWindowNuttall1024[];
extern const int16_t AudioWindowBlackmanNuttall1024[];
extern const int16_t AudioWindowWelch1024[];
extern const int16_t AudioWindowHamming1024[];
extern const int16_t AudioWindowCosine1024[];
extern const int16_t AudioWindowTukey1024[];
}

class AudioAnalyzeFFT_Base : public AudioStream
{
	// this defines the size of the FFT, and
	// MUST corresond to  the window size!
	const unsigned int NUM_BINS;
	const unsigned int NUM_PREV; // number of previous blocks blocklist can hold
public:
	AudioAnalyzeFFT_Base(unsigned int bins, 
						 unsigned int blks, 
						 const int16_t* win,
						 audio_block_t** bl,
						 int16_t* buf,
						 uint16_t* op,
						 uint32_t* su,
						 uint8_t nav) 
		: AudioStream(1, inputQueueArray),
		NUM_BINS(bins),
		NUM_PREV(blks),
		output(op),
	  	window(win), 
		blocklist(bl),
		buffer(buf),
		sum(su), naverage(nav),
		state(0), outputflag(false) 
	{
		arm_cfft_radix4_init_q15(&fft_inst, NUM_BINS*2, 0, 1);
	}


	bool available() {
		if (outputflag == true) {
			outputflag = false;
			return true;
		}
		return false;
	}


	float read(unsigned int binNumber) {
		if (binNumber >= NUM_BINS) return 0.0;
		return (float)(output[binNumber]) * (1.0f / 16384.0f);
	}


	float read(unsigned int binFirst, unsigned int binLast) {
		if (binFirst > binLast) {
			unsigned int tmp = binLast;
			binLast = binFirst;
			binFirst = tmp;
		}
		if (binFirst >= NUM_BINS) return 0.0;
		if (binLast >= NUM_BINS) binLast = NUM_BINS-1;
		uint32_t sum = 0;
		do {
			sum += output[binFirst++];
		} while (binFirst <= binLast);
		return (float)sum * (1.0f / 16384.0f);
	}


	void averageTogether(uint8_t n) {
		if (n == 0) n = 1;
		naverage = n;
	}


	void windowFunction(const int16_t *w) {
		window = w;
	}

	// C++ mandates this is inline, which is what we want
	uint32_t makeMagSq(int16_t* buffer, unsigned int i)
	{
		uint32_t tmp = *((uint32_t *)buffer + i); // real & imag
		return multiply_16tx16t_add_16bx16b(tmp, tmp);
	}

	
	virtual void update(void);
	uint16_t* output;
private:
	const int16_t *window;
	audio_block_t** blocklist;
	int16_t*  buffer;
	uint32_t* sum;
	uint8_t count;
	uint8_t naverage;
	uint8_t state; // OK for block size down to 2 samples!
	volatile bool outputflag;
	audio_block_t *inputQueueArray[1];
	arm_cfft_radix4_instance_q15 fft_inst;
};


class AudioAnalyzeFFT1024 : public AudioAnalyzeFFT_Base
{
		static const unsigned int NUM_BINS = 512;
		static const unsigned int NUM_PREV = NUM_BINS >  AUDIO_BLOCK_SAMPLES
												?(NUM_BINS*2 / AUDIO_BLOCK_SAMPLES)
												:1;
		audio_block_t *blocklist[NUM_PREV];
		int16_t buffer[NUM_BINS*4] __attribute__ ((aligned (4)));
		uint32_t su[NUM_BINS];
	public:
		AudioAnalyzeFFT1024() 
			: AudioAnalyzeFFT_Base(NUM_BINS, NUM_PREV,
								   AudioWindowHanning1024,
								   blocklist, buffer, output, 
								   su, 1)
			{}
		uint16_t output[NUM_BINS] __attribute__ ((aligned (4)));
};


class AudioAnalyzeFFT256n : public AudioAnalyzeFFT_Base
{
		static const unsigned int NUM_BINS = 128;
		static const unsigned int NUM_PREV = 0;
		int16_t buffer[NUM_BINS*4] __attribute__ ((aligned (4)));
		uint32_t su[NUM_BINS];
	public:
		AudioAnalyzeFFT256n() 
			: AudioAnalyzeFFT_Base(NUM_BINS, NUM_PREV,
								   AudioWindowHanning256,
								   nullptr, buffer, output, 
								   su, 8)
			{}
		uint16_t output[NUM_BINS] __attribute__ ((aligned (4)));
};

#endif
