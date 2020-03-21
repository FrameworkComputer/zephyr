.. _stm32_stm32g071rb_disco:

STM32 Go Discovery Kit USB-C Analyzer
#############################

Overview
********
Show usage of the discovery kit, which is primarily used as a PD analyzer and usb-c PD development platform. 
The discovery kit has a built in ST-LINK programmer, and a virtual com port usb cdc to uart serial terminal 
that connects to the stm32g071rb UART 3.
Other devices on the system include:
 * Captive USB-C cable with CC lines connected to the STM32's USBPD1 controller. 
 * USB-C port passthrough. 
 * 3 I2C INA230 voltage/current/power sensors connected to usb-C Vbus and CC1, CC2 lines. 
 * 4 LEDS
 * 4-way Joystick 
 * 0.96in 128x64 OLED Display with a SSD1315Z controller
 * 32khz RTC oscillator. 


Building and Running
********************

.. zephyr-app-commands::
   :app: samples/boards/stm32/stm32g071rb_disco
   :goals: build flash

Currently pyocd has some issues flashing this chip without special options. However you can flash using a cmsis pack with the following command.
.. code-block:: console
    pyocd flash --target stm32g071rbtx -O reset_type=hw -O connect_mode='under-reset' --pack Keil.STM32G0xx_DFP.1.2.0.pack zephyr/zephyr.elf
