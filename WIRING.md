# Wiring

Here's the wiring for the components

> [!IMPORTANT]
> For the display, you must change it inside the "User_Setup.h" on TFT_eSPI library

## PCM5102a DAC

- BCK       : 8
- WS/LCK    : 17
- DATA/DIN  : 18

## ILI9488 Display

### Display

- DC    : 9
- CS    : 10
- MOSI  : 11
- CLK   : 12
- BL    : 3
- RST   : 46

### Touch

- RST   : 6
- INT   : 7
- SCK   : 15
- SDA   : 16

## SD Card

- CS    : 2
- MISO  : 40
- SCK   : 41
- MOSI  : 42
