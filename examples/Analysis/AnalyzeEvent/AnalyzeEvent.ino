/*
 * Demonstration of the AudioAnalyseEvent object.
 * 
 * This program is in the public domain.
 * Jonathan Oakley Novermber 2021
 */
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>


// GUItool: begin automatically generated code
AudioAnalyzeEvent        eventL;           //xy=887,329
AudioSynthWaveform       wave1;
AudioOutputI2S           i2s2;           //xy=1121,330
AudioConnection          patchCord1;
AudioConnection          patchCord2;
AudioConnection          patchCord3;
AudioConnection          patchCord4;
AudioControlSGTL5000     sgtl5000_1;     //xy=1131,379
// GUItool: end automatically generated code

/*********************************************************************************/
struct context_t {
  char ch;
  int maxCount;
  int count;
  uint32_t maxDelUs;
} contextL = {'L',300},contextR = {'R',100};   // no need for "volatile", only changed in foreground code

// This function gets called every time it's triggered by the
// corresponding AudioAnalyzeEvent "owner", i.e. just after the
// audio update loop interrupt has occurred. By using the event,
// we can execute foreground code with the maximum possible
// available time before the next audio loop interrupt.
void evFn(EventResponderRef theEvent)
{
  AudioAnalyzeEvent* pData = (AudioAnalyzeEvent*) theEvent.getData();  
  uint32_t delUs = micros() - pData->getMicros(); // find out how long it is since object was updated
  context_t* pCtxt = (context_t*) theEvent.getContext();  

  // keep track of how quickly the event occurs after the 
  // audio update has been run: typically, *really* soon!
  if (pCtxt->maxDelUs < delUs)
    pCtxt->maxDelUs = delUs;

  // Don't do visible work every trigger (2.9ms): instead...
  pCtxt->count++;
  if (pCtxt->count >= pCtxt->maxCount) // ... make it every 500 times
  {
    Serial.printf(F("*** EVENT %c: %d updates after %dms: max delay %dus ***\n"),
                    pCtxt->ch,
                    pData->getCount(),millis(),pCtxt->maxDelUs);
    pCtxt->count = 0;
    pCtxt->maxDelUs = 0;
  }   
}

void setup() {
  pinMode(13,OUTPUT);
  
  Serial.begin(115200);
  while (!Serial)
    ;

  if (CrashReport) 
  {
    Serial.println(CrashReport);
    CrashReport.clear();
  }

  AudioMemory(10);

  // AudioAnalyzeEvent setup
  eventL.setEventFn(evFn,&contextL); // evFn will be triggered by our AudioAnalyzeEvent object's update()
  delay(10);
  Serial.printf(F("Setup(): %d events after %dms\n"),eventL.getCount(),millis());

  AudioStream::setAllClansActive(false);

  // set up left channel:
  patchCord1.connect(wave1,eventL);
  patchCord2.connect(eventL,i2s2);
  wave1.begin(0.1,220,WAVEFORM_SINE);
  sgtl5000_1.enable();
  sgtl5000_1.volume(0.5);
}

AudioAnalyzeEvent* pEventR;

/*
 * Loop, reandomly creating, destroying and connecting an event
 * object which feeds the right output channel.
 * 
 * The left channel event object will output on the serial port
 * every 300 events, with an increasing update count which is 
 * always a multiple of 300.
 * 
 * The right channel object will have a new count every time it's created,
 * so when it outputs the count is smaller. You will note that
 * it doesn't get updated until after it's been connected; also, because
 * its context has (deliberately) not been initialised at creation time,
 * the update counts are not a multiple of 100, but if it's allowed to
 * xist for long enough the counts will increment by 100 every time they're
 * reported.
 * 
 * The max delay report shows how long it takes from the event object update 
 * inside the audio engine, to the event triggering in this code. Typically
 * this is a few microseconds on a Teensy 4.1.
 */
void loop() {
  delay(250);

  switch (random(6))
  {
    default: // do nothing
      Serial.println(F("..."));
      break;
      
    case 1: // destroy our right-channel event object, if it exists
      if (NULL != pEventR)
      {
        delete pEventR;
        pEventR = NULL;
        Serial.println(F("Destroy"));
      }
      else
        Serial.println(F("."));
      break;

    case 2: // create our right-channel event object, if it doesn't exist
      if (NULL == pEventR)
      {
        pEventR = new AudioAnalyzeEvent;
        pEventR->setEventFn(evFn,&contextR); // same event function, different context
        Serial.println(F("Create"));
      }
      else
        Serial.println(F("."));
      break;

    case 3: // connect our right-channel event object, if it exists
      if (NULL != pEventR)
      {
        patchCord3.connect(wave1,*pEventR);
        patchCord4.connect(*pEventR,0,i2s2,1);
        Serial.println(F("Connect"));
      }
      else
        Serial.println(F("."));
      break;   
  }
}
