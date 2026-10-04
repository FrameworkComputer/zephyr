.. zephyr:board:: framework_wireless_dongle

Overview
********

The Framework Wireless Dongle is the USB-A receiver that ships with the
:zephyr:board:`framework_wireless_tp_kb`. It connects to Bluetooth Low Energy input devices and
presents them to the host as a USB HID device, without needing Bluetooth on the host. It is built
around the Nordic nRF54LM20A SoC and has no user interface of its own: no buttons and no LEDs.

The dongle ships with an MCUboot bootloader that supports serial recovery over USB.

Hardware
********

* nRF54LM20A SoC: 128 MHz Arm Cortex-M33, RISC-V FLPR coprocessor, 2 MB RRAM, 512 kB RAM,
  multiprotocol 2.4 GHz radio (Bluetooth LE, IEEE 802.15.4, proprietary)
* USB high-speed device on the USB-A plug, which also powers the dongle
* 32 MHz crystal, no 32.768 kHz crystal (the low frequency clock runs from the internal RC
  oscillator)
* Debug UART (UARTE20) on test pads

Supported Features
==================

.. zephyr:board-supported-hw::

Connections and IOs
===================

+-------+-------------+----------------------+
| Pin   | Function    | Usage                |
+=======+=============+======================+
| P2.02 | UARTE20 TX  | Debug UART, test pad |
+-------+-------------+----------------------+
| P2.00 | UARTE20 RX  | Debug UART, test pad |
+-------+-------------+----------------------+

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

SWD is available on test pads. Flashing with a J-Link or an nRF Util compatible probe uses the
standard flow, for example for the :zephyr:code-sample:`usb-cdc-acm` sample:

.. zephyr-app-commands::
   :zephyr-app: samples/subsys/usb/cdc_acm
   :board: framework_wireless_dongle/nrf54lm20a/cpuapp
   :goals: build flash

Flashing a plain application this way overwrites the factory bootloader. To keep it, build the
application for the ``image-0`` slot with sysbuild and MCUboot enabled, and flash only the
application domain. A plain ``west flash`` of a sysbuild build also writes the MCUboot image built
by sysbuild over the factory bootloader.

.. zephyr-app-commands::
   :zephyr-app: samples/subsys/usb/cdc_acm
   :board: framework_wireless_dongle/nrf54lm20a/cpuapp
   :goals: build flash
   :west-args: --sysbuild
   :gen-args: -DSB_CONFIG_BOOTLOADER_MCUBOOT=y
   :flash-args: --domain cdc_acm

Flashing over USB
=================

The factory MCUboot bootloader exposes a USB CDC ACM serial port for :ref:`mcumgr <mcu_mgr>`
serial recovery. As the dongle has no button, recovery mode is requested from the running
application through the :ref:`retention boot mode <retention_api>` (for example with the mcumgr
``os reset`` command with the boot mode argument, if the application enables it). Build the
application with MCUboot support so that the image is signed and linked for the application slot:

.. zephyr-app-commands::
   :zephyr-app: samples/subsys/usb/cdc_acm
   :board: framework_wireless_dongle/nrf54lm20a/cpuapp
   :goals: build
   :west-args: --sysbuild
   :gen-args: -DSB_CONFIG_BOOTLOADER_MCUBOOT=y

The image must be signed with a key the installed bootloader trusts. Upload the signed
application image with mcumgr and reset the device:

.. code-block:: console

   mcumgr --conntype serial --connstring dev=/dev/ttyACM0 image upload build/cdc_acm/zephyr/zephyr.signed.bin
   mcumgr --conntype serial --connstring dev=/dev/ttyACM0 reset

Debugging
=========

The debug UART (115200 baud) on the test pads is the default console. The SoC can be debugged
over SWD with J-Link or nRF Util using ``west debug``.

References
**********

* `Framework Computer`_

.. _Framework Computer: https://frame.work
