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

#include <Arduino.h>
#include "play_wav_buffered.h"

/* static */ uint8_t AudioPlayWAVbuffered::objcnt;
/* static */ void AudioPlayWAVbuffered::EventResponse(EventResponderRef evref)
{
	uint8_t* pb;
	size_t sz;
	
	AudioPlayWAVbuffered* pPWB = (AudioPlayWAVbuffered*) evref.getContext();
	pPWB->getNextRead(&pb,&sz);		// find out where and how much
	if (sz > pPWB->bufSize / 2)
		sz = pPWB->bufSize / 2;		// limit reads to a half-buffer at a time
	pPWB->loadBuffer(pb,sz);		// load more file data to the buffer
}


void AudioPlayWAVbuffered::loadBuffer(uint8_t* pb, size_t sz)
{
	size_t got;
	
SCOPE_HIGH();	
SCOPESER_TX(objnum);

	if (sz > 0) // read triggered, but there's no room - ignore the request
	{
		
{//----------------------------------------------------
	size_t av = getAvailable();
	if (av != 0 && av < lowWater && !eof)
		lowWater = av;
SCOPESER_TX((av >> 8) & 0xFF);
SCOPESER_TX(av & 0xFF);
}//----------------------------------------------------

		got = wavfile.read(pb,sz);	// try for that
		if (got < sz) // there wasn't enough data
		{
			memset(pb+got,0,sz-got); // zero the rest of the buffer
			eof = true;
		}

		readExecuted(got);
	}
	readPending = false;
SCOPE_LOW();
}


AudioPlayWAVbuffered::AudioPlayWAVbuffered(void) : 
		AudioStream(0, NULL),
		lowWater(0xFFFFFFFF),
		eof(false), readPending(false), objnum(objcnt++),
		data_length(0), total_length(0),
		state(STATE_STOP), state_play(STATE_STOP),leftover_bytes(0)
{
SCOPE_ENABLE();
SCOPESER_ENABLE();
	
	// prepare EventResponder to refill buffer
	// during yield(), if triggered from update()
	setContext(this);
	attach(EventResponse);
}


bool AudioPlayWAVbuffered::playSD(const char *filename, bool paused /* = false */)
{
	return play(SD.open(filename), paused);
}


bool AudioPlayWAVbuffered::play(const File _file, bool paused /* = false */)
{
	bool rv = false;
	
	stop();
	wavfile = _file;
	
	// ensure a minimal buffer exists
	if (nullptr == buffer)
		createBuffer(1024,inHeap);
	
	if (wavfile && nullptr != buffer) 
	{
		uint8_t* pb;
		size_t sz,stagger;
		constexpr int SLOTS=16;
		
		//* stagger pre-load:
		stagger = bufSize / 1024; 
		if (stagger > SLOTS)
			stagger = SLOTS;
		stagger = (bufSize>>1) / stagger;
		
		// load data
		emptyBuffer((objnum & (SLOTS-1)) * stagger); // ensure we start from scratch
		parseWAVheader(wavfile); // figure out WAV file structure
		getNextRead(&pb,&sz);	// find out where and how much				
		loadBuffer(pb,sz);		// load initial file data to the buffer
		read(nullptr,nextAudio); // skip the header
		
		data_length = total_length = audioSize;
		
		state_play = STATE_PLAYING;
		if (paused)
			state = STATE_PAUSED;
		else
			state = STATE_PLAYING;
		eof = false;
		rv = true;
		setInUse(true); // prevent changes to buffer memory
	}

	return rv;
}


void AudioPlayWAVbuffered::stop(void)
{
	if (state != STATE_STOP) 
	{
		wavfile.close();
		eof = true;
		state = STATE_STOP;
		setInUse(false); // allow changes to buffer memory
	}
}


void AudioPlayWAVbuffered::togglePlayPause(void) {
	// take no action if wave header is not parsed OR
	// state is explicitly STATE_STOP
	if(state_play >= 8 || state == STATE_STOP) return;

	// toggle back and forth between state_play and STATE_PAUSED
	if(state == state_play) {
		state = STATE_PAUSED;
	}
	else if(state == STATE_PAUSED) {
		state = state_play;
	}
}

// de-interleave channels of audio from buf into separate blocks
static void deinterleave(int16_t* buf,int16_t** blocks,uint16_t channels)
{
	if (1 == channels) // mono, do the simple thing
		memcpy(blocks[0],buf,AUDIO_BLOCK_SAMPLES * sizeof *buf);
	else
		for (uint16_t i=0;i<channels;i++)
		{
			int16_t* ps = buf+i;
			int16_t* pd = blocks[i];
			for (int j=0;j<AUDIO_BLOCK_SAMPLES;j++)
			{
				*pd++ = *ps;
				ps += channels;
			}
		}
}


void AudioPlayWAVbuffered::update(void)
{
	int16_t buf[chanCnt * AUDIO_BLOCK_SAMPLES];
	audio_block_t* blocks[chanCnt];	
	int16_t* data[chanCnt];
	int alloCnt = 0; // count of blocks successfully allocated
	
	// only update if we're playing and not paused
	if (state == STATE_STOP || state == STATE_PAUSED) return;

	// allocate the audio blocks to transmit
	while (alloCnt < chanCnt)
	{
		blocks[alloCnt] = allocate();
		if (nullptr == blocks[alloCnt])
			break;
		data[alloCnt] = blocks[alloCnt]->data;
		alloCnt++;
	}
	
	if (alloCnt >= chanCnt) // allocated enough - fill them with data
	{
		// try to fill buffer, settle for what's available
		size_t toRead = getAvailable();
		if (toRead > sizeof buf)
		{
			toRead = sizeof buf;
		}
		
		// also, don't play past end of data
		// could leave some data unread in buffer, but
		// it's not audio!
		if (toRead > data_length)
			toRead = data_length;
		
		// unbuffer and deinterleave to audio blocks
		result rdr = read((uint8_t*) buf,toRead);
		if (toRead < sizeof buf) // not enough data in buffer
		{
			memset(((uint8_t*) buf)+toRead,0,sizeof buf - toRead); // fill with silence
			stop(); // and stop: brutal, but probably better than losing sync
		}
		deinterleave(buf,data,chanCnt);

		if (ok != rdr 			// there's now room for a buffer read,
			&& !eof 			// and more file data available
			&& !readPending)  	// and we haven't already asked
		{
			triggerEvent(rdr);
			readPending = true;
		}
		
		// transmit: mono goes to both outputs, stereo
		// upward go to the relevant channels
		transmit(blocks[0], 0);
		if (1 == chanCnt)
			transmit(blocks[0], 1);
		else
		{
			for (int i=1;i<chanCnt;i++)
				transmit(blocks[i], i);
		}
		
		// deal with position tracking
		if (toRead <= data_length)
			data_length -= toRead;
		else
			data_length = 0;
	}
	
	// relinquish our interest in these blocks
	while (--alloCnt >= 0)
		release(blocks[alloCnt]);
	
}

/*
00000000  52494646 66EA6903 57415645 666D7420  RIFFf.i.WAVEfmt 
00000010  10000000 01000200 44AC0000 10B10200  ........D.......
00000020  04001000 4C495354 3A000000 494E464F  ....LIST:...INFO
00000030  494E414D 14000000 49205761 6E742054  INAM....I Want T
00000040  6F20436F 6D65204F 76657200 49415254  o Come Over.IART
00000050  12000000 4D656C69 73736120 45746865  ....Melissa Ethe
00000060  72696467 65006461 746100EA 69030100  ridge.data..i...
00000070  FEFF0300 FCFF0400 FDFF0200 0000FEFF  ................
00000080  0300FDFF 0200FFFF 00000100 FEFF0300  ................
00000090  FDFF0300 FDFF0200 FFFF0100 0000FFFF  ................
*/



bool AudioPlayWAVbuffered::isPlaying(void)
{
	uint8_t s = *(volatile uint8_t *)&state;
	return (s == STATE_PLAYING);
}


bool AudioPlayWAVbuffered::isPaused(void)
{
	uint8_t s = *(volatile uint8_t *)&state;
	return (s == STATE_PAUSED);
}


bool AudioPlayWAVbuffered::isStopped(void)
{
	uint8_t s = *(volatile uint8_t *)&state;
	return (s == STATE_STOP);
}

/**
 * Approximate progress in milliseconds.
 *
 * This actually reflects the state of play when the last
 * update occurred, so it will be out of date by up to
 * 2.9ms (with normal settings of 44100/16bit), and jump
 * in increments of 2.9ms. Also, it will differ from
 * what you hear, depending on the design and hardware 
 * delays.
 */
uint32_t AudioPlayWAVbuffered::positionMillis(void)
{
	uint8_t s = *(volatile uint8_t *)&state;
	if (s >= 8 && s != STATE_PAUSED) return 0;
	uint32_t tlength = *(volatile uint32_t *)&total_length;
	uint32_t dlength = *(volatile uint32_t *)&data_length;
	uint32_t offset = tlength - dlength;
	uint32_t b2m = *(volatile uint32_t *)&bytes2millis;
	return ((uint64_t)offset * b2m) >> 32;
}


uint32_t AudioPlayWAVbuffered::lengthMillis(void)
{
	uint8_t s = *(volatile uint8_t *)&state;
	if (s >= 8 && s != STATE_PAUSED) return 0;
	uint32_t tlength = *(volatile uint32_t *)&total_length;
	uint32_t b2m = *(volatile uint32_t *)&bytes2millis;
	return ((uint64_t)tlength * b2m) >> 32;
}






