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
 (c) Frank B
*/

#if defined(__IMXRT1062__)

#ifndef imxr_hw_h_
#define imxr_hw_h_

#define IMXRT_CACHE_ENABLED 2 // 0=disabled, 1=WT, 2= WB

#include <Arduino.h>
#include <imxrt.h>
#include <DMAChannel.h>

void set_audioClock(int nfact, int32_t nmult, uint32_t ndiv,  bool force = false); // sets PLL4

class SAIconfig
{
    public:
        enum class SAIcfg {none, I2S, TDM, SPDIF, PT8211, PDM}; // possible uses for SAI
        int which{0}; // pre-computed at instantiation: 1 or 2 (could add 3 in future)

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

        struct DMAisrInfo_t 
        {
            void* instance;
            void  (*isr)(void*);
        };

        void configSAI(SAIcfg cfg, double fs, bool only_bclk, int channels, bool rx, uint32_t extra);
        void configDMA(SAIcfg cfg, void* instance, void (*isr)(void*), size_t bufSz, volatile void* regAddr, int regSz, int regStep, bool rx);
	    static void isr1(void);
	    static void isr2(void);

        IMXRT_SAI_t& sai;
        static DMAisrInfo_t DMAisrInfo[2]; // derived class DMA ISR info

    protected:  // accessible from derived DAM ISR
       	DMAChannel dma;
	    uint32_t* buffer;

    public:
        SAIconfig(IMXRT_SAI_t& _sai) 
            : which{&_sai == &IMXRT_SAI1 ? 1 : 2}, sai{_sai}
            {}
        void configSAItx(SAIcfg cfg, double fs, bool only_bclk = false, int channels = 2, uint32_t extra = 0U)
            { configSAI(cfg, fs, only_bclk, channels, false, extra); }
        void configSAIrx(SAIcfg cfg, double fs, bool only_bclk = false, int channels = 2, uint32_t extra = 0U)
            { configSAI(cfg, fs, only_bclk, channels, true, extra); }
        void configDMAtx(SAIcfg cfg, void* instance, void (*isr)(void*), size_t bufSz, volatile void* regAddr, int regSz, int regStep)
            {configDMA(cfg, instance, isr, bufSz, regAddr, regSz, regStep, false); }
        void configDMArx(SAIcfg cfg, void* instance, void (*isr)(void*), size_t bufSz, volatile void* regAddr, int regSz, int regStep)
            {configDMA(cfg, instance, isr, bufSz, regAddr, regSz, regStep, true); }


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

#endif

#else
//No IMXRT - just provide watermark
#define IMXRT_CACHE_ENABLED 0
class SAIconfig
{
        IMXRT_SAI_t& sai;
    public:
        SAIconfig(IMXRT_SAI_t& _sai) : sai(_sai) {}

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
#endif
	
