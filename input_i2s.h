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

#ifndef _input_i2s_h_
#define _input_i2s_h_

#include <Arduino.h>
#include <AudioStream.h>
#include <DMAChannel.h>

#if !defined(I2S_RCSR_SR) // not always in the master header
#define I2S_RCSR_SR			((uint32_t)0x01000000)		// Software Reset
#endif // !defined(I2S_RCSR_SR)

class AudioInputI2S : public AudioStream
{
public:
	AudioInputI2S(void) : AudioStream(0, NULL) { begin(); }
	~AudioInputI2S();
	virtual void update(void);
protected:	
	AudioInputI2S(int dummy): AudioStream(0, NULL) {} // to be used only inside AudioInputI2Sslave !!
	static bool update_responsibility;

#if !defined(KINETISL)
	enum dmaState_t {AOI2S_Stop,AOI2S_Running,AOI2S_Paused};
	static dmaState_t dmaState;
	static DMAChannel dma;
	static void isr(void);
#else
	enum dmaState_t {AOI2S_Stop,AOI2S_Running,AOI2S_Paused};
	static dmaState_t dmaState;
	static DMAChannel dma1, dma2;
	static void isr1(void);
	static void isr2(void);
#endif

private:
	void begin(void);
	static audio_block_t *block_left;   // released in destructor
	static audio_block_t *block_right;  // released in destructor
#if !defined(KINETISL)	
	static uint16_t block_offset;
#endif	
};


class AudioInputI2Sslave : public AudioInputI2S
{
	void begin(void);	
public:
	AudioInputI2Sslave(void) : AudioInputI2S(0) { begin(); }
};

#endif