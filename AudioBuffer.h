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
  *
  * For playing from media, we need to:
  * * load large blocks from media to sequential memory in the buffer (single read() call)
  * * copy smaller data amounts from the buffer to temporary storage for de-interleaving
  * * flag when there is space to do a load from media
  *
  * For recording to media, we need to:
  * * save large blocks to media from sequential memory in the buffer (single write() call)
  * * copy smaller data amounts from temporary storage to the buffer
  * * flag when there is enough data to do a write to media
  *
  * This is very similar to a simple queue, except that we wish to deal in larger objects than
  * are usually used in queues, and the sizes used on input and output will typically be 
  * very different. For efficiency, we also want to be able to access the buffer directly
  * during medium read/write operations; for application use we need to use temporary storage,
  * because the different sizes will mean we're occasionally wrapping from buffer end back to
  * start, and we need a temporary buffer to (de)interleave audio blocks in any case.
  *
  * For example, a 16-bit stereo WAV file will require reading 512 bytes on every audio update;
  * an 8-channel file will take 2048 bytes.
  *
  * Typical playback buffer:
  * .....uuv|vvvvvvvv: . = unknown, u = used, v = valid
  * ^      ^  
  * |	   queueOut
  * queueIn
  *
  * .....uuu|uuvvvvvv: after next read()
  * nnnnnnnn|uuvvvvvv: after next readExecuted(): n = newly-read data
  * 		 ^ ^  
  * 		 | queueOut
  *    queueIn
  *  
  * Typical record buffer:
  * .....vvv|vvvvvv..: . = unknown, v = valid
  * 	 ^         ^  
  * 	 |         queueIn
  * 	 queueOut
  *
  * v....vvv|vvvvvvvv: after next write()
  * v....www|wwwwwwww: after next writeExecuted():  w = written
  * ^^  
  * |queueIn
  * queueOut
  *
  */
class AudioBuffer
{
  public:
	AudioBuffer() : buffer(0), queueOut(0), queueIn(0), isFull(false), bufSize(0), bufTypeX(none), bufState(empty) {}
	~AudioBuffer() {disposeBuffer();}
	enum result  {ok,halfEmpty,underflow,full,invalid};
	enum bufType {none,given,inHeap,inExt};
	enum bufState_e {empty,firstValid,secondValid,bothValid}; // state of buffer
	
  //private:
	uint8_t* buffer;	// memory used for buffering
	size_t queueOut;	// next read() will start from here
	size_t queueIn;		// next write() will start from here
	bool isFull;		// if true, queueIn == queueOut means buffer is completely full
	size_t bufSize;		// total size of buffer
	bufType bufTypeX;
	bufState_e bufState;
	
  public:
	// buffer memory maintenance:
	result createBuffer(size_t sz, bufType typ);  // create buffer for audio data, managed by class
	result createBuffer(uint8_t* buf, size_t sz);  // create buffer for audio data, managed by application
	result disposeBuffer(); // dispose of buffer: if it's of type "given", the application may free it after this call
	
	// "playback" mode:
	bool getNextRead(uint8_t** pbuf, size_t* psz);  // find out where (media) data needs to be read to
	void readExecuted(size_t bytes);  		// signal that buffer data has been read in (from  media)
	result read(uint8_t* dest, size_t bytes); 	// read buffer data into destination
	
	// "recording" mode
	result write(uint8_t* src, size_t bytes); 	// copy source data into buffer
	bool getNextWrite(uint8_t** pbuf, size_t* psz); // find out where (media) data needs to be written from
	void writeExecuted(size_t bytes); 		// signal that buffer data has been written out (to media)
	
	size_t getAvailable(); // find out how much valid data is available
	void emptyBuffer(size_t offset=0) {queueOut = queueIn = offset; isFull = false; bufState = empty;} // initialise the buffer to its empty state
};
 
 
class AudioWAVdata
{
  public:
	union tag_t
	{
	  char c[4];
	  uint32_t u;
	};

	struct RIFFhdr_t
	{
	  tag_t riff;
	  uint32_t flen; // *** only known after recording
	  tag_t wav;
	};

	struct chunk_t
	{
	  tag_t id;
	  uint32_t clen;
	};

	struct fmt_t
	{
	  chunk_t hdr;
	  uint16_t fmt; 
	  uint16_t chanCnt;
	  uint32_t sampleRate;
	  uint32_t byteRate;
	  uint16_t blockAlign;
	  uint16_t bitsPerSample;
	}__attribute__((packed));
	
	struct ext_t
	{
	  uint16_t extSize;
	  uint16_t validBitsPerSample;
	  uint32_t channelMask;

	  union {
		char subFormat[16];
		uint16_t subFmt;
	  };
	};
	
	struct ext_fmt_t
	{
		struct fmt_t fmt;
		struct ext_t ext;
	};
	
	struct wavhdr_t
	{
		RIFFhdr_t riff;
		fmt_t fmt;
		chunk_t data; // *** .clen value only known after recording
	};

	uint16_t format;  		// file format
	uint16_t bitsPerSample; // bits per sample
	uint16_t chanCnt; 		// number of channels
	uint16_t dataChunks;	// number of data chunks
	uint32_t samples; 		// number of samples
	uint32_t nextAudio;		// offset of next audio data
	uint32_t audioSize;		// number of bytes of audio
	uint32_t bytes2millis;	// (scaled) conversion from file bytes to milliseconds

	AudioWAVdata(uint16_t cct) : format(0), bitsPerSample(0), chanCnt(cct), 
					 dataChunks(0), samples(0), nextAudio(0)
					 {}
	AudioWAVdata() : AudioWAVdata(2) {}				 
	uint32_t getB2M(uint16_t chanCnt, uint32_t sampleRate, uint16_t bitsPerSample);
	uint16_t parseWAVheader(File& f); // parse WAV file
	void makeWAVheader(wavhdr_t* wav, uint16_t chans = 1, uint16_t fmt = 1, uint16_t bits = 16, uint32_t rate = AUDIO_SAMPLE_RATE);
};
 
 #endif // !defined(_AUDIO_BUFFER_H_)
 