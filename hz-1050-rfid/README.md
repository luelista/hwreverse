HZ-1050 RFID card reader 125 kHz
====================================

Simple RFID card reader for 125 kHz proxcards. Default firmware outputs the card serial number over UART and Wiegand.

Uses an ATmega8A microcontroller.

Initial Fuses :

- lfuse = 0x3fc0
- hfuse = 0x9966
- lock = 0xff00 *(not sure about lock, accidentally erased the flash -.-)*

Pin usage:

|Pin|Mode|Function|
|-|-|-|
|PC0 |output    |??|
|PC1 |output    |??|
|PD3 |input pullup||
|PD2 |output   |LED D4|
|PC2 |input : |baud rate config jumper|
|||
|PB3 |output  |toggled with 125kHz by Timer2 -> carrier frequency for card|
|PB0 |input  |modulated data from card  -> Timer1 Input Capture|




Other components:

* LM358 OP AMP
* 8 MHz crystal




