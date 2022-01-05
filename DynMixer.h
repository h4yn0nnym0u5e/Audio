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

#ifndef DYNMIXER_H_
#define DYNMIXER_H_

#include "Arduino.h"
#include "AudioStream.h"

#if !defined(SAFE_RELEASE_INPUTS)
#define SAFE_RELEASE_INPUTS(...)
#endif // !defined(SAFE_RELEASE_INPUTS)

class AudioMixerX : public AudioStream
{
#if defined(__ARM_ARCH_7EM__)
public:
	AudioMixerX(unsigned char ninputs) : // for now this WILL crash if the malloc fails!
		AudioStream(ninputs, inputQueueArray = (audio_block_t **) malloc(ninputs * sizeof *inputQueueArray)),
		_ninputs(ninputs)
    {
        //Serial.printf("\nninputs = %d %d\n\n", _ninputs, sizeof(this));
        multiplier = (int32_t*)malloc(_ninputs*sizeof *multiplier);
		for (int i=0; i<_ninputs; i++) multiplier[i] = 65536;
	}
    ~AudioMixerX()
    {
		SAFE_RELEASE_INPUTS();
        //Serial.println("freeing multipler!");
        free(multiplier);
        //Serial.println("freeing inputQueueArray!");
        free(inputQueueArray);
        //Serial.println("freeing all done!");
    }
	virtual void update(void);
	void gain(unsigned int channel, float gain) {
		if (channel >= _ninputs) return;
		if (gain > 32767.0f) gain = 32767.0f;
		else if (gain < -32767.0f) gain = -32767.0f;
		multiplier[channel] = gain * 65536.0f; // TODO: proper roundoff?
	}
private:
    unsigned char _ninputs;
	int32_t* multiplier;
    //audio_block_t *toBeIgnored[1];
	audio_block_t **inputQueueArray;

#elif defined(KINETISL)
public:
	AudioMixerX(unsigned char ninputs) : // for now this WILL crash if the malloc fails!
		AudioStream(ninputs, inputQueueArray = (audio_block_t **) malloc(ninputs * sizeof *inputQueueArray)),
		_ninputs(ninputs)
    {
        //Serial.printf("\nninputs = %d %d\n\n", _ninputs, sizeof(this));
        multiplier = (int32_t*)malloc(_ninputs*sizeof *multiplier);
		for (int i=0; i<_ninputs; i++) multiplier[i] = 256;
	}
    ~AudioMixerX()
    {
		SAFE_RELEASE_INPUTS();
        free(multiplier);
        free(inputQueueArray);
    }
	virtual void update(void);
	void gain(unsigned int channel, float gain) {
		if (channel >= _ninputs) return;
		if (gain > 127.0f) gain = 127.0f;
		else if (gain < -127.0f) gain = -127.0f;
		multiplier[channel] = gain * 256.0f; // TODO: proper roundoff?
	}
private:
	int16_t *multiplier;
	audio_block_t **inputQueueArray;
#endif
};


#endif // DYNMIXER_H_