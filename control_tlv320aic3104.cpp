/*
 * AudioControlTLV320AIC3104.cpp
 * 1 or more TLV320AIC3104 CODECS + PCA9546 I2C mux
 * Teensy Audio Library compatible
 * I2S/TDM handled elswewhere
 
	* This software is published under the MIT Licence
	* R. Palmer 2025
 */

#include "control_tlv320aic3104.h"

AudioControlTLV320AIC3104::AudioControlTLV320AIC3104(uint8_t codecs, bool useMCLK, uint8_t i2sMode, long sampleRate, int sampleLength )
{
	_codecs = codecs;
	_isRunning =  false;
	_i2c = &Wire;	
	_codec_I2C_address = AIC3104_I2C_ADDRESS;	
	_sampleLength = sampleLength;
	_usingMCLK = useMCLK;
	_i2sMode = i2sMode;
	if(codecs > 1)
		_i2sMode = AICMODE_TDM; // I2S only for one codec
	_sampleRate = sampleRate;
	_dualRate = (_sampleRate > 48000);
	_baseRate = (_sampleRate % 8000 == 0) ? 48000 : 44100;
}


// enable one or more codecs (-1 for all codecs)
// option for a reset GPIO pin, for testing or no hardware reset controller
// GPIO reset will only occur once per boot cycle
bool AudioControlTLV320AIC3104::enable(int8_t codec)
{
	bool ok;

	if(!_resetDone) // subsequent enable() calls should not reset codecs
	{
		reset();	
		delay(100); // allow enough time for muxes to stabilise
		resetCodecs();	// R8/9/10 (TDM 256 slot, slot ID, tristate DO when inactive)
		delay(100); // allow enough MCLK cycles for codecs to stabilise		
		_resetDone = true;
	}
	(_verbose > 1) && fprintf(stderr, "Enable CODEC %i\n", codec);
	if(codec > 1) // force TDM mode
		if(_i2sMode != AICMODE_TDM)
		{
			_verbose && fprintf(stderr, "Audio mode must be TDM for multiple CODECs %i\n ", codec);
			_i2sMode = AICMODE_TDM;
		}
	if(codec < 0 ) // all codecs (allow for || codec > 128
	{
		for(int i = 0; i < _codecs; i++)
		{
			ok = enableCodec(i);
			if(!ok)
			{
				_verbose && fprintf(stderr, "Codec %i not enabled\n", i);
			}
		}
		return true;	
	}
	else // a single codec
	{
		return enableCodec(codec); // single
	}
} 
void AudioControlTLV320AIC3104::resetCodecs(void)
{
	for(int i = 0; i < _codecs; i++)
	{
		// Explicit Switch to config register page 0 and soft reset
		writeRegister(0x00, 0x00, i); // code page 0
		writeRegister(0x01, 0x80, i); // soft reset
		delay(1); // reset timing?
		// PLL
		enablePll(!_usingMCLK, i);
		// Safe I2S/TDM key parameters
		writeRegister(0x08, 0x20, i); // hi-z on idle
		writeR9(i); 	// DSP mode and slot
		writeR10(i); 	// Only 16 bits implemented
	}
	delay(100);
}

// Per-codec enable
// See the TLV320AIC3104 datasheet for register details
// Note sequencing in SLAA403 p4:
//  Set samplerate and other I2S/TDM parameters prior to powering up DAC/DAC
//	The ADC and DAC must be powered down when setting the sample rate
//	The PGAs must be muted 
// 		before the ADC is powered down.
//	When selecting an input, unmute the PGAs 
//		after routing the input 
//		and powering up the ADC.
//	When selecting an output, power up and unmute the outputs 
//		after selecting the signal routing 
//		and powering up the DAC 
//		and unmuting the digital volume control.
bool AudioControlTLV320AIC3104::enableCodec(int8_t codec)
{
	writeRegister(8, 0x20, codec); 	// Put codec in hi-z DOUT idle - required for TDM
	if(readRegister(8, codec) == -1)
		return false;

		//	The ADC and DAC must be powered down when changing the sample rate.
	_isRunning = true;		// set before writing R7
		writeR7(codec);  // R7: Codec datapath
		// power up ADC/DAC after changing sample rate
		//	route the input 
		//	and power up the ADC
		//  then unmute the PGAs 
	writeRegister(19, (_inputMode << 7) + 4, codec);	// Reg 19 (0x13): ADC Left power On; LIN1/2 to PGA, soft step on, 0dB, single-ended mode
	writeRegister(22, (_inputMode << 7) + 4, codec);	// Reg 22 (0x16): ADC right power; LIN2 to PGA, soft step on, 0dB, single-ended  mode

	writeRegister(15, 0x00, codec);	// Reg 15: ADC PGA R 0dB, un-muted, LINE1 single-ended
	writeRegister(16, 0x00, codec);	// Reg 16: ADC PGA L 0dB, un-muted

	// R19 R21: differential or SE input
	inputMode(_inputMode, codec);
	writeRegister(12, (_hpfDefault << 6) | (_hpfDefault << 4), codec);		// Reg 12:  Audio Codec Digital Filter (Default = HPF and Digital Effects filter disabled)
	// Reg 107: Defaults are OK
	
	// enable Line1/2 in single-ended or differential mode	
	writeR9(codec); 									// R9: Audio Serial Data Interface Control Register B
	if (_i2sMode == AICMODE_DSP) 			// CODEC TDM slot
		writeR10(codec);								// Register 10: Audio Serial Data Interface Control Register C
		// Select the drive mode
		// Select the output signal routing 
		// power up the DAC 
		// leave DAC digital volume control muted
		// then power up and unmute the  HP (differential) and Line outputs 
	writeRegister(14, 0xC0, codec);			// Reg 14: Headset/Button Press Detection B (Fully differential AC-coupled drivers)
	writeRegister(109, _dacPower, codec);	// Reg 109: DAC Quiescent Current Adjustment, default 50% increase
	writeRegister(37, 0xC0, codec);			// Reg 37 (0x25): DAC power/driver register: DAC power on (left and right)
																			// HPLCOM = -HPLOUT 
																			// R38: HPRCOM = -HPROUT is default 
																			// R41 defaults: DACs to _L1/_R1 paths, independent volume controls
	writeRegister(40, AIC_15V, codec);	// Reg 40 (0x28): High-Power Output Stage, 1.5V, soft step
	writeRegister(42, AIC_PO_2S + AIC_PO_BG, codec);	// Reg 42 (0x2A): Output Driver Pop Reduction, 2 Secs, band gap VCM
	
	writeRegister(43, 0x80, codec);			// Reg 43 (0x2b): Left-DAC Digital Volume Control 0dB, muted 
	writeRegister(44, 0x80, codec);			// Reg 44 (0x2c): Right-DAC Digital Volume Control 
	
	// HP output - differential outputs so routing to HPxCOM is off (default)
	// writeRegister(54, 0x80, codec); 	// Reg 54: DAC_L1 to HPLCOM Volume Control Register
	// writeRegister(58, 0x08, codec);	// Reg 58: HPLCOM Output Level Control Register
	
	writeRegister(47, 0x80, codec);			// Reg 47: DAC_L1 to HPLOUT Volume Control Register
	writeRegister(64, 0x80, codec);			// Reg 64: DAC_R1 to HPROUT Volume Control Register
	
	writeRegister(51, 0x09, codec);			// Reg 51: HPLOUT Output Level Control Register
	writeRegister(65, 0x09, codec);			// Reg 65: HPROUT Output Level Control Register

	// Line output - enabled but pins not connected on reference design
	writeRegister(82, 0x80, codec);			// Reg 82 (0x52): DAC_L1 to LEFT_LOP/M Volume Control Register: routed, 0dB
	writeRegister(92, 0x80, codec);			// Reg 92 (0x5c): DAC_R1 to RIGHT_LOP/M Volume Control Register
	
	writeRegister(86, 0x09, codec);			// Reg 86 (0x56): LEFT_LOP/M Output Level Control Register: un-mute
	writeRegister(93, 0x09, codec);			// Reg 93 (0x5d): RIGHT_LOP/M Output Level Control Register
//	_verbose && fprintf(stderr, "Done enable(%i). R10 = 0x%02X\n\n", codec, readRegister(10, codec));
	return true;
}

void AudioControlTLV320AIC3104::writeR7(uint8_t codec)
{
	bool ok;
		// should set R7 to 44.1 or 48kHz base (p50)
		uint8_t val;
		val = (_baseRate == 48000) ? 0x00 : 0x80;	// Bit 7: AGC time constants
		val += (_dualRate) ? 0x60 : 0; 						// Bits 5-6 (dual rate)
		val +=  0x0A;						 									// Bits 1-4 (DAC data path): data to respective DACs 
		ok = writeRegister(7, val, codec);
}

// I2s/TDM mode and format
// Only 16-bit implemented
// re-sync not set
void AudioControlTLV320AIC3104::writeR9(uint8_t codec)		// p51
{
		uint8_t val = _sampleLength << 4;
		if(_i2sMode == AICMODE_DSP)  // probably unnecessary - only master mode? 
			val += 0x08;	// 256-clock mode
		val += _i2sMode << 6;		
		if (_reSync) 
				val += 0x07; // ADC & DAC
		writeRegister(9, val, codec);
}

// TDM slot offset
void AudioControlTLV320AIC3104::writeR10(uint8_t codec)	// p51
{
		uint8_t val = (codec << AIC_TM_SLOT_SHIFT) + AIC_FIRST_SLOT; // TDM offset in 16 bit slots, 2 per codec
		if(_i2sMode == AICMODE_TDM)
			val += AIC_TDM_OFFSET;
		writeRegister(10, val, codec);
}

// *** Untested ***
// Mute inputs and outputs, stop generating audio and power down
bool AudioControlTLV320AIC3104::stopAudio()
{
	_isRunning = false;
	for (int i = 0; i < _codecs; i++)
	{
		muxDecode(i);
		volume(0, -1, i);	// Mute the DACs
		writeRegister(15, 0x80, i);		// ADC L mute
		writeRegister(16, 0x80, i);		
		writeRegister(51, 0x08, i);		// HP OUT L mute
		writeRegister(65, 0x08, i);		
		delay(50); // wait for stepping to complete

		writeRegister(1, 0x80, i);	  // Reset codec to defaults and power down DACs and ADCs, etc
		writeRegister(8, 0x20, i); 		// Put codec DOUT in hi-z mode
	}
	return true;
}

#include "tlv320aic3104_comms.h" 
#include "tlv320aic3104_mux.h"
#include "tlv320aic3104_routeVol.h"
#include "tlv320aic3104_pll.h" 


