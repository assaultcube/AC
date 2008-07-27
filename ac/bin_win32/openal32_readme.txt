AssaultCube comes with two different OpenAL implementations:

- OpenAL 2.0.3 (Official)
- OpenAL-Soft 1.4.272 customized

Both have their pros and cons. By default, the official OpenAL (oalinst.exe)
is installed during setup. If you experience sound problems in AssaultCube
you might want to switch to OpenAL-Soft. This can be done by renaming the file

openal32_RemoveThisPartToUseOpenAL-Soft.dll

to

openal32.dll

This way, AssaultCube detects this file on startup and loads OpenAL-Soft. Otherwise
it would fall back to the system-wide installed Official OpenAL implementation.
