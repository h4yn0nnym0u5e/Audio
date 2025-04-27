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
	enum state_e {releasing=-3, silent=0, started, playing};
	void setLevel(float level,int16_t* levelPtr);
	void computeBendData(uint32_t* phasedata, int16_t* bp);
public:
	AudioSynthKarplusStrong() 
		: AudioStream(2, inputQueueArray), // drive and bend inputs
		  state(silent), 
		  _feedbackLevel(32686),
		  _driveLevel(0)
		{
			frequencyModulation(2.0f/12); // bend by 2 semitones
		}

	void noteOn(float frequency, float velocity);
	void noteOff(float velocity); 
	bool isPlaying(void) { return state != silent; }
	bool isStarted(void) { return state == playing; } // stimulus has been generated
	void setFeedbackLevel(float level) { setLevel(level,&_feedbackLevel); }
	void setDriveLevel(float level) { setLevel(level,&_driveLevel); }
	void frequencyModulation(float octaves)	// must do before noteOn()
	{
		if (octaves <= 0.1f) octaves = 0.1f;
		if (octaves  > 2.0f) octaves = 2.0f;
		maxBend = powf(2.0f, -octaves); // express as frequency factor
		modulation_factor = octaves * 4096.0f; // match modulated waveform calculation
	}
	
	virtual void update(void);

	// Limit lowest frequency to avoid eating all the audio blocks
	// At a sample rate of 44100Hz, 15.7Hz is about 2809 samples, or 22 blocks
	// These are allocated at noteOn(), so this should be OK. The default
	// AudioStream code limits a Teensy 4.x to 896 blocks.
	static constexpr float lowestFreq = 15.7f; // gets down to C0 / MIDI 12
	static constexpr int fracShift = 8; // use 24.8 indexes into buffer
	static constexpr int increment = 1<<fracShift;
	
private:
	int8_t state;     		// 0=silent, 1=begin on next update, 2=playing, -ve note releasing
	int32_t baseLen;		// 24.8 length in samples of base frequency's cycle
	int32_t bufferIndex;	// 24.8 index into current buffer: must have no fractional bits!
	
	int32_t  magnitude; // current output level
	class IndexableBuffer
	{
			static uint32_t seed;  // must start at 1
		public:
			IndexableBuffer() : readVal {0}, buffers{0}, bufferCount(0) {}
			bool allocate(uint16_t count);
			void release(size_t start = 0);
			void prefill(int samples, int32_t magnitude);

			// limit index to being within buffer
			// index is integer portion only
			int32_t limitToBuffer(int32_t index)
			{
				/*
				if (index < 0 || index >= sampleCount)
				{
					index = index % sampleCount;
					if (index < 0)
						index += sampleCount;
				}
				/*/
				// expect moderately sane index, so avoid
				// division by using multiple addition / subtractions
				while (index < 0) index += sampleCount;
				while (index >= sampleCount) index -= sampleCount;
				//*/

				return index;
			}

			// limit to buffer, fractional index
			// rounds to nearest sample
			int32_t limitToBufferFrac(int32_t index)
			{
				return limitToBuffer(index >> fracShift) << fracShift;
			}

			int16_t readVal;
			int16_t& operator[](int32_t index)
			{
				// buffers are invalid, return reference to a zero value
				if (0 == bufferCount) return readVal;

				int32_t frac = (uint32_t) index & (increment-1);
				index = limitToBuffer(index >> fracShift);
				// this division looks expensive, but replacing it 
				// with a shift is actually slower!
				int blockNum = index / AUDIO_BLOCK_SAMPLES;
				if (0 == frac) // return reference to read/write value in buffers
					return buffers[blockNum]->data[index-AUDIO_BLOCK_SAMPLES*blockNum];
				else
				{
					// likely this can be optimised using DSP instructions
					int32_t val1 = buffers[blockNum]->data[index-AUDIO_BLOCK_SAMPLES*blockNum];
					val1 *= (increment - frac);
					index = limitToBuffer(index+1);
					blockNum = index / AUDIO_BLOCK_SAMPLES;
					int32_t val2 = buffers[blockNum]->data[index-AUDIO_BLOCK_SAMPLES*blockNum];
					val2 *= frac;
					val1 += val2;
					readVal = val1 >> fracShift;
				}
				return readVal; // writing this will have no effect!
			}

			static constexpr int maxBufferCount = (int) (AUDIO_SAMPLE_RATE_EXACT / lowestFreq / AUDIO_BLOCK_SAMPLES) + 1;
			audio_block_t* buffers[maxBufferCount]; // dynamically use audio memory blocks: maximum 22 for C0	
			uint16_t bufferCount;	// number of audio blocks currently allocated for buffering
			int32_t sampleCount;	// number of samples in those blocks
	} theBuffer;
	int16_t _feedbackLevel;
	int16_t _driveLevel;
	float maxBend;
	uint32_t modulation_factor;
	audio_block_t* inputQueueArray[2];
};

#endif
