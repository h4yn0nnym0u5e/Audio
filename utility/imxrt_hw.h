/* Audio Library for Teensy 3.X
 * Copyright (c) 2019, Paul Stoffregen, paul@pjrc.com
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
/*
 (c) Frank B, Jonathan O
*/

#ifndef imxr_hw_h_
#define imxr_hw_h_

#include <Arduino.h>
#include <DMAChannel.h>

/************************************************/
// Used for development ONLY
#define protected public // Danger!!!!!!!
#define private   public // Danger!!!!!!!
/************************************************/

/*
 * Stuff that both KinetisK and iMXRT1062 have
 */
class SAIbase
{
    public:
        enum class SAIcfg {none, I2S, TDM, SPDIF, PT8211, PDM} cfg; // possible uses for SAI
        int which{0}; // pre-computed at instantiation: 1 or 2 (could add 3 in future)

    private:
        struct DMAisrInfo_t 
        {
            void* instance;
            void  (*isr)(void*);
        };

        void configDMA(void* instance, void (*isr)(void*), size_t bufSz, volatile void* regAddr, int regSz, int regStep, bool rx);
	    static void isr1(void);
	    static void isr2(void);

        static DMAisrInfo_t DMAisrInfo[2]; // derived class DMA ISR info

    protected:  // accessible from derived DMA ISR
       	DMAChannel dma{false};
	    uint32_t* buffer;

    public:
        SAIbase(SAIcfg _cfg, int _which) 
            : cfg(_cfg), which{_which}
            {}

        void configDMAtx(void* instance, void (*isr)(void*), size_t bufSz, volatile void* regAddr, int regSz, int regStep)
            {configDMA(instance, isr, bufSz, regAddr, regSz, regStep, false); }
        void configDMArx(void* instance, void (*isr)(void*), size_t bufSz, volatile void* regAddr, int regSz, int regStep)
            {configDMA(instance, isr, bufSz, regAddr, regSz, regStep, true); }


// Set FIFO watermarks to keep FIFO as full
// as possible, in case of DMA contention	
        static const int FIFOwatermark =
        #if defined(__IMXRT1062__)
            31 // Teensy 4.x
        #elif defined(__MK20DX128__)
            3 // Teensy 3.0
        #else
            7 // Teensy 3.1 / 3.2 / 3.5 / 3.6 (unused by LC)
        #endif
            ;		 
};

#if defined(__IMXRT1062__)

#define IMXRT_CACHE_ENABLED 2 // 0=disabled, 1=WT, 2= WB

#include <imxrt.h>

void set_audioClock(int nfact, int32_t nmult, uint32_t ndiv,  bool force = false); // sets PLL4

class SAIconfig : public SAIbase
{
    private:
        struct settings_t
        {
            // TCR2
            uint32_t div:8; // bit clock divide (from master clock; mclk / (div+1) / 2)
            uint32_t bcp:1; // bit clock polarity; 0 = active high
            // TCR4
            uint32_t frsz:5; // frame size (words - 1)
            uint32_t sywd:5; // sync width (bit clocks - 1)
            uint32_t fse:1; // frame sync early
            uint32_t fsp:1; // frame sync polarity
            // TCR5
            uint32_t wnw:5; // bits/word - 1
            // clock pins
            uint32_t mclk:1;
            uint32_t bclk:1;
            uint32_t lrclk:1;
            // 29 bits
        };


        void configSAI(SAIcfg cfg, double fs, bool only_bclk, int channels, bool rx, uint32_t extra);

        IMXRT_SAI_t& sai;
    public:
        SAIconfig(IMXRT_SAI_t& _sai, SAIcfg _cfg = SAIcfg::none) 
            : SAIbase(_cfg, &_sai == &IMXRT_SAI1 ? 1 : 2),
              sai{_sai}
            {}

        void configSAItx(SAIcfg cfg, double fs, bool only_bclk = false, int channels = 2, uint32_t extra = 0U)
            { configSAI(cfg, fs, only_bclk, channels, false, extra); }
        void configSAIrx(SAIcfg cfg, double fs, bool only_bclk = false, int channels = 2, uint32_t extra = 0U)
            { configSAI(cfg, fs, only_bclk, channels, true, extra); }

        void configSAItx(double fs, bool only_bclk = false, int channels = 2, uint32_t extra = 0U)
            { configSAI(cfg, fs, only_bclk, channels, false, extra); }
        void configSAIrx(double fs, bool only_bclk = false, int channels = 2, uint32_t extra = 0U)
            { configSAI(cfg, fs, only_bclk, channels, true, extra); }

};

#else // KINETISK

//No IMXRT - provide watermark and DMA only
#define IMXRT_CACHE_ENABLED 0
#define IMXRT_SAI1 1
//#define IMXRT_SAI2 2 // should never happen!
class SAIconfig : public SAIbase
{
        IMXRT_SAI_t& sai;
    public:
        SAIconfig(int _sai, SAIcfg _cfg = SAIcfg::none) 
            : SAIbase(_cfg, _sai)
        {}
};
#endif // hardware type
#endif // ndef imxr_hw_h_

	
