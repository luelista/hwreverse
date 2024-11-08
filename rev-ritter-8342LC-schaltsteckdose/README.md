# REV Ritter Schaltsteckdose "Typ 8342LC"

Fotos einer geöffneten Funkschaltsteckdose und der darin befindlichen Leiterplatte.

## Beschriftung Typenschild

```
Typ 8342LC
10A, max. 2300W, 230V~, 433,92MHz
Spannungsfrei nur bei gezogenem Stecker!
REV
REV Ritter GmbH
Frankenstr. 1-4
D-63776 Mömbris
www.rev.biz
05.16
```

## Funkprotokoll

Learning Code  (KlikAanKlikUit / IntertechnoV3 kompatibel)


```c
/** 
 * Analysis of received data
 * @param instance Pointer to a SubGhzBlockGeneric* instance
 */
static void subghz_protocol_intertechno_v3_check_remote_controller(SubGhzBlockGeneric* instance) {
    /*
 *  A frame is either 32 or 36 bits:
 *     
 *               _
 *   start bit: | |__________ (T,10T)
 *          _   _
 *   '0':  | |_| |_____  (T,T,T,5T)
 *          _       _
 *   '1':  | |_____| |_  (T,5T,T,T)
 *          _   _
 *   dimm: | |_| |_     (T,T,T,T)
 * 
 *              _
 *   stop bit: | |____...____ (T,38T)
 * 
 *  if frame 32 bits
 *                     SSSSSSSSSSSSSSSSSSSSSSSSSS all_ch  on/off  ~ch
 *  Key:0x3F86C59F  => 00111111100001101100010110   0       1     1111
 * 
 *  if frame 36 bits
 *                     SSSSSSSSSSSSSSSSSSSSSSSSSS  all_ch dimm  ~ch   dimm_level
 *  Key:0x42D2E8856 => 01000010110100101110100010   0      X    0101  0110
 * 
 */
```
https://github.com/flipperdevices/flipperzero-firmware/pull/1622/files#diff-a3f3c8d1accfa657ef2e2f9047d9109262a6b7785dd771ba88ddcede8c960433R347-R375


##### rev_off.sub
```
Filetype: Flipper SubGhz Key File
Version: 1
Frequency: 433920000
Preset: FuriHalSubGhzPresetOok650Async
Protocol: Intertechno_V3
Bit: 32
Key: 00 00 00 00 3F 86 C5 8F
```
(aus)


#### rev_on.sub
```
Filetype: Flipper SubGhz Key File
Version: 1
Frequency: 433920000
Preset: FuriHalSubGhzPresetOok650Async
Protocol: Intertechno_V3
Bit: 32
Key: 00 00 00 00 3F 86 C5 9F
```
(an)

