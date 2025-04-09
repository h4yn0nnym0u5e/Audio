/*
	TLV320AIC3104
	PLL
	*** Only tested at 44100 in TDM (BCLK inverted) and I2S (BCLK normal) modes ***
	* This software is published under the MIT Licence
	* R. Palmer 2025
*/

/* PLL Q is normally set to 2 when PLL is not used.
	 FsRef = 1/8 * CLK_IN * K * R / P
	 K = J.D			see p26.
	 in most cases K = 1 (J = 1, D = 0), P = 1; R = 8 will work (same outcome as Q = 2)
*/
unsigned long AudioControlTLV320AIC3104::setPllClkIn(long sampRate)
{
	// TDM and I2S BCLK frequencies are different, LRCLK frequencies are the same
	// Fs ref = K*R/8*P; K =J.D
	if (_i2sMode == AICMODE_TDM)
		switch (sampRate) // TDM
		{
			case 48000 : // p26
				_baseRate = 48000;
				pll.clk = 12288000; //BCLK
				pll.p = 1;
				pll.r = 1;
				pll.j = 8;
				pll.d = 0;
				break;
				
			case 44100 :
			default :
			_baseRate = 44100;
				pll.clk = 11289600;
				pll.p = 1;
				pll.r = 1;
				pll.j = 8;
				pll.d = 0;
		}
	else
		switch (sampRate) // I2S
		{
			case 48000 : 
				_baseRate = 48000;
				pll.clk = 1536000;
				pll.p = 1;
				pll.r = 4;
				pll.j = 16;
				pll.d = 0;
				break;
				
			case 44100 :
			default :
			_baseRate = 44100;
				pll.clk = 1411200;
				pll.p = 1;
				pll.r = 4;
				pll.j = 16;
				pll.d = 0;
		}
			
	return pll.clk;
}

// Set pll struct variables, except q, but don't write changes to CODECs.
int AudioControlTLV320AIC3104::setPll(uint32_t clk, uint32_t p, uint32_t r, uint32_t j,uint32_t d)
{
		if(j > 63 || j < 1)
			return 1;
		if(d  > 9999)
			return 1;
		if(p > 8 || p < 1)
			return 1;
		// if(q > 17 || q < 2) return 1; // Q is not changed
		if(r > 16) //value out of range
			return 1;
		pll.clk = clk;
		pll.p = p;
		pll.j = j;
		pll.d = d;
		// pll.q is unchanged
		setPllK();
		return 0;
}

// Write  CODEC PLL values from struct pll and enable PLL if required.
// Call after setPll()
void AudioControlTLV320AIC3104::enablePll(bool enabled, int codec)
{
	uint8_t r3 = (enabled) ? 0x80 : 0 | pll.q << 3 | pll.p;
	uint8_t r102 = (enabled)? 0x22: 0x02; // p75 - BCLK for PLLCLK_IN, MCLK for CLKDIV_IN 
	for(int i = 0; i < _codecs; i++)
	{
			writeRegister(4, pll.j << 2, codec);
			writeRegister(5, (pll.d >> 6) & 0xff , codec);
			writeRegister(6, (pll.d << 2) & 0xff, codec);
			writeRegister(11, pll.r & 0x0f, codec);
			writeRegister(102, r102, codec);	//
			writeRegister(3, r3, codec); // do this last 
	}
}

aic_pll AudioControlTLV320AIC3104::getPll()
{
	return pll;
}

float AudioControlTLV320AIC3104::setPllK()
{
	float j=pll.j;
	float d=pll.d;
	pll.k = j+d/10000.0f;
	return pll.k;
}
unsigned long AudioControlTLV320AIC3104::getPllFsRef(void)
{
	return ((pll.clk / 2048.0f) * pll.k * pll.r)/(pll.p);
}
