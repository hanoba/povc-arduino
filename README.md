# Arduino Software for POV Cylinder

The Arduino software consists the main program (**mpc.ino**) and the following libraries:

- **bt** - Driver SW for Bluetooth module
- **LDP8806** - Driver SW for LED strips
- **MemoryFree** - Functions to detect available free RAM memory
- **mpcgif** - Playback of GIF files located in RAM or Flash memory
- **pictures** - Internal GIF pictures stored in Flash
- **trace** - Functions for SW debugging

Periodic output of the pictures to the LED strips is done interrupt driven. There are two toggle frame buffers. Each frame buffer holds one picture with 40 x 151 pixels. Each pixel is a one byte color palette index. While one frame buffer is output to the LED strips via interrupt and DMA, the other frame buffer is prepared by the main program (e.g. by the function decoding the GIF pictures). Toggling of the frame buffers is done by the frame interrupt routine.

There is one frame interrupt per revolution triggered by the IR sensor. The frame interrupt routine measures (via a hardware timer) the evolution speed and programs periodic column interrupts (one per column, i.e. 150 interrupts per revolution) with a hardware timer. The column interrupt routine outputs the current column to the LED strips. For performance reasons output is done via three DMA channels that operate fully in parallel.

A full description of the POV Cylinder project can be found here: https://www.hackster.io/hanoba-diy/pov-cylinder-with-arduino-due-7016d5