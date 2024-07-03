# eQ-3 Bluetooth Smart Lock

## MCUs

### Bluetooth SOC
Broadcom BCM20736S  
Firmware: `res/raw/update_ota.bin`

enthaelt evtl auch Teile der application logic (`update_ota.bin` enthaelt strings wie "Fragment ACK %i received", application protocol hat command typ `FRAGMENT_ACK`)


### Application controller
STM8L052 C6T6

Firmware: 

* `res/raw/update.eq3` (Dateiformat siehe unpack_firmware.py)
* scheint zusätzlich verschluesselt und/oder gepackt (99% entropie)

Memory map:

```
0x000000-0x0007ff RAM
0x001000-0x0010ff Data EEPROM
0x005000-0x0057ff GPIO/peripheral registers
0x006000-0x0067ff Boot ROM
0x007f00-0x007fff CPU/SWIM/Debug registers
0x008000-0x00807f Interrupt vectors
0x008080-0x00ffff Flash memory
```

Bootloader evtl nicht gesperrt



