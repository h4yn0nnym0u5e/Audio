/* Hardware-SPDIF for Teensy 4
 * Copyright (c) 2019, Frank Bösing, f.boesing@gmx.de
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

#if defined(__IMXRT1062__)

#ifndef output_SPDIF3_h_
#define output_SPDIF3_h_

#include <Arduino.h>     // github.com/PaulStoffregen/cores/blob/master/teensy4/Arduino.h
#include <AudioStream.h> // github.com/PaulStoffregen/cores/blob/master/teensy4/AudioStream.h
#include <DMAChannel.h>  // github.com/PaulStoffregen/cores/blob/master/teensy4/DMAChannel.h

class AudioOutputSPDIF3 : public AudioStream
{
public:
	AudioOutputSPDIF3(void) : AudioStream(2, inputQueueArray) { begin(); }
	~AudioOutputSPDIF3();
	virtual void update(void);
	friend class AudioInputSPDIF3;
	friend class AsyncAudioInputSPDIF3;
	static void mute_PCM(const bool mute);
	static bool pll_locked(void);
protected:
	//AudioOutputSPDIF3(int dummy): AudioStream(2, inputQueueArray) {}
	static void config_spdif3(bool extSync=false);
	static audio_block_t *block_left_1st; // released in destructor
	static audio_block_t *block_right_1st; // released in destructor
	static bool update_responsibility;
	enum dmaState_t {AOI2S_Stop,AOI2S_Running,AOI2S_Paused};
	static dmaState_t dmaState;
	static DMAChannel dma;
	static void isr(void);	
private:
	static bool syncToInput; // use S/PDIF input to provide Tx clock: avoids glitches
	void begin(void);
	static uint32_t dpll_Gain() __attribute__ ((const));
	static audio_block_t *block_left_2nd; // released in destructor
	static audio_block_t *block_right_2nd; // released in destructor
	static audio_block_t block_silent; // no need to be released in destructor
	audio_block_t *inputQueueArray[2];
};


#endif // output_SPDIF3_h_
#endif // defined(__IMXRT1062__)
