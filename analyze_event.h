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

#ifndef analyze_event_h_
#define analyze_event_h_

#include "EventResponder.h"
#include "Arduino.h"
#include "AudioStream.h"

class AudioAnalyzeEvent : EventResponder, public AudioStream
{
public:
	AudioAnalyzeEvent(void) : AudioStream(1, inputQueueArray),
	  count(0) {}
	// Destructor:
	// AudioStream stops IRQs, unlinks, enables IRQs
	// EventResponder stops IRQs, detaches, enables IRQs
	~AudioAnalyzeEvent(void) {
		active = false;				// prevents crash during destruction (?!)
		//destructorDisableNVIC(); 	// prevents crash during destruction (?!): a bit brutal, stops everything
		EventResponder::_function = NULL;
		};
	void update(void);
	uint32_t getCount(void) {return count;}
	uint32_t getMicros(void) {return tstamp;}
	void* getContext(void) {return EventResponder::getContext();}
	void* getData(void) {return EventResponder::getData();}
	void setEventFn(EventResponderFunction evFn,void* context);
private:
	volatile uint32_t count;			//!< count of audio updates
	uint32_t tstamp;		//!< timestamp of last audio update
	audio_block_t *inputQueueArray[1]; //!< dummy "input queue" so we can wire it up
};

#endif
