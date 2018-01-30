The radio contains a NEC uPD7514 CPU which is 4-bit with internal mask rom 
(4 kByte), SRAM (256 byte) and a LCD controller. Datasheet is D7514G-NEC.pdf in this folder.

For the radio code and probably various other settings an EEPROM is included of 
type SDA 2506, made by SIEMENS. Its size is 1 kbit (128 byte).
It is accessed via a SPI-like serial data bus.
A decoder for the bus protocol is included, usable with SigRok / PulseView. 

To read the code, connect a logic analyzer to pins 2 (CE), 4 (D) and 5 (CLK).
Power up the 

When the display shows CODE, the EEPROM bytes at 0x65, 0x66, 0x67 and 0x68 are read.

| address   | info |
|-----------|-|
| 0x65      | seems to be always the value 0x37 |
| 0x66      | seems to somehow encode the number of failed attempts |
| 0x67-0x68 | the radio code in BCD with the bytes swapped |

E.g. for my radio:

| address | value |
|---------|-|
| 0x65    | 0x37 |
| 0x66    | 0x2c after code was entered correctly |
| 0x67    | 0x13 |
| 0x68    | 0x81 -> the code is 8113 (0x68 and 0x67 concatenated)|

When the code is entered incorrectly, the radio writes the following values to 0x66 after each successive try:
0x5c, 0x62, 0x68

