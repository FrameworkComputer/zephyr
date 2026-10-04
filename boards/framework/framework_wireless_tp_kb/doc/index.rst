.. zephyr:board:: framework_wireless_tp_kb

Overview
********

The Framework Wireless Touchpad Keyboard (development codename "Daisy") is a battery powered
keyboard with an integrated touchpad. It connects over Bluetooth Low Energy or over USB 2.0.
It is built around the Nordic nRF54LM20A SoC with a Nordic nPM1300 PMIC for battery charging and
power management.

The keyboard ships with an MCUboot bootloader that supports serial recovery over USB, so
applications can be updated without opening the device.

The board definition in Zephyr focuses on the board support, to allow users to build their own
projects on top of the standalone PCB. Provide your own definitions for battery charging, touchpad
and keyboard matrix.

Hardware
********

* nRF54LM20A SoC: 128 MHz Arm Cortex-M33, RISC-V FLPR coprocessor, 2 MB RRAM, 512 kB RAM,
  multiprotocol 2.4 GHz radio (Bluetooth LE, IEEE 802.15.4, proprietary)
* USB high-speed device (USB-C connector, also used for charging)
* nPM1300 PMIC on I2C (TWIM21)

  * BUCK2 supplies the 3.3 V system rail
  * BUCK1 supplies the keyboard backlight
  * Charger for the 1200 mAh Li-ion battery with NTC temperature monitoring

* PTP compatible Touchpad connected over I2C
* 8x18 key matrix, same as Framework Laptop 12
* PWM Keyboard backlight (Unused on official product)
* Four white status LEDs (PWM capable) for the Bluetooth profile/pairing state
* One RGB status LED (PWM capable)
* Caps Lock LED
* Pairing button
* Wired/Wireless mode slide switch
* Debug pin header with UART, I2C, power rails, and key matrix pins

Supported Features
==================

.. zephyr:board-supported-hw::

Connections and IOs
===================

+---------------+---------------+-------------------------------------------+
| Pin           | Function      | Usage                                     |
+===============+===============+===========================================+
| P2.02         | UARTE00 TX    | Debug UART, pin header                    |
+---------------+---------------+-------------------------------------------+
| P2.00         | UARTE00 RX    | Debug UART, pin header                    |
+---------------+---------------+-------------------------------------------+
| P3.03         | TWIM21 SCL    | nPM1300 PMIC                              |
+---------------+---------------+-------------------------------------------+
| P3.09         | TWIM21 SDA    | nPM1300 PMIC                              |
+---------------+---------------+-------------------------------------------+
| P3.04         | TWIM22 SCL    | Touchpad                                  |
+---------------+---------------+-------------------------------------------+
| P3.08         | TWIM22 SDA    | Touchpad                                  |
+---------------+---------------+-------------------------------------------+
| P3.06         | GPIO          | Touchpad interrupt (active low)           |
+---------------+---------------+-------------------------------------------+
| P3.12         | GPIO          | Touchpad power enable                     |
+---------------+---------------+-------------------------------------------+
| P1.03         | TWIM23 SCL    | I2C, pin header                           |
+---------------+---------------+-------------------------------------------+
| P1.02         | TWIM23 SDA    | I2C, pin header                           |
+---------------+---------------+-------------------------------------------+
| P1.22 - P1.25 | GPIO / PWM20  | White status LEDs 1-4 (active low)        |
+---------------+---------------+-------------------------------------------+
| P1.29         | GPIO / PWM21  | Green status LED                          |
+---------------+---------------+-------------------------------------------+
| P1.30         | GPIO / PWM21  | Blue status LED                           |
+---------------+---------------+-------------------------------------------+
| P1.31         | GPIO / PWM21  | Red status LED                            |
+---------------+---------------+-------------------------------------------+
| P3.07         | GPIO          | Caps Lock LED                             |
+---------------+---------------+-------------------------------------------+
| P3.11         | PWM22         | Keyboard backlight                        |
+---------------+---------------+-------------------------------------------+
| P3.10         | GPIO          | Pairing button (active low)               |
+---------------+---------------+-------------------------------------------+
| P3.05         | GPIO          | USB/Bluetooth mode switch                 |
+---------------+---------------+-------------------------------------------+
| P0.00 - P0.07 | GPIO          | Key matrix sense lines KSI0-7             |
+---------------+---------------+-------------------------------------------+
| P1.00, P1.01, | GPIO          | Key matrix drive lines KSO0-17            |
| P1.18, P1.19, |               |                                           |
| P1.04 - P1.17 |               |                                           |
+---------------+---------------+-------------------------------------------+
| P3.00         | GPIO          | Layout strap: ISO                         |
+---------------+---------------+-------------------------------------------+
| P3.01         | GPIO          | Layout strap: ANSI                        |
+---------------+---------------+-------------------------------------------+

P1.01 and P1.02 are NFC pins on the nRF54LM20A. The board sets ``nfct-pins-as-gpios`` on the NFCT
node, so the startup code configures them as GPIOs.

Flash Layout
============

The board's flash layout matches the factory bootloader:

+-------------------+----------+----------+---------------------------------+
| Partition         | Offset   | Size     | Usage                           |
+===================+==========+==========+=================================+
| ``mcuboot``       | 0x000000 | 96 kB    | MCUboot with USB serial recovery|
+-------------------+----------+----------+---------------------------------+
| ``image-0``       | 0x018000 | 1828 kB  | Application                     |
+-------------------+----------+----------+---------------------------------+
| ``storage``       | 0x1e1000 | 16 kB    | Settings storage                |
+-------------------+----------+----------+---------------------------------+

There is only one application slot, so when building MCUboot with sysbuild the board defaults to
``SB_CONFIG_MCUBOOT_MODE_SINGLE_APP``. The factory bootloader verifies ED25519 signatures, so the
board also defaults to ``SB_CONFIG_BOOT_SIGNATURE_TYPE_ED25519``. Sysbuild then signs images with
MCUboot's ``root-ed25519.pem``; set ``SB_CONFIG_BOOT_SIGNATURE_KEY_FILE`` to use a different key.

Programming and Debugging
*************************

.. zephyr:board-supported-runners::

Flashing with a debugger
========================

The SWD interface is available on the internal pin header together with the debug UART.
Flashing with a J-Link or an nRF Util compatible probe uses the standard flow, for example for
the :zephyr:code-sample:`blinky` sample:

.. zephyr-app-commands::
   :zephyr-app: samples/basic/blinky
   :board: framework_wireless_tp_kb/nrf54lm20a/cpuapp
   :goals: build flash

Flashing a plain application this way overwrites the factory bootloader. To keep it, build the
application for the ``image-0`` slot with sysbuild and MCUboot enabled, and flash only the
application domain. A plain ``west flash`` of a sysbuild build also writes the MCUboot image built
by sysbuild over the factory bootloader.

.. zephyr-app-commands::
   :zephyr-app: samples/basic/blinky
   :board: framework_wireless_tp_kb/nrf54lm20a/cpuapp
   :goals: build flash
   :west-args: --sysbuild
   :gen-args: -DSB_CONFIG_BOOTLOADER_MCUBOOT=y
   :flash-args: --domain blinky

Flashing over USB
=================

The factory MCUboot bootloader exposes a USB CDC ACM serial port for :ref:`mcumgr <mcu_mgr>`
serial recovery. Hold the pairing button while connecting the keyboard to USB to start the
bootloader in recovery mode. Build the application with MCUboot support so that the image is
signed and linked for the application slot:

.. zephyr-app-commands::
   :zephyr-app: samples/basic/blinky
   :board: framework_wireless_tp_kb/nrf54lm20a/cpuapp
   :goals: build
   :west-args: --sysbuild
   :gen-args: -DSB_CONFIG_BOOTLOADER_MCUBOOT=y

The image must be signed with a key the installed bootloader trusts. Upload the signed
application image with mcumgr and reset the device:

.. code-block:: console

   mcumgr --conntype serial --connstring dev=/dev/ttyACM0 image upload build/blinky/zephyr/zephyr.signed.bin
   mcumgr --conntype serial --connstring dev=/dev/ttyACM0 reset

Debugging
=========

The debug UART (115200 baud) on the pin header is the default console. The SoC can be debugged
over SWD with J-Link or nRF Util using ``west debug``.

References
**********

* `Framework Computer`_

.. _Framework Computer: https://frame.work
