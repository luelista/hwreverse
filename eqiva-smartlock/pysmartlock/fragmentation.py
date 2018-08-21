from binascii import hexlify

def ceildiv(a, b):
    return -(-a // b)

def chunks(l, n):
    """Yield successive n-sized chunks from l."""
    for i in range(0, len(l), n):
        yield l[i:i + n]

def fragment_header(is_first, num_remaining):
    return bytes([ ( 0x80 if is_first else 0x00 ) | (num_remaining & 0x7f) ])

def fragment_command(cmd):
    cmddata = cmd.serialize()
    PAYLOAD_SIZE  = 15
    frame_count = ceildiv(len(cmddata), PAYLOAD_SIZE)
    #print("fragment_command", "datalen=",len(cmddata), "fragment_count=",frame_count, hexlify(cmddata))
    frames = []
    for i in range(frame_count):
        frames.append(fragment_header(i == 0, frame_count - i - 1) +
            cmddata[i*PAYLOAD_SIZE : (i+1)*PAYLOAD_SIZE])
    return frames

class Defragmenter:
    def __init__(self):
        self.remaining = 0
        self.buf = bytes()
    def handle(self, fragmentData):
        index = fragmentData[0] & 127
        sendAck = True
        if (fragmentData[0] & 128) != 0:
            # new packet begins
            self.remaining = index
            self.buf = fragmentData[1:]
        elif self.remaining - 1 != index:
            print("Invalid fragment received")
            return False, None
        else:
            self.buf += fragmentData[1:]
            self.remaining = index
        if index == 0:
            return False, self.buf
        else:
            return True, None






