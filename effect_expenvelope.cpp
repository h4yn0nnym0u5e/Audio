/* Audio Library for Teensy 3.X
 * Copyright (c) 2021, Jonathan Oakley based on work which is
 * Copyright (c) 2017, Paul Stoffregen, paul@pjrc.com
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
#include "effect_expenvelope.h"



void inline AudioEffectExpEnvelope::doDelay()
{
  state = STATE_DELAY;
  count = delay_count;
  target = mult_hires - 1; // ensure we don't transition because we've "reached target"
}


void inline AudioEffectExpEnvelope::doAttack()
{
  state = STATE_ATTACK;
  count = attack_count; // wrong unless Idle, but still OK
  target = attack_target;
  factor = attack_factor; 
  factor1 = attack_factor1;
  transition_mult = EEE_ONE;  
}

void inline AudioEffectExpEnvelope::doHold()
{
  state = STATE_HOLD;
  count = hold_count;
  mult_hires = EEE_ONE;
  target = mult_hires - 1; // ensure we don't transition because we've "reached target"
  factor = 0;
  factor1 = EEE_ONE;
}


void inline AudioEffectExpEnvelope::doDecay()
{
  state = STATE_DECAY;
  count = decay_count;
  target = decay_target;
  factor = decay_factor; 
  factor1 = decay_factor1;
  transition_mult = sustain_mult;
}


void inline AudioEffectExpEnvelope::doSustain()
{
  state = STATE_SUSTAIN;
  count = 0xFFFF;
  mult_hires = sustain_mult;
  target = mult_hires - 1; // ensure we don't transition because we've "reached target"
  factor = 0;
  factor1 = EEE_ONE;
}


void inline AudioEffectExpEnvelope::doRelease()
{
  state = STATE_RELEASE;
  count = release_count; // may not apply from states other than Sustain
  target = release_target;
  factor = release_factor; 
  factor1 = release_factor1;
  transition_mult = 0;
}


void inline AudioEffectExpEnvelope::doForce()
{
}


void AudioEffectExpEnvelope::noteOn(void)
{
	__disable_irq();  /*************************************/
 
	if (state == STATE_IDLE || state == STATE_DELAY) 
	{ // (re-)start delay if delaying or idle...
    if (delay_count > 0) 
      doDelay();
  	else 
      doAttack();  // attack without delay!
	} 
	else if (state != STATE_FORCED // in middle of envelope
        && state != STATE_HOLD   // if in Hold...
        && state != STATE_ATTACK)// ...or Attack, no point re-triggering
	{
    // we used to force a brief release, but now...
    doAttack();
    // ... we think we can actually attack OK
	}
 
	__enable_irq(); /*************************************/
}


void AudioEffectExpEnvelope::noteOff(void)
{
	__disable_irq();  /*************************************/
  
	if (state != STATE_IDLE     // already silent
   && state != STATE_RELEASE  // already releasing
   && state != STATE_FORCED) 
    doRelease();
    
	__enable_irq();  /*************************************/
}


void AudioEffectExpEnvelope::update(void)
{
	audio_block_t *block;
	uint32_t *p, *end;
	uint32_t sample12, sample34, sample56, sample78, tmp1, tmp2;
  int32_t mult=0;

  do
  {
  	block = receiveWritable();
  	if (block)
	{
		if (state == STATE_IDLE) 
		{
			AudioStream::release(block);
			break;
		}
    	p = (uint32_t *)(block->data);
	}
	else
	{
		p = NULL;
		sample12 = sample34 = sample56 = sample78 = 0;
	}


	end = p + AUDIO_BLOCK_SAMPLES/2; // we're using the samples in pairs

	while (p < end) 
	{
		// we only care about the state when completing a region
		if (count == 0 || target == mult_hires) 
		{
		  switch (state) // this is current state, we're about to transition
		  {
			default:    // should never happen
			  doRelease();	// this should be safer than processing just two samples...
			  break;
			  
			case STATE_ATTACK:
			  if (hold_count > 0) 
				doHold();
			  else 
				doDecay();
			  continue;
				 
			case STATE_HOLD:
			  doDecay();
			  continue;
				
			case STATE_DECAY: 
			case STATE_RISING_DECAY: 
			  doSustain();
			  break;
				 
			case STATE_SUSTAIN:	// has no transition out, apart from noteOff()
			  count = 0xFFFF;
			  break;
				
			case STATE_RELEASE: 
			  if (mult_hires > 0) // got here before release really completed
			  {
				count = release_count; // keep going
	#if 0            
				Serial.print(mult_hires);
				Serial.print(' ');
				Serial.print(target);
				Serial.print(' ');
				Serial.print(mult);
				Serial.print(' ');
				Serial.println(transition_mult);
				Serial.println();
	#endif            
			  }
			  else
			  {
				state = STATE_IDLE;
				while (p < end) 
				{
				  if (nullptr != block)
				  {
					*p++ = 0;
					*p++ = 0;
					*p++ = 0;
					*p++ = 0;
				  }
				  else
					p += 4;
				}
			  }			 
			  break;
				 				 
			case STATE_FORCED: 
			  mult_hires = 0; // click!
			  target = mult_hires - 1; // ensure we don't transition because we've "reached target"
			  count = delay_count;
			  if (count > 0) 
			  {
				state = STATE_DELAY;
				factor = 0;
				factor1 = EEE_ONE;
			  } 
			  else 
				doAttack();
			  break;
				 
			case STATE_DELAY: 
			  doAttack();
			  continue;
		  }
		}
		
		if (STATE_IDLE == state) // already filled block, we're done here
			break;
		
		mult = mult_hires >> 14;  // 30-bit -> 16-bit
		// process 8 samples, using only mult and inc (16 bit resolution)
		if (nullptr != block)
		{
			sample12 = *p++;
			sample34 = *p++;
			sample56 = *p++;
			sample78 = *p++;
			p -= 4;
		}
		
		if (state < STATE_DYNAMIC_THRESHOLD) // stable, no need to change mult
		{
		  count--; // these are a fixed or indefinite time: do the count
		  
		  if (nullptr != block)
		  {
			tmp1 = signed_multiply_32x16b(mult, sample12);
			tmp2 = signed_multiply_32x16t(mult, sample12);
			sample12 = pack_16b_16b(tmp2, tmp1);
		  
			tmp1 = signed_multiply_32x16b(mult, sample34);
			tmp2 = signed_multiply_32x16t(mult, sample34);
			sample34 = pack_16b_16b(tmp2, tmp1);
		  
			tmp1 = signed_multiply_32x16b(mult, sample56);
			tmp2 = signed_multiply_32x16t(mult, sample56);
			sample56 = pack_16b_16b(tmp2, tmp1);
		  
			tmp1 = signed_multiply_32x16b(mult, sample78);
			tmp2 = signed_multiply_32x16t(mult, sample78);
			sample78 = pack_16b_16b(tmp2, tmp1);
		  }
		}
		else // dynamic states
		{
		  int32_t addFactor = multiply_32x32_rshift32(target,factor);
		  // mult = mult*(1 - attack) + target * attack: stabilises at "target"
		  // we use DSP instructions for speed, though we lose a couple of bits of precision
	#define MULT_STEP unsigned_saturate_rshift(mult_hires,16,(SHIFT - 16)); \
					  mult_hires = (multiply_32x32_rshift32(mult_hires,factor1) + \
									addFactor) << (32 - SHIFT); \
					  if (state < STATE_DYNAMIC_DOWN \
							  ?mult_hires >= transition_mult \
							  :mult_hires <= transition_mult) {/*count = 0;*/ target = mult_hires = transition_mult; addFactor = 0; factor1 = EEE_ONE;} \
					  
	
		  mult = MULT_STEP;
		  tmp1 = signed_multiply_32x16b(mult, sample12);
		  mult = MULT_STEP;
		  tmp2 = signed_multiply_32x16t(mult, sample12);
		  sample12 = pack_16b_16b(tmp2, tmp1);
		  
		  mult = MULT_STEP;
		  tmp1 = signed_multiply_32x16b(mult, sample34);
		  mult = MULT_STEP;
		  tmp2 = signed_multiply_32x16t(mult, sample34);
		  sample34 = pack_16b_16b(tmp2, tmp1);
		  
		  mult = MULT_STEP;
		  tmp1 = signed_multiply_32x16b(mult, sample56);
		  mult = MULT_STEP;
		  tmp2 = signed_multiply_32x16t(mult, sample56);
		  sample56 = pack_16b_16b(tmp2, tmp1);
		  
		  mult = MULT_STEP;
		  tmp1 = signed_multiply_32x16b(mult, sample78);
		  mult = MULT_STEP;
		  tmp2 = signed_multiply_32x16t(mult, sample78);
		  sample78 = pack_16b_16b(tmp2, tmp1);
			
		  if (target == mult_hires) // reached dynamic target...
			count = 0;              // ...next state
		  else
			count--; // ...safety net
		}
		
		if (nullptr != block)
		{
			*p++ = sample12;
			*p++ = sample34;
			*p++ = sample56;
			*p++ = sample78;
		}
		else
			p += 4;
		// adjust the long-term gain using 30 bit resolution (fix #102)
		// https://github.com/PaulStoffregen/Audio/issues/102
	}
//		if (p>end)
//			Serial.println("whoops...");
	if (nullptr != block)
	{
		transmit(block);
		AudioStream::release(block);
	}

  } while (0);
}

bool AudioEffectExpEnvelope::isActive()
{
	uint8_t current_state = *(volatile uint8_t *)&state;
	return current_state != STATE_IDLE;
}

bool AudioEffectExpEnvelope::isSustain()
{
	uint8_t current_state = *(volatile uint8_t *)&state;
  
	return current_state == STATE_SUSTAIN;
}


uint8_t AudioEffectExpEnvelope::getState()
{
  return *(volatile uint8_t *)&state;
}
