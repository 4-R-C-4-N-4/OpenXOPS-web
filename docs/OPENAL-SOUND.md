# OpenAL sound backend notes

`OPENXOPS_SOUND_BACKEND=openal` builds the native OpenAL backend. `SoundControl`
loads through the VFS, decodes bounded RIFF/WAVE chunks (PCM 8/16-bit mono or
stereo; stereo retains the left sample), and allocates one shared pool of 100 one-shot sources across all 24 sound slots;
sources bind to a sound only while playing. `PlaySound` applies the
legacy -100..100 volume scale and -100..100 pan with relative sources; 3D calls
use the legacy linear gain attenuation and reject positions at/after 335 units.
The OpenAL listener follows `SetCamera`. If an audio device is unavailable,
valid files still load and calls behave silently without failing game startup.

The generated audio regression test uses temporary synthetic WAV data only;
it does not access original game sounds. Run it with `ALSOFT_DRIVERS=null`.
