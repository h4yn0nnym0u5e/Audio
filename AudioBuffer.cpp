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

#include "Audio.h"
#include "AudioBuffer.h"

static const uint32_t B2M_44100 = (uint32_t)((double)4294967296000.0 / AUDIO_SAMPLE_RATE_EXACT);
static const uint32_t B2M_22050 = (uint32_t)((double)4294967296000.0 / AUDIO_SAMPLE_RATE_EXACT * 2.0);
static const uint32_t B2M_11025 = (uint32_t)((double)4294967296000.0 / AUDIO_SAMPLE_RATE_EXACT * 4.0);

/**
 * Dispose of allocated buffer by releasing it back to the heap.
 * Heap used depends on where it was allocated from: in the case of a buffer 
 * "given" to the instance, the buffer must be freed by the application
 * itself, after this function has been called.
 * \return ok
 */
AudioBuffer::result AudioBuffer::disposeBuffer()
{
	if (nullptr != buffer)
	{
		switch (bufTypeX) // free buffer, if we created it
		{
			case inHeap:
				free(buffer);
				break;

			case inExt:
				extmem_free(buffer);
				break;
				
			default:
				break;
		}
	}
	
	buffer = nullptr;
	bufSize = 0;
	bufTypeX = none;
	
	return ok;
}


/**
 * Create buffer by passing a memory pointer and size.
 * The application is responsible for managing the buffer, e.g.
 * by freeing it after disposeBuffer() has been called.
 */
AudioBuffer::result AudioBuffer::createBuffer(uint8_t* buf, //!< pointer to memory buffer
											  size_t sz)	//!< size of memory buffer
{
	disposeBuffer(); // ensure existing buffer is freed, if possible
	
	buffer = (uint8_t*) buf;	
	bufSize = sz;
	bufTypeX = given;	
	bufState = empty;

	return ok;
}


/**
 * Create buffer by passing a required size and memory type.
 * The class is responsible for managing the buffer, e.g.
 * by freeing it when disposeBuffer() has been called or an
 * instance is deleted.
 * \return ok if created, invalid if memory couldn't be allocated
 */
AudioBuffer::result AudioBuffer::createBuffer(size_t sz, //!< requested size of memory buffer
											  bufType typ)
{
	AudioBuffer::result rv = ok;
	void* buf = nullptr;

	switch (typ)
	{
		case inHeap:
			buf = malloc(sz);
			break;

		case inExt:
			buf = extmem_malloc(sz);
			break;
			
		default:
			break;
	}
	
	if (buf != nullptr)
	{
		rv = createBuffer((uint8_t*) buf,sz);
		bufTypeX = typ; // fix up buffer memory type
	}
	else
		rv = invalid;
	
	
	return rv;
}


/**
 * Read data from buffer.
 * The class deals with unwrapping the data if the requested size overlaps
 * the end of the buffer memory. It also keeps track of whether the buffer has
 * enough data to satisfy the request, and if it has become possible to refill
 * the buffer with new data.
 *
 * Data may be discarded by passing a NULL destination pointer.
 *
 * \return ok if data read and no refill needed; halfEmpty if a partial refill can be done;
 * underflow if a complete refill is needed; invalid if data was not read from the buffer
 * because the amount required was greater than that available
 */
AudioBuffer::result AudioBuffer::read(uint8_t* dest, //!< pointer to memory to copy buffer data to, or null
									  size_t bytes)  //!< amount of data required
{
	AudioBuffer::result rv = ok;
	size_t halfSize = bufSize / 2;
	bufState_e reqValid = bothValid;
	
	// figure out which part(s) of the buffer need to have data to fulfil the request
	if (nextIdx < halfSize && nextIdx + bytes <= halfSize)
		reqValid = firstValid;
	else
	{
		if (nextIdx >= halfSize && nextIdx + bytes <= bufSize) // block is in second half of buffer
			reqValid = secondValid;
	}
	
	if (bothValid == bufState || bufState == reqValid) // buffer is good enough - proceed
	{
		if (nextIdx + bytes >= bufSize) // need to do this in two chunks, or at least wrap nextIdx
		{
			if (nullptr != dest) 
			{
				memcpy(dest,buffer+nextIdx,bufSize-nextIdx);
				dest += bufSize-nextIdx;
			}
for (size_t i=0;i<bufSize-nextIdx;i++) buffer[nextIdx+i] |= 0x20;
			secondSize = 0;
			bytes -= bufSize-nextIdx; // this could be zero
			nextIdx = 0;
			
			if (bothValid == bufState)
				bufState = firstValid;
			else
				bufState = empty;
		}
		
		if (bytes > 0)
		{
			if (nullptr != dest) 
				memcpy(dest,buffer+nextIdx,bytes);
for (size_t i=0;i<bytes;i++) buffer[nextIdx+i] |= 0x20;
			
			if (nextIdx+bytes <= halfSize) // got everything from first half
				firstSize -= bytes;
			else if (nextIdx >= halfSize)
				secondSize -= bytes;
			else
			{
				firstSize  -= halfSize - nextIdx;
				secondSize -= nextIdx+bytes - halfSize;
			}
				
			nextIdx += bytes;
		}
		
		if (nextIdx >= halfSize && 
			(reqValid == bothValid || reqValid == firstValid))  // crossed the boundary
		{
			if (bothValid == bufState)
				bufState = secondValid;
			else
				bufState = empty;
		}
		
		// update return value as appropriate
		switch (bufState)
		{
			case empty:
				rv = underflow; // need full buffer read
				break;
				
			default:
				rv = halfEmpty; // need half buffer read
				break;
			
			case bothValid: // no read needed
				break;
		}			
	}
	else
		rv = invalid; // buffer was not read, insufficient data was available
	
	return rv;
}


/**
 * Check how much data remains in the buffer.
 * \return remaining data (bytes)
 */
size_t AudioBuffer::getAvailable()
{
	size_t halfSize = bufSize / 2;
	size_t rv = bufSize - nextIdx;
	
	switch (bufState)
	{
		case empty:
			rv = 0;
			break;
		
		case bothValid:
			if (nextIdx >= halfSize)
				rv += halfSize;
			break;
			
		case firstValid:
			rv -= halfSize;
			break;
			
		case secondValid:
			break;
	}
	
	rv = firstSize + secondSize;
	
	return rv;
}


/**
 * Get data on where to write next data into buffer.
 * Caller provides pointers to where the result should be; the caller should
 * then load the indicated zone with the required amount of data.
 *
 * If the required amount of data is 0 then no buffer write is needed.
 *
 * In rare circumstances only a half-buffer fill will be requested, even when 
 * the buffer is completely empty.
 */
void AudioBuffer::getNextBuffer(uint8_t** pbuf, //!< pointer to pointer returning buffer zone to fill next
								size_t* psz)	//!< pointer to available size of buffer zone to fill
{
	*psz = bufSize / 2;
	bufState_e bState = bufState;
	
	// ensure we request second half if completely empty
	// and next read will be from second half
	if (empty == bState && nextIdx >= *psz)
		bState = firstValid;
	
	switch (bState)
	{		
		case empty:
			*psz = bufSize;
			// fall through
		case secondValid:
			*pbuf = buffer;
			break;
			
		case firstValid:
			*pbuf = buffer+*psz;
			break;
			
		case bothValid:
			*psz = 0;
			break;
	}
	
	return;
}


/**
 * Signal that a required write to the buffer has been done.
 * It is assumed that the zone written will be one of the entire buffer,
 * or just the first or second half of the buffer, or a partial write
 * to the end of the first half and all of the second half.
 */
void AudioBuffer::bufferWritten(uint8_t* buf, size_t sz)
{
	size_t halfSize = bufSize / 2;
	
	if (buf == buffer) // wrote at buffer start...
	{
		if (sz == bufSize || secondValid == bufState)
		{
			bufState = bothValid; // ...whole buffer, or second half is already OK
			if (sz == bufSize)
				firstSize = secondSize = halfSize;
			else
				firstSize = sz;
		}
		else
		{
			bufState = firstValid; // ...just the first half
			firstSize = sz;
		}
	}
	else // wrote after beginning
	{
		if (sz == halfSize)  // wrote second half
		{
			if (firstValid == bufState)
				bufState = bothValid;
			else
				bufState = secondValid;
			secondSize = sz;
		}
		else // ASSUME partial fill to pre-load buffer
		{
			bufState = bothValid;
			firstSize = sz - halfSize; 	// partial
			secondSize = halfSize;	   	// full
			nextIdx = bufSize - sz;		// jump to first valid byte
		}
	}	
}

/********************************************************************************/
//#define CHARS_TO_ID(a,b,c,d) ((a<<24) | (b<<16) | (c<<8) | (d))
#define CHARS_TO_ID(a,b,c,d) ((d<<24) | (c<<16) | (b<<8) | (a))
static constexpr struct {
	  uint32_t RIFF,WAVE,fmt,data,fact;
} IDs = {CHARS_TO_ID('R','I','F','F'),
		 CHARS_TO_ID('W','A','V','E'),
		 CHARS_TO_ID('f','m','t',' '),
		 CHARS_TO_ID('d','a','t','a'),
		 CHARS_TO_ID('f','a','c','t')
		 };
 
uint16_t AudioWAVdata::parseWAVheader(File& f)
{
	uint32_t seekTo;
	RIFFhdr_t rhdr = {0};
	data_t dt = {0};
  
	chanCnt = 0;
	samples = 0;
	dataChunks = 0;
	f.seek(0);
	f.read(&rhdr,sizeof rhdr);
	if (rhdr.riff.u == IDs.RIFF && rhdr.wav.u == IDs.WAVE)
	{
		do
		{
			uint32_t tmp,b2m;
			   
			f.read(&dt.hdr, sizeof dt.hdr);
			seekTo = f.position() + dt.hdr.clen;

			switch (dt.hdr.id.u)
			{
			  case IDs.fmt:
				f.read(&dt.fmt,dt.hdr.clen);

				format = dt.fmt;
				bitsPerSample = dt.bitsPerSample; 
				chanCnt = dt.chanCnt;

				if (format == 0xFFFE) // extensible
				{
					format = dt.subFmt;
				}

				if (dt.sampleRate == 44100) {
					b2m = B2M_44100;
				} else if (dt.sampleRate == 22050) {
					b2m = B2M_22050;
				} else if (dt.sampleRate == 11025) {
					b2m = B2M_11025;
				} else {
					b2m = B2M_44100; // wrong, but we should say something
				}
				
				b2m /= chanCnt;

				if (bitsPerSample == 16) 
					b2m >>= 1;

				bytes2millis = b2m;			  
			  
				break;
			  
			  case IDs.data:
				samples += dt.hdr.clen / chanCnt / (bitsPerSample / 8);
				if (0 == dataChunks++) // first data chunk: we only allow use of one
				{
					nextAudio = f.position(); 	// audio data starts here
					audioSize = dt.hdr.clen;	// and is this many bytes
				}
				break;
			  
			  case IDs.fact:
				f.read(&tmp,sizeof tmp);
				break;
		  }
		  f.seek(seekTo);
		} while (seekTo < rhdr.flen);
	}

	f.seek(0);

	return chanCnt;
}	
