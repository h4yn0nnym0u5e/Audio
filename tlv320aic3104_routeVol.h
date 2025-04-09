/*
TLV320AIC3104
Routing and volume control
*/

/*************** INPUTS **************/
// Input mode - single ended or differential
// In differential mode '-' inputs should be grounded for unbalanced signals to reduce noise.
// both channels must have the same mode (R19, p 56)
bool AudioControlTLV320AIC3104::inputMode(inputModes mode, int8_t codec)
{
	_inputMode = mode; 
	if (!_isRunning) // if issued before enable() this just sets the default input mode 
		return false;
	
	uint8_t start, end, xmode;
	xmode = (mode << 7) + 0x04;
	if(codec < 0)
	{
		start = 0;
		end = _codecs;
	}
	else
	{
		start= codec;
		end = codec+1;
	}

	for(int i = start; i < end; i++)
	{
		writeRegister(19, xmode, i);
		writeRegister(22, xmode, i);
	}
	return true;
}


// Provided for compatibility with AudioControl.h
// inputSelect(AUDIO_INPUT_MIC) will set this to AIC_MIC_GAIN_DEFAULT and may be overridden here. AUDIO_INPUT_LINEIN sets the gain to AIC_LINE_GAIN_DEFAULT.
// Two-options for PGA gain (simpler version of inputLevel() )
// 0 = line; 1 = mic
// the Line differential input is used for both mic/line in reference hardware
bool AudioControlTLV320AIC3104::inputSelect(int level, int8_t channel, int8_t codec)
{
#ifdef AIC_MIC_USES_LINE_INPUT
	gainInteger((level==1)? AIC_MIC_GAIN_DEFAULT : AIC_LINE_GAIN_DEFAULT, channel, codec);
#else
	if(_isVerbose) 
		fprintf(stderr, "Alternate mic input not inplemented\n");
#endif

	return true;
}

// Set the maximum input level in dB (-60 to 0 dB)
bool AudioControlTLV320AIC3104::inputLevel(float gain, int8_t channel, int8_t codec) 	
{
	return gainInteger(gainToStep(-gain), channel, codec);
}

// PGA has 0..60dB gain range
bool AudioControlTLV320AIC3104::gain(float gain, int8_t channel, int8_t codec) 	
{
	return gainInteger(gainToStep(gain), channel, codec);
}

// calculate the PGA step from dB  
uint8_t AudioControlTLV320AIC3104::gainToStep(float gain)
{
	gain = constrain(gain, 0, AIC_MAX_PGA_GAIN); // p54
	return 0.5 + (gain * 0x77)/AIC_MAX_PGA_GAIN; // 0.5 db steps from 0 to 59.5dB
}

// Set the PGA gain in steps
bool AudioControlTLV320AIC3104::gainInteger(uint8_t gainStep, int8_t channel, int8_t codec)
{

	uint8_t start, end;
	if(codec < 0)
	{
		start = 0;
		end = _codecs;
	}
	else
	{
		start= codec;
		end = codec+1;
	}
	for(int i = start; i < end; i++)
	{
		
		if(channel <= 0)
			if(!writeRegister(15, gainStep & 0x7f, i))
				return false;
		if(channel < 0 || channel & 1)
			writeRegister(16, gainStep & 0x7f, i);
	}
	return true;
}


/************ OUTPUTS **********************/
// Change DAC quiescent current
// Run before enable()
bool AudioControlTLV320AIC3104::DACpower(dacPwr pwr, int8_t codec)
{
	 _dacPower = pwr;
	return true;
}

// DAC PGA and HP output level
// For Teensy Audio, DAC digital volume always at 0dB (p60)
// For the Teensy Audio reference hardware, differential mode HP out is used due to its greater drive ability.  
// DAC Line and HP outs are set up in enable()
// Line outs are enabled but not used by the reference hardware

// vol float 0..1
bool AudioControlTLV320AIC3104::volume(float vol, int8_t channel, int8_t codec)
{
	vol = constrain(vol, 0.0, 1.0);
	uint8_t volStep = calcStep(vol);
	uint8_t DACmute = 0;
	if(vol < .0001)
			DACmute = 0x80;
	uint8_t start, end;
	if(codec < 0)
	{
		start = 0;
		end = _codecs;
	}
	else
	{
		start= codec;
		end = codec+1;
	}
	
	for(int i = start; i < end; i++)
	{
		
		if(channel <= 0) // LEFT
		{
			if(!writeRegister(47, volStep  | 0x80, i)) // HP
				return false;
			writeRegister(82, volStep | 0x80, i); // L_OP
			writeRegister(43, DACmute, i); // DAC (mute vol ~0)
		}
		if(channel < 0 || channel & 1) //RIGHT
		{
			writeRegister(64, volStep | 0x80, i); // HP
			writeRegister(92, volStep | 0x80, i); // R_OP
			writeRegister(44, DACmute, i); 		// DAC
		}
	}
	return true;

}

// logarithmic slope
uint8_t AudioControlTLV320AIC3104::calcStep(float vol)
{
	int8_t vs2 = -40.0 * log10(vol);  // 0.5 db steps
	return vs2 & 0x7f;
}

bool AudioControlTLV320AIC3104::enableLineOut(bool enable, int8_t codec)
{
	char value;
	if(enable)
		value = 0x09; // output level control: 0dB, not muted, powered up
	else 
		value = 0x08; // output level control: muted

	// LEFT_LOP
	if(!writeRegister(0x56, value, codec))
		return false;
	// RIGHT_LOP
	writeRegister(0x5D, value, codec);

	return true;
}

/* When issued before the codecs are enabled, the default is changed for all channels and codecs.
 * Digital effects filter is ignored and disabled.
 * Option values
	00: ADC high-pass filter disabled
	01: ADC high-pass filter –3-dB frequency = 0.0045 × ADC fS
	10: ADC high-pass filter –3-dB frequency = 0.0125 × ADC fS
	11: ADC high-pass filter –3-dB frequency = 0.025 × ADC fS
*/
bool AudioControlTLV320AIC3104::setHPF(uint8_t option, int8_t channel, int8_t codec)
{
	_hpfDefault = option & 0x03;
	if(!_isRunning)
		return true;
	
	uint8_t start, end;	
	if(codec < 0) // single or all codecs
	{
		start = 0;
		end = _codecs;
	}
	else
	{
		start = codec;
		end = codec+1;
	}
	
	uint8_t value = 0;
	if(channel <= 0) 								// left, right or both channels
		value = _hpfDefault << 6 ;		// L
	if(channel < 0 || channel & 1)
		value += _hpfDefault << 4;		// R
	
	for(int i = start; i < end; i++)
	{
		writeRegister(12, value, i);
	}
	return true;
}

