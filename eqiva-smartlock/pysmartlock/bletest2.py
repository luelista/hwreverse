import gatt
from binascii import hexlify, unhexlify
from command import *
from fragmentation import Defragmenter, fragment_command
from cryptomgr import CryptoMgr, parse_pairing_code

macAddress = 'aa:bb:cc:dd:ee:ff'
pairingData = parse_pairing_code('M........')
# for pairing set:
userId = 0xff; deviceKey = None
# after pairing set:
# userId = 4; deviceKey = unhexlify("***")

manager = gatt.DeviceManager(adapter_name='hci0')




"""class AnyDeviceManager(gatt.DeviceManager):
    def device_discovered(self, device):
        print("Discovered [%s] %s" % (device.mac_address, device.alias()))

manager = AnyDeviceManager(adapter_name='hci1')
manager.start_discovery()
"""
class AnyDevice(gatt.Device):
    def connect_succeeded(self):
        super().connect_succeeded()
        print("[%s] Connected" % (self.mac_address))

    def connect_failed(self, error):
        super().connect_failed(error)
        print("[%s] Connection failed: %s" % (self.mac_address, str(error)))

    def disconnect_succeeded(self):
        super().disconnect_succeeded()
        print("[%s] Disconnected" % (self.mac_address))

    def services_resolved(self):
        super().services_resolved()

        self.fragments_to_send = []
        self.defrag = Defragmenter()
        self.cryptoMgr = CryptoMgr(deviceKey)

        print("[%s] Resolved services" % (self.mac_address))
        for service in self.services:
            print("[%s]  Service [%s]" % (self.mac_address, service.uuid))
            for characteristic in service.characteristics:
                print("[%s]    Characteristic [%s]" % (self.mac_address, characteristic.uuid))
        

        keyble_service = next(
            s for s in self.services
            if s.uuid == '58e06900-15d8-11e6-b737-0002a5d5c51b')

        self.tx_characteristic = next(
            c for c in keyble_service.characteristics
            if c.uuid == '3141dd40-15db-11e6-a24b-0002a5d5c51b')
        rx_characteristic = next(
            c for c in keyble_service.characteristics
            if c.uuid == '359d4820-15db-11e6-82bd-0002a5d5c51b')
        rx_characteristic.enable_notifications()

        
        self.send_command(ConnectionRequestCommand(userId, self.cryptoMgr.applicationNonce))
    
    def send_command(self, command):
         print("> Cmd" , command , command.serialize().hex())
         if is_encrypted_command_type(command.getCommandType()):
             #print("")
             #print("java testcryptomgr "+command.getCommandData().hex()+" "+self.cryptoMgr.deviceNonce.hex()+" "+self.cryptoMgr.applicationNonce.hex()+" "+self.cryptoMgr.pairingKey.hex())
             command = self.cryptoMgr.encrypt_packet(command)
             print("> Encrypted ",command)
         fragments = fragment_command(command)
         #for fragment in fragments:
         #    self.tx_characteristic.write_value(fragment)
         self.fragments_to_send.extend(fragments)
         self.send_next_fragment()
        
    def send_next_fragment(self):
        frag = self.fragments_to_send.pop(0)
        print("> Frag ",hexlify(frag))
        self.tx_characteristic.write_value(frag)

    def characteristic_value_updated(self, characteristic, value):
        print("< Frag ", hexlify(value))
        sendAck, packetData = self.defrag.handle(value)
        if sendAck:
            self.send_command(FragmentAckCommand(value[0]))
        command = parse_command(packetData, self.cryptoMgr)
        
        self.handle_command(command)

    def handle_command(self, command):
        print("< Cmd ",command)
        if isinstance(command, FragmentAckCommand):
            self.send_next_fragment()
        if isinstance(command, ConnectionInfoCommand):
            self.cryptoMgr.deviceNonce = command.deviceNonce
            if command.pairingCtr == userId:
                # already connected/paired
                #self.send_command(RawCommand.COMMAND_OPEN)
                #self.send_command(StatusRequestCommand())
                self.send_command(UserNameSetCommand(userId, "l33t h4xx0r"))
            else:
                # pairing required:
                
                req=self.cryptoMgr.generate_pairing_request(command.pairingCtr, pairingData['key'])
                #print("my output:",req)
                #print("java testcryptomgr "+pairingData['key'].hex()+" "+self.cryptoMgr.deviceNonce.hex()+" "+self.cryptoMgr.applicationNonce.hex()+" "+self.cryptoMgr.pairingKey.hex())
                
                #d=input("Java output? ").split(":")
                #req=PairingRequestCommand(req.pairingCtr, bytes.fromhex(d[0]), req.securityCounter, bytes.fromhex(d[1])) 
                self.send_command(req)
        if isinstance(command, AnswerWithoutSecurityCommand):
            if command.isSuccess():
                print("Success!")
                self.send_command(RawCommand.COMMAND_OPEN)
            else:
                print("Pairing failed, wrong key/pairing not enabled?")
        if command.getCommandType() == CommandTypes.STATUS_CHANGED_NOTIFICATION:
            self.send_command(StatusRequestCommand())


#firmware_version_characteristic.read_value()


device = AnyDevice(mac_address=macAddress, manager=manager)
device.connect()

manager.run()