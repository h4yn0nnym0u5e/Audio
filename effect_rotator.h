/* Audio Library 
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

#if !defined(_effect_rotator_h_)
#define _effect_rotator_h_

#include <Arduino.h>          // github.com/PaulStoffregen/cores/blob/master/teensy4/Arduino.h
#include <AudioStream.h>      // github.com/PaulStoffregen/cores/blob/master/teensy4/AudioStream.h
#include <arm_math.h>         // github.com/PaulStoffregen/cores/blob/master/teensy4/arm_math.h
#include <utility/dspinst.h>

// waveforms.c
//extern "C" {
extern "C" const int16_t AudioWaveformSine[257];
//}

class AudioEffectRotator : public AudioStream
{
    audio_block_t *inputQueueArray[3];   // X, Y, angle 
    void update(void);
    int revOffset; // used to ensure reversed rotation starts at old angle
    bool isReversed;
    bool revCalled;
    
  public:
    AudioEffectRotator() 
      : AudioStream(3, inputQueueArray), 
        revOffset(0), isReversed(false), revCalled(false)
      {}
    void reverse(bool flag) {if (flag != isReversed) revCalled = true; }
};


class AudioMixerSummer : public AudioStream
{
    audio_block_t *inputQueueArray[4];   // like AudioMixer4
    void update(void);
    
  public:
    AudioMixerSummer() 
      : AudioStream(4, inputQueueArray)
      {}
};



#endif // !defined(_effect_rotator_h_)
