# V2 Face

Created by Daniel Casner Last updated Nov 06, 2017

Cozmo V1 had a 128x64 pixel 1bit per pixel monochrome OLED display.

V2 replaces this with a higher resolution color LCD

* Resolution 184 x 96 pixels
* Color depth 18 bits (6R 6G 6B) (However, 16-bit RGB_565 format is planned for driving the display)
* Active area 23.2mm x 12.1mm
* Manufacturer Datasheet [pdf](ST0103A1W-WSNLW-F_A01%20170519.pdf)


## Driving the LCD

The LCD is connected to the application processor by a 4-wire SPI interface. No kernel driver is intended. Instead the engine (or other application) should use the spidev api to talk directly to the LCD controller.

The LCD includes the common ST7789 driver chip [pdf](ST7789.pdf).

An example program to paint images on the display is available in the robot2 source tree.

### Manufacturer Recommended Register Settings:

[See also sample script](ST0103A1+ST7789V%20Initial%20Code%20Ver01%2020170825.txt)

#### Recommended Settings for Driver Chip Contrast
Vcom setting

```
comm_out(0xBB); //VCOMS (BBh): VCOMS Setting
data_out(0x36);
```

This Vcom command is able to adjust the contrast, and we would like to suggest you to set as (0x36)


#### Gamma Setting

Gamma settings

```
comm_out(0xE0); //PVGAMCTRL (E0h): Positive Voltage Gamma Control
data_out(0xD0);
data_out(0x10);
data_out(0x16);
data_out(0x0A);
data_out(0x0A);
data_out(0x26);
data_out(0x3C);
data_out(0x53);
data_out(0x53);
data_out(0x18);
data_out(0x15);
data_out(0x12);
data_out(0x36);
data_out(0x3C);
 
comm_out(0xE1); //NVGAMCTRL (E1h): Negative Voltage Gamma Control
data_out(0xD0);
data_out(0x11);
data_out(0x19);
data_out(0x0A);
data_out(0x09);
data_out(0x25);
data_out(0x3D);
data_out(0x35);
data_out(0x54);
data_out(0x17);
data_out(0x15);
data_out(0x12);
data_out(0x36);
data_out(0x3C);
```