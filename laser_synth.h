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

#if defined(__IMXRT1052__) || defined(__IMXRT1062__)
#if !defined(_laser_synth_h_)
#define _laser_synth_h_

// Set this to S16 or U16 to determine black and white levels
// S16 => black = -32768, white = 32767
// U16 => black = 0,      white = 32767
#define RGB_LEVELS S16
								
#define CONVERT_RGB_U16(n) ((int) n * 257) / 2
#define CONVERT_RGB_S16(n) ((int) n * 257) - 32768
#define BLACK_U16 0
#define BLACK_S16 (-32768)
#define CONVERT_RGB_(l,n) CONVERT_RGB_##l(n)
#define xCONVERT_RGB_(l,n) CONVERT_RGB_(l,n)
#define CONVERT(n) xCONVERT_RGB_(RGB_LEVELS,n)
#define BLACK_LEVEL CONVERT(0)
#define BLANKING(n) CONVERT(((n)?255:0))

#if !defined(htons)
#define htons(x) (((((x) >> 8) & 0xFF) | ((x) << 8)) & 0xFFFF)
#endif // !defined(htons)

#endif // !defined(_laser_synth_h_)
#endif // defined(__IMXRT1052__) || defined(__IMXRT1062__)
