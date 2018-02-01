#!/usr/bin/env python3
"""
struct cob_file_entry {
    uint8_t filenameLength;
    uint16_t fileSize;
    uint32_t offset;
    char[filenameLength] filename;
}
"""
import struct, sys, os

cob = open(sys.argv[1], "rb")
magic = cob.read(4)
if magic != b"CoB1":
    raise "invalid magic "+str(magic)

filelist = []

fnlen = cob.read(1)[0]
while fnlen > 0:
    buf = cob.read(6)
    filesize, offset = struct.unpack("<HL", buf)
    fn = cob.read(fnlen).decode()
    print(fnlen, filesize, offset, fn)
    filelist.append((fn, filesize, offset))
    fnlen = cob.read(1)[0]

for fn, filesize, offset in filelist:
    buf = cob.read(filesize)
    target = "extracted/"+fn
    os.makedirs(target[:target.rindex("/")], exist_ok=True)
    outfile = open("extracted/"+fn,"wb")
    outfile.write(buf)
    outfile.close()
    print("Extracted "+str(filesize)+" bytes into "+fn)



