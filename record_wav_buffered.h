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

#if !defined(record_wav_buffered_h_)
#define record_wav_buffered_h_

#include "Arduino.h"
#include "AudioStream.h"
#include "SD.h"
#include "EventResponder.h"
#include "AudioBuffer.h"

//*
#define SCOPE_PIN 25
#define SCOPE_SERIAL Serial1
#define SCOPESER_SPEED 57600
//*/
#include "oscope.h"

class AudioRecordWAVbuffered : public EventResponder, public AudioBuffer, public AudioWAVdata, public AudioStream
{
public:
	AudioRecordWAVbuffered(void);
	bool recordSD(const char* filename, bool paused = false);
	bool record(const File _file, bool paused = false);
	bool record(void)  { if (isPaused())  toggleRecordPause(); return isRecording(); }
	bool pause(void) { if (isRecording()) toggleRecordPause(); return isPaused();  }
	bool cueSD(const char* filename) { return recordSD(filename,true); }
	bool cue(const File _file) { return record(_file,true); }
	void toggleRecordPause(void);
	void stop(void);
	bool isRecording(void);
	bool isPaused(void);
	bool isStopped(void);	
	uint32_t positionMillis(void);
	uint32_t lengthMillis(void);
	virtual void update(void);
	
	static uint8_t objcnt;
	// debug members
	size_t lowWater;
private:
	audio_block_t *inputQueueArray[2];
	enum state_e {STATE_STOP,STATE_PAUSED,STATE_RECORDING};
	File wavfile;
	wavhdr_t header; 	// WAV header
	static void EventResponse(EventResponderRef evref);
	void flushBuffer(uint8_t* pb, size_t sz);
	
	bool eof;
	bool writePending;
	uint8_t objnum;
	uint32_t data_length;		// number of bytes remaining in current section
	uint32_t total_length;		// number of audio data bytes in file
	
	uint8_t state;
	uint8_t state_record;
	uint8_t leftover_bytes;
};

#endif // !defined(record_wav_buffered_h_)
