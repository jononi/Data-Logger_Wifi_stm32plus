# Data Logger on Wifi Using ESP8266, STM32F4 Discovery Board and stm32plus library

Purpose:
Add high level routines to interact with REST APIs for smart home projects using the excellent stm32plus C++ library, stm32f4 cortex M4 microcontroller and ESP8266 as a serial <-> Wifi 

Hardware:

[STM32F4 discovery board](http://www.st.com/en/evaluation-tools/stm32f4discovery.html): cheap development board for the STM32F407 Cortex M4 microcontroller, clocked at 168MHz, with 192KB RAM and 1MB Flash, and numerous peripherals, embedded in the microcontroller (USARTs, SPIs, SDIO, CAN, USB, FSMC, DMA, ADC, DAC...) and added to the board (MEMS microphone, audio DAC, accelerometer, STlink v2 loader/debugger interface)

ESP8266 Wifi board: I use the 07 variant with ceramic chip antenna. I flashed an AT commands firmware (AI-v0.9.5.0 AT Firmware.bin) so I can use it as a transparent usart <-> WiFi translator

Optional: a generic HX8352A 400x240 lcd panel wit parallel interface and touch controller. STM32plus library supports a lot of lcd drivers including this one with a nice DMA based interface.


Software:

[STM32PLUS C++ library](https://github.com/andysworkshop/stm32plus): ST Microelectronics has excellent and cheap dev boards but no easy and free software to exploit their equipement. Without Andy's library, my Discovery boards will be still collecting dust. This is a very rich library with a lot of complex and rich hardware libraries. It also supports other microcontrollers within STM32 family.



IoT Platform: [UbiDots](https://ubidots.com/) I picked this service for the availability of examples to interact with their REST API with many hardware options, such as an AT commands based solution and the other dev platform that I use (Particle's Photon). The free tier allowance is also among the best that I could find.

current status: testing AT commands communication with ESP8266
