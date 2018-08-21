
from aes import AES
import struct
import os
import re
from fragmentation import chunks
from binascii import unhexlify, hexlify
from command import CommandTypes, PairingRequestCommand, SecureCommand

def parse_pairing_code(qrcode_data):
    m = re.fullmatch("M([A-F0-9]{12})K([A-F0-9]{32})([A-Z0-9]{10})", qrcode_data)
    mac = ":".join(chunks(m.group(1),2))
    return {'mac': mac, 'key': unhexlify(m.group(2)), 'label': m.group(3) }

def aes_ecb_encrypt(data, key):
    algo = AES(key)
    encr = bytes(algo.encrypt(data))
    
    #print ("AES(data=%s, key=%s) -> %s"%(data.hex(), key.hex(), encr.hex()))
    return encr

def build_block_nonce(nonceType, commandType, directionalNonce, packetSecurityCounter, blockCounter):
    nonce= struct.pack("!BB8sHHH",
        nonceType, commandType, directionalNonce, 0, packetSecurityCounter, blockCounter)
    #print("block_nonce : ",hexlify(nonce))
    return nonce

def ceildiv(a, b):
    return -(-a // b)

class CryptoMgr:
    def __init__(self, pairingKey):
        self.nonceCounter = 0
        self.pairingKey = pairingKey

        # nonce for device to app packets, transmitted in CONNECTION_REQUEST
        self.applicationNonce = os.urandom(8)
        self.applicationCounter = 0
        # nonce for app to device packets, transmitted in CONNECTION_INFO
        self.deviceNonce = None
        self.deviceCounter = 0

    
    def internal_crypto(indata, commandType, directionalNonce, packetSecurityCounter, pairingKey):
        indata = bytearray(indata)
        blockCount = ceildiv(len(indata), 16)
        for block in range(blockCount):
            ctr = build_block_nonce(1, commandType, directionalNonce, packetSecurityCounter, block + 1)
            
            key = aes_ecb_encrypt(ctr, pairingKey)
            for j in range(min(16, len(indata) - 16*block)):
                indata[16*block + j] ^= key[j]
        return indata

    
    def generate_auth_value(plaintext, commandType, directionalNonce, packetSecurityCounter, pairingKey):
        blockCount = ceildiv(len(plaintext), 16)
        padded = plaintext + (16*blockCount - len(plaintext)) * b"\0"
        #print("generate_auth_value : padded=",hexlify(padded))
        # hash the data
        nonce = bytearray(build_block_nonce(9, commandType, directionalNonce, packetSecurityCounter, len(plaintext)))
        preHash = aes_ecb_encrypt(nonce, pairingKey)
        for i in range(blockCount):
            for j in range(16):
                nonce[j] = preHash[j] ^ padded[(i * 16) + j]
            preHash = aes_ecb_encrypt(nonce, pairingKey)

        # take first four bytes
        hash = bytearray(preHash[0:4])

        # encrypt it again for good measure
        nonce = build_block_nonce(1, commandType, directionalNonce, packetSecurityCounter, 0)
        key = aes_ecb_encrypt(nonce, pairingKey)
        for j in range(4):
            hash[j] ^= key[j]

        return hash

    def encrypt_packet(self, packet):
        plaintext = packet.getCommandData()
        padding_length = (15-(len(plaintext)-9)%15)-1
        plaintext += padding_length * b"\0"
        self.deviceCounter += 1
        #print("plaintext: "+plaintext.hex())
        ciphertext = CryptoMgr.internal_crypto(plaintext, packet.getCommandType(), self.deviceNonce, self.deviceCounter, self.pairingKey)
        #print("ciphertext: "+ciphertext.hex())
        authValue = CryptoMgr.generate_auth_value(plaintext, packet.getCommandType(), self.deviceNonce, self.deviceCounter, self.pairingKey)
        return SecureCommand(packet.getCommandType(), ciphertext, self.deviceCounter, authValue)

    def decrypt_packet(self, packet):
        if self.applicationCounter >= packet.securityCounter:
            raise Exception("Could not decrypt frame, the device security counter is lower than previous received") 
        plaintext = CryptoMgr.internal_crypto(packet.encryptedData, packet.getCommandType(), self.applicationNonce, packet.securityCounter, self.pairingKey)
        authValue = CryptoMgr.generate_auth_value(plaintext, packet.getCommandType(), self.applicationNonce, packet.securityCounter, self.pairingKey)
        if authValue != packet.authValue:
            raise Exception("WARNING: Mismatch in authValue!!\ncalculated="+authValue.hex()+"\nreceived=  "+packet.authValue.hex())
        self.applicationCounter = packet.securityCounter
        return bytes([packet.getCommandType()]) + plaintext

    def generate_pairing_request(self, pairingCtr, pairingQrcodeKey):
        self.deviceCounter += 1
        self.pairingKey = os.urandom(16)
        print("Generating device key : ",hexlify(self.pairingKey))
        plaintext = bytes([pairingCtr]) + self.pairingKey + bytes([0,0,0,0,0,0])
        #print("Pairing plaintext: ",len(plaintext), hexlify(plaintext))
        ciphertext = CryptoMgr.internal_crypto(
            self.pairingKey, CommandTypes.PAIRING_REQUEST,
            self.deviceNonce, self.deviceCounter, pairingQrcodeKey)
        authValue = CryptoMgr.generate_auth_value(
            plaintext, CommandTypes.PAIRING_REQUEST,
            self.deviceNonce, self.deviceCounter, pairingQrcodeKey)
        return PairingRequestCommand(pairingCtr, ciphertext, self.deviceCounter, authValue)


