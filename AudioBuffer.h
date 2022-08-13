/* Audio Library for Teensy 3.x, 4.x
 * Copyright (c) 2022, Jonathan Oakley, teensy-jro@0akley.co.uk
 *
 * Development of this audio library was enabled by PJRC.COM, LLC by sales of
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
 
 #if !defined(_AUDIO_BUFFER_H_)
 #define _AUDIO_BUFFER_H_
 
 #include <Arduino.h>
 #include <FS.h>
 
 /**
  * Implementation of a double-buffer aimed at audio processing.
  * The aim is to have a minimal overhead while providing simple mechanisms
  * for signalling that (part of) the buffer needs re-filling, a source-agnostic
  * way of re-filling, content-agnostic readout, and built-in memory management.
  */
class AudioBuffer
{
  public:
	AudioBuffer() : buffer(0), nextIdx(0), bufSize(0), bufTypeX(none), bufState(empty) {}
	~AudioBuffer() {disposeBuffer();}
	enum result  {ok,halfEmpty,underflow,invalid};
	enum bufType {none,given,inHeap,inExt};
	enum bufState_e {empty,firstValid,secondValid,bothValid}; // state of buffer
	
  //private:
	uint8_t* buffer;	// memory used for buffering
	size_t nextIdx;		// index of next unused sample
	size_t bufSize;		// size of buffer
	size_t firstSize;	// amount of data in first part of buffer...
	size_t secondSize;	// ...and in second
	bufType bufTypeX;
	bufState_e bufState;
	
  public:
	result createBuffer(size_t sz, bufType typ);  // create buffer for audio data, managed by class
	result createBuffer(uint8_t* buf, size_t sz);  // create buffer for audio data, managed by application
	result disposeBuffer(); // dispose of buffer: if it's of type "given", the application may free it after this call
	result read(uint8_t* dest, size_t bytes); // read buffer data into destination
	void getNextBuffer(uint8_t** pbuf, size_t* psz); // find out where data needs to be written to
	void bufferWritten(uint8_t* buf, size_t sz); // tell object data has been written into the buffer
	size_t getAvailable(); // find out how much valid data is available
	void emptyBuffer() {nextIdx = 0; bufState = empty;} // initialise the buffer to its empty state
};
 
 
class AudioWAVdata
{
	union tag_t
	{
	  char c[4];
	  uint32_t u;
	};

	struct RIFFhdr_t
	{
	  tag_t riff;
	  uint32_t flen;
	  tag_t wav;
	};

	struct chunk_t
	{
	  tag_t id;
	  uint32_t clen;
	};

	struct data_t
	{
	  chunk_t hdr;
	  uint16_t fmt; // 16, 18 or 40
	  uint16_t chanCnt;
	  uint32_t sampleRate;
	  uint32_t byterate;
	  uint16_t blockAlign;
	  uint16_t bitsPerSample;

	  uint16_t extSize;
	  uint16_t validBitsPerSample;
	  uint32_t channelMask;

	  union {
		char subFormat[16];
		uint16_t subFmt;
	  };
	};

  public:
	uint16_t format;  		// file format
	uint16_t bitsPerSample; // bits per sample
	uint16_t chanCnt; 		// number of channels
	uint16_t dataChunks;	// number of data chunks
	uint32_t samples; 		// number of samples
	uint32_t nextAudio;		// offset of next audio data
	uint32_t audioSize;		// number of bytes of audio

	AudioWAVdata() : format(0), bitsPerSample(0), chanCnt(0), 
					 dataChunks(0), samples(0), nextAudio(0)
					 {}
	uint16_t parseWAVheader(File& f); // parse WAV file	
};
 
 #endif // !defined(_AUDIO_BUFFER_H_)
 