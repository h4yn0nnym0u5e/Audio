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
#include "analyze_event.h"
extern uint32_t stuff[];

void AudioAnalyzeEvent::update(void)
{
	audio_block_t *block;

	// the input connection just serves to ensure update()
	// is called, maybe at a specific point (not sure why
	// that'd be needed at the moment, but The Future..)
stuff[13]++;
	block = receiveReadOnly();
stuff[13]++;
	if (block)	// data received...
	{
stuff[13]+=100;
		transmit(block);	// ... may as well forward it
stuff[13]+=100;
		release(block);
stuff[13]+=100;
	}
	
stuff[13]++;
	
	count++;				// count updates of this object
stuff[13]++;
	tstamp = micros();		// log when it occurred
stuff[13]++;
	if (NULL != this->_function)	// trigger the event, if it exists
	{
stuff[13]+=1000;
		this->triggerEvent(0,this);	// with this object as its data (use EvRef.getData())
stuff[13]+=1000;
	}
stuff[13]++;
}


void AudioAnalyzeEvent::setEventFn(EventResponderFunction evFn,void* ctxt = NULL)
{
	attach(evFn);		// function to be called when event triggered, with "this" as its parameter...
	setContext(ctxt); 	// ...and the supplied context pointer
}

