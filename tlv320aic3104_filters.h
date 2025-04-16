/*
TLV320AIC3104
Filters
 *** UNTESTED ***
	* This software is published under the MIT Licence
	* R. Palmer 2024
*/
/*
// vol float 0..1
bool hpFilter(bool filterOn, int8_t codec)
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
calculate NO,N1, D1 and write Reg65-76 - p26
		writeRegister(12, 0x40, i); // enable filter
	
		}
	}
	return true;

}
*/
//
// Enable ADC DC offset removal in CODEC hardware (High-Pass filter)
// Datasheet reference: TLV320AIC3104, SLAS510F - FEB 2007, Rev DEC 2016
// CODEC 1-pole Filter.  Datasheet calls it HPF but it can be any 1-pole filter
// realizable by the following the transfer function:
//           n0 + n1 * z^-1
// H(z) =  ------------------    (10.3.3.2.1 - pg 26)
//           d0 - d1 * z^-1
// d0 =  32768 //defined by hardware (presumably fixed-point unity)
// For the high-pass filter case (bilinear transform method),
// d1 = n0 = -n1
// d1 = d0*e^(-2*pi*fc/fs) = 32768*e^(-2*pi*8/44100) = 32730.7, round up
// d1 =  32731 = 0x7FDB
// n0 =  32731 = 0x7FDB
// n1 =  -32731 = 0x8025 //2s complement
//
//    NOTE: Increasing the sampling rate will increase the filter cut-off proportionately.
//          The selected cut-off should be acceptable up to 96 kHz sampling rate.

/*
16 bit pre-calculated bi-quad coefficients calculated with  TIBQ.exe (https://www.ti.com/tool/COEFFICIENT-CALC)
		10 hz		20 Hz		50 Hz
N0	0x7FE7	0x7FD0	0x7F8A
N1	0x8019	0x8030	0x8076
D1	0x7FD0	0x7FA1	0x7F16
*/

struct bi_quad {
	int freq;
	uint8_t coeff[6];	// N0 MSB, N0 LSB, N1 MSB, N1 LSB, D1 MSB, D1 LSB
}
bq[3] = {{10, {0x7F, 0XE7, 0X80, 0X19, 0X7F, 0XD0}},
		 {20, {0x7F, 0XD0, 0X80, 0X30, 0X7F, 0XA1}},
		 {50, {0x7F, 0X8A, 0X80, 0X76, 0X7F, 0X16}}
				};
void AudioControlTLV320AIC3104::HPF(int freq, int8_t channel, int8_t codec)
{
	int cst, cend, f;

	switch (freq)
	{
		case 0:
			f = 999;
			break;
		case 10:
			f = 0;
			break;
		case 20:
			f = 1;
			break;
		case 50:
			f = 2;
			break;
		default: 
			f = 0;
	}
	if(codec < 0)
	{
		cst = 0;
		cend = _codecs;
	}
	else
	{
		cst = codec;
		cend = cst + 1;
	}
	if( f < 100) // only need to program coefficients if HPF is being turned on 
	{
		setRegPage(1, codec); // ADC HPF coefficient registers are in Reg Page 1
		Serial.printf("Set HPF row %i for codecs %i to %i, channel %i\n", f, cst, cend, channel);
		for(int cod = cst; cod < cend; cod++)
			for(int i = 0; i < 6; i++)
			{
				if(channel & 1)
					writeRegister(65+i, bq[f].coeff[i], cod);
				if(channel & 2)
					writeRegister(71+i, bq[f].coeff[i], cod);
			}
		setRegPage(0, codec); // back to Page 0
	}
	Serial.printf("%s ADC HPF for codecs %i to %i, channel %i\n", (f < 100) ? "ENABLE" : "DISABLE", cst, cend, channel);
	uint8_t val = 0x30 | (channel & 3) << 6;	// top two bits + reserved (p77)
	for(int cod = cst; cod < cend; cod++)
	{
				writeRegister(107, val, cod); // use coefficients instead of defaults (p29)
				if(f < 100)
					writeRegister(12, 0xf0, cod); // turn on HPF
				else 
					writeRegister(12, 0, cod); // turn off HPF
	}
}


/*
int AudioControlTLV320AIC3104::configureDCRemovalIIR(bool enable, int8_t codec){

	//Explicit Switch to config register page 0:
	if(writeRegister(0x00, 0x00, codec))	//Page 1/Register 0: Page Select Register
		return 1;

	//
	//  Config Page 0 commands
	//
	// Digital filter register: 
	if(enable)
		writeRegister(0x0C, 0x50, codec);	// enable HPF on L&R Channels		
	else
	{
		writeRegister(0x0C, 0x00, codec);	// disable HPF on L&R Channels
		return 0; // nothing else to do
	}
	
	writeRegister(0x6B, 0xC0, codec);	// HPF coeff select register: Use programmable coeffs

	//Switch to config register page 1:
	writeRegister(0x00, 0x01, codec);	//Page 1/Register 0: Page Select Register

	//
	//  Config Page 0 commands
	//
	//Left Channel HPF Coeffiecient Registers
	//N0:
	writeRegister(0x41, 0x7F, codec);	// Page 1/Register 65: Left-Channel ADC High-Pass Filter N0 Coefficient MSB Register
	writeRegister(0x42, 0xDB, codec);	//Page 1/Register 66: Left-Channel ADC High-Pass Filter N0 Coefficient LSB Register
	//N1:
	writeRegister(0x43, 0x80, codec);	//Page 1/Register 67: Left-Channel ADC High-Pass Filter N1 Coefficient MSB Register
	writeRegister(0x44, 0x25, codec);	//Page 1/Register 68: Left-Channel ADC High-Pass Filter N1 Coefficient LSB Register
	//D1:
	writeRegister(0x45, 0x7F, codec);	//Page 1/Register 69: Left-Channel ADC High-Pass Filter D1 Coefficient MSB Register
	writeRegister(0x46, 0xDB, codec);	//Page 1/Register 70: Left-Channel ADC High-Pass Filter D1 Coefficient LSB Register
	//Right Channel HPF Coeffiecient Registers
	//N0:
	writeRegister(0x47, 0x7F, codec);	//Page 1/Register 71: Right-Channel ADC High-Pass Filter N0 Coefficient MSB Register
	writeRegister(0x48, 0xDB, codec);	//Page 1/Register 72: Right-Channel ADC High-Pass Filter N0 Coefficient LSB Register
	//N1:
	writeRegister(0x49, 0x80, codec);	//Page 1/Register 73: Right-Channel ADC High-Pass Filter N1 Coefficient MSB Register
	writeRegister(0x4A, 0x25, codec);	//Page 1/Register 74: Right-Channel ADC High-Pass Filter N1 Coefficient LSB Register
	//D1:
	writeRegister(0x4B, 0x7F, codec);	//Page 1/Register 75: Right-Channel ADC High-Pass Filter D1 Coefficient MSB Register
	writeRegister(0x4C, 0xDB, codec);	//Page 1/Register 76: Right-Channel ADC High-Pass Filter D1 Coefficient LSB Register
	//Explicitly restore to config Page 0
	writeRegister(0x00, 0x00, codec);	//Page 1/Register 0: Page Select Register
	return 0;
}
*/
