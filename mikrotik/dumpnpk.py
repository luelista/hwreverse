#!/usr/bin/env python
###############################################################################
# source: http://routing.explode.gr/node/96
# notes : slightly modified to create directories and save the files
###############################################################################
# npk format
# ---
# 0-4  : '\x1e\xf1\xd0\xba'
# 4-8  : len(data - 8) ===> The size of the package
# 8-14 : '\x01\x00 \x00\x00\x00'
# 14-30: description ===> 16 chars to put a short name
# 30-34: ?? | ==> version #1 - used in this header again (revision, 'f' (102), minor, major)
# 34-38: ?? | ==> version #2 - used in the data part (epoch time of package build)
#           |  Actualy seems like header[30:42] == each_data_header[12:24]...
#           |  Both appear as integers in /var/pdb/.../version
# 38-42: 0
# 42-46: 0
# 46-48: 16
# 48-50: 4 |
# 50-52: 0 | ==> Maybe int size of the architecture identifier that follows
# 52-56: "i386"
# 56-58: 2
# 58-62: long description size ===> how many chars follow
# 62-x : long description text
#   +24: '\x03\x00"\x00\x00\x00\x01\x00system\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00'
#   +8 : '2f\t\x02\x00\x00\x00\x00' |
#   +8 : '2f\t\x02\x00\x00\x00\x00' | ==> two same headers (separators?)
#                                         first 4 like header[30:34]
#                                         last 4 always 0
#
# what follows are headers of 1 short (type) and 1 int (size):
# type 7, 8: some kind of script
# type 4: data
#
# the content is directly after the header
#
# in case of data it is commpressed with zlib
#
# uncompressed data has 30 byte headers:
#
# 0-1  : permissions: 237 is executable (755), 164 is non executable (644) |
# 1-2  : file type: 65 dir, 129 file                                       | ==> ST_MODE from stat()
# 2-4  : 0 - could be user/group
# 4-8  : 0 - could be user/group
# 8-12 : last modification time (ST_MTIME) as reported by stat()
# 12-24: version stuff and a 0 (see above...)
# 24-28: data size
# 28-30: file name size
#
# then comes the file name and after that the data
###############################################################################

import os
import sys
import zlib
from struct import pack, unpack
from time import ctime
from binascii import hexlify


type_names = {
		1: 'package_header',
		2: 'package_description',
		3: 'package_depends',
		4: 'files_zlib',
		7: 'script_oninstall',
		8: 'script_onuninstall',
		9: 'package_signature',
		16: 'architecture',
		18: 'system_header',
		21: 'squashfs',
		23: 'hash',
		}
for k,v in type_names.iteritems():
	globals()['TYPE_'+v.upper()] = k

def parse_tlv(header, i):
    typ, length = unpack("HI", header[i:i+6])
    value = header[i+6:i+6+length]
    return i+6+length, typ, value

def parse_dependencies(contents):
	count, = unpack("H", contents[0:2])
	print "  Number of dependencies: ",count
	for i in range(count):
	    parse_package_header(contents[2+i*32:2+i*32+32])
	    

def parse_package_header(header):
	shortdesc, revision, unk1, minor, major, timestamp = unpack("16sBBBBI", header[0:24])
	shortdesc = shortdesc.split('\0', 1)[0]
	print "  Short description: ", shortdesc
	print "  Version: ", "%d.%d.%d"%(major, minor, revision)
	print "  Build time: ", ctime(timestamp)
	print "  Unknown: ", unk1, hexlify(header[24:])
	return shortdesc

def parse_npk(filename):

	f = open(filename, "r")
	data = f.read()
	f.close()

	header = data[:8]
	#dsize = unpack("I", header[58:62])[0]	# Description size
	#header += data[62:62+dsize+40]

	print "[*] " + repr(header[38:58])
	print "[i] Magic:", repr(header[0:4]), "should be:", repr('\x1e\xf1\xd0\xba')
	print "[i] Size after this:", unpack("I", header[4:8])[0], "Header size:", len(header), "Data size:", len(data)
	#print "[i] Unknown stuff:", repr(header[8:14]), "should be:", repr('\x01\x00 \x00\x00\x00')
	#print "[i] Short description:", header[14:30]
	#print "[i] Revision, unknown, Minor, Major:", repr(header[30:34]), unpack("BBBB", header[30:34])
	#print "[i] Build time:", repr(header[34:38]), ctime(unpack("I", header[34:38])[0])
	#print "[i] Some other numbers:", unpack("IIHHH", header[38:52]), "should be: (0, 0, 16, 4, 0)"
	#print "[i] Architecture:", header[52:56]
	#print "[i] Another number:", unpack("H", header[56:58]), "should be: (2,)"
	#print "[i] Long description:", repr(header[62:62+dsize])
	#print "[i] Next 24 chars:", repr(header[62+dsize:62+dsize+24])
	#print "    Should be:", repr('\x03\x00"\x00\x00\x00\x01\x00system\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00')
	#print "[i] Separators:", repr(header[62+dsize+24:62+dsize+32]), repr(header[62+dsize+32:62+dsize+40])
	#print "    First 4:", unpack("BBBB",header[62+dsize+24:62+dsize+28]), unpack("BBBB",header[62+dsize+32:62+dsize+36])
	#print ""

	data = data[8:]
	res=[]
	while len(data)>6:
		type = unpack("H", data[:2])[0]
		size = unpack("I", data[2:6])[0]
		if type == TYPE_PACKAGE_HEADER:
			print ""
			print "----------------------------------------"
			
		print "\x1b[1;33m%s\x1b[0m  len=0x%08x"%(type_names.get(type, '??? 0x%04x'%type), size)
		contents = data[6:6+size]
		if type == TYPE_FILES_ZLIB:
			contents = zlib.decompress(contents)
			print "[i] Uncompressing data..."
		elif type == TYPE_SCRIPT_ONINSTALL:
			print "[i] Contents (oninstall):", repr(contents)
		elif type == TYPE_SCRIPT_ONUNINSTALL:
			print "[i] Contents (onuninstall):", repr(contents)
		elif type == 18 or type == 1: #short description + version number
			pkgname = parse_package_header(contents)
		elif type == TYPE_PACKAGE_DEPENDS: 
			parse_dependencies(contents)
		elif type == TYPE_ARCHITECTURE:
			print "  Architecture: ", contents
		elif type == TYPE_SQUASHFS: #squashfs
			if size == 0:
				print "Skipping empty squashfs"
			else:
				print "  Writing to "+pkgname+".squashfs"
				with open("./"+pkgname+".squashfs", "wb") as f:
				    f.write(contents)
		elif type == TYPE_HASH:
			print "  Hash: ", contents
		elif type == TYPE_PACKAGE_DESCRIPTION:
			print "  Long description:", contents
		elif type == TYPE_PACKAGE_SIGNATURE:
			print "Signature: ", hexlify(contents)
                else:
                        if size<50:
				print "  raw:",repr(contents)
			else:
				print "  raw:",repr(contents[0:50]),"..."

		res.append({"type": type, "size": size, "contents": contents})
		data = data[6+size:]
	print "[i] Returning the raw header and the rest of the file (each part in a list)"
	return header, res

def parse_data(data):
	res = []
	while len(data)>30:
		header = data[:30]
		dsize = unpack("I", header[24:28])[0]
		fsize = unpack("H", header[28:30])[0]
		if len(data) - 30 - fsize - dsize < 0:
			dsize = len(data) - 30 - fsize
		filename = data[30:30+fsize]
		filecontents = data[30+fsize:30+fsize+dsize]
		res.append({"header": header, "file": filename, "data": filecontents})
		data = data[30+fsize+dsize:]
	return res

def extract_files(j):
	print "[i] Dumping files:"
	data = parse_data(j["contents"])
	for k in data:
		perm, type, z1, z2, tim = unpack("BBHII", k["header"][:12])
		if k["file"].startswith("/"):
			k["file"] = "." + k["file"]
		if perm == 237:
			perm = "ex"
		if perm == 164:
			perm = "nx"
		if type == 65:
			type = "dir"
			if not os.path.exists(k["file"]):
				os.mkdir(k["file"])
		if type == 129:
			type = "fil"
			print "[*] -> " + k["file"]
			try:
				f = open(k["file"], "wb+")
				f.write(k['data'])
				f.close()
			except:
				pass
if __name__ == "__main__":
	if len(sys.argv) > 1:
		for i in sys.argv[1:]:
			print "INPUT FILE: ",i
			header, res = parse_npk(i)
			for j in res:
				if j["type"] == TYPE_FILES_ZLIB:
					extract_files(j)
