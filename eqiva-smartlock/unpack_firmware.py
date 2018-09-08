#!/usr/bin/env python3
import struct, sys
from binascii import unhexlify

def printf(str, *args):
    sys.stderr.write(str % args)

with open(sys.argv[1], "rb") as f:
    while True:
        data = f.read(4)
        if len(data)<4: printf("eof\n"); break
        length, = struct.unpack("!H", unhexlify(data))
        
        sys.stdout.buffer.write(unhexlify(f.read(2 * length - 4)))
        cksum, = struct.unpack("!H", unhexlify(f.read(4)))
        printf("Converted block of 0x%x bytes with cksum 0x%04x\n", length, cksum)

