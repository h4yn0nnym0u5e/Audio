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

#ifndef output_tdm_h_
#define output_tdm_h_

#include <Arduino.h>     // github.com/PaulStoffregen/cores/blob/master/teensy4/Arduino.h
#include <AudioStream.h> // github.com/PaulStoffregen/cores/blob/master/teensy4/AudioStream.h
#include <DMAChannel.h>  // github.com/PaulStoffregen/cores/blob/master/teensy4/DMAChannel.h

// not defined in imxrt.h

#define I2S_TCSR_SR (1UL<<24) 

#define I2S_TCR3_CFR (1UL<<24) 
#define I2S_RCR3_CFR (1UL<<24) 
#define I2S_RCR4_FCOMB(n)               ((uint32_t)(n & 0x3)<<26)	// FIFO Combine Mode
#define I2S_RCR4_FCOMB_DISABLED          I2S_RCR4_FCOMB(0) // No FIFO combining
#define I2S_RCR4_FCOMB_ENABLED_ON_WRITES I2S_RCR4_FCOMB(1) // <-- this is the one you want.
#define I2S_RCR4_FCOMB_ENABLED_ON_READS  I2S_RCR4_FCOMB(2) 
#define I2S_RCR4_FCOMB_ENABLED_ON_RW     I2S_RCR4_FCOMB(3)
#define I2S_TCR4_FCOMB(n)               ((uint32_t)(n & 0x3)<<26)	// FIFO Combine Mode
#define I2S_TCR4_FCOMB_DISABLED          I2S_TCR4_FCOMB(0)
#define I2S_TCR4_FCOMB_ENABLED_ON_READS  I2S_TCR4_FCOMB(1) // <--- this is the one you want
#define I2S_TCR4_FCOMB_ENABLED_ON_WRITES I2S_TCR4_FCOMB(2)
#define I2S_TCR4_FCOMB_ENABLED_ON_RW     I2S_TCR4_FCOMB(3)
class AudioOutputTDMbase : public AudioStream
{
public:
	AudioOutputTDMbase(int nch, audio_block_t** queues, int p) 
		: AudioStream(nch, queues), channels(nch), pin(p) 
		{ begin(pin); }
	//virtual void update(void);
	void begin(int pin);
	friend class AudioInputTDM;
	static bool update_responsibility;
protected:
	int channels;
	int pin;
	static int pin_mask;
	static void config_tdm(int pin = 1);
	static const int MAX_TDM_INPUTS = 64;
	static audio_block_t *block_input[MAX_TDM_INPUTS];
	static DMAChannel dma;
	static void isr(void);
private:
	static volatile enum TDMstate_e {INACTIVE, ACTIVE, STOPPING, STOPPED} state;  // state of TDM hardware
#if defined(KINETISK)
	static uint32_t tdm_tx_buffer[AUDIO_BLOCK_SAMPLES*16];
#elif defined(__IMXRT1062__)	
	static uint32_t* tdm_tx_malloc; // actual allocation
	static uint32_t* tdm_tx_buffer;	// allocation rounded to 32-byte boundary
	static uint32_t  tdm_txbuf_len; // space available in TX buffer
#endif // hardware-dependent
};

/*
 * This is the original TDM object, which packs 16x 16-bit
 * channels into each frame, as used by e.g. ADAU1966A.
 * The Design Tool object correspondingly has 16 inputs.
 */
class AudioOutputTDM16 : public AudioOutputTDMbase
{
public:
	AudioOutputTDM16(int pin=1) : AudioOutputTDMbase(16, inputQueueArray, pin) {}
	virtual void update(void);
private:
	audio_block_t *inputQueueArray[16];
};

class AudioOutputTDM : public AudioOutputTDM16
{ public: AudioOutputTDM()  : AudioOutputTDM16(1) {} };

#if defined(__IMXRT1062__)
class AudioOutputTDMB : public AudioOutputTDM16
{ public: AudioOutputTDMB() : AudioOutputTDM16(2) {} };

class AudioOutputTDMC : public AudioOutputTDM16
{ public: AudioOutputTDMC() : AudioOutputTDM16(3) {} };

class AudioOutputTDMD : public AudioOutputTDM16
{ public: AudioOutputTDMD() : AudioOutputTDM16(4) {} };
#endif // defined(__IMXRT1062__)



/*
 * For the generalised TDM object, we may use up to 4 OUT1x pins, each of which
 * could have 16x 16-bit packed channels, or for most parts 8x 32-bit channels.
 * BCLK is 256fs in this case. We could thus have up to 64x output channels.
 *
 * There is also the case for e.g. the PCM3168 where a 96kHz sample rate
 * requires two OUT pins running at BCLK = 128fs to output 8x 32-bit channels.
 *
 * These two cases are both worth supporting, but are mutually exclusive.
 *
 * It appears we can do this with 6 classes of TDM output object:
 * * 4 classes using 1 pin, packing 16x 16-bit, or 8x 32-bit if odd ports are unused
 * * 2 classes using 2 pins, providing 8 ports sent as 4 channels to each pin
 *
 * The first of these is equivalent to the "legacy" TDM object using OUT1A
 *
 * In this table, a channel is a 16-bit slot in the TDM data stream. It shows
 * the location in the transmit buffer where the port data is placed. 0-F are
 * the data for the TDMA object's ports 0-15; b is the placement of the TDMB 
 * data, so for example 5b6 shows TDMA channel 5 data, the TDMB ch5, then 
 * TDMA ch6.
 * Channel: 0...4...8...12..16..20..24..28..32..36..40..44..48..52..56..60..
 * TDMA     0123456789ABCDEF................................................
 * TDMA&B   0b1b2b3b4b5b6b7b8b9bAbBbCbDbEbFb................................
 * TDMA&B&C 0bc1bc2bc3bc4bc5bc6bc7bc8bc9bcAbcBbcCbcDbcEbcFbc................
 * TDMA&&&D 0bcd1bcd2bcd3bcd4bcd5bcd6bcd7bcd8bcd9bcdAbcdBbcdCbcdDbcdEbcdFbcd
 *
 * PCM3168 dual rate mode using 2 pins for 8 channels @ 96kHz, BCLK = 128fs
 * Channel: 0...4...8...12..16..20..24..28..
 * TDMAB    0.4.1.5.2.6.3.7.................
 * TDMCD    ................0.4.1.5.2.6.3.7.
 * 
 * The existing TDM transmit buffer is big enough for two blocks of audio data
 * for each of the 16 input ports, thus 2 * 128 * 16 * 2 = 8192 bytes. For the
 * full 64 channels that would grow to 32kB, which is a bit profligate for an
 * unusual use-case. We will try to allocate / reallocate the transmit buffer
 * at instantiation time, bearing in mind the need to be on a 32-byte boundary
 * for DMA to work, and that the buffer may already be in use.
 *
 * As Teensy 3.x are no longer manufactured, we don't support multi-TDM for
 * those boards at the moment.
 */


#endif
