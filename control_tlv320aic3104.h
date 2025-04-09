/*
 * control_tlv320aic3104.h
 
 *** This code is still ALPHA and should not be used in any critical applications.

 * TLV320AIC3104 codecs and PCA9546 I2C muxes
 * Teensy Audio compatible (I2S and TDM)
 * I2S/TDM managed elsewhere
 
 * It is the user's responsibility to create the AudioControlTLV320AIC3104 object.
 * It is the user's responsibility to issue Wire.begin() and set the I2C clock rate.
 
 * Only one control object required for single or multiple (with I2C multiplexed ) CODECS on a board. 
 
 * Solo or multiple codecs
 * Solo mode can use the simpler function formats, or the mode complex ones with codec == 0
 * Multi code assumes 4 CODECS per board and a maximum of 4 boards
 * Each board muxes I2C_0 lines (1:4) 
 * TDM slot number == CODEC number
 
 * The codec identifier is only required in function calls when when multiple CODECS are used.
 * Using the default codec value (0) will only set the external multiplexing logic if more than one codec has been enabled in audioMode().
 
 * Some page references are to an older version of the AIC3104 datasheet. Generally adding 4 to the page number give the correct reference.
 
 * See additional notes in other files.
 
 * This software is published under the MIT Licence
 * R. Palmer 2025
 */

#ifndef _TLV320AIC3104_H
#define _TLV320AIC3104_H

#define DEFAULT_RESET_PIN 22
#define PROD_I2S false			// true = turn off I2C muxing for a single codec
#define I2CSPEED 100000
#define MUX_MAX 8		// PCA9548 has 3 address pins
#define IGNORE_CODECS	// don't read or write to codecs that aren't provisioned: i.e. when # codecs specified > discovered muxes * 4
//#define SINGLE_CODEC	// no multiplexers - just one CODEC

#include <Arduino.h> 
//#include "input_tdmA.h"
//#include "output_tdmA.h"
#include "AudioControl.h"
#include <Wire.h>

#define AIC3104_I2C_ADDRESS 	0x18 	
#define AIC_I2C_TIMEOUT				50		// (microSecs) wait before abandoning I2C read
// I2C bus delays may be needed with more than two boards
#define I2C_COMPLETE_DELAY 		0			// (microSecs)delay before changing MUX to ensure last I2C transaction is complete
#define I2C_LONG_DELAY 		0					// delay after switching MUXes

#define DEFAULT_SAMPLERATE 		44100	// Teensy Audio standards
#define DEFAULT_SAMPLE_LENGTH	16

#define AICMODE_I2S			0x0
#define AICMODE_DSP			0x1
#define AICMODE_RJ			0x2
#define AICMODE_LJ			0x3
#define AICMODE_TDM			AICMODE_DSP
#define AIC_TDM_OFFSET	1			// Teensy Audio: invert BCLK, offset slots by 1 BCLK
#define AIC_FIRST_SLOT	0 		// shift first CODEC for testing later slots
#define AIC_TDM_CLOCKS	256		// 16 x 16-bit slots
#define AIC_TM_SLOT_SHIFT 5 	// (2 x 16 = 32 bits)

#define AICWORD_16			0x0
#define AICWORD_20			0x1
#define AICWORD_24			0x2
#define AICWORD_32			0x3

// Register 11 overflow bits
#define AIC_OVF_ADC_L		0x80
#define AIC_OVF_ADC_R		0x40
#define AIC_OVF_DAC_L		0x20
#define AIC_OVF_DAC_R		0x10

// Multi CODEC/board mode
// 16 x 16 bit slots in Teensy TDM
#define AIC_MAX_BOARDS 				2 		// Also number of board enable pins 
#define AIC_CODECS_PER_BOARD	4			// 2 bits (also mux channels)
#define AIC_MUX_PINS 					2 		// mux SCL: n = SQRT(AIC_CODECS_PER_BOARD)
#define AIC_MUX_MASK					0x03	
#define AIC_MAX_CODECS 				(AIC_CODECS_PER_BOARD * AIC_MAX_BOARDS)
#define AIC_MAX_CHANNELS 			(AIC_MAX_CODECS * 2)

#define AIC_CODEC_CLOCK_SOURCE 0 	// pll.dIV default (R101, p70)
#define AIC_PLL_SOURCE			0			// MCLK default(R102, p70)
#define AIC_CLIDIV_SOURCE		0			// MCLK default 

#define AIC_MIC_USES_LINE_INPUT 		// reference hardware uses same input for both
#define AIC_MIC_GAIN_DEFAULT 120		// steps
#define AIC_LINE_GAIN_DEFAULT 0 		// steps
#define AIC_MAX_PGA_GAIN		(59.5)	// dB

#define AIC_HPF_DISABLE			0x00		// Pwr on default. P29/52. R12. Bits R:4-5 and L:6-7
#define AIC_HPF_0045				0x01  	// Set as default to remove DC offset: -3dB @ 0.2 Hz 
#define AIC_HPF_0125				0x02		// -3dB @ 0.5 Hz
#define AIC_HPF_025					0x03		// most aggressive -3dB @ 1 Hz

#define AIC_PO_BG						0x02		// drive power off VCM output to band gap ref (p36)
#define AIC_15V							0x40		// HP VCM 1.5V (p639)
#define AIC_PO_100MS				0x60		// 100 mS HP power on
#define AIC_PO_2S						0x90		// 2 Sec HP power on (avoid pop: slow charge output caps)

#define AIC_ALL_CODECS 			-1

#define TCA9546_BASE_ADDRESS 					 0x70

enum dacPwr{DAC_DEF = 0, DAC_50 = 0x40, DAC_100 = 0xc0};
// Input modes
enum inputModes {AIC_SINGLE, AIC_DIFF};
struct aic_pll {
	unsigned long clk, p, r, j, d, q;
	float 	k;
};

// Implements Teensy Audio AudioControl class
class AudioControlTLV320AIC3104  : public AudioControl
{

public:
	AudioControlTLV320AIC3104(uint8_t codecs = 1, bool useMCLK = true, uint8_t i2sMode = AICMODE_I2S,  long sampleRate = 44100, int sampleLength = 16); // default: standard Teensy Audio I2S, one CODEC
	~AudioControlTLV320AIC3104();	

	// BOARD controls
	// Issue these before enable()
	void resetPin(uint8_t pin); // GPIO reset will only occur once per boot cycle. Not required for reference hardware
	void reset();	
	uint8_t begin(); // returns number of boards found
	uint8_t begin(uint8_t pin); // resetPin + begin
	//void audioMode();
	
	// MUX
	bool muxWrite(uint8_t muxAddress, uint8_t value);
	uint8_t muxRead(uint8_t muxAddress);
	uint8_t muxProbe(); // look for mux chips and assign codec id's in order returns number of muxes found.
	void listMuxes();
	
	// PLL 
	unsigned long setPllClkIn(long sampRate = 44100); // potted Pll values - values will usually work if PLL is required (!useMCLK)
	int setPll(uint32_t clk, uint32_t p, uint32_t r, uint32_t j,uint32_t d);
	aic_pll getPll(); // set specific variables, but do not update codec.
	unsigned long getPllFsRef(); // return calculated fsRef for assigned pll values	
	void i2cBus(TwoWire *i2c); // Wire.begin is user responsibility 
	void setI2Cclock(uint32_t I2Crate); // other devices may reset the clock rate
	
/* CODEC
	* default arguments set all channels in all CODECs
	* codec == -1 sets all declared codecs
	* channel == -1 sets both channels in a CODEC
	*/
	bool enableCodec(int8_t codec); // enable a single codec - no reset
	bool enable(int8_t codec);	// Enable all CODECs I2S defaults
	bool enable() { return enable(-1); } 
	bool disable() { return stopAudio(); }// will disable all if in multi mode
	bool stopAudio();	
	bool DACpower(dacPwr pwr, int8_t codec = -1);  // Run before enable()
	
/* DAC - HPOUT and Line outs are controlled in tandem
 * LOP/ROP not used on default Teensy hardware
 */
	bool volume(float vol, int8_t channel, int8_t codec) ;			// volume 0.0 to 1.0 (log scale)
	bool volume(float vol) { return volume(vol, -1, -1); } // AudioControl.h
	bool enableLineOut(bool enable, int8_t codec = -1); 
	
/* ADC
 * When inputMode, inputlevel or setHPF commands are issued before enable() they set the defaults.
 * After enable() they changes whichever channels/codecs are selected.
 */
	bool inputMode(inputModes mode, int8_t codec = -1); // 0 = single ended; 1 = differential	
	
/* PGA gain (input level) can be controlled directly, or by using inputSelect for LINE/MIC 
 * gainVal is in dB 
 * gain: 0.. 59.5
 * inputLevel: -59.5 .. 0
 */
	bool gain(float gainVal, int8_t channel = -1, int8_t codec = -1); 	// 0..60dB gain range
	bool inputLevel(float gainVal, int8_t channel = -1, int8_t codec = -1); // 0 to -59.5dB
	bool inputLevel(float gainVal){ return inputLevel(gainVal, -1, -1); } // see AudioControl.h

	bool inputSelect(int level, int8_t channel = -1, int8_t codec = -1); 		// n=0: Line level, PGA gain = 0dB; n=1: Mic level, PGA gain = 59.5dB See inputLevel();
	bool inputSelect(int level) { return inputSelect(level, -1, -1); } 		// see AudioControl.h
	
	// HPF will remove the DC offset from signal (P29 and P52)
	bool setHPF(uint8_t option, int8_t channel = -1, int8_t codec = -1); // when issued before the codecs are enabled, all channels and codecs are set. Default is Fo = 0.0045 Fs

	void setVerbose(int verbosity); // 0 = off Diagnostics. 1 and 2 are increasingly verbose. Beware, this will block if USB Serial isn't connected.

	// only used for debugging
	void muxDecode(uint8_t codec);
	int readRegister(uint8_t reg, uint8_t codec);
protected:
	TwoWire *_i2c = &Wire;
	bool volumeInteger(int gainStep, int8_t channel = -1, int8_t codec = -1);

	bool gainInteger(uint8_t gainStep, int8_t channel = -1, int8_t codec = -1); // in PGA steps (p 50)
	uint8_t gainToStep(float gain);  // converts dB gain to register setting

	
private:
	void resetCodecs(void); // reset all the codecs to a known state
	bool writeRegister(uint8_t reg, uint8_t value, uint8_t codec);
	void enablePll(bool enabled = false, int codec =  -1); // used only by enable()
	float setPllK();
	uint8_t calcStep(float vol);
	void writeR7(uint8_t codec);
	void writeR9(uint8_t codec);
	void writeR10(uint8_t codec);
	bool enableHpOut(bool enable, int8_t codec = -1);
	
	uint8_t _resetPin	= DEFAULT_RESET_PIN;
	bool _resetDone = false;
	uint8_t _codec_I2C_address; 
	uint8_t _mux_I2C_address[MUX_MAX]; 
	uint8_t _activeMuxes = 0;
	
	inputModes _inputMode = AIC_DIFF;	
	uint8_t _i2sMode = AICMODE_I2S;
	uint32_t _sampleRate = 44100;	
	uint32_t _baseRate = 44100;
	bool _dualRate = false; // true is untested
	int _sampleLength = 16;			// only 16 bits tested
	bool _usingMCLK = true;
	dacPwr _dacPower = DAC_DEF;	
	uint8_t _hpfDefault = AIC_HPF_0045; // enabled at lowest freq

	int _lastCodec = -1; 	// used by muxDecode (force change on first use)
	int _lastBoard = -1;
	int8_t _codecs = 1; // default to single CODEC mode	
	bool _reSync = false;	
	bool _isRunning;
	int _verbose = 0;

	uint32_t _I2Cclockrate = 400000; 
	// defaults R3..R7, R11: P=8, R=1, J=1, D=0, Q=2, (K=0.0)
	aic_pll pll = {11289600, 1, 1, 8, 0, 2, 8.0}; // TDM 44100 defaults. {clk, p, r, j, d, q, k};
};

#endif /* _TLV320AIC3104_H */
