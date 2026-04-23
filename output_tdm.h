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
#include <utility/imxrt_hw.h>

class AudioOutputTDM_Base : public AudioStream, private SAIconfig
{
		static const uint32_t txBufSz = sizeof(((audio_block_t*)0)->data) * 2 * 16;
	public:
		AudioOutputTDM_Base(IMXRT_SAI_t& _sai) 
			: AudioStream(16, inputQueueArray), 
			SAIconfig(_sai, SAIconfig::SAIcfg::TDM) 
			{ begin(); }
		virtual void update(void);
		void begin(void);

	protected:
		static void config_tdm(void);
		audio_block_t *block_input[16];
		bool update_responsibility;
		void isr(void);
		static void DMAisr(void* instance);

	private:
		audio_block_t *inputQueueArray[16];
};

class AudioOutputTDM : public AudioOutputTDM_Base
{
	public:
		AudioOutputTDM(void) : AudioOutputTDM_Base(IMXRT_SAI1) {}
};

#if defined(__IMXRT1062__)
class AudioOutputTDM2 : public AudioOutputTDM_Base
{
	public:
		AudioOutputTDM2(void) : AudioOutputTDM_Base(IMXRT_SAI2) {}
};
#endif // defined(__IMXRT1062__)


#endif
