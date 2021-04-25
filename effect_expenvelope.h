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

#ifndef effect_expenvelope_h_
#define effect_expenvelope_h_
#include "Arduino.h"
#include "AudioStream.h"
#include "utility/dspinst.h"
#include <math.h>

// computes limit((uval << lshift), 2**bits)
static inline int32_t unsigned_saturate_rshift(int32_t val, int bits, int rshift) __attribute__((always_inline, unused));
static inline int32_t unsigned_saturate_rshift(int32_t val, int bits, int rshift)
{
#if defined (__ARM_ARCH_7EM__)
  int32_t out;
  asm volatile("usat %0, %1, %2, asr %3" : "=r" (out) : "I" (bits), "r" (val), "I" (rshift));
  return out;
#elif defined(KINETISL)
  int32_t out, max;
  out = val >> rshift;
  max = 1 << (bits - 1);
  if (out >= 0) {
    if (out > max - 1) out = max - 1;
  } else {
    out = 0;
  }
  return out;
#endif
}


#define SAMPLES_PER_MSEC (AUDIO_SAMPLE_RATE_EXACT/1000.0f)
#define SHIFT 30
#define EEE_ONE (1L << SHIFT) // scale to unity gain at high resolution
#define MAX_MULT 65535    // maximum 16-bit multiplier value
#define TF 0.95f          // target factor for exponential: switch state here, we'll never get to 1.00!

class AudioEffectExpEnvelope : public AudioStream
{
public:
  // constructor - start with a sensible set of defaults
	AudioEffectExpEnvelope() : AudioStream(1, inputQueueArray) {
		state = STATE_IDLE;
		delay(0.0f);  // default values...
		attack(10.5f); // 10.5ms = 463 samples
		hold(2.5f);
		decay(35.0f);
    sustain(0.5f);
		release(300.0f);
	}

 
 /********* action functions ***************************************************************/
  void noteOn();
	void noteOff();
  
 /********* settings functions ***************************************************************/
  // These updated settings functions take into account any adjustments made while the
  // enevlope is in the relevant state, and attempt to adjust the ongoing process
  // in a sensible fashion.
	void delay(float milliseconds) 
	{
    uint32_t old_delay_count = delay_count;
    
    __disable_irq();
   delay_count = milliseconds2count(milliseconds);

    if (STATE_DELAY == state)
    {
      if (count > delay_count) // should have finished already!
        count = 0;
      else // new delay count is different, and there's still time left
        count = delay_count - (old_delay_count - count);
    }
    __enable_irq();
	}
	
	void attack(float milliseconds, float target_factor = TF) 
	{
		attack_count = milliseconds2count(milliseconds);
    set_factors(milliseconds,EEE_ONE,target_factor,
                &attack_count,&attack_factor,&attack_factor1,&attack_target,NULL);
		if (attack_count == 0) 
		  attack_count = 1;

    // is attack active? change if so
    if (STATE_ATTACK == state)
    {
      target = attack_target;
      factor = attack_factor; 
      factor1 = attack_factor1;
      //transition_mult = sustain_mult;  // should already be this...
      count = attack_count; // too much, but level trigger should catch it
    }   
    __enable_irq();
	}

 
	void hold(float milliseconds) 
	{
    uint32_t old_hold_count = hold_count;
    
    __disable_irq();
		hold_count = milliseconds2count(milliseconds);

    if (STATE_HOLD == state)
    {
      if (count > hold_count) // should have finished already!
        count = 0;
      else // new hold count is different, and there's still time left
        count = hold_count - (old_hold_count - count);
    }
    __enable_irq();
	}

 
	void decay(float milliseconds, float target_factor = TF) 
	{
    set_factors(milliseconds,EEE_ONE - sustain_mult,target_factor,
                &decay_count,&decay_factor,&decay_factor1,&decay_target,&decay_tf);
    decay_target = EEE_ONE - decay_target;
    decay_ms = milliseconds;
    
    // is decay active? change if so
    if (STATE_DECAY == state)
    {
      target = decay_target;
      factor = decay_factor; 
      factor1 = decay_factor1;
      transition_mult = sustain_mult;  // should already be this...
      count = decay_count; // too much, but level trigger should catch it
    }    
    
    __enable_irq();
	}

 
	void sustain(float level) 
	{
    int32_t old_sustain_mult = sustain_mult;
    
		if (level < 0.0f) 
		  level = 0;
		else 
		  if (level > 1.0f) 
		    level = 1.0f;

    __disable_irq();
    
		sustain_mult = level * EEE_ONE;
    // sustain level changed, need to re-calculate
    decay(decay_ms,decay_tf);
    release(release_ms,release_tf);

    // is decay active? change if so
    if (STATE_DECAY == state)
    {
      if (mult_hires >= sustain_mult) // haven't got down that far yet
      {
        target = decay_target;
        factor = decay_factor; 
        factor1 = decay_factor1;
        count = decay_count; // too much, but level trigger should catch it
      }
      else // new sustain level is higher than we've decayed to!
      {
        float milliseconds = decay_ms * (sustain_mult - mult_hires) / (EEE_ONE - old_sustain_mult);
        
        set_factors(milliseconds,sustain_mult - mult_hires,decay_factor,
                    &count,&factor,&factor1,&target,NULL);
        target += mult_hires;
        transition_mult = sustain_mult;  
        state = STATE_RISING_DECAY;
      }
    }    
    __enable_irq();
	}

 
	void release(float milliseconds, float target_factor = TF) 
	{
    set_factors(milliseconds,sustain_mult,target_factor,
                &release_count,&release_factor,&release_factor1,&release_target,&release_tf);
    release_target = sustain_mult - release_target;
    release_ms = milliseconds;
    
    // is release active? change if so
    if (STATE_RELEASE == state)
    {
      target = release_target;
      factor = release_factor; 
      factor1 = release_factor1;
	  }

    __enable_irq();
	}
	
	 //ElectroTechnique 2020 - close the envelope to silence it
	void close(){
	 __disable_irq();
		  mult_hires = 0;//Zero gain
		  //inc_hires = 0;//Same as STATE_DELAY
		  state = STATE_IDLE;
	  __enable_irq();
	}

   
 /********* information functions ***************************************************************/
 uint8_t getState();
	bool isActive();
	bool isSustain();
	using AudioStream::release;
	virtual void update(void);
  
private:
  enum stateList {
  // static states, envelope not changing
  STATE_IDLE, 
  STATE_DELAY, 
  STATE_HOLD,  
  STATE_SUSTAIN, 
  // dynamic states, envelope changing
  STATE_DYNAMIC_THRESHOLD, // placeholder, not a "real" state
  // changing upwards...
  STATE_ATTACK,  
  STATE_RISING_DECAY, // special: in Decay, but Sustain level has been changed to above current level
  //... and downwards
  STATE_DYNAMIC_DOWN, // placeholder, not a "real" state
  STATE_DECAY, 
  STATE_RELEASE, 
  STATE_FORCED,  
  STATE_INVALID  
  } ;
  /*
   * Compute and set the precalculated factors required for a dynamic portion of the 
   * envelope waveform (attack, decay, release).
   * 
   * LEAVES INTERRUPTS DISABLED because the calling function usually has some tidying up
   * to do before variables are in a consistent state: the caller MUST re-enable 
   * interrupts before returning, or mayhem WILL occur!
   */
  void set_factors(float milliseconds,int32_t change, float target_factor,
                   uint32_t* count,int32_t* factor,int32_t* factor1,int32_t* target, float* tf_record)
  {
      uint32_t c = milliseconds2count(milliseconds);
      if (0 == c)
        c = 1;

      // limit to vaguely-sane values
      if (target_factor > 0.9999f)
        target_factor = 0.9999f;
      if (target_factor < 0.5f)
        target_factor = 0.5f;

      __disable_irq(); /******************* interrupts disabled! *************************/
      if (tf_record)
        *tf_record = target_factor;
      *count = c;
      *factor = (int32_t)(-log(1-target_factor)*EEE_ONE/c/8);
      *factor1 = EEE_ONE - *factor;
      *target = change / target_factor;   
  }

  
	uint32_t milliseconds2count(float milliseconds) 
	{
    if (milliseconds < 0.0f)  // negative times not allowed
      milliseconds = 0.0f;
    //if (milliseconds > 100.0f) // this should be OK
    //  milliseconds = 100.0f;
		uint32_t c = ((uint32_t)(milliseconds*SAMPLES_PER_MSEC)+7)>>3;
		//if (c > 65535) c = 65535; // allow up to 11.88 seconds (comment ASSUMES 44kHz sample rate here!)
		return c;
	}
 
	audio_block_t *inputQueueArray[1];
	// state
	stateList  state;      // idle, delay, attack, hold, decay, sustain, release, forced
	uint32_t count;      // how much time remains in this state, in 8 sample units
	int32_t  mult_hires; // attenuation, 0=off, 0x40000000=unity gain
	int32_t  transition_mult;  // change envelope state when we hit this value
  int32_t target,factor,factor1;

  // state transitions
  void doDelay();
  void doAttack();
  void doHold();
  void doDecay();
  void doSustain();
  void doRelease();
  void doForce();


	// settings
	uint32_t delay_count;
	uint32_t attack_count;
  uint32_t hold_count;
	uint32_t decay_count;
  float decay_ms; // needed to re-calculate if sustain level changed
  float decay_tf;
	int32_t  sustain_mult;
	uint32_t release_count;
  float release_ms; // needed to re-calculate if sustain level changed
  float release_tf;

  int32_t attack_factor;  // fraction of change we add each time
  int32_t attack_factor1; // 1-fraction, precomputed
  int32_t attack_target;  // where change will end

  int32_t decay_factor;  // fraction of change we add each time
  int32_t decay_factor1; // 1-fraction, precomputed
  int32_t decay_target;  // where change will end

  int32_t release_factor;  // fraction of change we add each time
  int32_t release_factor1; // 1-fraction, precomputed
  int32_t release_target;  // where change will end
};

#undef SAMPLES_PER_MSEC
#endif // effect_expenvelope_h_
