/* Audio Library for Teensy 4.x
 * Copyright (c) 2024, Jonathan Oakley
 *
 * Development of this audio library was enabled by PJRC.COM, LLC by sales of
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

#if defined(__IMXRT1052__) || defined(__IMXRT1062__)
#if !defined(effect_protect_stall_h_)
#define effect_protect_stall_h_

#include <Arduino.h>     // github.com/PaulStoffregen/cores/blob/master/teensy4/Arduino.h
#include <AudioStream.h> // github.com/PaulStoffregen/cores/blob/master/teensy4/AudioStream.h
#include <laser_synth.h>

class AudioEffectProtectStall : public AudioStream
{
	static const int CHANNELS = 7;
	int16_t _floatToSample(float f) 
	{ 
		int i = (int16_t) (f*32768.0f); 
		if (i >  32767) i = 32767;
		if (i < -32768) i = -32768;
		
		return (int16_t) i;
	}
	
public:
	AudioEffectProtectStall(void) : 
		AudioStream(CHANNELS, inputQueueArray),
		permanent_blocks{0},
		stallThreshold(2), stallTimeout(50),
		RGBsafeValue(BLACK_LEVEL),
		blankSafeValue(CONVERT(255)),
		updateCount(0), updatesOK(true)
		{ }
	void update(void);
	unsigned int getUpdateCount(void) { return updateCount; }
	bool isUpdating(void) { bool result = updatesOK; updatesOK = true; return result;}
	void setRGBsafevalue(float v)    { RGBsafeValue   = _floatToSample(v); fillPermanentBlocks(); }
	void setBlankSafeValue(float v)  { blankSafeValue = _floatToSample(v); fillPermanentBlocks(); }
	void setStallThreshold(int v) 	 { stallThreshold = v; }
	void setStallTimeout(uint32_t v) { stallTimeout   = v; }
//private:
	audio_block_t* permanent_blocks[CHANNELS];
	
	int 	 stallThreshold;	// stall threshold to use
	uint32_t stallTimeout;		// milliseconds before protection starts

	int 	 RGBsafeValue;		// set safe value for RGB channels
	int 	 blankSafeValue;	// set safe value for blanking channel
	
	unsigned int updateCount;	// count of updates
	bool 		 updatesOK;		// have updates ever failed
	uint32_t 	 lastMoved;		// last time we saw the galvos move
	uint32_t 	 lastUpdate;	// last time update() was called
	
	void grabPermanentBlocks(void); // allocate ourselves some permanent blocks
	void fillPermanentBlocks(void); // fill permanent blocks with safe values
	bool stallCheck(audio_block_t** blocks); // check the input data for stalled galvos
	audio_block_t* inputQueueArray[CHANNELS];
};


#endif // !defined(effect_protect_stall_h_)
#endif // defined(__IMXRT1052__) || defined(__IMXRT1062__)
