/*
	tlv320aic3104_mux.h
	1:4 TCA9546/PCA9546 I2C mux 
*/

bool AudioControlTLV320AIC3104::muxWrite(uint8_t muxAddress, uint8_t value) 
{
	uint8_t error;
  _i2c->beginTransmission(muxAddress);
  _i2c->write(value);
  error = _i2c->endTransmission(true);
  return (error == 0);
}


uint8_t AudioControlTLV320AIC3104::muxRead(uint8_t muxAddress)
{ 
	uint8_t val;
  _i2c->requestFrom(muxAddress, (uint8_t)1);
  val = _i2c->read();
  return val;
	
}

uint8_t AudioControlTLV320AIC3104::muxProbe() 
{
	_activeMuxes = 0;
	delay(100);
	int result;
	uint8_t addr;
	for(int i = 0; i < MUX_MAX; i++)
	{
		_mux_I2C_address[_activeMuxes] = 0;
		addr = TCA9546_BASE_ADDRESS + i;
		_i2c->beginTransmission(addr); 
		result = _i2c->endTransmission(true);
		if(result  == 0)
		{
			_mux_I2C_address[_activeMuxes] = TCA9546_BASE_ADDRESS + i;
			_verbose && fprintf(stderr, "Found mux at 0x%2X \n", TCA9546_BASE_ADDRESS + i); 
			_activeMuxes++;
		}
		else
		{
			if(result  == 4)
				fprintf(stderr, "Bus error on probe: 0x%2X \n", addr); 			
		}		
		delay(1);
	}
	if(_activeMuxes * 4 != _codecs && _verbose)
		fprintf(stderr, "Error: Supplied number of codecs %i does not match discovered %i\n", _codecs, _activeMuxes * 4); 
	return _activeMuxes;
}


void AudioControlTLV320AIC3104::listMuxes() 
{
	fprintf(stderr, "%i active muxes found\n", _activeMuxes);
	for(int i = 0; i < MUX_MAX; i++)
		if(_mux_I2C_address[i] > 0)
			fprintf(stderr, "Mux %i = 0x%2X\n", i, _mux_I2C_address[i]);
		
}


void AudioControlTLV320AIC3104::muxDecode(uint8_t codec) 
{
	if(codec == _lastCodec)
		return;
	uint8_t board = codec >> 2;
	uint8_t channel = codec & 0x03;
	uint8_t mask = 1 << channel; 
	
	delayMicroseconds(I2C_COMPLETE_DELAY); // ensure last I2C transaction is complete
	if(board == _lastBoard)
	{
		muxWrite(_mux_I2C_address[board], mask); 
		delayMicroseconds(I2C_LONG_DELAY); // settle bus
	}
	else
		for(int i = 0; i < _codecs / 4; i++) // 1:4 board select
		{
			delayMicroseconds(I2C_LONG_DELAY); // settle bus
			if(i == board)
				muxWrite(_mux_I2C_address[i], mask); 
			else
				muxWrite(_mux_I2C_address[i], 0); // disable all channels
		delayMicroseconds(I2C_LONG_DELAY); // settle bus
		}
	_lastCodec = codec;
	_lastBoard = board;
}

