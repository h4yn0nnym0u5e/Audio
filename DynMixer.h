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

// Avoid having to maintain near-identical blocks of code
// by using some sensible macro substitutions
#if defined(__ARM_ARCH_7EM__)
#define MULTI_UNITYGAIN 65536
#define MULTI_TYPE int32_t
#define MAX_GAIN ((float) MULTI_UNITYGAIN / 2.0f - 1.0f)
#elif defined(KINETISL)
#define MULTI_UNITYGAIN 256
#define MULTI_TYPE int16_t
#endif

#define MAX_GAIN ((float) MULTI_UNITYGAIN / 2.0f - 1.0f)
#define MAX_PAN 1.0f
#define EPSILON 0.00001f // very small: prevents division by zero

class AudioMixer : public AudioStream
{
public:
	AudioMixer(unsigned char ninputs) : 
		AudioStream(ninputs, inputQueueArray = (audio_block_t **) malloc(ninputs * sizeof *inputQueueArray)),
		_ninputs(ninputs)
    {
        multiplier = (int32_t*)malloc(_ninputs*sizeof *multiplier);
		if (NULL != multiplier)
			for (int i=0; i<_ninputs; i++) multiplier[i] = MULTI_UNITYGAIN;
	}
	
    ~AudioMixer()
    {
		SAFE_RELEASE_INPUTS();
        free(multiplier);
        free(inputQueueArray);
    }
	
	virtual void update(void);
	
	void gain(unsigned int channel, float gain) {
		if (channel >= _ninputs || NULL == multiplier) return;
		if (gain > MAX_GAIN) gain = MAX_GAIN;
		else if (gain < -MAX_GAIN) gain = -MAX_GAIN;
		multiplier[channel] = gain * (float) MULTI_UNITYGAIN; // TODO: proper roundoff?
	}
	
	void gain(float gain) {
		if (NULL == multiplier) return;
		if (gain > MAX_GAIN) gain = MAX_GAIN;
		else if (gain < -MAX_GAIN) gain = -MAX_GAIN;
		MULTI_TYPE gainI = gain * (float) MULTI_UNITYGAIN; // TODO: proper roundoff?
		for (int i=0; i<_ninputs; i++) multiplier[i] = gainI;		
	}
	
	uint8_t getChannels(void) {return num_inputs;}; // actual number, not requested
private:
    unsigned char _ninputs;
	MULTI_TYPE* multiplier;
	audio_block_t **inputQueueArray;
};


#define MULTI_CENTRED ((MULTI_TYPE) (MULTI_UNITYGAIN*sqrt(0.5f)))
class AudioMixerStereo : public AudioStream
{
public:
	AudioMixerStereo(unsigned char ninputs) : 
		AudioStream(ninputs, inputQueueArray = (audio_block_t **) malloc(ninputs * sizeof *inputQueueArray)),
		_ninputs(ninputs)
    {
        multiplier = (mulRec*) malloc(_ninputs*sizeof *multiplier);
		if (NULL != multiplier)
		{
			for (int i=0; i<_ninputs; i++) 
				multiplier[i] = {1.0f, 0.0f, -1, false, MULTI_CENTRED,MULTI_CENTRED};
			setPanLaw(0.22f);
		}
	}
	
    ~AudioMixerStereo()
    {
		SAFE_RELEASE_INPUTS();
        free(multiplier);
        free(inputQueueArray);
    }
	
	virtual void update(void);
	
	
	/**
	 * Set gain on a single channel.
	 */
	void gain(unsigned int channel, float gain) 
	{
		if (channel >= _ninputs || NULL == multiplier) return;
		if (gain > MAX_GAIN) gain = MAX_GAIN;
		else if (gain < -MAX_GAIN) gain = -MAX_GAIN;
		
		setGainPan(channel,gain,multiplier[channel].pan);
	}
	
	
	/**
	 * Pan a single mono channel to a position in the stereo field.
	 */
	void pan(unsigned int channel, float pan) 
	{
		if (channel >= _ninputs || NULL == multiplier) return;
		if (pan > MAX_PAN) pan = MAX_PAN;
		else if (pan < -MAX_PAN) pan = -MAX_PAN;
		
		if (multiplier[channel].balanceChannel >= 0) // was used for balance...
		{
			short otherChannel = multiplier[channel].balanceChannel;
			
			multiplier[channel].balanceChannel = -1;	  // ...revert to pan mode...
			multiplier[otherChannel].balanceChannel = -1; // ...on both channels
		}
		
		setGainPan(channel,multiplier[channel].gain,pan);
	}
	
	
	/**
	 * Balance a pair of channels in the stereo field.
	 */
	void balance(unsigned int chLeft, unsigned int chRight, float bal) 
	{
		if (chLeft >= _ninputs || chRight >= _ninputs || NULL == multiplier) return;
		if (bal > MAX_PAN) bal = MAX_PAN;
		else if (bal < -MAX_PAN) bal = -MAX_PAN;
		
		if (multiplier[chLeft].balanceChannel < 0  // haven't set up this channel yet
 		 && multiplier[chRight].balanceChannel < 0) // or this one
		{
			multiplier[chLeft].balanceChannel = chRight; // tell them they're a pair
			multiplier[chRight].balanceChannel = chLeft;	
			multiplier[chLeft].isLeft = true;			 // and which is which
			multiplier[chRight].isLeft = false;
		}
			
		setGainPan(chLeft,multiplier[chLeft].gain,bal);
	}
	
	/**
	 * Balance a pair of channels in the stereo field.
	 * Given only one channel, so use that as left and the next one up as right.
	 */
	void balance(unsigned int chLeft, float bal) 
	{
		balance(chLeft,chLeft+1,bal);
	}
			
	
	/**
	 * Set gain on all channels at once.
	 */
	void gain(float gain) 
	{
		if (NULL == multiplier) return;
		if (gain > MAX_GAIN) gain = MAX_GAIN;
		else if (gain < -MAX_GAIN) gain = -MAX_GAIN;
		
		for (int i=0; i<_ninputs; i++)
		{	
			if (multiplier[i].balanceChannel < 0 || multiplier[i].isLeft)
				setGainPan(i,gain,multiplier[i].pan);
		}
	}
	
	
	/**
	 * Set the "pan law".
	 * Values from 0.707 down to 0.01 are probably about right, depending.
	 * Simulates an analogue circuit's behaviour. Sort of.
	 */
	void setPanLaw(float law)
	{
		panLaw = law;//sqrt(0.5f);
		normalise = 1.0f/(1.0f / panLaw + 1.0f);
		normalise = (normalise + 1.0f)/normalise;
		
		for (int i=0; i<num_inputs; i++)
		{	
			if (multiplier[i].balanceChannel < 0 || multiplier[i].isLeft)
				setGainPan(i,multiplier[i].gain,multiplier[i].pan);
		}
	}
	
	uint8_t getChannels(void) {return num_inputs;}; // actual number, not requested
private:
	float panLaw;
	float normalise;
	void setGainPan(unsigned int channel,float gain,float pan);
    unsigned char _ninputs;
	struct mulRec {float gain,pan; short balanceChannel; bool isLeft; MULTI_TYPE mL,mR; } *multiplier;
	audio_block_t **inputQueueArray;
};


#endif // DYNMIXER_H_