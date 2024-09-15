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

#ifndef effect_delay_ext_h_
#define effect_delay_ext_h_
#include <Arduino.h>     // github.com/PaulStoffregen/cores/blob/master/teensy4/Arduino.h
#include <AudioStream.h> // github.com/PaulStoffregen/cores/blob/master/teensy4/AudioStream.h
#include "spi_interrupt.h"
#include "extmem.h"


class AudioEffectDelayExternal : public AudioStream, public AudioExtMem
{
	static const int   CHANNEL_COUNT = 8;
	static const int   SAMPLE_BITS = 8*sizeof ((audio_block_t*) 0)->data[0]; // assume audio block data is integer type
	static const int   SIG_SHIFT = 8; 			// bit shift to preserve significance
	static const int   SIG_MULT = 1<<SIG_SHIFT;	// multiplier to preserve significance
	static const int   SIG_MASK = SIG_MULT-1;	// mask of fractional bits
	static constexpr float MOD_SCALE = AUDIO_SAMPLE_RATE_EXACT / 1000.0f 
								  / pow(2, SAMPLE_BITS-1) 
								  * SIG_MULT * (1<<16);
public:
	AudioEffectDelayExternal(AudioEffectDelayMemoryType_t type, 
							 float milliseconds=1e6,
							 bool forceInitialize = true)
	  : AudioStream(CHANNEL_COUNT+1, inputQueueArray), 
		AudioExtMem(type, (milliseconds*(AUDIO_SAMPLE_RATE_EXACT/1000.0f))+0.5f, forceInitialize),
		mod_depth{0}, activemask(0)
		{}
	AudioEffectDelayExternal() : AudioEffectDelayExternal(AUDIO_MEMORY_23LC1024) {}
	
	~AudioEffectDelayExternal() {}
	
	void delay(uint8_t channel, float milliseconds) {
		if (channel >= CHANNEL_COUNT || memory_type >= AUDIO_MEMORY_UNDEFINED) return;
		if (!initialisationDone)
			initialize();
		if (milliseconds < 0.0f) milliseconds = 0.0f;
		uint32_t n = (milliseconds*(AUDIO_SAMPLE_RATE_EXACT/1000.0f))+0.5f;
		n += AUDIO_BLOCK_SAMPLES;
		if (n > memory_length - AUDIO_BLOCK_SAMPLES)
			n = memory_length - AUDIO_BLOCK_SAMPLES;
		delay_length[channel] = n;
		uint8_t mask = activemask;
		if (activemask == 0 && IS_SPI_TYPE) AudioStartUsingSPI();
		activemask = mask | (1<<channel);
	}
	
	void disable(uint8_t channel) {
		if (channel >= CHANNEL_COUNT) return;
		if (!initialisationDone)
			initialize();
		uint8_t mask = activemask & ~(1<<channel);
		activemask = mask;
		if (mask == 0 && IS_SPI_TYPE) AudioStopUsingSPI();
	}
	
	float setModDepth(uint8_t channel,
					  float milliseconds) //!< how far from nominal delay a full-scale modulation input gives
	{
		float result = -1.0f;
		
		if (channel < CHANNEL_COUNT)
		{
			float base = (float) (delay_length[channel] - AUDIO_BLOCK_SAMPLES)
						/ (AUDIO_SAMPLE_RATE_EXACT/1000.0f); // unmodulated delay
			float maxl = (float) memory_length
						/ (AUDIO_SAMPLE_RATE_EXACT/1000.0f); // max delay in milliseconds
			if (base - milliseconds < 0.0f) // modulation too big
				milliseconds = base;		// max possble
			if (base + milliseconds > maxl)
				milliseconds = maxl - base;
			
			// modulation depth is now sane
			int32_t n = milliseconds*MOD_SCALE + 0.5f; // scale to usable integer
			mod_depth[channel] = n;
			result = (float) n / MOD_SCALE; // actual exact modulation depth
		}
		
		return result;
	}
	
	virtual void update(void);
	
// move these to private later
	int32_t mod_depth[CHANNEL_COUNT];
private:
	uint32_t delay_length[CHANNEL_COUNT]; // # of sample delay for each channel (128 = no delay)
	uint16_t  activemask;      // which output channels are active
	audio_block_t *inputQueueArray[CHANNEL_COUNT+1];
};

#endif
