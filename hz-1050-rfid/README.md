HZ-1050 RFID card reader 125 kHz
====================================

Uses an ATmega8A microcontroller

Pin usage:

PC0 output    ??
PC1 output    ??
PD3 input pullup
PD2 output   LED D4
PC2 input : baud rate config jumper

PB3 output  toggled with 125kHz by Timer2 -> carrier frequency for card
PB0 input  modulated data from card  -> Timer1 Input Capture





