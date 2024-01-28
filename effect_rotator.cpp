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

#include "effect_rotator.h"

void AudioEffectRotator::update(void)
{
  audio_block_t* blockX, *blockY, *blockR;

  blockX = receiveWritable(0);
  blockY = receiveWritable(1);
  blockR = receiveReadOnly(2);

  if (revCalled) // change direction of rotation
  {
    if (nullptr != blockR) // can ensure reverse starts at same point
    {
      int16_t start;
      int16_t angle = blockR->data[0];

      if (isReversed) // currently reversed
      {
        start = revOffset - angle;
        revOffset = start - angle;
      }
      else
      {
        start = revOffset + angle;
        revOffset = start + angle;
      }     
    }
    isReversed = !isReversed;
    revCalled = false;
  }

  // We need real XY data, even if one is silent.
  // If both are silent, we can just exit.
  if (nullptr == blockX && nullptr != blockY)
  { 
    blockX = allocate();
    if (nullptr != blockX)
      memset(blockX->data, 0, sizeof blockX->data);
  }
  
  if (nullptr == blockY && nullptr != blockX)
  { 
    blockY = allocate();
    if (nullptr != blockY)
      memset(blockY->data, 0, sizeof blockY->data);
  }

  if (nullptr != blockX)
  {
    if (nullptr != blockY)
    {
      if (nullptr != blockR) // we have rotation - do it
      {
        for (int i=0;i<AUDIO_BLOCK_SAMPLES;i++)
        {
          int32_t val1, val2, val3, val4;
          int32_t sinI, cosI;
          int16_t angle = blockR->data[i]; // ±32k
          if (isReversed)
            angle = -angle;
          angle += revOffset; // will over/underflow as angle is int16_t            
          int index = angle / 256; // -128 ..  127
          uint32_t scale = angle & 0xFF, XYval;
          index += 128; // 0 .. 255
          
          // these values are int16_t, ±32k
          // we've shifted to anti-phase, so negate the results
          val1 = -AudioWaveformSine[index];
          val2 = -AudioWaveformSine[index+1];
          sinI = val1 * (256 - scale) + val2 * scale; // 24 bit

          index += 64;
          if (index >= 256)
            index -= 256;
          val1 = -AudioWaveformSine[index];
          val2 = -AudioWaveformSine[index+1];
          cosI = val1 * (256 - scale) + val2 * scale;

          // pack XY value
          XYval  = blockX->data[i] << 16;
          XYval |= blockY->data[i] & 0xFFFF;

          // make 24-bit intermediate results
          val1 = signed_multiply_32x16t(cosI,XYval); // X*cos(R)
          val2 = signed_multiply_32x16b(cosI,XYval); // Y*cos(R)
          val3 = signed_multiply_32x16t(sinI,XYval); // X*sin(R)
          val4 = signed_multiply_32x16b(sinI,XYval); // Y*sin(R)

          // 16-bit final rotation result
          blockX->data[i] = (val1 - val4) >> 7;
          blockY->data[i] = (val2 + val3) >> 7;
        }
      }
      transmit(blockX,0);
      transmit(blockY,1);
      
      release(blockY);
    }
    release(blockX);
  }
  if (nullptr != blockR) release(blockR);
}


//=========================================================================
void AudioMixerSummer::update(void)
{
  audio_block_t* block = nullptr, *toAdd;

  for (int i=0;i<3;i++)
  {
    toAdd = receiveReadOnly(i);
    if (nullptr != toAdd)
    {
      if (nullptr == block)
      {
        block = allocate();
        if (nullptr != block)
          memcpy(block->data,toAdd->data, sizeof block->data);
      }
      else
      {
        // simple add, allowing over/underflow - good for rotation!
        for (int k=0;k<AUDIO_BLOCK_SAMPLES;k++)
          block->data[k] += toAdd->data[k];
      }
      release(toAdd);
    }
  }

  if (nullptr != block)
  {
    transmit(block,0);
    release(block);
  }
}
