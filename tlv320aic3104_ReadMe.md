# Teensy Audio TLV320AIC3104 8x8 CODEC Board Control library

A Teensy Audio Control library for the Texas Instruments TLV320AIC3104 stereo channel CODEC. This CODEC is TDM-compatible as can be programmed to offset its read/write slots to anywhere in a 256 slot TDM cycle and put the DO line in a Hi-Z state when not transmitting.

The library was created specifically for the associated four-CODEC Teensy Audio board (https://github.com/palmerr23/Teensy8x8AudioBoard). As the AIC320 only supports a single I2C address, there is some code to ensure the correct CODEC is being programmed via a multiplexed I2C bus.

The library contains drivers for the PCA9546 I2C multiplexer.

It can also support a single AIC3104 in I2S mode.

## Compatibility
Arduino 4.x with Teensyduino 1.59 or later and the supplied TDMA driver.

## Single CODEC I2S operation 
I2S mode is the default for single codecs, however TDM may be selected using the i2sMode argument.

In I2S mode, the first CODEC on the mux with the lowest I2C address will be programmed. Other CODECs present will be reset on enable().

In I2S mode, avoid the more complex forms of the various function calls, accepting the default arguments where they are available.

## TDM operation with multiple CODECs
With the TDMA driver supplied with this library, two stacked boards are practical, providing 16x16 operation.

With the default Teensy Audio TDM  driver, odd numbered channel data will be corrupted.

A PCA9544 I2C multiplexer selects one of the four CODECs on each board.

There are three on-board jumpers to theoretically allow up to eight muxes (boards) to be stacked with a single Teensy. 

Boards may have any address within the range. Probing on start-up will assign audio channel TDM slots in increasing order of discovered mux addresses.

_As of Teensyduino 1.59, EVEN channel samples are not transferred correctly by the Teensy Audio TDM driver. The TDMA driver supplied with this library corrects this issue._

While up to eight boards may theoretically be stacked, there is a practical limit with the standard Teensy Audio Library TDM driver which provides a 16 x 16-bit duplex TDM interface - that is two 8x8 boards. (Note 
Alternate Audio libraries, such as that managed by Jonathan Oakley https://github.com/h4yn0nnym0u5e/Audio/tree/feature/multi-TDM, provide the ability to support more channels.

As more than two stacked boards has not been tested, it is possible that the DI and DO signals may be corrupted because of long signal paths. On each board the 0 ohm jumpers at R5 and R7 may be replaced by 47 ohm resistors.

At a hardware level, there are also jumpers on the PCB to allow alternate DI/DO pins to be used. This feature is untested and the Teensy may not be able to drive more than two boards simultaneously, due to reflections and distortion of the high frequency signals by multiple long PCB tracks. 

Please read the hardware notes in this repo before attempting to stack more than two boards or use alternate DI/DO pins.


## Available Hardware

The hardware is described at https://github.com/palmerr23/Teensy8x8AudioBoard

# Function Reference

The TDMA driver operates in the same way as the standard Teensy Audio TDM driver. A 'fixed' TDM2 driver is not provided.

### Constructor

```
AudioControlTLV320AIC3104 aic(uint8_t codecs = 1, bool useMCLK = true, uint8_t i2sMode = AICMODE_I2S,  long sampleRate = 44100, int sampleLength = 16);
```

The simplest form for a single codec defaults to I2S mode with standard teensy audio parameters:

```
AudioControlTLV320AIC3104 aic( );
```

For TDM operation the simplest form is: 

```
AudioControlTLV320AIC3104 aic(n); // (n > 1)
```

### AudioMemory( )

Two AudioMemory blocks are required for each provisioned input or output for stable operation. This is independent of the number of channels with active patchcords.

Thus, a single board (4 CODECS) will require 20 audio blocks to be allocated.

### Wire

Defining and initialising the Wire library is the responsibility of the user application. 

This must be completed before begin( ) is called.

### begin( )

Resets the CODECs and muxes and then probes for muxes.

begin( ) may be called with no arguments if the standard reset pin (22) is used.

### enable( )

Called without arguments, enable( ) configures all attached CODEC control registers.

enable(codec) may be useful when debugging hardware.

### inputMode(inputModes mode, int8_t codec)

Set single-ended (AIC_SINGLE) or differential (AIC_DIFF) input mode.

When called before enable( ), inputMode sets the default input mode and the second argument is ignored.

When called after enable( ), all CODECS (codec = -1) or a single CODEC may be affected.

In differential mode '-' inputs should be grounded for unbalanced signals to reduce noise.

When differential input signals are connected in single-ended mode, crosstalk will occur between channels due to the CODEC's internal multiplexing (see Fig 10-13, p36).

The library defaults to differential mode to avoid this issue, at a small cost to noise performance.

### inputSelect(int level, int8_t channel, int8_t codec)

The balanced inputs (L1) are used for both mic and line inputs. PGA gain is adjustable between 0 and 59.5 dB.

Compliant with the Teensy audioControl standard and the SGTL5000 implementation, this sets an input to either MIC (-59.5 dB [-62.5 dBm]) or LINE (0 dB [-2.5 dBm]).

For finer gain control, use inputLevel( ).

### inputLevel(float gainVal, int8_t channel, int8_t codec) 

and

### gain(float gainVal, int8_t channel, int8_t codec)

The two functions are equivalent, setting the maximum input level or gain of an input channel. 

The CODEC's balanced inputs (LINE1Lx and LINE1Rx) are used for both mic and line inputs. PGA gain is adjustable between 0 and 59.5 dB.

For inputLevel( ) the range is -59.9 to 0 dB.

For gain( ) the range is 0 to 59 dB.
 
Values outside these ranges are constrained. 

When called without channel and codec arguments, all codecs and channels are affected.

### setHPF(uint8_t option, int8_t channel = -1, int8_t codec = -1)
Input channel DC removal filter. These standard digital filter settings are not very useful for audio use, due to the high corner frequencies.

```
0 = off	- power on default

1 = 0.0045 Fs (198 Hz @ Fs = 44.1kHz) - library default

2 = 0.0125 Fs (551 Hz)

3 = 0.025  Fs (1102 Hz)
```

### volume(float value, int8_t channel, int8_t codec)

Sets the volume of an output channel. The range is 0.0 .. 1.0 
 
When called without channel and codec arguments, all codecs and channels are affected.

For most applications, using other means to control the output level is preferable to changing the default volume level using this function, due to the increase in digital noise.

## Hardware validation and debugging

### setVerbose(int verbosity)
Sets the level of messages on stderr. 

```
0: turns messages off.

1: will provide some error messages on startup if the hardware doesn't match the supplied number of CODECs, etc.

2: provides additional messages as hardware is enabled or values changed 
```

Should be left at the default (0) for production, as the writes may block execution if USB isn't connected.

### listMuxes( )
Useful for checking that the board jumpers are set as required.

Should be called after begin( ), where the muxes are probed and recorded.

## Examples
- Basic operation including dynamic patching of inputs and outputs
- Network transport using the VBAN protocol https://vb-audio.com/Voicemeeter/vban.htm

## CPU Load

CPU load for the basic TDM transfers is low. 

* 16 output channels driven by a single sine generator, and no inputs processed consumes 0.12% CPU on a T4.0

* Adding 2 channels of peak readings increases it to 0.35%.

* 8 channels = 0.66%

* 16 channels = 1.1%



