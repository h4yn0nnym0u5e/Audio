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
#include <laser_synth.h>


/*
 * Prepare to play back from a file
 */
bool AudioPlayWAVbuffered::prepareFile(bool paused, float startFrom, size_t startFromI)
{
	bool rv = false;
	
	if (wavfile && nullptr != buffer) 
	{
		uint8_t* pb;
		size_t sz,stagger,skip;
		constexpr int SLOTS=16;
		int actualSlots = SLOTS;
		
		//* stagger pre-load:
		actualSlots = bufSize / 1024; 
		if (actualSlots > SLOTS)
			actualSlots = SLOTS;
		stagger = (bufSize>>1) / actualSlots;
		
		// load data
		emptyBuffer((objnum & (actualSlots-1)) * stagger); // ensure we start from scratch
		parseWAVheader(wavfile); 	// figure out WAV file structure
		getNextRead(&pb,&sz);		// find out where and how much the buffer pre-load is
		
		switch (fileFormat)
		{
			default:
				eof = true;
				
			case WAV:
				data_length = total_length = audioSize; // all available data
				eof = false;
				
				if (startFrom <= 0.0f && 0 == startFromI) // not starting later in time or bytes
				{
					skip = firstAudio;
				}
				else // we want to start playback later into the file: compute sector containing start point
				{
					if (0 == startFromI) // use time, not absolute file position
						startFromI = millisToPosition(startFrom,AUDIO_SAMPLE_RATE);
					skip = startFromI & (512-1);	// skip partial sector...
					startFromI -= skip;				// ...having loaded from sector containing start point
					wavfile.seek(startFromI);
					data_length  = audioSize - skip + firstAudio - startFromI; // where we started playing, so already "used"
				}
				break;
				
			case ILDA:
				eof = false;
				((AudioPlayILDA*) this)->records = 0;
				((AudioPlayILDA*) this)->samples = 0;
				skip = 0;
				break;
		}
		
		loadBuffer(pb,sz,true);	// load initial file data to the buffer: may set eof
		read(nullptr,skip);	// skip the header
		
		fileLoaded = ARM_DWT_CYCCNT;
		
		state_play = STATE_PLAYING;
		if (paused)
			state = STATE_PAUSED;
		else
			state = STATE_PLAYING;
		rv = true;
		setInUse(true); // prevent changes to buffer memory
		fileState = filePrepared;
	}
	
	return rv;
}


/* static */ uint8_t AudioPlayWAVbuffered::objcnt;
void AudioPlayWAVbuffered::EventResponse(EventResponderRef evref)
{
	uint8_t* pb;
	size_t sz;
	
	switch (evref.getStatus())
	{
		case STATE_LOADING: // playing pre-load: open and prepare file, ready to switch over
			fileState = fileEvent;
			wavfile = ppl->open();
			if (prepareFile(STATE_PAUSED == state,0.0f,ppl->fileOffset))
				fileState = fileReady;
			else
			{
				//fileState = ending;
				triggerEvent(STATE_LOADING); // re-trigger first file read
			}
			break;
			
		case STATE_PLAYING: // request from update() to re-fill buffer
			getNextRead(&pb,&sz);		// find out where and how much
			if (sz > bufSize / 2)
				sz = bufSize / 2;		// limit reads to a half-buffer at a time
			loadBuffer(pb,sz);		// load more file data to the buffer
			break;
			
		case STATE_STOP: // stopped from interrupt - finish the job
// if (!eof) Serial.printf("STOP event before EOF at %lu\n",millis());		
			stop();
			break;
			
		default:
			asm("nop");
			break;
	}
}


static void EventDespatcher(EventResponderRef evref)
{
	AudioPlayWAVbuffered* pPWB = (AudioPlayWAVbuffered*) evref.getContext();
	
	pPWB->EventResponse(evref);
}


void AudioPlayWAVbuffered::loadBuffer(uint8_t* pb, size_t sz, bool firstLoad /* = false */)
{
	size_t got = 0;
	int gotNothingCount = 0;
	
SCOPE_HIGH();	
SCOPESER_TX(objnum);

	if (sz > 0 && !eof) // read triggered, but there's no room or already stopped - ignore the request
	{
		
{//----------------------------------------------------
	size_t av = getAvailable();
	if (av != 0 && av < lowWater && !eof)
		lowWater = av;
SCOPESER_TX((av >> 8) & 0xFF);
SCOPESER_TX(av & 0xFF);
}//----------------------------------------------------

		uint32_t now = micros();
		
		switch (fileFormat)
		{
			default:
			case WAV:
				got = wavfile.read(pb,sz);	// try for that
				
				if (got < sz) // there wasn't enough data
				{
					if (got < 0)
						got = 0;

					memset(pb+got,0,sz-got); // zero the rest of the buffer
					eof = true;
				}
				readExecuted(got);
				
				break;
				
			case ILDA: // we need to loop, but try to keep reads on SD card boundary
				while (sz >= 512 && gotNothingCount < 2)
				{
					size_t toGet = sz & (-(512LL));
					got = wavfile.read(pb,toGet);	// try for that, rounded to sector size
					
					if (got < toGet) // there wasn't enough data
					{
						if (got < 0) // error
						{
							got = 0;
							gotNothingCount = 99; // bail immediately
						}
						else if (0 == got) // only allow one EOF in a row
							gotNothingCount++;
						else
							gotNothingCount = 0;
						wavfile.seek(0); // assume failure was no more data: loop
					}
					pb += got;
					sz -= got;
					readExecuted(got);
				}
				if (sz > 0)
					dummyReadExecuted(sz); // tell buffer to flip to other half
				
				break;
		}
		if (!firstLoad) // empty on first load, don't record result
			bufferAvail.newValue(getAvailable()); // worse than lowWater
		readMicros.newValue(micros() - now); // a slight over-estimate, but not by much
	}
	readPending = false;
SCOPE_LOW();
}


/**
 * Adjust header information if file has changed since playback started.
 * This can happen in looper applications, where we want to begin playback
 * while still recording the tail of the file.
 *
 * Called from the application, so we are guaranteed the EventResponder
 * will not access the file to re-fill buffers during this function.
 *
 * \return amount the audio size changed by: could be 0
 */
uint32_t AudioPlayWAVbuffered::adjustHeaderInfo(void)
{
	uint32_t result = 0;
	
	if (wavfile) // we'd better be playing, really!
	{
		size_t readPos = wavfile.position(); // keep current position safe
		AudioWAVdata newWAV;
		
		// parse the current header, then seek back to where we were
		newWAV.parseWAVheader(wavfile);
		wavfile.seek(readPos);
		
		if (newWAV.audioSize != audioSize) // file header has been changed since we started
		{
			result = newWAV.audioSize - audioSize;  // change is this (in bytes)
			__disable_irq(); // audio update may change this...
				data_length += result;
			__enable_irq(); // ...safe now
			total_length += result;
			samples = newWAV.samples;
		}
	}
	
	return result;
}


/* Constructor */
AudioPlayWAVbuffered::AudioPlayWAVbuffered(void) : 
		AudioStream(0, NULL),
		lowWater(0xFFFFFFFF),
		wavfile(0), ppl(0), preloadRemaining(0),
		eof(false), readPending(false), objnum(objcnt++),
		data_length(0), total_length(0),
		state(STATE_STOP), state_play(STATE_STOP),
		playState(silent), fileState(silent),
		leftover_bytes(0)
{
SCOPE_ENABLE();
SCOPESER_ENABLE();
	
	// prepare EventResponder to refill buffer
	// during yield(), if triggered from update()
	setContext(this);
	attach(EventDespatcher);
}


/*
 * Destructor
 *
 * Could be called while active, so need to take care that destruction
 * is done in a sane order. 
 *
 * MUST NOT destruct the object from an ISR!
 */
AudioPlayWAVbuffered::~AudioPlayWAVbuffered(void)
{
	stop(); // close file, relinquish use of preload buffer, any triggered event cleared, set to STATE_STOPPING
	// Further destructor actions:
	// This destructor exits, wavfile is destructed
	// ~AudioStream: unlinked from connections and update list
	// ~AudioWAVdata
	// ~AudioBuffer: ~MemBuffer disposes of buffer memory, if bufType != given
	// ~EventResponder: detached from event list
}

bool AudioPlayWAVbuffered::playSD(const char *filename, bool paused /* = false */, float startFrom /* = 0.0f */)
{
	return play(SD.open(filename), paused, startFrom);
}


bool AudioPlayWAVbuffered::play(const File _file, bool paused /* = false */, float startFrom /* = 0.0f */)
{
	bool rv = false;
	
	playCalled = ARM_DWT_CYCCNT;
	firstUpdate = fileLoaded = 0;
	
	stop();
	wavfile = _file;
	
	// ensure a minimal buffer exists
	if (nullptr == buffer)
		createBuffer(1024,inHeap);
	
	rv = prepareFile(paused,startFrom,0); // get file ready to play
	if (rv)
	{
		fileState = fileReady;
		playState = file;
	}

	return rv;
}


/*
 * Play audio starting from pre-loaded buffer, and switching to filesystem when that's exhausted.
 *
 * Note that while there's an option to play from a point later than the first sample of the pre-load,
 * this could result in skipping the pre-loaded data altogether, followed by a delay while
 * the first samples are loaded from the filesystem. It's up to the user to manage this. 
 *
 * Note also that the two startFrom values are cumulative; if you pre-load starting at 100.0ms, 
 * then play() starting at 50.0ms, playback starts 150.0ms into the audio file.
 */
bool AudioPlayWAVbuffered::play(AudioPreload& p, bool paused /* = false */, float startFrom /* = 0.0f */)
{
	bool rv = false;
	
	playCalled = ARM_DWT_CYCCNT;
	firstUpdate = fileLoaded = 0;
	
	stop();
	
	if (p.isReady())
	{
		bool justPlay = false;  // assume we can use the pre-load
		
		ppl = &p;				// using this preload
		preloadRemaining = ppl->valid; // got this much data left
		chanCnt = ppl->chanCnt;	// we need to know the channel count to de-interleave
		
		if (startFrom > 0.0f)
		{
			float plms = (float) p.valid / p.sampleSize / AUDIO_SAMPLE_RATE; // audio pre-loaded [ms]
			if (plms > startFrom) // can use some of the pre-load
			{
				int nsamp = startFrom * AUDIO_SAMPLE_RATE / 1000.0f; // samples to skip
				preloadRemaining -= nsamp * p.sampleSize; // ensure we're on a sample boundary 
			}
			else // just do a normal play()
				justPlay = true;
		}
		
		if (justPlay)
			rv = play(p.filepath,*p.pFS,paused,startFrom + p.startSample / AUDIO_SAMPLE_RATE / 1000.0f);
		else
		{
			playState = sample;		// start by playing pre-loaded data
			fileState = fileLoad;	// load file buffer on first event
			ppl->setInUse(true); // prevent changes to buffer memory

			state_play = STATE_PLAYING;
			if (paused)
				state = STATE_PAUSED;
			else
				state = STATE_PLAYING;
			rv = true;
		}
	}
	
	return rv;
}


void AudioPlayWAVbuffered::stop(uint8_t fromInt /* = false */)
{
	bool eventTriggered = false;
	if (state != STATE_STOP) 
	{
		state = STATE_STOPPING; // prevent update() from doing anything
		if (wavfile) // audio file is open
		{
			if (fromInt)	// can't close file, SD action may be in progress
			{
				triggerEvent(STATE_STOP); // close on next yield()
				eventTriggered = true;
			}
			else
			{
				wavfile.close();
				state = state_play = STATE_STOP;
				playState /* = fileState */ = silent;
			}
		}
		else // file is closed, can always stop immediately
		{
			state = state_play = STATE_STOP;
			playState /* = fileState */ = silent;
		}
		
		if (!eventTriggered) // if we didn't just trigger a stop event...
			clearEvent();	 // ...clear pending read, file will be closed!
		
		if (!eventTriggered && nullptr != ppl) // preload is in use
		{
			ppl->setInUse(false);
			ppl = nullptr;
		}
		
		readPending = false;
		eof = true;
		setInUse(false); // allow changes to buffer memory
	}
}


void AudioPlayWAVbuffered::togglePlayPause(void) {
	// take no action if wave header is not parsed OR
	// state is explicitly STATE_STOP
	if(state_play >= 8 || state == STATE_STOP || state == STATE_STOPPING) return;

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
	audio_block_t* blocks[chanCnt];	
	int16_t* data[chanCnt];
	int alloCnt = 0; // count of blocks successfully allocated
	
	// only update if we're playing and not paused, and it's a WAV file
	if ((state == STATE_STOP || state == STATE_STOPPING || state == STATE_PAUSED)
		&& fileFormat == WAV) 
		return;
	
	// just possible the channel count will be zero, if a file suddenly goes AWOL:
	if (0 == chanCnt)
	{
		stop(true);
		return;
	}

	if (0 == firstUpdate)
		firstUpdate = ARM_DWT_CYCCNT;
	
	// if just started, we may need to trigger the first file read
	if (!readPending && fileLoad == fileState)
	{
		triggerEvent(STATE_LOADING); // trigger first file read
		readPending = true;
		fileState = fileReq;
	}

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
		switch (fileFormat)
		{
			case WAV:
			{
				int16_t buf[chanCnt * AUDIO_BLOCK_SAMPLES];
				size_t toFill = sizeof buf, got = 0;
				result rdr = ok;
				
				// if we're using pre-load buffer, try to fill from there
				if (sample == playState)
				{
					if (preloadRemaining >= toFill) // enough or more!
					{
						memcpy(buf,ppl->buffer+(ppl->valid - preloadRemaining),toFill);
						preloadRemaining -= toFill;
						got = toFill;
						toFill = 0;
					}
					else // not enough, but some
					{
						memcpy(buf,ppl->buffer+(ppl->valid - preloadRemaining),preloadRemaining);
						got = preloadRemaining;
						preloadRemaining = 0;
						toFill -= got;
					}

					if (0 == preloadRemaining)
						playState = file;
				}
				
				// preload, if in use, is needed until the file buffer is ready
				if (nullptr != ppl && file == playState && fileReady == fileState)
				{
					ppl->setInUse(false); // finished with preload
					ppl = nullptr;
				}
				
				// try to fill buffer from file, settle for what's available
				if (file == playState && toFill > 0) // file is ready and we still need data
				{
					size_t toRead = getAvailable();
					if (toRead > toFill)
					{
						toRead = toFill;
					}
					
					// also, don't play past end of data
					// could leave some data unread in buffer, but
					// it's not audio!
					if (toRead > data_length)
						toRead = data_length;
					
					// unbuffer
					rdr = read((uint8_t*) buf + got,toRead);
					
					if (invalid != rdr) // we got enough
						got += toRead;				
				}
				
				if (got < sizeof buf) // not enough data in buffer
				{
					memset(((uint8_t*) buf)+got,0,sizeof buf - got); // fill with silence
					stop(true); // and stop (within ISR): brutal, but probably better than losing sync
				}
				
				// deinterleave to audio blocks
				deinterleave(buf,data,chanCnt);

				if (ok != rdr 			// there's now room for a buffer read,
					&& !eof 			// and more file data available
					&& !readPending)  	// and we haven't already asked
				{
					triggerEvent(STATE_PLAYING); // trigger a file read
					readPending = true;
				}
				
				// deal with position tracking
				if (got <= data_length)
					data_length -= got;
				else
					data_length = 0;
					}
				break;
				
			case ILDA:
			{
				if (state == STATE_STOP || state == STATE_STOPPING || state == STATE_PAUSED)
				{
					AudioPlayILDA* thisILDA = (AudioPlayILDA*) this;
					// stopped: still need to putput something sane
					for (int i=0;i<AUDIO_BLOCK_SAMPLES;i++)
					{
						// use last galvo position: this would be dangerous, but...
						*data[0]++ = thisILDA->lastX;
						*data[1]++ = thisILDA->lastY;
						*data[2]++ = thisILDA->lastZ;
						
						// ...we turn the lasers off...
						*data[3]++ = BLACK_LEVEL;
						*data[4]++ = BLACK_LEVEL;
						*data[5]++ = BLACK_LEVEL;
						
						// ...and enable blanking
						*data[6]++ = BLANKING(1);
						
					}
				}
				else // playing a file: get more data
				{
					result rdr = ok;
					bool readNeeded = false;
					int toRead = AUDIO_BLOCK_SAMPLES; 	// we need to create this many samples
					AudioPlayILDA* thisILDA = (AudioPlayILDA*) this;
					ILDAformatAny rec;
					
					while (toRead > 0)
					{
						if (0 == thisILDA->samples)
						{
							if (thisILDA->records > 0) // we're still reading the current set of records
							{
								// unbuffer
								rdr = read((uint8_t*) &rec, AudioPlayILDA::sizes[thisILDA->recFormat]);
								thisILDA->records--;
								if (ok != rdr)
									readNeeded = true;
								
								// convert to unpacked audio data
								switch (thisILDA->recFormat)
								{
									case 0: // XYZp
										thisILDA->unpacked.X = htons(rec.f0.X);
										thisILDA->unpacked.Y = htons(rec.f0.Y);
										thisILDA->unpacked.Z = htons(rec.f0.Z);
										{
											int idx = rec.f0.index;
											if (idx > thisILDA->paletteValid)
												idx = thisILDA->paletteValid - 1;
											thisILDA->unpacked.R = CONVERT(thisILDA->palette[idx].R);
											thisILDA->unpacked.G = CONVERT(thisILDA->palette[idx].G);
											thisILDA->unpacked.B = CONVERT(thisILDA->palette[idx].B);
										}
										thisILDA->unpacked.status = rec.f0.status;
										break;
										
									case 1: // XYp
										thisILDA->unpacked.X = htons(rec.f1.X);
										thisILDA->unpacked.Y = htons(rec.f1.Y);
										thisILDA->unpacked.Z = 0;
										{
											int idx = rec.f1.index;
											if (idx > thisILDA->paletteValid)
												idx = thisILDA->paletteValid - 1;
											thisILDA->unpacked.R = CONVERT(thisILDA->palette[idx].R);
											thisILDA->unpacked.G = CONVERT(thisILDA->palette[idx].G);
											thisILDA->unpacked.B = CONVERT(thisILDA->palette[idx].B);
										}
										thisILDA->unpacked.status = rec.f1.status;
										break;
										
									case 2: // palette
									{
										int n = thisILDA->records; // remaining entries
										
										if (thisILDA->palette != thisILDA->defaultPalette // we have some valid palette memory
										 && thisILDA->palette != nullptr)
										{
											
											*thisILDA->palette = rec.f2; // safe: palette must have at least 2 entries
											
											if (n > thisILDA->paletteSize-1) // only read...
												n = thisILDA->paletteSize-1; // ...as many as will fit
											
											rdr= read((uint8_t*) thisILDA->palette+1, n * AudioPlayILDA::sizes[thisILDA->recFormat]);
											if (ok != rdr)
												readNeeded = true;
											
											n = thisILDA->records - n; // how many didn't fit?
										}
										
										if (n > 0) // need to discard unusable entries
										{
											rdr= read(nullptr, n * AudioPlayILDA::sizes[thisILDA->recFormat]); // discard
											if (ok != rdr)
												readNeeded = true;
										}
									}
										break;
										
									case 4:
										thisILDA->unpacked.X = htons(rec.f4.X);
										thisILDA->unpacked.Y = htons(rec.f4.Y);
										thisILDA->unpacked.Z = htons(rec.f4.Z);
										thisILDA->unpacked.R = CONVERT(rec.f4.R);
										thisILDA->unpacked.G = CONVERT(rec.f4.G);
										thisILDA->unpacked.B = CONVERT(rec.f4.B);
										thisILDA->unpacked.status = rec.f4.status;
										break;
										
									case 5:
										thisILDA->unpacked.X = htons(rec.f5.X);
										thisILDA->unpacked.Y = htons(rec.f5.Y);
										thisILDA->unpacked.Z = 0;
										thisILDA->unpacked.R = CONVERT(rec.f5.R);
										thisILDA->unpacked.G = CONVERT(rec.f5.G);
										thisILDA->unpacked.B = CONVERT(rec.f5.B);
										thisILDA->unpacked.status = rec.f5.status;
										break;
								}
								
							}
							else // out of records, get a header
							{
								ILDAheader_s hdr;
								
								// unbuffer
								rdr = read((uint8_t*) &hdr, sizeof hdr);
								if (ok != rdr)
									readNeeded = true;
								
								if (underflow != rdr && hdr.ilda.u == 0x41444C49) // TODO: fix magic number
								{
									thisILDA->recFormat = hdr.format;
									thisILDA->records   = htons(hdr.records);							
								}
								continue; // restart while() loop, we now have a new record count and format
							}
							thisILDA->samples = thisILDA->samplesPerPoint;
						}
						
						if (2 == thisILDA->recFormat) // palette, nothing unpacked
						{
							thisILDA->samples = 0;
							thisILDA->records = 0;
						}
						else
						{
							*data[0]++ = thisILDA->unpacked.X;
							*data[1]++ = thisILDA->unpacked.Y;
							*data[2]++ = thisILDA->unpacked.Z;
							
							if (thisILDA->unpacked.status & 0x40) // blanked
							{
								*data[3]++ = BLACK_LEVEL;
								*data[4]++ = BLACK_LEVEL;
								*data[5]++ = BLACK_LEVEL;
								*data[6]++ = BLANKING(1);
							}
							else
							{
								*data[3]++ = thisILDA->unpacked.R;
								*data[4]++ = thisILDA->unpacked.G;
								*data[5]++ = thisILDA->unpacked.B;
								*data[6]++ = BLANKING(0);
							}

							toRead--;
							thisILDA->samples--;
						}
					}
					thisILDA->lastX = *(data[0] -1);
					thisILDA->lastY = *(data[1] -1);
					thisILDA->lastZ = *(data[2] -1);
					
					if (readNeeded 			// there's now room for a buffer read,
						&& !eof 			// and more file data available
						&& !readPending)  	// and we haven't already asked
					{
						triggerEvent(STATE_PLAYING); // trigger a file read
						readPending = true;
					}
				}
				
			}
				break;
				
			default:
				break;
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
	return (s == STATE_STOP || s == STATE_STOP);
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

//===================================================================================================
// AudioPlayILDA stuff
const int AudioPlayILDA::sizes[6] = {sizeof(ILDAformat0), sizeof(ILDAformat1), sizeof(ILDAformat2), 
                                             -1, sizeof(ILDAformat4), sizeof(ILDAformat5)};

/*
 * Start using a different block of memory as the palette.
 */
void AudioPlayILDA::setPaletteMemory(ILDAformat2* addr, int entries, int valid /* = -1 */)
{
	if (-1 == valid)
		valid = entries;
	
	if (valid >= 2) // palettes must have at least 2 entries
	{
		palette 	 = addr;
		paletteSize  = entries;
		paletteValid = valid;
	}
}

void AudioPlayILDA::copyPalette(ILDAformat2* dst, const ILDAformat2* src, int entries)
{
	if (nullptr == src)
		src = defaultPalette;
	memcpy(dst,src,entries * sizeof *dst);
}



// ILDA standard color palette 
const AudioWAVdata::ILDAformat2 AudioPlayILDA::defaultPalette[256] = {
    {   0,   0,   0 },  // Black/blanked (fixed)
    { 255, 255, 255 },  // White (fixed)
    { 255,   0,   0 },  // Red (fixed)
    { 255, 255,   0 },  // Yellow (fixed)
    {   0, 255,   0 },  // Green (fixed)
    {   0, 255, 255 },  // Cyan (fixed)
    {   0,   0, 255 },  // Blue (fixed)
    { 255,   0, 255 },  // Magenta (fixed)
    { 255, 128, 128 },  // Light red
    { 255, 140, 128 },
    { 255, 151, 128 }, // 10
    { 255, 163, 128 },
    { 255, 174, 128 },
    { 255, 186, 128 },
    { 255, 197, 128 },
    { 255, 209, 128 },
    { 255, 220, 128 },
    { 255, 232, 128 },
    { 255, 243, 128 },
    { 255, 255, 128 },  // Light yellow
    { 243, 255, 128 }, //20
    { 232, 255, 128 },
    { 220, 255, 128 },
    { 209, 255, 128 },
    { 197, 255, 128 },
    { 186, 255, 128 },
    { 174, 255, 128 },
    { 163, 255, 128 },
    { 151, 255, 128 },
    { 140, 255, 128 },
    { 128, 255, 128 },  // Light green 30
    { 128, 255, 140 },
    { 128, 255, 151 },
    { 128, 255, 163 },
    { 128, 255, 174 },
    { 128, 255, 186 },
    { 128, 255, 197 },
    { 128, 255, 209 },
    { 128, 255, 220 },
    { 128, 255, 232 },
    { 128, 255, 243 }, // 40
    { 128, 255, 255 },  // Light cyan
    { 128, 243, 255 },
    { 128, 232, 255 },
    { 128, 220, 255 },
    { 128, 209, 255 },
    { 128, 197, 255 },
    { 128, 186, 255 },
    { 128, 174, 255 },
    { 128, 163, 255 },
    { 128, 151, 255 }, // 50
    { 128, 140, 255 },
    { 128, 128, 255 },  // Light blue
    { 140, 128, 255 },
    { 151, 128, 255 },
    { 163, 128, 255 },
    { 174, 128, 255 },
    { 186, 128, 255 },
    { 197, 128, 255 },
    { 209, 128, 255 },
    { 220, 128, 255 }, // 60
    { 232, 128, 255 },
    { 243, 128, 255 },
    { 255, 128, 255 }, // Light magenta
    { 255, 128, 243 },
    { 255, 128, 232 },
    { 255, 128, 220 },
    { 255, 128, 209 },
    { 255, 128, 197 },
    { 255, 128, 186 },
    { 255, 128, 174 }, // 70
    { 255, 128, 163 },
    { 255, 128, 151 },
    { 255, 128, 140 },
    { 255,   0,   0 },  // Red (cycleable)
    { 255,  23,   0 },
    { 255,  46,   0 },
    { 255,  70,   0 },
    { 255,  93,   0 },
    { 255, 116,   0 },
    { 255, 139,   0 }, // 80
    { 255, 162,   0 },
    { 255, 185,   0 },
    { 255, 209,   0 },
    { 255, 232,   0 },
    { 255, 255,   0 },  //Yellow (cycleable)
    { 232, 255,   0 },
    { 209, 255,   0 },
    { 185, 255,   0 },
    { 162, 255,   0 },
    { 139, 255,   0 }, // 90
    { 116, 255,   0 },
    {  93, 255,   0 },
    {  70, 255,   0 },
    {  46, 255,   0 },
    {  23, 255,   0 },
    {   0, 255,   0 },  // Green (cycleable)
    {   0, 255,  23 },
    {   0, 255,  46 },
    {   0, 255,  70 },
    {   0, 255,  93 }, // 100
    {   0, 255, 116 },
    {   0, 255, 139 },
    {   0, 255, 162 },
    {   0, 255, 185 },
    {   0, 255, 209 },
    {   0, 255, 232 },
    {   0, 255, 255 },  // Cyan (cycleable)
    {   0, 232, 255 },
    {   0, 209, 255 },
    {   0, 185, 255 }, // 110
    {   0, 162, 255 },
    {   0, 139, 255 },
    {   0, 116, 255 },
    {   0,  93, 255 },
    {   0,  70, 255 },
    {   0,  46, 255 },
    {   0,  23, 255 },
    {   0,   0, 255 },  // Blue (cycleable)
    {  23,   0, 255 },
    {  46,   0, 255 }, // 120
    {  70,   0, 255 },
    {  93,   0, 255 },
    { 116,   0, 255 },
    { 139,   0, 255 },
    { 162,   0, 255 },
    { 185,   0, 255 },
    { 209,   0, 255 },
    { 232,   0, 255 },
    { 255,   0, 255 },  // Magenta (cycleable)
    { 255,   0, 232 }, // 130
    { 255,   0, 209 },
    { 255,   0, 185 },
    { 255,   0, 162 },
    { 255,   0, 139 },
    { 255,   0, 116 },
    { 255,   0,  93 },
    { 255,   0,  70 },
    { 255,   0,  46 },
    { 255,   0,  23 },
    { 128,   0,   0 },  // Dark red  140
    { 128,  12,   0 },
    { 128,  23,   0 },
    { 128,  35,   0 },
    { 128,  47,   0 },
    { 128,  58,   0 },
    { 128,  70,   0 },
    { 128,  81,   0 },
    { 128,  93,   0 },
    { 128, 105,   0 },
    { 128, 116,   0 }, // 150
    { 128, 128,   0 },  // Dark yellow
    { 116, 128,   0 },
    { 105, 128,   0 },
    {  93, 128,   0 },
    {  81, 128,   0 },
    {  70, 128,   0 },
    {  58, 128,   0 },
    {  47, 128,   0 },
    {  35, 128,   0 },
    {  23, 128,   0 }, // 160
    {  12, 128,   0 },
    {   0, 128,   0 },  // Dark green
    {   0, 128,  12 },
    {   0, 128,  23 },
    {   0, 128,  35 },
    {   0, 128,  47 },
    {   0, 128,  58 },
    {   0, 128,  70 },
    {   0, 128,  81 },
    {   0, 128,  93 }, // 170
    {   0, 128, 105 },
    {   0, 128, 116 },
    {   0, 128, 128 },  // Dark cyan
    {   0, 116, 128 },
    {   0, 105, 128 },
    {   0,  93, 128 },
    {   0,  81, 128 },
    {   0,  70, 128 },
    {   0,  58, 128 },
    {   0,  47, 128 }, // 180
    {   0,  35, 128 },
    {   0,  23, 128 },
    {   0,  12, 128 },
    {   0,   0, 128 },  // Dark blue
    {  12,   0, 128 },
    {  23,   0, 128 },
    {  35,   0, 128 },
    {  47,   0, 128 },
    {  58,   0, 128 },
    {  70,   0, 128 }, // 190
    {  81,   0, 128 },
    {  93,   0, 128 },
    { 105,   0, 128 },
    { 116,   0, 128 },
    { 128,   0, 128 },  // Dark magenta
    { 128,   0, 116 },
    { 128,   0, 105 },
    { 128,   0,  93 },
    { 128,   0,  81 },
    { 128,   0,  70 }, // 200
    { 128,   0,  58 },
    { 128,   0,  47 },
    { 128,   0,  35 },
    { 128,   0,  23 },
    { 128,   0,  12 },
    { 255, 192, 192 },  // Very light red
    { 255,  64,  64 },  // Light-medium red
    { 192,   0,   0 },  // Medium-dark red
    {  64,   0,   0 },  // Very dark red
    { 255, 255, 192 },  // Very light yellow     210
    { 255, 255,  64 },  // Light-medium yellow
    { 192, 192,   0 },  // Medium-dark yellow
    {  64,  64,   0 },  // Very dark yellow
    { 192, 255, 192 },  // Very light green
    {  64, 255,  64 },  // Light-medium green
    {   0, 192,   0 },  // Medium-dark green
    {   0,  64,   0 },  // Very dark green
    { 192, 255, 255 },  // Very light cyan
    {  64, 255, 255 },  // Light-medium cyan
    {   0, 192, 192 },  // Medium-dark cyan       220
    {   0,  64,  64 },  // Very dark cyan
    { 192, 192, 255 },  // Very light blue
    {  64,  64, 255 },  // Light-medium blue
    {   0,   0, 192 },  // Medium-dark blue
    {   0,   0,  64 },  // Very dark blue
    { 255, 192, 255 },  // Very light magenta
    { 255,  64, 255 },  // Light-medium magenta
    { 192,   0, 192 },  // Medium-dark magenta
    {  64,   0,  64 },  // Very dark magenta
    { 255,  96,  96 },  // Medium skin tone      230
    { 255, 255, 255 },  // White (cycleable)
    { 245, 245, 245 },
    { 235, 235, 235 }, 
    { 224, 224, 224 },  // Very light gray (7/8 intensity)
    { 213, 213, 213 },
    { 203, 203, 203 },
    { 192, 192, 192 },  // Light gray (3/4 intensity)
    { 181, 181, 181 },
    { 171, 171, 171 },
    { 160, 160, 160 },  // Medium-light gray (5/8 int.)   240
    { 149, 149, 149 },
    { 139, 139, 139 },
    { 128, 128, 128 },  // Medium gray (1/2 intensity)
    { 117, 117, 117 },
    { 107, 107, 107 },
    {  96,  96,  96 },  // Medium-dark gray (3/8 int.)
    {  85,  85,  85 },
    {  75,  75,  75 },
    {  64,  64,  64 },  // Dark gray (1/4 intensity)
    {  53,  53,  53 }, // 250
    {  43,  43,  43 },
    {  32,  32,  32 },  // Very dark gray (1/8 intensity)
    {  21,  21,  21 },
    {  11,  11,  11 },
    {   0,   0,   0 } // Black
};




