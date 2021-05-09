#include <Audio.h>

/*********************************************************************************/
class SynthVoice
{
  AudioConnection outputCord;
  virtual AudioStream& getOutputStream(void) = 0;
  public:
    virtual ~SynthVoice(){};
    virtual void noteOn(float freq, float vel, int chan=-1) = 0;
    virtual void noteOn(int MIDInote, int MIDIvel, int chan=-1) = 0;
    virtual void noteOff(void) = 0;
    virtual bool isPlaying(void) = 0;
    int connect(AudioStream& str) { return connect(str,0);}
    int connect(AudioStream& str, int inpt) {return outputCord.connect(getOutputStream(),0,str,inpt);}   
};


    
/*********************************************************************************/
class WaveformVoice final : public SynthVoice
{
    AudioSynthWaveform wave;
  
  static short wave_type[4];
    
    AudioStream& getOutputStream(void) {AudioStream& result {wave}; return result;};
  public:
    ~WaveformVoice(){};
    void noteOn(int MIDInote, int MIDIvel, int chan=-1){};
    void noteOn(float freq, float vel, int chan=-1)
    {
      wave.begin(vel,freq,(chan<0)?WAVEFORM_SINE:(wave_type[chan&3]));
    }

    void noteOff(void){};
    bool isPlaying(void) {return true;};
};

short WaveformVoice::wave_type[] = {
    WAVEFORM_SINE,
    WAVEFORM_SQUARE,
    WAVEFORM_SAWTOOTH,
    WAVEFORM_TRIANGLE};


/*********************************************************************************/
class WaveAndEnvVoice final : public SynthVoice
{
  // the actual synthesis engine and its connections
public:
    AudioSynthWaveform wave;
    AudioEffectExpEnvelope env;
    AudioAmplifier amp;
private:
 // connections need to be defined after the objects they connect, 
 // in order for initialisation to work properly
    AudioConnection cord1;
    AudioConnection cord2;
    bool isNew;
  
    static short wave_type[4];
    
    AudioStream& getOutputStream(void) {AudioStream& result {amp}; return result;};
  public:
    WaveAndEnvVoice() : cord1(wave,env), cord2(env,amp), isNew(true)
    {
      env.attack(129.2);
      env.hold(2.1);
      env.decay(181.4);
      env.sustain(0.3);
      env.release(284.5);
      amp.gain(0.5);
    };
    ~WaveAndEnvVoice(){};
    void noteOn(int MIDInote, int MIDIvel, int chan=-1){};
    void noteOn(float freq, float vel, int chan=-1)
    {
      if (isNew)
        wave.begin(vel,freq,(chan<0)?WAVEFORM_SINE:(wave_type[chan&3]));
      else
      {
        wave.amplitude(vel);
        wave.frequency(freq);
      }
      env.noteOn();
      isNew = false;
    }

    void noteOff(void){env.noteOff();};
    bool isPlaying(void) {return env.isActive();};
    
};

short WaveAndEnvVoice::wave_type[] = {
    WAVEFORM_PULSE,
    WAVEFORM_SQUARE,
    WAVEFORM_SAWTOOTH,
    WAVEFORM_TRIANGLE};    

/*********************************************************************************/
 class KarplusVoice final : public SynthVoice
{
    AudioSynthKarplusStrong wave;
  
    AudioStream& getOutputStream(void) {AudioStream& result {wave}; return result;};
    uint32_t stop_at;
  public:
    ~KarplusVoice(){};
    void noteOn(int MIDInote, int MIDIvel, int chan=-1){};
    void noteOn(float freq, float vel, int chan=-1)
    {
      wave.noteOn(freq,vel);
      stop_at = 0;
    }

    void noteOff(void){ stop_at = millis()+500;};
    bool isPlaying(void) {return (0 == stop_at || millis() < stop_at);};
};

/*********************************************************************************/
// Classic S&H voice: a bit clicky if you use a triangle wave, for some reason
// Note this may require an updated AudioEffectBitcrusher, as the current library
// one limits the sampleRate to 64 samples, and we need a much longer one
 class SampleAndHoldVoice final : public SynthVoice
{
  
/* Convert GUI tool output to SynthVoice:
// GUItool: begin automatically generated code
AudioSynthNoiseWhite     noise1;         //xy=255,2114
AudioSynthWaveform       waveform1;      //xy=289,1975
AudioEffectBitcrusher    bitcrusher1;    //xy=419,2113
AudioEffectExpEnvelope   expEnv1;        //xy=450,1971
AudioFilterStateVariable filter1;        //xy=582,1984
AudioMixer4              mixer1;         //xy=777,1988
AudioOutputI2S           i2s1;           //xy=912,2251
AudioConnection          patchCord1(noise1, bitcrusher1);
AudioConnection          patchCord2(waveform1, expEnv1);
AudioConnection          patchCord3(bitcrusher1, 0, filter1, 1);
AudioConnection          patchCord4(expEnv1, 0, filter1, 0);
AudioConnection          patchCord5(filter1, 0, mixer1, 0);
AudioConnection          patchCord6(filter1, 1, mixer1, 1);
AudioConnection          patchCord7(filter1, 2, mixer1, 2);
// GUItool: end automatically generated code
*/
public:
    // AudioStream elements are public, so the parameters are accessible
    AudioSynthNoiseWhite     noise1;         //xy=255,2114
    AudioSynthWaveform       waveform1;      //xy=289,1975
    AudioEffectBitcrusher    bitcrusher1;    //xy=419,2113
    AudioEffectExpEnvelope   expEnv1;        //xy=450,1971
    AudioFilterStateVariable filter1;        //xy=582,1984
    AudioMixer4              mixer1;         //xy=777,1988

    // Constructor and destructor also public, as required
    SampleAndHoldVoice() : 
              // seem to have to make connections here :-(
              patchCord1(noise1, bitcrusher1),
              patchCord2(waveform1, expEnv1),
              patchCord3(bitcrusher1, 0, filter1, 1),
              patchCord4(expEnv1, 0, filter1, 0),
              patchCord5(filter1, 0, mixer1, 0),
              patchCord6(filter1, 1, mixer1, 1),
              patchCord7(filter1, 2, mixer1, 2),
              
              isNew(true)
    {
      // set some vaguely-sane default values
      expEnv1.attack(129.2);
      expEnv1.hold(2.1);
      expEnv1.decay(181.4);
      expEnv1.sustain(0.3);
      expEnv1.release(284.5);
      
      mixer1.gain(0,0.5f); // just use low-pass output by default
      mixer1.gain(1,0);
      mixer1.gain(2,0);
      mixer1.gain(3,0.0f);

      bitcrusher1.bits(5);
      bitcrusher1.sampleRate(5.0f);

      filter1.octaveControl(1.5f);
      filter1.resonance(2.0f);
    };
    ~SampleAndHoldVoice(){};
    
    void noteOn(int MIDInote, int MIDIvel, int chan=-1){};
    void noteOn(float freq, float vel, int chan=-1)
    {
      // avoid clicks due to setting phase to 0 if
      // we're re-starting the voice at a new
      // frequency and amplitude
      if (isNew)
        waveform1.begin(vel,freq,WAVEFORM_SQUARE);
      else
      {
        waveform1.amplitude(vel);
        waveform1.frequency(freq);
      }
      noise1.amplitude(1.0f);
      expEnv1.noteOn();
      filter1.frequency(freq*4); // filter tracks note frequency
      isNew = false;
    }
    
    void noteOff(void){expEnv1.noteOff();};
    bool isPlaying(void) {return expEnv1.isActive();};

    // keep internal connectivity private
private:
    AudioConnection          patchCord1; // (noise1, bitcrusher1);
    AudioConnection          patchCord2; // (waveform1, expEnv1);
    AudioConnection          patchCord3; // (bitcrusher1, 0, filter1, 1);
    AudioConnection          patchCord4; // (expEnv1, 0, filter1, 0);
    AudioConnection          patchCord5; // (filter1, 0, mixer1, 0);
    AudioConnection          patchCord6; // (filter1, 1, mixer1, 1);
    AudioConnection          patchCord7; // (filter1, 2, mixer1, 2);
    // Define which AudioStream output is the voice output:
    // has to be output 0 at the moment, probably not an issue
    AudioStream& getOutputStream(void) {AudioStream& result {mixer1}; return result;};
    bool isNew;
};

/*********************************************************************************/
// Minimal Hammond
/*
// GUItool: begin automatically generated code
AudioSynthWaveform       waveform7; //xy=268,2172
AudioSynthWaveform       waveform8; //xy=268,2214
AudioSynthWaveform       waveform9; //xy=268,2268
AudioSynthWaveform       waveform6; //xy=269,2132
AudioSynthWaveform       waveform4; //xy=270,2048
AudioSynthWaveform       waveform5; //xy=270,2088
AudioSynthNoiseWhite     noise1;         //xy=271,2357
AudioSynthWaveform       waveform1;      //xy=276,1921
AudioSynthWaveform       waveform2; //xy=277,1966
AudioSynthWaveform       waveform3; //xy=278,2007
AudioMixer4              mixer1;         //xy=490,1985
AudioMixer4              mixer2; //xy=495,2112
AudioEffectEnvelope      envelope1;      //xy=506,2345
AudioMixer4              mixer3; //xy=669,2119
AudioOutputI2S           i2s1;           //xy=912,2251
AudioConnection          patchCord1(waveform7, 0, mixer2, 2);
AudioConnection          patchCord2(waveform8, 0, mixer2, 3);
AudioConnection          patchCord3(waveform9, 0, mixer3, 2);
AudioConnection          patchCord4(waveform6, 0, mixer2, 1);
AudioConnection          patchCord5(waveform4, 0, mixer1, 3);
AudioConnection          patchCord6(waveform5, 0, mixer2, 0);
AudioConnection          patchCord7(noise1, envelope1);
AudioConnection          patchCord8(waveform1, 0, mixer1, 0);
AudioConnection          patchCord9(waveform2, 0, mixer1, 1);
AudioConnection          patchCord10(waveform3, 0, mixer1, 2);
AudioConnection          patchCord11(mixer1, 0, mixer3, 0);
AudioConnection          patchCord12(mixer2, 0, mixer3, 1);
AudioConnection          patchCord13(envelope1, 0, mixer3, 3);
// GUItool: end automatically generated code
*/
class MinHammondVoice final : public SynthVoice
{
  public:
    MinHammondVoice() :
                patchCord1(waveform7, 0, mixer2, 2),
                patchCord2(waveform8, 0, mixer2, 3),
                patchCord3(waveform9, 0, mixer3, 2),
                patchCord4(waveform6, 0, mixer2, 1),
                patchCord5(waveform4, 0, mixer1, 3),
                patchCord6(waveform5, 0, mixer2, 0),
                patchCord7(noise1, envelope1),
                patchCord8(waveform1, 0, mixer1, 0),
                patchCord9(waveform2, 0, mixer1, 1),
                patchCord10(waveform3, 0, mixer1, 2),
                patchCord11(mixer1, 0, mixer3, 0),
                patchCord12(mixer2, 0, mixer3, 1),
                patchCord13(envelope1, 0, mixer3, 3),
                drawbars{8,8,8, 5,3,5, 5,3,1},
                playing(false)
                {
                  envelope1.attack(1.2f);
                  envelope1.hold(2.1f);
                  envelope1.decay(1.4f);
                  envelope1.sustain(0.0f);
                  envelope1.release(1.5f);

#define MIXLEVEL 0.1f
                  for (int i=0;i<4;i++)
                  {
                    mixer1.gain(i,MIXLEVEL);
                    mixer2.gain(i,MIXLEVEL);
                    mixer3.gain(i,MIXLEVEL);
                  }
                   mixer3.gain(2,MIXLEVEL*MIXLEVEL); // waveform9 only goes through this stage!
                };
    ~MinHammondVoice(){};
    void noteOn(int MIDInote, int MIDIvel, int chan=-1) 
    {
      int octave = MIDInote / 12; // middle C = 60 -> octave 5
      int off = MIDInote % 12;
      float fmult = (float)(1<<octave) / 32.0f;
      waveform1.begin(drawbars[0]*MIDIvel*64.0f,freqTable[off]*fmult/2,WAVEFORM_SINE);
      waveform3.begin(drawbars[2]*MIDIvel*64.0f,freqTable[off]*fmult  ,WAVEFORM_SINE);
      waveform4.begin(drawbars[3]*MIDIvel*64.0f,freqTable[off]*fmult*2,WAVEFORM_SINE);
      waveform6.begin(drawbars[5]*MIDIvel*64.0f,freqTable[off]*fmult*4,WAVEFORM_SINE);
      waveform9.begin(drawbars[8]*MIDIvel*64.0f,freqTable[off]*fmult*8,WAVEFORM_SINE);
      
      off += 4;
      if (off < 11)
        waveform7.begin(drawbars[6]*MIDIvel*64.0f,freqTable[off]   *fmult*4,WAVEFORM_SINE);
      else
        waveform7.begin(drawbars[6]*MIDIvel*64.0f,freqTable[off-12]*fmult*8,WAVEFORM_SINE);
        
      off += 3;
      if (off < 11)
      {
        waveform7.begin(drawbars[1]*MIDIvel*64.0f,freqTable[off]   *fmult  ,WAVEFORM_SINE);
        waveform7.begin(drawbars[4]*MIDIvel*64.0f,freqTable[off]   *fmult*2,WAVEFORM_SINE);
        waveform7.begin(drawbars[7]*MIDIvel*64.0f,freqTable[off]   *fmult*4,WAVEFORM_SINE);
      }
      else
      {
        waveform7.begin(drawbars[1]*MIDIvel*64.0f,freqTable[off-12]*fmult*2,WAVEFORM_SINE);
        waveform7.begin(drawbars[4]*MIDIvel*64.0f,freqTable[off-12]*fmult*4,WAVEFORM_SINE);
        waveform7.begin(drawbars[7]*MIDIvel*64.0f,freqTable[off-12]*fmult*8,WAVEFORM_SINE);
      }
      //noise1.amplitude(0.0);
      //envelope1.noteOn();
      playing = true;
    };
    void noteOn(float freq, float vel, int chan=-1) {};
    void noteOff(void) 
    {
      waveform1.amplitude(0.0);
      waveform2.amplitude(0.0);
      waveform3.amplitude(0.0);
      waveform4.amplitude(0.0);
      waveform5.amplitude(0.0);
      waveform6.amplitude(0.0);
      waveform7.amplitude(0.0);
      waveform8.amplitude(0.0);
      waveform9.amplitude(0.0);
      playing = false;
    };
    bool isPlaying(void) {return playing;};

  // GUItool: begin automatically generated code
      AudioSynthWaveform       waveform7; //xy=268,2172
      AudioSynthWaveform       waveform8; //xy=268,2214
      AudioSynthWaveform       waveform9; //xy=268,2268
      AudioSynthWaveform       waveform6; //xy=269,2132
      AudioSynthWaveform       waveform4; //xy=270,2048
      AudioSynthWaveform       waveform5; //xy=270,2088
      AudioSynthNoiseWhite     noise1;         //xy=271,2357
      AudioSynthWaveform       waveform1;      //xy=276,1921
      AudioSynthWaveform       waveform2; //xy=277,1966
      AudioSynthWaveform       waveform3; //xy=278,2007
  private:
      AudioEffectEnvelope      envelope1;     //xy=506,2345
      AudioMixer4              mixer1;        //xy=490,1985
      AudioMixer4              mixer2;        //xy=495,2112
      AudioMixer4              mixer3;        //xy=669,2119
      
      AudioConnection          patchCord1; // (waveform7, 0, mixer2, 2);
      AudioConnection          patchCord2; // (waveform8, 0, mixer2, 3);
      AudioConnection          patchCord3; // (waveform9, 0, mixer3, 2);
      AudioConnection          patchCord4; // (waveform6, 0, mixer2, 1);
      AudioConnection          patchCord5; // (waveform4, 0, mixer1, 3);
      AudioConnection          patchCord6; // (waveform5, 0, mixer2, 0);
      AudioConnection          patchCord7; // (noise1, envelope1);
      AudioConnection          patchCord8; // (waveform1, 0, mixer1, 0);
      AudioConnection          patchCord9; // (waveform2, 0, mixer1, 1);
      AudioConnection          patchCord10; // (waveform3, 0, mixer1, 2);
      AudioConnection          patchCord11; // (mixer1, 0, mixer3, 0);
      AudioConnection          patchCord12; // (mixer2, 0, mixer3, 1);
      AudioConnection          patchCord13; // (envelope1, 0, mixer3, 3);
  // GUItool: end automatically generated code
      AudioStream& getOutputStream(void) {AudioStream& result {mixer3}; return result;};
      static float freqTable[12]; // fundamental tonewheel frequencies: not quite in tune
      uint8_t drawbars[9];
      bool playing;
};

// frequencies are integer ratios using gears:
#define BASE_FREQ (20.0f * 16)
float MinHammondVoice::freqTable[12]=
  {BASE_FREQ* 85/104, // C
   BASE_FREQ* 71/ 82, // C#
   BASE_FREQ* 67/ 73, // D
   BASE_FREQ*105/108, // C#
   BASE_FREQ*103/100, // E
   BASE_FREQ* 84/ 77, // F
   BASE_FREQ* 74/ 64, // F#
   BASE_FREQ* 98/ 80, // G
   BASE_FREQ* 96/ 74, // G#
   BASE_FREQ* 88/ 64, // A: 440Hz, exact, MIDI note 69
   BASE_FREQ* 67/ 46, // A#
   BASE_FREQ*108/ 70, // B
};  
