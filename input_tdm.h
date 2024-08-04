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

#ifndef _input_tdm_h_
#define _input_tdm_h_

#include <Arduino.h>     // github.com/PaulStoffregen/cores/blob/master/teensy4/Arduino.h
#include <AudioStream.h> // github.com/PaulStoffregen/cores/blob/master/teensy4/AudioStream.h
#include <DMAChannel.h>  // github.com/PaulStoffregen/cores/blob/master/teensy4/DMAChannel.h

#include <output_tdm.h>

class AudioInputTDMbase : public AudioStream, AudioHardwareTDM
{
public:
	AudioInputTDMbase(int p) : AudioStream(0, NULL), pin(p)  { begin(); }
	//virtual void update(void);
	void begin(int pin = 1);
protected:
	int pin;
	static int pin_mask;
	static bool update_responsibility;
	static DMAChannel dma;
	static void isr(void);
	static const int MAX_TDM_INPUTS = 64;
	static audio_block_t *block_incoming[MAX_TDM_INPUTS];
private:
	static volatile enum TDMstate_e {INACTIVE, ACTIVE, STOPPING, STOPPED} state;  // state of TDM hardware
#if defined(KINETISK)
	static uint32_t tdm_rx_buffer[AUDIO_BLOCK_SAMPLES*16];
#elif defined(__IMXRT1062__)	
	static uint32_t* tdm_rx_malloc; // actual allocation
	static uint32_t* tdm_rx_buffer;	// allocation rounded to 32-byte boundary
	static uint32_t  tdm_rxbuf_len; // space available in TX buffer
#endif // hardware-dependent
};

class AudioInputTDM16 : public AudioInputTDMbase
{
public:	
	AudioInputTDM16(int pin) : AudioInputTDMbase(pin) {}
	virtual void update(void);
};

class AudioInputTDM : public AudioInputTDM16
{ public: AudioInputTDM()  : AudioInputTDM16(1) {} };

class AudioInputTDMB : public AudioInputTDM16
{ public: AudioInputTDMB()  : AudioInputTDM16(2) {} };

class AudioInputTDMC : public AudioInputTDM16
{ public: AudioInputTDMC()  : AudioInputTDM16(3) {} };

class AudioInputTDMD : public AudioInputTDM16
{ public: AudioInputTDMD()  : AudioInputTDM16(4) {} };

#endif
