/*
 * tlv320aic3104_comms.h
 * Comms, reset and verbose mode

 * This software is published under the MIT Licence
 * R. Palmer 2025
 */

void AudioControlTLV320AIC3104::i2cBus(TwoWire *i2c)
{
	_i2c = i2c;
}

// Define the reset pin, if required. Issue before begin()
void AudioControlTLV320AIC3104::resetPin(uint8_t pin)
{
	_resetPin = pin;
	_verbose && fprintf(stderr, "Reset pin is %i\n", _resetPin);
}

void AudioControlTLV320AIC3104::reset()
{
	digitalWrite(_resetPin, LOW);
	delay(1);
	digitalWrite(_resetPin, HIGH);
	delay(100); // allow CODECS to settle			
	_resetDone = true;	
}
uint8_t AudioControlTLV320AIC3104::begin()
{
	// _i2c->setWireTimeout(AIC_I2C_TIMEOUT, true);
	pinMode(_resetPin, OUTPUT);
	digitalWrite(_resetPin, HIGH);
	delay(500); // CODECS may still be resetting after power up
	reset();
	delay(500); // CODECS and muxes will reset
#ifdef SINGLE_CODEC
	return true;
#else
	return muxProbe();
#endif
}

uint8_t AudioControlTLV320AIC3104::begin(uint8_t pin)
{
	resetPin(pin);
	return begin();
}

// Write a codec register 
// See tlv320aic3104_mux.h for mux comms
bool AudioControlTLV320AIC3104::writeRegister(uint8_t reg, uint8_t value, uint8_t codec)
{
#ifdef IGNORE_CODECS
	if(codec >= _activeMuxes * 4)
		return false;
#endif
#ifndef SINGLE_CODEC
	muxDecode(codec);
#endif
	int bytes = 0;
	//uint8_t buf[2] = { static_cast<uint8_t>(reg & 0xFF), static_cast<uint8_t>(value & 0xFF) };
//if(reg == 9) fprintf(stderr, "W9[%i] 0x%02x\n", codec,value);
	_i2c->beginTransmission(_codec_I2C_address); 
	  bytes = _i2c->write(reg); // separate writes for register number and value
	  bytes += _i2c->write(value); 
  _i2c->endTransmission(true); 		
	if(bytes != 2)
	{
		fprintf(stderr, "Failed to write register %d on I2c codec\n", reg);
		return false;
	}
	return true;
}

// Read a codec register 
// See tlv320aic3104_mux.h for mux comms
int AudioControlTLV320AIC3104::readRegister(uint8_t reg, uint8_t codec)
{
	int bytes;
#ifdef IGNORE_CODECS
	if(codec >= _activeMuxes * 4)
		return 0xFFFF; // value won't naturally occur
#endif
#ifndef SINGLE_CODEC
	muxDecode(codec);
#endif
	_i2c->beginTransmission(_codec_I2C_address); 
	 bytes = _i2c->write(reg); 
 	_i2c->endTransmission(false);  // or TLV will enter auto-increment mode and return value of reg+1
	if(bytes != 1)
		fprintf(stderr,"failed I2C read setup: reg %d on codec %i\n", reg, codec);
	bytes = _i2c->requestFrom(_codec_I2C_address, (uint8_t)1); 
	if(bytes < 1)
	{
		fprintf(stderr,"I2C data read fail on codec %i\n", codec);
		return -1;
	}	
	uint8_t val = (uint8_t)_i2c->read();
  delayMicroseconds(I2C_COMPLETE_DELAY);
	return val;
}

void AudioControlTLV320AIC3104::setVerbose(int verbosity)
{
	_verbose = verbosity;
	_verbose && fprintf(stderr, "Verbosity set to %i\n", _verbose);
}

void AudioControlTLV320AIC3104::setI2Cclock(uint32_t I2Crate)
{
	_I2Cclockrate = I2Crate;
}
AudioControlTLV320AIC3104::~AudioControlTLV320AIC3104()
{
	_isRunning = 0;
}