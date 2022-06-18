/* Audio Library for Teensy 3.X
 * Copyright (c) 2017, Paul Stoffregen, paul@pjrc.com
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

#if defined(__IMXRT1052__) || defined(__IMXRT1062__)
#ifndef output_tdm2_h_
#define output_tdm2_h_

#include "Arduino.h"
#include "AudioStream.h"
#include "DMAChannel.h"

#define AUDIO_TDM_BLOCKS 16

class AudioOutputTDM2 : public AudioStream
{
public:
	AudioOutputTDM2(void) : AudioStream(AUDIO_TDM_BLOCKS, inputQueueArray) { begin(); }
	~AudioOutputTDM2();
	virtual void update(void);
	friend class AudioInputTDM2;
protected:
	static void config_tdm(void);
	static audio_block_t *block_input[AUDIO_TDM_BLOCKS]; // released in destructor
	static bool update_responsibility;
	enum dmaState_t {AOI2S_Stop,AOI2S_Running,AOI2S_Paused};
	static dmaState_t dmaState;
	static DMAChannel dma;
	static void isr(void);
private:
	void begin(void);
	audio_block_t *inputQueueArray[AUDIO_TDM_BLOCKS];
};


#endif
#endif