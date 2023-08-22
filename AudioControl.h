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

#ifndef AudioControl_h_
#define AudioControl_h_

#include <stdint.h>
#include "Wire.h"

// A base class for all Codecs, DACs and ADCs, so at least the
// most basic functionality is consistent.

#define AUDIO_INPUT_LINEIN  0
#define AUDIO_INPUT_MIC     1

class AudioControl
{
public:
	virtual ~AudioControl() = default;
	virtual bool enable(void) = 0;
	virtual bool disable(void) = 0;
	virtual bool volume(float volume) = 0;      // volume 0.0 to 1.0
	virtual bool inputLevel(float volume) = 0;  // volume 0.0 to 1.0
	virtual bool inputSelect(int n) = 0;
};

class AudioControlI2C : public AudioControl
{
	public:
		AudioControlI2C(TwoWire& _wire, uint8_t a, uint8_t b, uint8_t s, uint8_t m) :
			wire(&_wire), i2c_base(b), i2c_step(s), i2c_max(m)
			{ setAddress(a); }
		virtual ~AudioControlI2C() = default;
		void setAddress(uint8_t addr); // include-in-OSC
		void setWire(uint8_t wnum, uint8_t addr); // include-in-OSC
		void setWire(TwoWire& wref, uint8_t addr);
		void setWire(uint8_t wnum) { setWire(wnum,i2c_addr); }
		void setWire(TwoWire& wref) { setWire(wref,i2c_addr); }
	protected:
		uint8_t i2c_addr;	// configured address
		TwoWire* wire;
	private:
		uint8_t i2c_base;	// lowest valid address
		uint8_t i2c_step;	// step between addresses
		uint8_t i2c_max;	// max step value
		static TwoWire* wires[];
};

#endif
