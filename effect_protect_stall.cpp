/* Audio Library for Teensy 4.x
 * Copyright (c) 2024, Jonathan Oakley
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

#if defined(__IMXRT1062__)
#include "effect_protect_stall.h"

/*
 * Permanently allocate enough blocks to provide output to
 * the hardware in the future, even if a bug subsequently
 * starves the system of blocks. 
 *
 * Must be run exactly once during the lifetime of the object.
 */
void AudioEffectProtectStall::grabPermanentBlocks(void)
{
	for (int i=0;i<CHANNELS;i++)
		permanent_blocks[i] = allocate();
}


/*
 * Fill our permanent blocks with known-safe values
 */
void AudioEffectProtectStall::fillPermanentBlocks(void)
{
	for (int j=0;j<CHANNELS;j++)
		if (nullptr != permanent_blocks[j]) // check for unavailable
		{
			for (int i=0;i<AUDIO_BLOCK_SAMPLES;i++)
			{
				switch (j)
				{
					case 0: // X
					case 1: // Y
						permanent_blocks[j]->data[i] = -16384; // (i + (i>=AUDIO_BLOCK_SAMPLES/4 && i<AUDIO_BLOCK_SAMPLES*3/4)?(AUDIO_BLOCK_SAMPLES/2-2*i)
						break;

					case 3: // Z
						permanent_blocks[j]->data[i] = 0;
						break;

					case 4: // R
					case 5: // G
					case 6: // B
						permanent_blocks[j]->data[i] = RGBsafeValue;
						break;

					case 7: // blanking
						permanent_blocks[j]->data[i] = blankSafeValue;
						break;
				}
			}
		}
}


bool galvosAreRunning(audio_block_t** blocks, int threshold) __attribute__((weak));
bool galvosAreRunning(audio_block_t** blocks, int threshold)
{
	return true;
}


/*
 * Check to see if galvos have been stopped for too long
 */
bool AudioEffectProtectStall::stallCheck(audio_block_t** blocks)
{
	bool running = galvosAreRunning(blocks, stallThreshold);
	
	if (running)  // OK for now
		lastMoved = millis();
		
	if (millis() - lastMoved >= stallTimeout)
		running = false;
	
	digitalWriteFast(LED_BUILTIN,!running);
	
	return running;
}

void AudioEffectProtectStall::update(void)
{
	audio_block_t* blocks[CHANNELS];
	
	if (nullptr == permanent_blocks[0]) // no safe blocks yet...
	{
		grabPermanentBlocks();	// ...make them...
		fillPermanentBlocks();	// ... and fill them
	}
	
	// get the input data
	for (int i = 0; i < CHANNELS; i++)
		blocks[i] = receiveReadOnly(i);
	
	// do protection here:
	if (!stallCheck(blocks)) // stalled...
	{
		for (int i=3;i<CHANNELS;i++) // ...leave XYZ as they are...
		{
			if (nullptr != blocks[i])
				release(blocks[i]);
			blocks[i] = permanent_blocks[i]; // ...emit safe RGB blocks
		}
	}
	
	// send the output data
	for (int i = 0; i < CHANNELS; i++)
	{
		transmit(blocks[i],i);
		if (nullptr != blocks[i] 
		 && permanent_blocks[i] != blocks[i])
		 release(blocks[i]);
	}
	
	updateCount++;
	if ((millis() - lastUpdate) > (1000.0f * AUDIO_BLOCK_SAMPLES / AUDIO_SAMPLE_RATE_EXACT + 1))
		updatesOK = false;
	lastUpdate = millis();
}

#endif // defined(__IMXRT1062__)
