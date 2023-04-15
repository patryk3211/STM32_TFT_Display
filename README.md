# STM32 TFT Display Driver
This is a simple display driver I wrote for [this LCD Panel](https://www.waveshare.com/wiki/3.5inch_RPi_LCD_(A)), it uses the ILI9486 LCD driver IC and XPT2046 for the touch panel. It is used in a CNC machine I am building. You shouldn't probably use this in any of your projects but I can't really stop you can I? I wrote this because I was bored, if you want a real LCD you can for example use the [TFT eSPI library](https://github.com/Bodmer/TFT_eSPI), it is way better than my little project and it supports a lot more microcontrollers.

## API Description
There are two important files, `tft_driver.h` and `gfx.h`. These two header contain basically all of the functionality of this library.
### tft_driver.h - The core of the driver
This file contains all the necesarry functions to initialize and send commands to the display. You can use this file alone to draw things on the display as well as receive touch panel data. All functions are documented in the header file.
### gfx.h - A simple graphics library
This file is a helper which makes building GUIs easier. It will handle touch interrupts for you and build chains of LCD commands to make your life easier.

## How to use
You can use the `main.cpp` file as an example of how to use this library. You should be able to quite easily compile this project using the [PlatformIO extension for VSCode](https://platformio.org/)
