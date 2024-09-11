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

#if defined(__IMXRT1052__) || defined(__IMXRT1062__)
#ifndef _input_i2s2_h_
#define _input_i2s2_h_

#include <Arduino.h>     // github.com/PaulStoffregen/cores/blob/master/teensy4/Arduino.h
#include <AudioStream.h> // github.com/PaulStoffregen/cores/blob/master/teensy4/AudioStream.h
#include <DMAChannel.h>  // github.com/PaulStoffregen/cores/blob/master/teensy4/DMAChannel.h

#if !defined(I2S_RCSR_SR) // not always in the master header
#define I2S_RCSR_SR			((uint32_t)0x01000000)		// Software Reset
#endif // !defined(I2S_RCSR_SR)


class AudioInputI2S2 : public AudioStream
{
public:
	AudioInputI2S2(void) : AudioStream(0, NULL) { begin(); }
	~AudioInputI2S2();
	virtual void update(void);
protected:
	AudioInputI2S2(int dummy): AudioStream(0, NULL) {} // to be used only inside AudioInputI2Sslave !!
	static bool update_responsibility;
	enum dmaState_t {AOI2S_Stop,AOI2S_Running,AOI2S_Paused};
	static dmaState_t dmaState;
	static DMAChannel dma;
	static void isr(void);
private:
	void begin(void);
	static audio_block_t *block_left;  // released in destructor
	static audio_block_t *block_right; // released in destructor
	static uint16_t block_offset;
};


class AudioInputI2S2slave : public AudioInputI2S2
{
	void begin(void);
public:
	AudioInputI2S2slave(void) : AudioInputI2S2(0) { begin(); }
	friend void dma_ch1_isr(void);
};

#endif
#endif //#if defined(__IMXRT1052__) || defined(__IMXRT1062__)
