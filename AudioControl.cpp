/* Audio Library for Teensy 3.x and 4.x
 * Copyright (c) 2014, Paul Stoffregen, paul@pjrc.com
 * Copyright (c) 2022, Jonathan Oakley
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
#include "AudioControl.h"


TwoWire* AudioControlI2C::wires[] = {&Wire
#if defined(WIRE_IMPLEMENT_WIRE1)
,&Wire1
#if defined(WIRE_IMPLEMENT_WIRE2)
,&Wire2
#if defined(ARDUINO_TEENSY_MICROMOD)
,Wire3
#endif//  defined(ARDUINO_TEENSY_MICROMOD)
#endif // defined(WIRE_IMPLEMENT_WIRE2)
#endif // defined(WIRE_IMPLEMENT_WIRE1)
};
#define MAX_WIRE (sizeof wires / sizeof wires[0] - 1)

void AudioControlI2C::setAddress(uint8_t addr)
{
	if (addr < i2c_base) // using "step" mode
	{
		if (addr > i2c_max)
			addr = i2c_max;
		i2c_addr = i2c_base + addr * i2c_step;
	}
	else // set directly - better be sane!
		i2c_addr = addr;
}

void AudioControlI2C::setWire(uint8_t wnum, uint8_t addr)
{
	setAddress(addr);
	if (wnum > MAX_WIRE) wnum = MAX_WIRE;
	wire = wires[wnum];
}
#undef MAX_WIRE

void AudioControlI2C::setWire(TwoWire& wref, uint8_t addr)
{
	setAddress(addr);
	wire = &wref;
}



