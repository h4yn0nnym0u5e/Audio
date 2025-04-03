/* Audio Library for Teensy 3.X
 * Copyright (c) 2016, Paul Stoffregen, paul@pjrc.com
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

#ifndef synth_karplusstrong_h_
#define synth_karplusstrong_h_
#include <Arduino.h>     // github.com/PaulStoffregen/cores/blob/master/teensy4/Arduino.h
#include <AudioStream.h> // github.com/PaulStoffregen/cores/blob/master/teensy4/AudioStream.h
#include "utility/dspinst.h"


class AudioSynthKarplusStrong : public AudioStream
{
	enum state_e {silent, started, playing};
	void setLevel(float level,int16_t* levelPtr);
public:
	AudioSynthKarplusStrong() 
		: AudioStream(1, inputQueueArray),
		  state(silent), 
		  _feedbackLevel(32686),
		  _driveLevel(0)
		{}

	void noteOn(float frequency, float velocity);
	void noteOff(float velocity); 
	void setFeedbackLevel(float level) { setLevel(level,&_feedbackLevel); }
	void setDriveLevel(float level) { setLevel(level,&_driveLevel); }	
	
	virtual void update(void);
	static constexpr float lowestFreq = 15.7f; // gets down to C0 / MIDI 12
	
private:
	uint8_t state;     		// 0=steady output, 1=begin on next update, 2=playing
	int32_t bufferLen;		// total length of buffered audio (samples)
	int32_t bufferIndex;	// index into current buffer
	
	int32_t  magnitude; // current output level
	class IndexableBuffer
	{
			static uint32_t seed;  // must start at 1
		public:
			IndexableBuffer() : buffers{0}, bufferCount(0) {}
			bool allocate(uint16_t count);
			void release(void);
			void prefill(int samples, int32_t magnitude);
			int32_t limitToBuffer(int32_t index)
			{
				if (index < 0 || index >= sampleCount)
				{
					index = index % sampleCount;
					if (index < 0)
						index += sampleCount;
				}
				return index;
			}

			int16_t& operator[](int32_t index)
			{
				index = limitToBuffer(index);
				int blockNum = index / AUDIO_BLOCK_SAMPLES;
				return buffers[blockNum]->data[index-AUDIO_BLOCK_SAMPLES*blockNum];
			}

			static constexpr int maxBufferCount = (int) (AUDIO_SAMPLE_RATE_EXACT / lowestFreq / AUDIO_BLOCK_SAMPLES) + 1;
			audio_block_t* buffers[maxBufferCount]; // dynamically use audio memory blocks: maximum 22 for C0	
			uint16_t bufferCount;	// number of audio blocks currently allocated for buffering
			int32_t sampleCount;
	} theBuffer;
	int16_t _feedbackLevel;
	int16_t _driveLevel;
	audio_block_t* inputQueueArray[1];
};

#endif
