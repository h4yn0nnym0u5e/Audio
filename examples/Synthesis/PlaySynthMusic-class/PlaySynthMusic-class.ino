// Implement a 16 note polyphonic midi player  :-)
// Compare this to the original PlaySynthMusic example
//
// Music data is read from memory.  The "Miditones" program is used to
// convert from a MIDI file to this compact format.
//
// This example code is in the public domain.

#include <Audio.h>

#include "PlaySynthMusic.h"

unsigned char *sp = score;

#define AMPLITUDE (0.2)

// allocate a wave type to each channel.
// The types used and their order is purely arbitrary.
short wave_type[16] = {
  WAVEFORM_SINE,
  WAVEFORM_SQUARE,
  WAVEFORM_SAWTOOTH,
  WAVEFORM_TRIANGLE,
  WAVEFORM_SINE,
  WAVEFORM_SQUARE,
  WAVEFORM_SAWTOOTH,
  WAVEFORM_TRIANGLE,
  WAVEFORM_SINE,
  WAVEFORM_SQUARE,
  WAVEFORM_SAWTOOTH,
  WAVEFORM_TRIANGLE,
  WAVEFORM_SINE,
  WAVEFORM_SQUARE,
  WAVEFORM_SAWTOOTH,
  WAVEFORM_TRIANGLE
};

/*==========================================================
   As exported from the Design Tool.
   This is a super-simple voice, but even a more complex one,
   with many objects and connections, similarly needs to be
   designed only once, and instantiated multiple times.

  ----------------------------------------------------------
  #include <Audio.h>
  #include <Wire.h>
  #include <SPI.h>
  #include <SD.h>
  #include <SerialFlash.h>

  // GUItool: begin automatically generated code
  AudioSynthWaveform       wav;      //xy=303,404
  AudioInputI2S            i2s1;           //xy=353,290
  AudioEffectEnvelope      env;      //xy=449,401

  AudioConnection          patchCord1{wav, env};

  // GUItool: end automatically generated code
  ----------------------------------------------------------

*/

/*
   Make a class (nearly) directly from the exported code above:
*/
class Voice
{
  public:

    // Copied straight from the Design Tool export,
    // with just one line commented out:

    // GUItool: begin automatically generated code
    AudioSynthWaveform       wav;      //xy=303,404
    //AudioInputI2S i2s1;   not needed, except to allow export from Design Tool
    AudioEffectEnvelope      env;      //xy=449,401

    AudioConnection          patchCord1{wav, env};

    // GUItool: end automatically generated code

    // Moved to constructor, to reduce initialisation code in setup():
    Voice()
    {
      // set envelope parameters, for pleasing sound :-)
      env.attack(9.2);
      env.hold(2.1);
      env.decay(31.4);
      env.sustain(0.6);
      env.release(84.5);
      // uncomment these to hear without envelope effects
      //env.attack(0.0);
      //env.hold(0.0);
      //env.decay(0.0);
      //env.release(0.0);
    }
};

// Instantiate 16 voices, one for each MIDI channel
Voice voices[16];

//==========================================================
// Mix the 16 channels down to 4 audio streams
class VoiceMixer
{
  public:
    AudioMixer4     mixer;
    AudioConnection p1, p2, p3, p4;

    // Constructor ASSUMES pVoice points to array of
    // (at least) four Voice objects!
    VoiceMixer(Voice* pVoice)
      : p1{pVoice[0].env, 0, mixer, 0},
        p2{pVoice[1].env, 0, mixer, 1},
        p3{pVoice[2].env, 0, mixer, 2},
        p4{pVoice[3].env, 0, mixer, 3}
    {}
};

// Four mixers are needed to handle 16 channels of music
VoiceMixer mixer1{voices + 0},
           mixer2{voices + 4},
           mixer3{voices + 8},
           mixer4{voices + 12};

//==========================================================
// Now create 2 mixers for the main output
AudioMixer4     mixerLeft;
AudioMixer4     mixerRight;
AudioOutputI2S  audioOut;

// Mix all channels to both the outputs
AudioConnection patchCord33{mixer1.mixer, 0, mixerLeft, 0};
AudioConnection patchCord34(mixer2.mixer, 0, mixerLeft, 1);
AudioConnection patchCord35(mixer3.mixer, 0, mixerLeft, 2);
AudioConnection patchCord36(mixer4.mixer, 0, mixerLeft, 3);
AudioConnection patchCord37(mixer1.mixer, 0, mixerRight, 0);
AudioConnection patchCord38(mixer2.mixer, 0, mixerRight, 1);
AudioConnection patchCord39(mixer3.mixer, 0, mixerRight, 2);
AudioConnection patchCord40(mixer4.mixer, 0, mixerRight, 3);

AudioConnection patchCord41(mixerLeft, 0, audioOut, 0);
AudioConnection patchCord42(mixerRight, 0, audioOut, 1);

AudioControlSGTL5000 codec;
//==========================================================

// Initial value of the volume control
int volume = 50;

void setup()
{
  Serial.begin(115200);
  //while (!Serial) ; // wait for Arduino Serial Monitor
  delay(200);

  // http://gcc.gnu.org/onlinedocs/cpp/Standard-Predefined-Macros.html
  Serial.print("Begin ");
  Serial.println(__FILE__);

  // Proc = 1.65 (2.21),  Mem = 2 (16)
  // Audio connections require memory to work.
  // The memory usage code indicates that 16 is the
  // maximum, so give it 18 just to be sure.
  AudioMemory(18);

  codec.enable();
  codec.volume(0.45);

  // reduce the gain on some channels, so half of the channels
  // are "positioned" to the left, half to the right, but all
  // are heard at least partially on both ears
  mixerLeft.gain(1, 0.36);
  mixerLeft.gain(3, 0.36);
  mixerRight.gain(0, 0.36);
  mixerRight.gain(2, 0.36);

  Serial.println("setup done");

  // Initialize processor and memory measurements
  AudioProcessorUsageMaxReset();
  AudioMemoryUsageMaxReset();
}


unsigned long last_time = millis();
void loop()
{
  unsigned char c, opcode, chan;
  unsigned long d_time;

  // Change this to if(1) for measurement output every 5 seconds
  if (1) {
    if (millis() - last_time >= 5000) {
      Serial.print("Proc = ");
      Serial.print(AudioProcessorUsage());
      Serial.print(" (");
      Serial.print(AudioProcessorUsageMax());
      Serial.print("),  Mem = ");
      Serial.print(AudioMemoryUsage());
      Serial.print(" (");
      Serial.print(AudioMemoryUsageMax());
      Serial.println(")");
      last_time = millis();
    }
  }

  // Volume control
  //  uncomment if you have a volume pot soldered to your audio shield
  /*
    int n = analogRead(15);
    if (n != volume) {
    volume = n;
    codec.volume((float)n / 1023);
    }
  */

  // read the next note from the table
  c = *sp++;
  opcode = c & 0xF0;
  chan = c & 0x0F;

  if (c < 0x80) {
    // Delay
    d_time = (c << 8) | *sp++;
    delay(d_time);
    return;
  }
  if (*sp == CMD_STOP) {
    for (chan = 0; chan < 10; chan++) {
      // This should really be a noteKill() method of the Voice class:
      voices[chan].env.noteOff();
      voices[chan].wav.amplitude(0);
    }
    Serial.println("DONE");
    while (1)
      ;
  }

  // It is a command

  // Stop the note on 'chan'
  if (opcode == CMD_STOPNOTE) {
    // This should really be a noteOff() method of the Voice class:
    voices[chan].env.noteOff();
    return;
  }

  // Play the note on 'chan'
  if (opcode == CMD_PLAYNOTE)
  {
    unsigned char note = *sp++;
    unsigned char velocity = *sp++;

    // This should really be a noteOn() method of the Voice class:
    AudioNoInterrupts();
    voices[chan].wav.begin(AMPLITUDE * velocity2amplitude[velocity - 1],
                           tune_frequencies2_PGM[note],
                           wave_type[chan]);
    voices[chan].env.noteOn();
    AudioInterrupts();

    return;
  }

  // replay the tune
  if (opcode == CMD_RESTART) {
    sp = score;
    return;
  }
}
