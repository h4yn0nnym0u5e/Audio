# Buffered WAV player / recorder
The AudioPlayWAVxxx and AudioRecordWAVxxx objects are intended to provide good-performance playback from and recording to WAV files stored on SD card or other filesystem, while imposing a minimal set of constraints on the sketch programmer. Multiple files may be used simultaneously.

## Constraints
### Memory buffers
Each object must have its own memory buffer to hold audio data awaiting playback or storage to the filesystem. The larger the buffer the more resilience the system will have to filesystem (or other) delays, but obviously at a penalty of reducing the memory available for other aspects of your application.
### Yield()
Transfers between buffers and filesystem are scheduled using the EventResponder library, which is set up to execute them when the yield() function is called. The user _does not have to_ add explicit calls to yield(), as these will occur whenever loop() is allowed to return, and possibly in some (undocumented?) library functions. It is almost always good practise to allow loop() to return often as this allows internal "housekeeping" activities to occur; if you cannot do this, be sure to include yield() calls that execute sufficiently often to keep the buffers updated.
### Filesystem access
Because buffer transfers occur during yield(), and this is effectively part of the user's application, other filesystem operations (like reading/writing settings or log files) can be undertaken freely by the sketch.