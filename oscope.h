// Use hardware pin to trigger oscilloscope,
// or do nothing. Debug only, really.

#if !defined(oscope_h_)
#define oscope_h_

// May have been given null definitions: remove these
#undef SCOPE_ENABLE
#undef SCOPE_HIGH
#undef SCOPE_LOW
#undef SCOPE_TOGGLE
#undef SCOPESER_ENABLE
#undef SCOPESER_TX

#if defined(SCOPE_PIN)
extern bool scope_pin_value;
#define SCOPE_ENABLE() pinMode(SCOPE_PIN,OUTPUT)
#define SCOPE_HIGH() digitalWrite(SCOPE_PIN,scope_pin_value = 1)
#define SCOPE_LOW() digitalWrite(SCOPE_PIN,scope_pin_value = 0)
#define SCOPE_TOGGLE() digitalWrite(SCOPE_PIN,scope_pin_value = !scope_pin_value)
#else
#define SCOPE_ENABLE(...) 
#define SCOPE_HIGH(...) 
#define SCOPE_LOW(...) 
#define SCOPE_TOGGLE(...) 
#endif // defined(SCOPE_PIN)

#if defined(SCOPE_SERIAL)
#if !defined(SCOPESER_SPEED)
#define SCOPESER_SPEED
#endif // !defined(SCOPESER_SPEED)
#define SCOPESER_ENABLE() SCOPE_SERIAL.begin(SCOPESER_SPEED)
#define SCOPESER_TX(x) SCOPE_SERIAL.write(x)
#else
#define SCOPESER_ENABLE(...) 
#define SCOPESER_TX(...) 
#endif // defined(SCOPE_SERIAL)

#endif // !defined(oscope_h_)
