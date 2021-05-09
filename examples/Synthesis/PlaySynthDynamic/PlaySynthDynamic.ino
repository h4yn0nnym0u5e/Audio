// Implement a 16 note polyphonic midi player  :-)
//
// Music data is read from memory.  The "Miditones" program is used to
// convert from a MIDI file to this compact format.
//
// This example code is in the public domain.
 
#include <Audio.h>
#include <Wire.h>
#include <SD.h>
#include <SPI.h>
#include <SerialFlash.h>

#include "PlaySynthMusic.h"
#include "voice.h"

unsigned char *sp = score;

#define AMPLITUDE (0.2)
#define COUNT_OF(x) (sizeof x / sizeof x[0])

// Allow 16 waveforms, one for each MIDI channel
SynthVoice *waves[16] = {NULL};

// Record the patch selected for each channel
int8_t wave_patch[16] = {0};

// Route each waveform through a patchcord
AudioConnection* waveCords[16]={NULL};

// Four mixers are needed to handle 16 channels of music
AudioMixer4 mixArray[4];

// Now create 2 mixers for the main output
AudioMixer4     mixerLeft;
AudioMixer4     mixerRight;
AudioOutputI2S  audioOut;

// Mix all channels to both the outputs
AudioConnection patchCord33(mixArray[0], 0, mixerLeft, 0);
AudioConnection patchCord34(mixArray[1], 0, mixerLeft, 1);
AudioConnection patchCord35(mixArray[2], 0, mixerLeft, 2);
AudioConnection patchCord36(mixArray[3], 0, mixerLeft, 3);
AudioConnection patchCord37(mixArray[0], 0, mixerRight, 0);
AudioConnection patchCord38(mixArray[1], 0, mixerRight, 1);
AudioConnection patchCord39(mixArray[2], 0, mixerRight, 2);
AudioConnection patchCord40(mixArray[3], 0, mixerRight, 3);
AudioConnection patchCord41(mixerLeft, 0, audioOut, 0);
AudioConnection patchCord42(mixerRight, 0, audioOut, 1);

AudioControlSGTL5000 codec;

// Initial value of the volume control
int volume = 50;

extern unsigned long _heap_end;
extern char *__brkval;
uint32_t FreeMem(){ // for Teensy 3.0
  char* p = (char*) malloc(10000); // size should be quite big, to avoid allocating fragment!
  free(p);
  return (char *)&_heap_end - p; // __brkval;
}

void setup()
{
  Serial.begin(115200);
  //while (!Serial) ; // wait for Arduino Serial Monitor
  delay(200);
  
// http://gcc.gnu.org/onlinedocs/cpp/Standard-Predefined-Macros.html
  Serial.print("Begin ");
  Serial.println(__FILE__);
  
  // Proc = 12 (13),  Mem = 2 (8)
  // Audio connections require memory to work.
  // The memory usage code indicates that 10 is the maximum
  // so give it 12 just to be sure.
  AudioMemory(18); // LOL: had to leave this...
  
  codec.enable();
  codec.volume(0.45);

  // reduce the gain on some channels, so half of the channels
  // are "positioned" to the left, half to the right, but all
  // are heard at least partially on both ears
  mixerLeft.gain(1, 0.26);
  mixerLeft.gain(3, 0.56);
  mixerRight.gain(0, 0.26);
  mixerRight.gain(2, 0.26);

  // create at startup to avoid heap fragmentation?
  for (uint8_t chan=0;chan<COUNT_OF(waveCords);chan++)
    waveCords[chan] = new AudioConnection(); // create new, but we won't destroy it

  Serial.println("usage created deleted active freeMem");
  
  // Initialize processor and memory measurements
  AudioProcessorUsageMaxReset();
  AudioMemoryUsageMaxReset();
}


unsigned long last_time = millis();
uint8_t lastEnv0=0;
static uint8_t keepLogging;
static uint16_t change_patch;
static int activeWaves=0,created,deleted;

void waveDelete(int chan)
{
  if (NULL != waves[chan])
  {
    delete waves[chan];
    waves[chan]=NULL;    
    activeWaves--;
    deleted++; 
  }
}

void loop()
{
  unsigned char c,opcode,chan;
  unsigned long d_time;
  int activeFlags=0;
  
  // read the next note from the table
  c = *sp++;
  opcode = c & 0xF0;
  chan = c & 0x0F;

  // Volume control
  //  uncomment if you have a volume pot soldered to your audio shield
  /*
  int n = analogRead(15);
  if (n != volume) {
    volume = n;
    codec.volume((float)n / 1023);
  }
  */

    // delete unused wave generators from memory
  int flag=1;
  for (uint8_t i=0;i<COUNT_OF(waves);i++)
  {
    if (NULL != waves[i] && !waves[i]->isPlaying())
    {
      waveDelete(i);
      change_patch &= ~(1<<i); // clear any pending patch change request
      //Serial.print('-');
    }
    if (NULL != waves[i])
    {
      activeFlags |= flag;
    }
    flag <<= 1;
  }

  if (activeWaves > 0)
    keepLogging = 10;
  
// Change this to measure performance. Or not.
static enum {mon_none,mon_text,mon_graph} monType = mon_graph;
switch (monType) 
{
  default:
    break;
  case mon_text:
    if(millis() - last_time >= 100 && (opcode != CMD_STOP || keepLogging > 0)) 
    {
      last_time = millis();
      keepLogging--;
      Serial.print("Proc = ");
      Serial.print(AudioProcessorUsage());
      Serial.print(" (");    
      Serial.print(AudioProcessorUsageMax());
      Serial.print("),  Mem = ");
      Serial.print(AudioMemoryUsage());
      Serial.print(" (");    
      Serial.print(AudioMemoryUsageMax());
      Serial.print("), ");
      Serial.print(activeWaves);
      Serial.print(" waves active: ");    
      Serial.print(activeFlags,HEX);
      Serial.print(", heap free=");
      Serial.println(FreeMem());
      
      last_time = millis();
    }
    break;

  case mon_graph:
    if(millis() - last_time >= 100 && (opcode != CMD_STOP || keepLogging > 0)) 
    {
      last_time = millis();
      keepLogging--;

      Serial.print(AudioProcessorUsage());
      Serial.print(' ');
      Serial.print(created/200.0);
      Serial.print(' ');
      Serial.print(deleted/200.0);
      Serial.print(' ');
      Serial.print(activeWaves);
      Serial.print(' ');
      float fre = FreeMem();
#define FREE_SUB 460000      
      fre = fre>FREE_SUB?((fre - FREE_SUB)/1000.0):0;
      Serial.println(fre);
    }
      break;
}  

  if(c < 0x80) {
    // Delay
    d_time = (c << 8) | *sp++;
    delay(d_time);
  }
  
  if(opcode == CMD_STOP) {
    if (0 == chan) // first stop
    {
      delay(10);
      for (chan=0; chan<COUNT_OF(waves); chan++) 
      {
        //wav->env.release(2000.0f);
        if (waves[chan])
          waves[chan]->noteOff();
        //waves[chan]->amplitude(0);
      }
      Serial.println("DONE");
    }
    else
      sp--; // just keep re-reading the STOP entry on every loop()  
  }

  // It is a command
  
  // Stop the note on 'chan'
  if(opcode == CMD_STOPNOTE) {
    waves[chan]->noteOff();
  }

  
  // Change patch on 'chan'
  if(opcode == CMD_PATCH) {
    wave_patch[chan] = *sp++;
    change_patch |= 1<<chan; // mark channel for patch change
  }

  
  // Play the note on 'chan'
  if(opcode == CMD_PLAYNOTE) {
    unsigned char note = *sp++;
    unsigned char velocity = *sp++;
    if (chan < 100)
    {
      int8_t patch = wave_patch[chan] % 32; // just to be safe...
      bool useMIDI = false;
      AudioNoInterrupts();
      if (NULL == waves[chan] // channel inactive, create a voice for it
       || (change_patch & 1<<chan))
      {
        // Patch change required and old voice not yet deleted
        if ((change_patch & 1<<chan) && NULL != waves[chan])
        {
          waveDelete(chan);
        }
          
        switch (patch)
        {
          case 0:
          case 4:
          case 10:
          case 15:
          case 18:
          case 22:
          case 25:
          case 29:
            waves[chan] = new SampleAndHoldVoice;
            break;
            
          case 1:
          case 6:
          case 8:
          case 13:
          case 14:
          case 20:
          case 28:
            waves[chan] = new KarplusVoice;
            break;

          case 2:
          case 5:
          case 9:
          case 12:
          case 17:
          case 26:
          case 23:
            waves[chan] = new MinHammondVoice;
            useMIDI = true;
            break; 
            
          default: 
            waves[chan] = new WaveAndEnvVoice;
            patch-=3;
            break;
            
        } 
        activeWaves++;
        created++;
        //Serial.print('+');
        Serial.print(waves[chan]->connect(mixArray[chan&3],chan>>2)?"nope ":"");
      }
      /*
      waves[chan]->begin(AMPLITUDE * velocity2amplitude[velocity-1],
                         tune_frequencies2_PGM[note],
                         wave_type[chan]);
                         */
      if (useMIDI)
        waves[chan]->noteOn(note,velocity,patch);
      else
        waves[chan]->noteOn(tune_frequencies2_PGM[note],
                           AMPLITUDE * velocity2amplitude[velocity-1],
                           patch&3);
      //envs[chan]->noteOn();
      AudioInterrupts();
    }
  }

  // replay the tune
  if(opcode == CMD_RESTART) {
    sp = score;
  }
}
